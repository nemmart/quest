// src/runtime/r_signal.cpp
//
// Derivation: docs/Project4/DERIVATION.md §5; per-instruction listing
// and semantics evidence there. The plain-return path's footprint is
// the WSAVS image ALONE:
//   - the walk (ef69..ef6f) is loads and register moves only;
//   - the (code, type) loads (ef56..ef67) are pure reads whichever
//     argc path runs, and the values do not influence the walk;
//   - every clobbered register and flag is restored by WRTN from the
//     image / frame word.
// Master capture (run3, seq=36) confirms bit-for-bit: pure image,
// entry registers restored, wsp popped by 2 (argc=0), pc=7017EF45.
//
// The anomaly path (saved-wfp > cursor, signed — ef6e WSGT 3,2) is
// predicted by the same read-only walk and falls back whole BEFORE any
// store (SharedProtocol terminal-composition rule: its two outcomes
// are the ?FATAL terminal and the never-observed R.SIGREC restart
// dispatch through [0x70000124]).
#include "r_signal.hpp"
#include "../hw/Machine.hpp"
#include "../hw/Memory.hpp"
#include "../hw/RTBridge.hpp"
#include "../hw/RTStubs.hpp"
#include "../debug/Capture.hpp"
#include <stdexcept>

namespace {
constexpr int32_t FRAME_WALK_LIMIT = 1024;   // cycle guard (loud, METHOD §8)

int32_t rd(hw::Machine& m, uint32_t addr) {
  return static_cast<int32_t>(m.memory->read_wide(addr));
}
}

namespace emu_rt {

uint32_t rq_signal(hw::Machine& machine) {
  if(machine.rt_pending_return != 0) {
    // Nested-in-fallback guard. Live today: the sole caller is DEF?ON
    // (ef41), terminal — the clone reaches it only inside a fallback
    // span. Dormant until the DEF?ON lift, like the satellites.
    hw::RTStubs::log_call(machine, "R?SIGNAL", "(native-skip: inside fallback span)");
    return hw::RTStubs::entry_address("R?SIGNAL");
  }

  hw::RTBridge bridge(machine);
  int32_t F = bridge.entry_wsp() + 10;

  // --- the walk, pure reads (ef69: cursor=own frame; ef6a: next =
  // [cursor-2] = saved wfp; ef6c: 0 -> plain return; ef6e: next >
  // cursor (signed) -> anomaly; else descend) ---
  int32_t cursor = F;
  int32_t guard = 0;
  for(;;) {
    if(++guard > FRAME_WALK_LIMIT)
      throw std::runtime_error("R?SIGNAL walk: wfp chain exceeds 1024 (cycle?)");
    int32_t next = rd(machine, static_cast<uint32_t>(cursor) - 2);
    if(next == 0)
      break;                                  // ef8d WRTN — the live path
    if(machine.frame_precedes(cursor, next))   // Ruling A (Project 12): compare in master coordinates (Mapper frame_precedes)
      // Restart/?FATAL machinery (ef70..ef89): stores begin here in
      // the emulated body, so this is the last moment a whole-routine
      // fallback is clean. Both engines emulate the anomaly path
      // identically (to the ?FATAL terminal, or through the
      // [0x70000124] -> R.SIGREC dispatch if it ever fires).
      return [&]{
        hw::RTStubs::log_call(machine, "R?SIGNAL",
          "(native-fallback: wfp-chain anomaly — restart/?FATAL path, emulating)");
        machine.rt_pending_return = static_cast<uint32_t>(machine.ac[3]);
        return hw::RTStubs::entry_address("R?SIGNAL");
      }();
    cursor = next;
  }

  // --- plain return: footprint is the image; the argument/area reads
  // and the walk leave no trace WRTN does not erase ---
  hw::RTStubs::log_call(machine, "R?SIGNAL", "(native)");
  bridge.emulate_frame();
  debug::Capture::native_footprint(machine);
  return bridge.native_return();
}

} // namespace emu_rt
