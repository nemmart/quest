// src/runtime/i_alloc.cpp
//
// See i_alloc.hpp for scope and docs/I_ALLOC.md for the derivation.
// Residue tables: I_ALLOC.md "Residue maps", verified word-for-word
// against docs/captures/i_alloc-master.txt and i_freew-master.txt.

#include "i_alloc.hpp"
#include "i_lock.hpp"
#include "../hw/Machine.hpp"
#include "../hw/Memory.hpp"
#include "../hw/RTBridge.hpp"
#include "../hw/RTStubs.hpp"
#include "../debug/Capture.hpp"

namespace {

// P24 CORRECTION (Aug 2026, wide-carry fix): this used to be a
// verbatim replica of the OLD (>>31) add() carry formula. Now the ALU
// carry-out (bit 32), matching the fixed hw/EagleInstruction.cpp. Every
// call site discards c (c_unused) — the size-class arithmetic never
// consumes it — so this is formula hygiene, not a behavior change here.
int32_t add_c(int64_t src, int64_t dst, int32_t& c) {
  int64_t result = dst + src;
  int64_t carry = ((dst & 0xFFFFFFFF) + (src & 0xFFFFFFFF)) >> 32;
  c = static_cast<int32_t>(carry & 0x01);
  return static_cast<int32_t>(result & 0xFFFFFFFF);
}

constexpr int32_t MINUS_ONE = -1;

// Entry / helper addresses (quest-rt.dis).
constexpr uint32_t A_I_ALLOC   = 0x7017E866;
constexpr uint32_t A_RET_ALLOC = 0x7017E86B;  // e868 push, ISZTS-bumped twice
constexpr uint32_t A_RET_UNL_A = 0x7017E89D;  // e89B push (alloc's unlock call)
constexpr uint32_t A_RET_FREE  = 0x7017E958;  // e956 push (free's unlock call)
constexpr uint32_t A_RET_WATER = 0x7017E9CD;  // e9CB push (coalescer -> water helper)
constexpr uint32_t A_UNLOCK_RET= 0x7017EA08;  // LCALL I.UNLOCK return (wrapper)

} // namespace

namespace rt {

int32_t heap_class_size(int32_t request, int32_t heap_class) {
  int32_t c_unused;
  int32_t n = request;
  // LDSP valid range [1,4]; out of range falls through to the class-3
  // path at 0x7017E88A (I_ALLOC.md "LDSP mechanics").
  switch(heap_class) {
   case 1:   // 7017e881: WNADI 31 ; WLSHI -5 (logical)
    n = add_c(31, n, c_unused);
    n = static_cast<int32_t>(static_cast<uint32_t>(n) >> 5);
    break;
   case 2:   // 7017e886: WADI 3 ; WLSHI -2 (logical)
    n = add_c(3, n, c_unused);
    n = static_cast<int32_t>(static_cast<uint32_t>(n) >> 2);
    break;
   case 4:   // straight to the join
    break;
   case 3: default:  // 7017e88a: WINC ; WHLV (arithmetic >>1)
    n = add_c(1, n, c_unused);
    n = n >> 1;
    break;
  }
  n = n << 1;                    // 7017e88c WLSI 1,0 — shift by ONE (empirical)
  n = add_c(4, n, c_unused);     // 7017e88d WADI 4,0 — header
  if(!(8 <= n))                  // 7017e88e-91: minimum 8 (WSLE skip)
    n = 8;
  return n;
}

} // namespace rt

namespace emu_rt {

namespace {

struct HeapView {
  int32_t brk, mode, freeq, defer, lo, hi, owner;
  void load(hw::Memory& m) {
    brk  = m.read_wide(rt::HEAP_BREAK);
    mode = m.read_wide(rt::HEAP_MODE);
    freeq= m.read_wide(rt::HEAP_FREEQ);
    defer= m.read_wide(rt::HEAP_DEFER);
    lo   = m.read_wide(rt::HEAP_LOWMARK);
    hi   = m.read_wide(rt::HEAP_HIMARK);
    owner= m.read_wide(rt::HEAP_OWNER);
  }
};

// Gates shared by alloc and free — ALL reads, no writes. A false return
// means: run the whole call emulated (RTStubs::entry_address), which is
// correct for every reason a gate can fail (contention -> authentic REC
// abort, chain/list activity -> authentic QSEARCH trap or insert,
// MEMI mode, ownership violation -> authentic O.SERROR).
const char* shared_gate_reason(hw::Machine& machine, const HeapView& h) {
  if(rt::heap_lock_contended(*machine.memory, rt::HEAP_LOCK))
    return "(native-fallback: contended)";
  if(rt::heap_lock_has_waiters(*machine.memory, rt::HEAP_LOCK))
    return "(native-fallback: waiters)";
  if(h.defer > 0)                    // drain would run (WSGT 2,2 at 0x7017EA0C)
    return "(native-fallback: deferred)";
  if(h.mode != 0)                    // MEMI extend mode
    return "(native-fallback: memi-mode)";
  if(h.owner != 0 && h.owner != machine.wsb)   // 0x7017E8A5/E970
    return "(native-fallback: owner)";
  return nullptr;
}

// psr as the body sees it: WSSVS set ovk=1 and cleared ovr
// (hw/EagleStack.cpp). Used for the LCALL frame wides in residue.
int32_t body_psr(hw::RTBridge& bridge) {
  return (bridge.entry_psr() | 0x8000) & ~0x4000;
}

// The heap lock's NET effect across an uncontended acquire+release with
// no waiters: owner wide := 0 (acquire), state wide := 0 (release; the
// transient count and bit cancel). Gates guarantee the preconditions.
void lock_roundtrip(hw::Memory& m) {
  m.write_wide(rt::HEAP_LOCK, 0);
  m.write_wide(rt::HEAP_LOCK + 2, 0);
}

// Fallback: run the whole call emulated ON A SINGLE PAIR BATCH. The
// master's run-to-return produces one native_span batch for this call
// (it cannot see that the clone declined), so the clone must mirror it:
// arm rt_pending_return at the LJSR return so inner native breaks are
// swallowed (hw/Machine.cpp) and the batch ends at the same post-call
// point with native_span set.
uint32_t fallback(hw::Machine& machine, hw::RTBridge& bridge,
                  const char* entry_name, const char* reason) {
  hw::RTStubs::log_call(machine, entry_name, reason);
  // machine.ac[3] still holds the LJSR return address: the native fn
  // runs before any body instruction. (bridge saved_ac covers 0-2 only.)
  (void)bridge;
  machine.rt_pending_return = static_cast<uint32_t>(machine.ac[3]);
  return hw::RTStubs::entry_address(entry_name);
}

} // namespace

uint32_t i_alloc(hw::Machine& machine) {
  hw::RTBridge bridge(machine, hw::RTBridge::SS);
  hw::Memory& mem = *machine.memory;
  HeapView h;
  h.load(mem);

  int32_t request = bridge.entry_ac(0);
  int32_t heap_class = bridge.entry_ac(1);
  int32_t wsp0 = bridge.entry_wsp();
  int32_t F = wsp0 + 12;

  // ---- gates (reads only) ----
  const char* why = shared_gate_reason(machine, h);
  if(!why && h.freeq != MINUS_ONE) why = "(native-fallback: free-list)";
  if(!why && request <= 0) why = "(native-fallback: request)";
  int32_t size = 0, newbreak = 0, block = 0;
  if(!why) {
    size = rt::heap_class_size(request, heap_class);
    newbreak = h.brk - size;
    block = newbreak + 4;
    // Stack-collision check 0x7017E8FE-E902 runs at core depth wsp=F+6:
    // (wsl - size) must exceed that wsp, else the 0x11613 error path.
    if(!((machine.wsl - size) > (F + 6)))
      why = "(native-fallback: collision)";
  }
  if(why)
    return fallback(machine, bridge, "I.ALLOC", why);

  hw::RTStubs::log_call(machine, "I.ALLOC", "(native)");

  // ---- WSSVS image, result patched into saved-ac0 (header writer
  //      0x7017E92A via the passed-in frame pointer) ----
  bridge.emulate_frame_ss();
  int32_t result = (heap_class == 2) ? (block << 1) : block;  // class 2: byte ptr (WLSI 1,2)
  mem.write_wide(static_cast<uint32_t>(F) - 8, result);

  // ---- residue finals (I_ALLOC.md table; last-writer values) ----
  int32_t psr = body_psr(bridge);
  mem.write_wide(static_cast<uint32_t>(F) + 2,  static_cast<int32_t>(A_RET_ALLOC));
  mem.write_wide(static_cast<uint32_t>(F) + 4,  static_cast<int32_t>(A_RET_UNL_A));
  mem.write_wide(static_cast<uint32_t>(F) + 6,  static_cast<int32_t>(rt::HEAP_LOCK));
  mem.write_wide(static_cast<uint32_t>(F) + 8,  (psr << 16) | 1);
  mem.write_wide(static_cast<uint32_t>(F) + 10, size);
  mem.write_wide(static_cast<uint32_t>(F) + 12, size);
  mem.write_wide(static_cast<uint32_t>(F) + 14, block);
  mem.write_wide(static_cast<uint32_t>(F) + 16, F);
  // c at the unlock LCALL: WADC 1,1 at 0x7017E90B. P24 CORRECTION
  // (user ruling, Aug 29 2026): WADC x,x = add(~x,x) has NO ALU
  // carry-out (0xFFFFFFFF, no bit 32), so c=0 — the pre-fix c=1 (and
  // the 0xF017EA08 in the old capture) was the >>31 bug. The fixed
  // master saves 0x7017EA08 here.
  mem.write_wide(static_cast<uint32_t>(F) + 18,
                 static_cast<int32_t>(A_UNLOCK_RET));

  // ---- heap effects ----
  machine.wsl = machine.wsl - size;                       // STASL 0x7017E903
  mem.write_wide(rt::HEAP_BREAK, newbreak);               // LWSTA 0x7017E906
  mem.write_wide(rt::HEAP_LOWMARK, newbreak + 2);         // water 0x7017E937
  mem.write_wide(static_cast<uint32_t>(newbreak), MINUS_ONE);   // sentinel 0x7017E90C
  mem.write_wide(static_cast<uint32_t>(block) - 2, -size);      // header 0x7017E920
  mem.write_wide(static_cast<uint32_t>(block) + size - 4, -size);  // trailing 0x7017E922
  lock_roundtrip(mem);

  debug::Capture::native_footprint(machine);
  uint32_t pc = bridge.native_return_ss(wsp0);
  machine.ac[0] = result;   // WRTN restores the patched saved-ac0 slot
  return pc;
}

namespace {

// Shared body of the I.FREE family. `block` is the normalized word
// pointer; `entry_name` names the entry for logs and fallback.
uint32_t free_common(hw::Machine& machine, hw::RTBridge& bridge,
                     const char* entry_name, int32_t block) {
  hw::Memory& mem = *machine.memory;
  HeapView h;
  h.load(mem);

  int32_t wsp0 = bridge.entry_wsp();
  int32_t F = wsp0 + 12;

  // ---- gates (reads only) ----
  const char* why = shared_gate_reason(machine, h);
  int32_t leading = 0, size = 0;
  if(!why) {
    leading = mem.read_wide(static_cast<uint32_t>(block) - 2);
    size = -leading;
    if(!(leading < 0))                                        // allocated block
      why = "(native-fallback: not-allocated)";
    else if(!(h.lo <= block && block <= h.hi))                // WCLM 0x7017E825
      why = "(native-fallback: range)";
    else if(!(h.lo <= block + size - 4 && block + size - 4 <= h.hi))   // I?INHPW second WCLM
      why = "(native-fallback: range-end)";
    else if(static_cast<int32_t>(mem.read_wide(static_cast<uint32_t>(block) + size - 4)) != leading)  // 0x7017E82D
      why = "(native-fallback: trailing)";
    // read_wide returns uint32_t; WSGT is a SIGNED compare — cast, or a
    // -1 sentinel reads as 4294967295 > 0 (caught by gate-reason logs).
    else if(static_cast<int32_t>(mem.read_wide(static_cast<uint32_t>(block) - 4)) > 0)       // predecessor merge 0x7017E981
      why = "(native-fallback: pred-merge)";
    else if(static_cast<int32_t>(mem.read_wide(static_cast<uint32_t>(block) + size - 2)) > 0)  // successor merge 0x7017E9AB
      why = "(native-fallback: succ-merge)";
    else if(block != h.brk + 4)                               // bottom-adjacent 0x7017E9C4
      why = "(native-fallback: not-adjacent)";
  }
  if(why)
    return fallback(machine, bridge, entry_name, why);

  hw::RTStubs::log_call(machine, entry_name, "(native)");

  int32_t restored = block + size - 4;   // 0x7017E9C6-C8: break := block-4+size

  // ---- WSSVS image (saved-ac0 = ENTRY ac0, unnormalized: the
  //      normalization runs AFTER WSSVS and nothing patches the slot) ----
  bridge.emulate_frame_ss();

  // ---- residue finals ----
  int32_t psr = body_psr(bridge);
  mem.write_wide(static_cast<uint32_t>(F) + 2,  static_cast<int32_t>(A_RET_FREE));
  mem.write_wide(static_cast<uint32_t>(F) + 4,  static_cast<int32_t>(rt::HEAP_LOCK));
  mem.write_wide(static_cast<uint32_t>(F) + 6,  (psr << 16) | 1);
  mem.write_wide(static_cast<uint32_t>(F) + 8,  block);
  mem.write_wide(static_cast<uint32_t>(F) + 10, size);
  mem.write_wide(static_cast<uint32_t>(F) + 12, block + size);
  mem.write_wide(static_cast<uint32_t>(F) + 14, F);
  // c=0 at the unlock LCALL: WNEG of -size (nonzero) borrows, so the
  // fixed ALU carry is 0. P24 NOTE: same VALUE as pre-fix (the old
  // >>31 formula also gave 0 here), but the justification is now plain
  // borrow semantics; the old "NOT borrow intuition" caveat described
  // the buggy formula and is retired.
  mem.write_wide(static_cast<uint32_t>(F) + 16, static_cast<int32_t>(A_UNLOCK_RET));
  mem.write_wide(static_cast<uint32_t>(F) + 18, static_cast<int32_t>(A_RET_WATER));
  mem.write_wide(static_cast<uint32_t>(F) + 20, block);   // water helper WPSH scratch

  // ---- heap effects ----
  mem.write_wide(rt::HEAP_BREAK, restored);               // 0x7017E9C8
  mem.write_wide(rt::HEAP_LOWMARK, restored + 2);         // water helper
  mem.write_wide(static_cast<uint32_t>(restored), MINUS_ONE);  // sentinel 0x7017E9CE
  machine.wsl = machine.wsl + size;                       // 0x7017E9D2-D5
  lock_roundtrip(mem);

  debug::Capture::native_footprint(machine);
  return bridge.native_return_ss(wsp0);   // nothing patched: entry ACs return
}

} // namespace

uint32_t i_freeb(hw::Machine& machine) {
  hw::RTBridge bridge(machine, hw::RTBridge::SS);
  // 0x7017E947 WMOVR 0: byte pointer -> word pointer (logical >>1)
  int32_t block = static_cast<int32_t>(static_cast<uint32_t>(bridge.entry_ac(0)) >> 1);
  return free_common(machine, bridge, "I.FREEB", block);
}

uint32_t i_freew(hw::Machine& machine) {
  hw::RTBridge bridge(machine, hw::RTBridge::SS);
  return free_common(machine, bridge, "I.FREEW", bridge.entry_ac(0));
}

uint32_t i_free(hw::Machine& machine) {
  hw::RTBridge bridge(machine, hw::RTBridge::SS);
  // 0x7017E94E WSGE 0,0 ; WMOVR 0: halve ONLY if negative (a byte
  // pointer has bit 31 set for segment-7 addresses).
  int32_t a = bridge.entry_ac(0);
  int32_t block = (a >= 0) ? a
                : static_cast<int32_t>(static_cast<uint32_t>(a) >> 1);
  return free_common(machine, bridge, "I.FREE", block);
}

} // namespace emu_rt
