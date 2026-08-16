// src/runtime/i_lock.cpp
#include "i_lock.hpp"
#include "../hw/Machine.hpp"
#include "../hw/Memory.hpp"
#include "../hw/RTBridge.hpp"
#include "../hw/RTStubs.hpp"
#include "../debug/Capture.hpp"

namespace rt {

static constexpr int32_t LOCK_BIT=0x8000;   // WSZBO mask: 0x8000>>(32&0x0F)

bool heap_lock_contended(hw::Memory& memory, uint32_t lock) {
  return (memory.read_word(lock+2) & LOCK_BIT) != 0;
}

bool heap_lock_has_waiters(hw::Memory& memory, uint32_t lock) {
  // WMESS tests (wide[lock+2] ^ 0) & 0x0000FFFF, i.e. the low half.
  return (memory.read_wide(lock+2) & 0xFFFF) != 0;
}

void heap_lock_acquire(hw::Memory& memory, uint32_t lock) {
  // 7017e7d4 XNISZ [ac2+0x3]  — count += 1 (mod 0x10000)
  int32_t count=static_cast<int32_t>((memory.read_word(lock+3)+1) & 0xFFFF);
  memory.write_word(lock+3, static_cast<uint32_t>(count));

  // 7017e7d9 WSZBO 2,1 (ac1=32) — set bit 15 of [lock+2]. The
  // instruction ALWAYS writes the word back, set or not; the skip
  // reports the previous state. Callers must have checked
  // heap_lock_contended() first: this replicates only the taken path.
  int32_t word=static_cast<int32_t>(memory.read_word(lock+2));
  memory.write_word(lock+2, static_cast<uint32_t>(word|LOCK_BIT));

  // 7017e7db WSUB 0,0 ; 7017e7dc XWSTA 0,[ac2+0x0] — owner wide := 0
  memory.write_wide(lock, 0);

  // 7017e7de XNDSZ [ac2+0x3] — count -= 1, restoring the entry value
  count=static_cast<int32_t>((static_cast<uint32_t>(count)-1) & 0xFFFF);
  memory.write_word(lock+3, static_cast<uint32_t>(count));
}

int32_t heap_lock_release(hw::Memory& memory, uint32_t lock) {
  // 7017e7f8 WMESS with ac0=0, ac1=0, ac2=lock+2, ac3=0x0000FFFF.
  // Callers must have checked heap_lock_has_waiters() first, so the
  // compare succeeds: the wide is replaced by ac1 (= 0), clearing the
  // lock bit in [lock+2] and the count in [lock+3] together.
  int32_t previous=memory.read_wide(lock+2);
  memory.write_wide(lock+2, 0);
  return previous;
}

} // namespace rt

namespace emu_rt {

// Shared entry handling: one argument, the lock object's address, taken
// from the arg slot (the callers push it with LPEF, so the slot content
// is the address itself — no extra dereference, unlike ?FILL_WORDS).
// Fallback protocol (METHOD.md sec. 12): the master run-to-returns on
// translated_bits alone, so every clone-side fallback must arm
// rt_pending_return at the return address (still in ac3 at native-fn
// time) to produce the matching single native_span batch.
static uint32_t fall_back(hw::Machine& machine, const char* name,
                          const char* reason) {
  hw::RTStubs::log_call(machine, name, reason);
  machine.rt_pending_return = static_cast<uint32_t>(machine.ac[3]);
  return hw::RTStubs::entry_address(name);
}

static bool lock_args(hw::RTBridge& bridge, uint32_t& lock) {
  if(bridge.arg_count()!=1)
    return false;
  lock=bridge.arg_pointer(1);
  return true;
}

uint32_t i_lock(hw::Machine& machine) {
  hw::RTBridge bridge(machine);
  uint32_t lock;

  if(!lock_args(bridge, lock))
    return fall_back(machine, "I.LOCK", "(native-fallback)");

  // Contended: the body would block in SYSCALL 0525 (REC). Detected
  // before any side effect, so emulation can run the whole routine.
  if(rt::heap_lock_contended(*machine.memory, lock))
    return fall_back(machine, "I.LOCK", "(native-fallback: contended)");

  hw::RTStubs::log_call(machine, "I.LOCK", "(native)");
  // WSAVR image; frame_size 0, and the body writes no locals — its
  // whole footprint is the lock object.
  bridge.emulate_frame();
  rt::heap_lock_acquire(*machine.memory, lock);
  debug::Capture::native_footprint(machine);
  return bridge.native_return();
}

uint32_t i_unlock(hw::Machine& machine) {
  hw::RTBridge bridge(machine);
  uint32_t lock;

  if(!lock_args(bridge, lock))
    return fall_back(machine, "I.UNLOCK", "(native-fallback)");

  // Waiters registered: the body would signal them via SYSCALL 0523,
  // which the OS layer does not implement. Fall back rather than abort.
  if(rt::heap_lock_has_waiters(*machine.memory, lock))
    return fall_back(machine, "I.UNLOCK", "(native-fallback: waiters)");

  hw::RTStubs::log_call(machine, "I.UNLOCK", "(native)");
  bridge.emulate_frame();
  rt::heap_lock_release(*machine.memory, lock);
  debug::Capture::native_footprint(machine);
  return bridge.native_return();
}

} // namespace emu_rt
