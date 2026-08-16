// src/runtime/r_signal.hpp
//
// R?SIGNAL / ?ERROR (0x7017EF54, 59 code words; the 226-word extent in
// Layering.md counts the ?SNAP traceback strings at EF8E..F030) — the
// public raise/second-chance entry. Derivation:
// docs/Project4/DERIVATION.md §5.
//
// Body: load (code, type) — from the signal area when argc<=0, from
// the args when argc>0 (?ERROR(code[,type]); one-arg and >2-arg forms
// default type=-1) — then walk the wfp chain downward from its own
// frame. Chain ends at 0: plain WRTN (the LIVE path — observed and
// capture-confirmed on the QUEST_FAIL_OPEN death). Chain jumps UPWARD
// (saved-wfp > cursor): non-local restart dispatch — stash
// code/ret|c/type in that frame's restore slots, and if the vector
// [0x70000124] (installed by I.GINIT at ea8a: -> 0x7017EB63 ->
// R.SIGREC) is positive, rewrite the frame's return pc from it, clear
// the ON-chain head [wsb-0x40], wfp=frame, WRTN through it; vector
// <= 0 -> LCALL ?FATAL. Neither anomaly outcome has ever been
// observed; both fall back whole on a pure-read prediction (the walk
// is read-only up to the decision point).
//
// R.SIGNAL (0x7017EF51, WSAVS 0 + WBR into the shared walk): zero
// static callers anywhere — derived, deliberately not implemented
// (the O.SIGNAL/R.SIGREC precedent).
#pragma once
#include <cstdint>
namespace hw { class Machine; }
namespace emu_rt {
uint32_t rq_signal(hw::Machine& machine);
}
