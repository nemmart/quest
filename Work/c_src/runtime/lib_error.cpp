// src/runtime/lib_error.cpp
//
// See lib_error.hpp for scope and docs/Project2/DERIVATION.md for the
// instruction-level derivation, residue maps, and gate rationale.
// Every memory word written here is a "final value" from the
// DERIVATION.md residue tables; intermediate emulated writes that a
// later writer overwrites are not replicated — with one deliberate
// exception: the inner heap calls are made by invoking the REAL
// emu_rt::i_freew / emu_rt::i_alloc at staged machine state, so their
// residue (including [F+36], which survives) and heap effects are
// reproduced by the same validated code on both engines.

#include "lib_error.hpp"
#include "error_handler.hpp" // Generation 3: handler_mode (mv-attic gate)
#include "t_area.hpp"
#include "i_alloc.hpp"
#include "i_lock.hpp"
#include "../hw/Machine.hpp"
#include "../hw/Memory.hpp"
#include "../hw/RTBridge.hpp"
#include "../hw/RTStubs.hpp"
#include "../hw/NativeRegistry.hpp"
#include "../os/OSProcess.hpp"
#include "../debug/Capture.hpp"
#include "../debug/CallStack.hpp"
#include <cstdio>
#include <cstdlib>

namespace {

// Entry / site addresses (quest-rt.dis; stable across the project).
constexpr uint32_t A_LIB_ERROR   = 0x7017E33A;
constexpr uint32_t A_DEFAULT_EH  = 0x7017E3D2;
constexpr uint32_t A_O_SIGNAL    = 0x7017EDED;
constexpr uint32_t A_RET_FREEW   = 0x7017E381;  // LJSR I.FREEW return
constexpr uint32_t A_RET_ALLOC   = 0x7017E39C;  // LJSR I.ALLOC return
constexpr uint32_t A_RET_XCALL   = 0x7017E3D0;  // handler dispatch return (?LIB_ERROR's WRTN)
constexpr uint32_t A_RET_TAREA_H = 0x7017E3D8;  // handler's T?AREA return (residue)
constexpr uint32_t A_RET_OSIG    = 0x7017E3EF;  // O?SIGNAL return (handler's WRTN)

// psr as the body sees it after WSAVS (ovk=1) + first LCALL (ovr=0).
int32_t body_psr(int32_t entry_psr) {
  return (entry_psr | 0x8000) & ~0x4000;
}

// Nested-span rule + standard fallback (DERIVATION.md "Dispatch and
// pairing design"). A dispatch inside another routine's
// emulated-fallback span must NOT re-arm rt_pending_return: the
// master's run-to-return is keyed on the OUTER return, so the clone's
// span must end there too. Outside a span, this is the standard
// single-span fallback (METHOD.md sec. 12).
uint32_t fall_back(hw::Machine& machine, const char* name, const char* reason) {
  hw::RTStubs::log_call(machine, name, reason);
  if(machine.rt_pending_return == 0)
    machine.rt_pending_return = static_cast<uint32_t>(machine.ac[3]);
  return hw::RTStubs::entry_address(name);
}

// The heap gates shared with i_alloc.cpp's shared_gate_reason (that
// function is file-local there; the constants and lock predicates are
// the exported contract). All reads, no writes.
const char* heap_shared_gate(hw::Machine& machine) {
  hw::Memory& mem = *machine.memory;
  if(rt::heap_lock_contended(mem, rt::HEAP_LOCK))
    return "(native-fallback: contended)";
  if(rt::heap_lock_has_waiters(mem, rt::HEAP_LOCK))
    return "(native-fallback: waiters)";
  if(static_cast<int32_t>(mem.read_wide(rt::HEAP_DEFER)) > 0)
    return "(native-fallback: deferred)";
  if(static_cast<int32_t>(mem.read_wide(rt::HEAP_MODE)) != 0)
    return "(native-fallback: memi-mode)";
  int32_t owner = static_cast<int32_t>(mem.read_wide(rt::HEAP_OWNER));
  if(owner != 0 && owner != machine.wsb)
    return "(native-fallback: owner)";
  return nullptr;
}

// I.FREEW's own gates for a given block (mirrors free_common's checks;
// a false here means emu_rt::i_freew would fall back mid-flight, so
// ?LIB_ERROR must fall back at ITS entry instead). Returns the block
// size through size_out on success.
const char* freew_gates(hw::Machine& machine, int32_t block, int32_t& size_out) {
  hw::Memory& mem = *machine.memory;
  int32_t brk = static_cast<int32_t>(mem.read_wide(rt::HEAP_BREAK));
  int32_t lo  = static_cast<int32_t>(mem.read_wide(rt::HEAP_LOWMARK));
  int32_t hi  = static_cast<int32_t>(mem.read_wide(rt::HEAP_HIMARK));
  int32_t leading = static_cast<int32_t>(mem.read_wide(static_cast<uint32_t>(block) - 2));
  int32_t size = -leading;
  size_out = size;
  if(!(leading < 0))
    return "(native-fallback: free/not-allocated)";
  if(!(lo <= block && block <= hi))
    return "(native-fallback: free/range)";
  if(!(lo <= block + size - 4 && block + size - 4 <= hi))
    return "(native-fallback: free/range-end)";
  if(static_cast<int32_t>(mem.read_wide(static_cast<uint32_t>(block) + size - 4)) != leading)
    return "(native-fallback: free/trailing)";
  if(static_cast<int32_t>(mem.read_wide(static_cast<uint32_t>(block) - 4)) > 0)
    return "(native-fallback: free/pred-merge)";
  if(static_cast<int32_t>(mem.read_wide(static_cast<uint32_t>(block) + size - 2)) > 0)
    return "(native-fallback: free/succ-merge)";
  if(block != brk + 4)
    return "(native-fallback: free/not-adjacent)";
  return nullptr;
}

// Replay one WRTN from the frame image in memory (authentic algorithm:
// hw/EagleStack.cpp WRTN). Returns the popped return pc. Trusts the
// frame words this translation just wrote (or, for the outer frame,
// bridge.emulate_frame wrote).
uint32_t replay_wrtn(hw::Machine& machine) {
  hw::Memory& mem = *machine.memory;
  machine.wsp = machine.wfp;
  uint32_t F = static_cast<uint32_t>(machine.wsp);
  int32_t value = static_cast<int32_t>(mem.read_wide(F));
  machine.wfp = static_cast<int32_t>(mem.read_wide(F - 2));
  machine.ac[2] = static_cast<int32_t>(mem.read_wide(F - 4));
  machine.ac[1] = static_cast<int32_t>(mem.read_wide(F - 6));
  machine.ac[0] = static_cast<int32_t>(mem.read_wide(F - 8));
  int32_t word = static_cast<int32_t>(mem.read_wide(F - 10));
  machine.ac[3] = machine.wfp;
  machine.set_psr(static_cast<uint32_t>(word) >> 16);
  machine.wsp = static_cast<int32_t>(F) - 12 - 2 * (word & 0x7FFF);
  machine.c = static_cast<uint32_t>(value) >> 31;
  return static_cast<uint32_t>(value) & 0x7FFFFFFF;
}

// Build the ?DEFAULT_ERROR_HANDLER frame + locals + O?SIGNAL argument
// pushes + LCALL word, and set the machine to the exact O?SIGNAL
// dispatch state. Wh = the handler's entry wsp (the wide holding the
// XCALL word); the handler's frame is Fh = Wh+10. ac0/ac1/ac2/ret/c
// are the handler's entry values (its WSAVS image); psr is the psr at
// its entry (already ovk=1/ovr=0 in every reachable path — body_psr
// is applied for the pushed words regardless, matching the emulated
// pushes). B is the condition record base; code is read from [B+1]
// exactly where the emulated body reads it.
void handler_body_to_boundary(hw::Machine& machine, int32_t Wh,
                              int32_t e_ac0, int32_t e_ac1, int32_t e_ac2,
                              uint32_t ret_addr, int32_t e_c, int32_t psr,
                              uint32_t B) {
  hw::Memory& mem = *machine.memory;
  uint32_t Fh = static_cast<uint32_t>(Wh) + 10;
  int32_t pb = body_psr(psr);
  int32_t code = static_cast<int32_t>(mem.read_wide(B + 0x1));

  // WSAVS 3 image.
  mem.write_wide(Fh - 8, e_ac0);
  mem.write_wide(Fh - 6, e_ac1);
  mem.write_wide(Fh - 4, e_ac2);
  mem.write_wide(Fh - 2, machine.wfp);
  mem.write_wide(Fh, static_cast<int32_t>(ret_addr) | (e_c << 31));
  // Locals: WADC 1,1 -> -1 (always; c momentarily 1), WSUB 2,2 -> 0
  // (c := 0 — the boundary carry), code.
  mem.write_wide(Fh + 2, -1);
  mem.write_wide(Fh + 4, 0);
  mem.write_wide(Fh + 6, code);
  // XPEF pushes (arg refs) and the O?SIGNAL LCALL word.
  mem.write_wide(Fh + 8, static_cast<int32_t>(Fh) + 6);
  mem.write_wide(Fh + 10, static_cast<int32_t>(Fh) + 4);
  mem.write_wide(Fh + 12, static_cast<int32_t>(Fh) + 2);
  mem.write_wide(Fh + 14, (pb << 16) | 3);
  // Surviving T?AREA-frame residue (wfp and ret slots; everything else
  // that call wrote is overwritten by the pushes above).
  mem.write_wide(Fh + 16, static_cast<int32_t>(Fh));
  mem.write_wide(Fh + 18, static_cast<int32_t>(A_RET_TAREA_H) | (e_c << 31));

  // O?SIGNAL dispatch state.
  machine.ac[0] = code;
  machine.ac[1] = -1;
  machine.ac[2] = 0;
  machine.ac[3] = static_cast<int32_t>(A_RET_OSIG);
  machine.c = 0;
  machine.ovk = 1;
  machine.ovr = 0;
  machine.wfp = static_cast<int32_t>(Fh);
  machine.wsp = static_cast<int32_t>(Fh) + 14;
}

// QUEST_LIBERROR_VALIDATE=N (validation only): the N-th ?LIB_ERROR
// call in this process goes native and ends with a transfer to the
// (still-emulated) O?SIGNAL entry — the documented deliberate-
// divergence rig. Earlier calls fall back normally, which lets run
// N=2 exercise the old-buffer free path ([B+3] set by signal 1).
// Unset (production): 0 — never; the o-signal-unregistered gate rules.
int32_t validate_target() {
  static const int32_t v = [] {
    const char* e = std::getenv("QUEST_LIBERROR_VALIDATE");
    return e ? static_cast<int32_t>(std::strtol(e, nullptr, 10)) : 0;
  }();
  return v;
}

// True when O?SIGNAL has a REAL translation. The native registry is
// the dispatch mechanism but not the right predicate: every RT entry
// gets a log-and-continue STUB registered (RTStubs), so lookup() is
// non-null even pre-integration. translated_bits marks actual
// translation_table entries only.
bool o_signal_translated() {
  return hw::RTStubs::active &&
         A_O_SIGNAL >= hw::RTStubs::start && A_O_SIGNAL < hw::RTStubs::stop &&
         hw::RTStubs::translated_bits[A_O_SIGNAL - hw::RTStubs::start] != 0;
}

} // namespace

// Project 1 review correction (binding): an unhandled signal runs to
// DEF?ON, an RT-RANGE terminal. A native span must NOT head there —
// the master ends terminal/no-span, the clone ends span/no-terminal,
// and compare_pair reports structural divergence on identical state.
// Nor can the fallback happen inside Project 1's wrapper mid-span
// (instruction counts skew between engines). The ONLY clean point is
// a pure-read gate at THIS routine's entry: predict "no handler will
// be found" and fall back whole, so both engines emulate the entire
// chain to the terminal with equal counts.
//
// The prediction is Project 1's chain-walk semantics, so it is theirs
// to implement; this WEAK default answers "cannot prove a handler
// exists" (false), which makes every signal fall back — safe in every
// configuration — until Project 1's strong definition replaces it at
// link time. Proposed contract (REPORT.md §3): pure reads only, true
// iff the signal walk starting at [wsb-0x40] would find a handler for
// this condition code.
namespace rt {
__attribute__((weak)) bool signal_has_handler(hw::Machine& machine,
                                              int32_t code) {
  (void)machine; (void)code;
  return false;
}
} // namespace rt

namespace {

} // namespace

namespace emu_rt {

uint32_t lib_error(hw::Machine& machine) {
  if(machine.rt_pending_return != 0)
    return hw::RTStubs::entry_address("?LIB_ERROR");   // nested-span rule

  hw::RTBridge bridge(machine);
  hw::Memory& mem = *machine.memory;
  int32_t W = bridge.entry_wsp();
  uint32_t F = static_cast<uint32_t>(W) + 10;
  uint32_t B = rt::t_area(machine) + 8;
  int32_t argc = bridge.arg_count();

  bool o_sig_native = o_signal_translated();

  // ---- gates (reads only; DERIVATION.md gate list) ----
  static int32_t call_ordinal = 0;   // validation rig only (clone-only code)
  call_ordinal++;
  const char* why = nullptr;
  int32_t code_peek = bridge.arg_wide(1);   // pure read, pre-staging
  if(call_ordinal != validate_target()) {
    if(!o_sig_native)
      why = "(native-fallback: o-signal-untranslated)";
    else if(rt::handler_mode == rt::HandlerMode::MV &&
            !rt::signal_has_handler(machine, code_peek))
      // Generation-2 relic, kept verbatim for the mv attic only: the
      // old checker compared strict counts at terminal pairs, so an
      // unhandled signal had to fall back whole for symmetric
      // emulation to the door. Generation 3 makes the terminal pair a
      // crossing (docs/CheckerHistory.md): in native/check modes the
      // chain runs native to the ?FATAL door and transfers there with
      // the contracted state (Contract §3.13).
      why = "(native-fallback: no-handler/terminal-bound)";
  }
  uint32_t H = mem.read_wide(B + 0x1E);
  if(!why && !(H == 0 || H == A_DEFAULT_EH))
    why = "(native-fallback: handler-slot)";
  if(!why && (machine.wsl & static_cast<int32_t>(0x80000000)) == 0 &&
     W + 40 > machine.wsl)
    why = "(native-fallback: headroom)";

  int32_t oldbuf = static_cast<int32_t>(mem.read_wide(B + 0x3));
  int32_t free_size = 0;
  if(!why && oldbuf != 0) {
    why = heap_shared_gate(machine);
    if(!why)
      why = freew_gates(machine, oldbuf, free_size);
  }

  int32_t len = 0, request = 0;
  if(!why && argc > 1) {
    len = bridge.arg_word(2);                       // narrow @[F-14], sign-extended
    if(len < 0)
      why = "(native-fallback: negative-length)";
    if(!why && oldbuf == 0)
      why = heap_shared_gate(machine);              // not yet checked on this path
    // request = ((len+1) >> 1 logical) + 1; carries provably 0.
    request = static_cast<int32_t>(static_cast<uint32_t>(len + 1) >> 1) + 1;
    if(!why && static_cast<int32_t>(mem.read_wide(rt::HEAP_FREEQ)) != -1)
      why = "(native-fallback: free-list)";
    if(!why && request <= 0)
      why = "(native-fallback: request)";
    if(!why) {
      // Collision check against the POST-free heap image, at the inner
      // call's core depth (their F_i = F+16, check at F_i+6 = F+22).
      int32_t alloc_size = rt::heap_class_size(request, 3);
      int32_t wsl_after = machine.wsl + free_size;   // STASL restore, 0 if no free
      if(!((wsl_after - alloc_size) > static_cast<int32_t>(F) + 22))
        why = "(native-fallback: collision)";
    }
  }
  if(why)
    return fall_back(machine, "?LIB_ERROR", why);

  hw::RTStubs::log_call(machine, "?LIB_ERROR", "(native)");

  // ---- committed: WSAVS image, then the body in program order ----
  bridge.emulate_frame();                            // [F-8 .. F]

  if(H == 0) {                                       // lazy install
    mem.write_wide(B + 0x1E, A_DEFAULT_EH);
    mem.write_wide(B + 0x20, 0);
    H = A_DEFAULT_EH;
  }
  mem.write_word(B, mem.read_word(B) | 0x8000);      // WBTO 2,0: signal latch
  int32_t code = bridge.arg_wide(1);                 // wide @[F-12]
  mem.write_wide(B + 0x1, code);

  if(oldbuf != 0) {
    // Stage the exact emulated LJSR state (DERIVATION.md step 4) and
    // run the validated translation; gates above guarantee its native
    // path. ac1 is the entry value (untouched by the body so far).
    machine.ac[0] = oldbuf;
    machine.ac[1] = bridge.entry_ac(1);
    machine.ac[2] = static_cast<int32_t>(B);
    machine.ac[3] = static_cast<int32_t>(A_RET_FREEW);
    machine.c = 0;
    machine.ovk = 1;
    machine.ovr = 0;
    machine.wfp = static_cast<int32_t>(F);
    machine.wsp = static_cast<int32_t>(F) + 4;
    uint32_t pc2 = emu_rt::i_freew(machine);
    if(pc2 != A_RET_FREEW)
      fprintf(stderr, "?LIB_ERROR: inner I.FREEW fell back despite gates (pc=%08X)\n", pc2);
    mem.write_wide(B + 0x3, 0);
  }

  int32_t c_x = 0;                                   // carry at the XCALL
  int32_t wcmv_leftover = 0, wcmv_dst_end = 0;       // e3C2 T?AREA residue inputs
  if(argc > 1) {
    mem.write_word(F + 2, static_cast<uint32_t>(len) & 0xFFFF);  // narrow local
    machine.ac[0] = request;
    machine.ac[1] = 3;
    machine.ac[2] = static_cast<int32_t>(B);
    machine.ac[3] = static_cast<int32_t>(A_RET_ALLOC);
    machine.c = 0;
    machine.ovk = 1;
    machine.ovr = 0;
    machine.wfp = static_cast<int32_t>(F);
    machine.wsp = static_cast<int32_t>(F) + 4;
    uint32_t pc2 = emu_rt::i_alloc(machine);
    if(pc2 != A_RET_ALLOC)
      fprintf(stderr, "?LIB_ERROR: inner I.ALLOC fell back despite gates (pc=%08X)\n", pc2);
    int32_t newbuf = machine.ac[0];                  // patched saved-ac0 return
    mem.write_wide(F + 4, newbuf);
    mem.write_wide(B + 0x3, newbuf);
    // Clamp with the emulated read order (saved local, then a fresh
    // re-read through the argument pointer — alias-faithful). The slot
    // must be addressed from the ENTRY wsp: machine.wsp was re-staged for
    // the inner call, so bridge.arg_* (live-wsp-relative) is wrong here.
    uint32_t slot2 = static_cast<uint32_t>(W) - 4;   // = F-14, the arg-2 ref
    int32_t saved = static_cast<int32_t>(mem.read_word(F + 2));
    saved = (saved << 16) >> 16;
    uint32_t msgp = mem.read_wide(slot2);            // e3B2: @[F-14]
    int32_t fresh = static_cast<int32_t>(mem.read_word(msgp));
    fresh = (fresh << 16) >> 16;
    int32_t n = (fresh >= saved) ? saved : fresh;
    // buffer[0] := clamped length (narrow store through [B+3]).
    mem.write_word(static_cast<uint32_t>(newbuf), static_cast<uint32_t>(n) & 0xFFFF);
    // WCMV: n bytes, forward, from msg byte 2 to buffer byte 2
    // (n <= fresh, so no space padding on any gated path).
    uint32_t msgptr = mem.read_wide(slot2);          // e3BC reload of [F-14]
    uint32_t dstb = static_cast<uint32_t>(newbuf) * 2 + 2;
    uint32_t srcb = msgptr * 2 + 2;
    for(int32_t i = 0; i < n; i++)
      mem.write_byte(dstb + static_cast<uint32_t>(i),
                     mem.read_byte(srcb + static_cast<uint32_t>(i)));
    c_x = (fresh - n) != 0;                          // WCMV: src leftover
    wcmv_leftover = fresh - n;
    wcmv_dst_end = static_cast<int32_t>(dstb) + n;
  }

  // ---- tail: last T?AREA frame (e3C2) residue, XCALL word, then the
  //      exact handler-dispatch state. The T?AREA frame words are
  //      overwritten by the handler's WSAVS image below, but they are
  //      the master-visible state at the XCALL instant (and what a
  //      QUEST_CAPTURE=7017E3D2 ENTRY block shows), so replicate them
  //      for exact intermediate fidelity too.
  int32_t pb = body_psr(bridge.entry_psr());
  int32_t area = static_cast<int32_t>(B) - 8;
  int32_t companion = static_cast<int32_t>(mem.read_wide(B + 0x20));
  int32_t tarea_ac1, tarea_ac2;
  if(argc > 1) {   // post-WCMV registers at the e3C2 call
    tarea_ac1 = wcmv_leftover;
    tarea_ac2 = wcmv_dst_end;
  } else {
    tarea_ac1 = bridge.entry_ac(1);
    tarea_ac2 = static_cast<int32_t>(B);
  }
  mem.write_wide(F + 8, area);                       // patched saved-ac0
  mem.write_wide(F + 10, tarea_ac1);
  mem.write_wide(F + 12, tarea_ac2);
  mem.write_wide(F + 14, static_cast<int32_t>(F));
  mem.write_wide(F + 16, 0x7017E3C6 | (c_x << 31));
  mem.write_wide(F + 6, (pb << 16) | 0);             // XCALL word
  machine.ac[0] = area;
  machine.ac[1] = companion;
  machine.ac[2] = static_cast<int32_t>(H);
  machine.ac[3] = static_cast<int32_t>(A_RET_XCALL);
  machine.c = c_x;
  machine.ovk = 1;
  machine.ovr = 0;
  machine.wfp = static_cast<int32_t>(F);             // the handler's caller frame
  machine.wsp = static_cast<int32_t>(F) + 6;
  debug::Capture::native_footprint(machine);         // XCALL-boundary NATIVE block
                                                     // (diff vs master ENTRY at 7017E3D2)
  handler_body_to_boundary(machine, static_cast<int32_t>(F) + 6,
                           area, companion, static_cast<int32_t>(H),
                           A_RET_XCALL, c_x, pb, B);

  debug::Capture::native_footprint(machine);         // boundary NATIVE block

  if(!o_sig_native)   // QUEST_LIBERROR_VALIDATE rig ONLY: an RT-range
                      // transfer, INTENTIONALLY pairing-divergent (used
                      // to harvest the boundary pair + captures). Never
                      // reachable in production (validate_target()==0).
    return hw::RTBridge::native_transfer(machine, A_O_SIGNAL);

  uint32_t pc3 =
    machine.process->native_registry.lookup(A_O_SIGNAL)(machine);
  if(pc3 != A_RET_OSIG)
    return pc3;   // transfer (handler dispatch / terminal) or their fallback:
                  // rt_pending_return armed by their code; pairing per run_steps.

  // O?SIGNAL returned normally: replay the handler's WRTN, then our own.
  uint32_t mid = replay_wrtn(machine);               // handler frame at wfp = Fh
  if(mid != A_RET_XCALL)
    fprintf(stderr, "?LIB_ERROR: handler WRTN replay pc=%08X (expected %08X)\n",
            mid, A_RET_XCALL);
  // replay_wrtn restored machine.wfp = F (the handler's saved wfp), so
  // the second replay unwinds ?LIB_ERROR's own frame.
  uint32_t ret = replay_wrtn(machine);   // caller wfp restored from [F-2]
  machine.call_stack->native_return(static_cast<int32_t>(ret));
  machine.native_break = true;
  return ret;
}

uint32_t lib_error_code(hw::Machine& machine) {
  if(machine.rt_pending_return != 0)
    return hw::RTStubs::entry_address("?LIB_ERROR_CODE");   // nested-span rule

  hw::RTBridge bridge(machine);
  hw::Memory& mem = *machine.memory;
  uint32_t F = static_cast<uint32_t>(bridge.entry_wsp()) + 10;
  uint32_t B = rt::t_area(machine) + 8;

  hw::RTStubs::log_call(machine, "?LIB_ERROR_CODE", "(native)");

  int32_t code = static_cast<int32_t>(mem.read_wide(B + 0x1));
  bridge.set_return_ac(0, code);          // the saved-ac0 slot patch
  bridge.emulate_frame();                 // [F-8 .. F], slot pre-patched

  // T?AREA residue (LCALL at wsp = F; DERIVATION.md table).
  int32_t pb = body_psr(bridge.entry_psr());
  mem.write_wide(F + 2, (pb << 16) | 0);
  mem.write_wide(F + 4, static_cast<int32_t>(B) - 8);        // patched saved-ac0: area
  mem.write_wide(F + 6, bridge.entry_ac(1));
  mem.write_wide(F + 8, bridge.entry_ac(2));
  mem.write_wide(F + 10, static_cast<int32_t>(F));
  mem.write_wide(F + 12, 0x7017DE2B | (bridge.entry_carry() << 31));

  debug::Capture::native_footprint(machine);
  return bridge.native_return();
}

uint32_t default_error_handler(hw::Machine& machine) {
  if(machine.rt_pending_return != 0)
    return hw::RTStubs::entry_address("?DEFAULT_ERROR_HANDLER");  // nested-span rule

  // Reachable with rt_pending_return == 0 only if some future caller
  // dispatches the handler outside an emulated ?LIB_ERROR (today the
  // e3CD XCALL is the sole site and it always sits inside a fallback
  // span when the clone emulates it — the native ?LIB_ERROR path runs
  // handler_body_to_boundary directly instead). Kept correct anyway.
  hw::RTBridge bridge(machine);
  int32_t W = bridge.entry_wsp();

  const char* why = nullptr;
  if(!o_signal_translated())
    why = "(native-fallback: o-signal-untranslated)";
  if(!why) {
    uint32_t Bp = rt::t_area(machine) + 8;
    int32_t code_peek = static_cast<int32_t>(machine.memory->read_wide(Bp + 0x1));
    if(!rt::signal_has_handler(machine, code_peek))
      why = "(native-fallback: no-handler/terminal-bound)";
  }
  if(!why && (machine.wsl & static_cast<int32_t>(0x80000000)) == 0 &&
     W + 24 > machine.wsl)
    why = "(native-fallback: headroom)";
  if(why)
    return fall_back(machine, "?DEFAULT_ERROR_HANDLER", why);

  hw::RTStubs::log_call(machine, "?DEFAULT_ERROR_HANDLER", "(native)");

  // ac3 still holds the dispatch return address (XCALL set it; nothing
  // native has touched it) — capture before the body overwrites it.
  uint32_t ret_addr = static_cast<uint32_t>(machine.ac[3]);
  uint32_t B = rt::t_area(machine) + 8;
  handler_body_to_boundary(machine, W,
                           bridge.entry_ac(0), bridge.entry_ac(1),
                           bridge.entry_ac(2), ret_addr,
                           bridge.entry_carry(), bridge.entry_psr(), B);

  debug::Capture::native_footprint(machine);

  uint32_t pc3 =
    machine.process->native_registry.lookup(A_O_SIGNAL)(machine);
  if(pc3 != A_RET_OSIG)
    return pc3;
  uint32_t ret = replay_wrtn(machine);
  machine.call_stack->native_return(static_cast<int32_t>(ret));
  machine.native_break = true;
  return ret;
}

} // namespace emu_rt
