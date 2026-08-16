// src/debug/Capture.hpp
//
// Empirical frame-region capture for runtime-translation derivation
// (SessionPlan task C checklist: "empirical captures are the safety net").
//
// Enabled by environment variable QUEST_CAPTURE=<entry-pc-hex>, e.g.
//   QUEST_CAPTURE=7017DA75 ./emulator QUEST QUEST_SERVER @QUEST
//
// When an emulated QUEST machine's pc arrives at the entry address
// (LCALL executed, WSAVS not yet run), a snapshot of the registers and
// the stack region [wsp-8 .. wsp+84] is appended to capture-<label>.txt,
// and a one-shot return capture is armed at the return address (ac3).
// When pc reaches it, a second snapshot of the same region (same base)
// is written, plus the destination region for register-argument
// routines (entry ac2 as a word address). Diffing A/B enumerates the
// routine's complete memory footprint; diffing B against the native
// translation's footprint validates residue fidelity bit-for-bit.
//
// Cost when the env var is unset: one static bool test per run_steps
// call site visit. Not thread-safe across machines beyond per-machine
// arming state keyed by label (adequate: the game is non-reentrant and
// captures are a derivation tool, not production).
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace debug {

class Capture {
public:
  // Called from Machine::run_steps for every executed pc of machines in
  // the QUEST binary (rtcov != nullptr). Fast-exits unless enabled.
  static void check(hw::Machine& machine);

  // Wrapper-side dump for native-translation footprint diffing: when
  // QUEST_CAPTURE is set, a translation wrapper can call this just
  // before native_return to write the same region snapshot from the
  // clone's memory (file capture-<label>.txt, tag NATIVE). Diffing the
  // master's RETURN blocks against the clone's NATIVE blocks validates
  // residue fidelity word-for-word. No-op when capture is disabled.
  static void native_footprint(hw::Machine& machine);

private:
  static void snapshot(hw::Machine& machine, const char* tag,
                       uint32_t base, uint32_t dest);
};

} // namespace debug
