// src/runtime/o_signal.hpp
//
// The O.SEARCH -> O.SET signal cluster (0x7017EDDD-0x7017EF05):
// O?SIGNAL, the O.S* shorthand entries, O.SERROR (whose symbol extent
// holds the shared signal body), and O.SET with its interior walkers
// (EE62 select loop, EE7A chain-search helper, EE9D area walker).
// Full derivation: docs/Project1/DERIVATION.md.
//
// Convention: every implemented entry is LCALL/XCALL + WSAVS 0
// (bridge LCALL_FRAME). O.SIGNAL (0x7017EDE7) and R.SIGREC
// (0x7017EE02) are WSSVS entries with ZERO static callers anywhere:
// derived in the doc, deliberately not implemented or registered.
// O.SEARCH (0x7017EDDD) has a single dead caller (I.FFALT, a fault
// vector the emulator replaces with a C++ throw): derived, not
// implemented (no validation path — METHOD.md sec. 9).
//
// Signal flow (register roles at the shared body):
//   ac0 = condition type (-1 ERROR, -2 FIXEDOVF, -3 OVF, -4 UNDF,
//         -5 ZERODIV; O?SIGNAL passes *arg1, which may be positive
//         for user conditions), ac1 = key2 (*arg2 from O?SIGNAL, 0
//         from every fixed entry), ac2 = condition code (0x1160x /
//         0x1161x constants; O.SERROR takes the CALLER's ac2).
// The body calls O.SET (records the signal in the task area), runs
// the select loop over the [wsb-0x40] frame chain with the same
// chain-search helper O.ON uses (rt::chain_search, reused from
// runtime/o_on.hpp), and dispatches XCALL-style through the found
// node's handler address — or to DEF?ON (0x7017EF05) on exhaustion,
// where the terminal machinery detaches the clone. The dispatch is a
// TRANSFER (RTBridge::native_transfer); the handler-returned tail at
// EE40-EE55 stays emulated on both engines (the transfer's pushed
// return address points there).
//
// The I?LINEID gate: the EE9D walker's first act is to test the wide
// at code address 0x7017EEA0 (a WLDAI immediate; 0 in this binary),
// branching all line-number machinery -- including the LCALL I?LINEID
// -- out of existence when it is <= 0. The native replicates the test
// as a memory read; a positive value falls back to emulation from
// entry (before any store), preserving the locked terminal-branch
// decision in its actual, cheaper form. See DERIVATION.md sec. 3.
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace rt {

// The EE9D walker, live path only (gate closed). Pure reads.
// out1/out2 are the values stored to [wsb-0x36]/[wsb-0x38]: the first
// frame (walking the [wsb-0x40] chain outward via frame[+8]) whose
// frame[+4] points at a positive word yields (frame[+6], frame[+4]);
// otherwise (0, 0).
struct WalkerResult { int32_t out1; int32_t out2; };
bool walker_gate_open(hw::Machine& machine);           // wide[0x7017EEA0] > 0
void signal_walker(hw::Machine& machine, WalkerResult& out);

// The EE62 select loop: walk frames from [wsb-0x40] via frame[+8],
// rt::chain_search each (key2 pre-zeroed for type<=0, per the helper
// preamble); found -> handler = node[+6]; exhausted -> DEF?ON.
// last_* capture the final helper invocation for residue laying
// (only the last invocation's frame image survives in memory).
struct SelectResult {
  bool found;
  int32_t frame;          // registering frame (handler's ac1), 0 if not found
  int32_t handler;        // node[+6], or 0x7017EF05 (DEF?ON) if not found
  bool any_search;        // false when the frame chain was empty
  int32_t last_frame;     // frame argument of the last helper call
  int32_t last_node;      // its patched saved-ac1 (result node / backstop-or-0)
  int32_t last_scratch;   // its abandoned scratch wide (STATS backstop slot)
  bool last_found;        // whether the last call took the ret+0 return
};
void select_frames(hw::Machine& machine, int32_t type, int32_t raw_key2,
                   SelectResult& out);

} // namespace rt

namespace emu_rt {
uint32_t o_qsignal(hw::Machine& machine);   // O?SIGNAL
uint32_t o_set(hw::Machine& machine);       // O.SET
uint32_t o_serror(hw::Machine& machine);    // O.SERROR
uint32_t o_sconve(hw::Machine& machine);    // O.SCONVE (CONVERSION)
uint32_t o_ssubsc(hw::Machine& machine);    // O.SSUBSC (SUBSCRIPTRANGE)
uint32_t o_sfixed(hw::Machine& machine);    // O.SFIXED (FIXEDOVERFLOW)
uint32_t o_szerod(hw::Machine& machine);    // O.SZEROD (ZERODIVIDE)
uint32_t o_soverf(hw::Machine& machine);    // O.SOVERF (OVERFLOW)
uint32_t o_sunder(hw::Machine& machine);    // O.SUNDER (UNDERFLOW)
}
