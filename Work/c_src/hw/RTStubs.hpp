// src/hw/RTStubs.hpp
//
// Log-and-continue native entry points for every PL/1 runtime symbol in
// QUEST.PR, plus RT-range coverage and RT-entry sync support.
//
// Each stub logs an "rtcalls" trace line and returns its own entry address,
// so execution proceeds into the emulated routine unchanged. Stubs are
// registered in the CLONE only; the master (and non-lockstep runs) stay
// pure emulation. This exercises the real native-dispatch path
// (registration -> lookup -> dispatch -> return-to-emulation) at every
// runtime call before any real translation lands. As routines are
// translated, their stubs are replaced by real implementations one at a
// time (SessionPlan.md task B onward).
//
// Coverage: a byte-per-word bitmap over [start, stop) is marked for every
// executed pc in the RT range on QUEST machines (all roles, all modes) and
// dumped at shutdown — the safety net that catches entry paths the stub
// hooks don't see (hidden live code, computed jumps).
//
// Sync: entry_bits marks every runtime entry address. Under -lockstep,
// master and clone batches break when pc arrives at an entry, so the
// existing pair checker verifies both engines at the same entry with the
// same argument state. The sync identity is the entry address as a logical
// event: today both sides arrive by emulation; once a routine is native in
// the clone, the clone arrives by dispatch on the same address — the same
// architectural point — so entry checks remain valid permanently.
#pragma once
#include "NativeRegistry.hpp"
#include <cstdint>
#include <string>

namespace debug { class SymbolTable; }
namespace os { class OSProcess; }

namespace hw {

class Machine;

class RTStubs {
public:
  // Set by initialize() for the QUEST binary; read from the instruction
  // hot loop (Machine::run_steps).
  static bool active;
  static uint32_t start;         // RT range [start, stop): ?CHAR_TO_UNSIGNED..?NTOP
  static uint32_t stop;
  static uint8_t* entry_bits;       // byte per word in range: 1 = runtime entry
  static uint8_t* translated_bits;  // 1 = entry has a native translation (clone runs native; master runs body to return)
  static uint8_t* l2_bits;          // 1 = the entry is L2 (condition-system machinery,
                                    // docs/Layering.md census + L2Contract.md §5). The
                                    // CROSSINGS-ONLY CHECKER (user-ratified, Aug 2026)
                                    // keys pairing on layer transitions, not entry
                                    // addresses: L1→L2 entries and L2→L1 exits are
                                    // rendezvous; interior L2→L2 is invisible. Entries
                                    // NOT tagged here are L0/L1 fabric and pair as they
                                    // always have.
  static uint8_t* terminal_bits;    // 0 = not terminal; 1 = DETACH; 2 = ABORT. Under
                                    // -lockstep, one final verified pair forms here, then the
                                    // clone detaches and the master runs on unverified
                                    // (Lockstep::detach). Marked for BOTH roles.
  // ---- Fault injector (Project 5; docs/Project5/REPORT.md) ----
  // QUEST_INJECT=<site>:<type>:<code>[:RESUME] — synthesize an O?SIGNAL
  // raise SYMMETRICALLY at pc==inject_site on QUEST clients (both
  // lockstep roles; never the server). TRIPWIRE: validation-only
  // machinery — it perturbs the game exactly like a real raise site
  // and must never be armed in ordinary play sessions.
  static uint32_t inject_site;     // 0 = disarmed globally
  static int32_t  inject_type;
  static int32_t  inject_code;
  static bool     inject_resume;   // 4-arg raise with a negative flag (see inject_fire)

  // Project 8 H7: QUEST_BAD_TOKEN=1 arms a ONE-SHOT corruption of the
  // first non-local I.GOTO's target on the clone (native/check modes
  // only — frames.cpp consumes it), exercising the ABORT-INTENDED
  // third result class end to end, once, on purpose (NativeDesign §6).
  static bool     bad_token_armed;
  static uint32_t inject_fire(Machine& machine);   // stages the call; returns the next pc

  static uint32_t terminal_test_pc; // QUEST_TERMINAL=<hex>[:ABORT]: one extra terminal
                                    // address for testing, any pc; 0 = none
  static uint8_t  terminal_test_kind; // 1=DETACH (default) or 2=ABORT for the test pc
  // Resolve the terminal kind at a pc (terminal_bits, the game-range
  // terminal-site table, or the test pc); 0 if not terminal.
  static uint8_t terminal_kind(uint32_t pc);
  // True if pc is any terminal point (used by Machine's any-address check).
  static bool is_terminal_pc(uint32_t pc);

  // ---- Crossings-only checker helpers (docs/CrossingsChecker.md) ----
  // True if pc is an L2-tagged entry (in-range check included).
  static bool is_l2_entry(uint32_t pc);
  // True if the dispatch sites should DEFER a native call at this target:
  // an L1→L2 crossing into a translated L2 entry. The site sets
  // Machine::pending_native and returns the entry pc, so the batch breaks
  // AT the entry (the crossing rendezvous, argument state compared) and
  // the native implementation runs on resume.
  static bool defer_dispatch(uint32_t pc);
  // True at the L1→L2 RETURN-crossing pcs (a dispatched handler's WRTN
  // back into the signal tail): DISPATCH_RET 0x7017EE40 and ?LIB_ERROR's
  // O?SIGNAL-return 0x7017E3EF (L2Contract.md §5). Arrival with no
  // pending span is a rendezvous.
  static bool is_return_crossing(uint32_t pc);

  // Resolve the stub table and RT range against a process's symbols.
  // Only acts for the QUEST binary; idempotent (master and clone share
  // the same addresses).
  static void initialize(debug::SymbolTable& symbols, const std::string& program);

  // Register every resolved stub in the given registry (clone only —
  // caller gates on role).
  static void register_stubs(NativeRegistry& registry, debug::SymbolTable& symbols);

  // Coverage bitmap for a process's instance (created on first request).
  // Returns nullptr when inactive or not a QUEST process.
  static uint8_t* coverage_for(os::OSProcess* process);

  // Write rtcov-<label>.txt per covered instance: covered addresses with
  // symbol attribution. Called once at shutdown from Launch.
  static void dump_coverage();

  // Stub body helper: log the entry (rtcalls trace type) and return the
  // entry address so emulation continues into the routine.
  static uint32_t log_and_continue(Machine& machine, const char* name, uint32_t entry);

  // Log a runtime call with a tag (e.g. "(native)"). Used by translations.
  static void log_call(Machine& machine, const char* name, const char* tag);

  // Resolved entry address for a runtime symbol (0xFFFFFFFF if unknown).
  // Lets a translation fall back to emulation on unexpected input.
  static uint32_t entry_address(const char* name);
};

} // namespace hw
