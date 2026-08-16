// src/runtime/def_on.hpp
//
// DEF?ON (0x7017EF05, 42 words) — the no-handler default: entered
// XCALL-style (argc 0) from O?SIGNAL's dispatch on chain exhaustion,
// with ac2 = its own entry address (the handler slot value; confirmed
// by the run2/run3 captures). Derivation: docs/Project4/DERIVATION.md §6.
//
// Body: area = O?AREA(); type = [area+0x2].
//   type > 0        : copy (key2, code, type) to locals, P?DEFON(&) —
//                     the user-condition default (resume via C?INIT for
//                     type 2, resignal otherwise).
//   type == -1      : R?SIGNAL(); then resume (WRTN) iff bit15 of
//                     narrow [area+0x16] is set, else ?FATAL.
//   other type <= 0 : resignal O?SIGNAL(-1, key2, code), then the
//                     R?SIGNAL tail as above.
//
// *** REGISTERED by the Project 5 lift (was staged) *** DEF?ON is a terminal_table entry;
// the detach machinery pairs-and-halts the clone at this pc.
// Registering the translation is the first step of the DEF?ON-lift
// session (move the detach point deeper per Layering.md), NOT a
// this-session change: it must land together with the terminal-table
// change and the fault-injection validation. REPORT.md §5 has the
// checklist. Until then this file is compiled, reviewable, dead code.
#pragma once
#include <cstdint>
namespace hw { class Machine; }
namespace rt {
// Pure-read prediction of native DEF?ON's outcome for a signal about
// to be dispatched on exhaustion (Project 5 lift; used by o_signal's
// exhaustion path). Inputs are the SIGNAL'S values (the O.SET record
// and the [wsb-0x2A] flag store are part of the dispatch footprint,
// so at DEF?ON entry [area+2]==type and the flag wide==flag).
// True  => defq_on will end at a shared native boundary (resume
//          return or resignal transfer) — safe to dispatch natively.
// False => defq_on would FALL BACK toward the ?FATAL terminal; the
//          caller must fall back WHOLE instead, or the clone's native
//          prefix skews the instruction counts compared at the
//          terminal pair.
bool def_on_would_run_native(hw::Machine& machine, int32_t type,
                             int32_t key2);
}
namespace emu_rt {
uint32_t defq_on(hw::Machine& machine);
}
