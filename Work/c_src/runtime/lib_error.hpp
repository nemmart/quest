// src/runtime/lib_error.hpp
//
// Native translations of the PL/1 error-reporting trio:
//   ?LIB_ERROR             0x7017E33A  (LCALL, 1-2 args)
//   ?LIB_ERROR_CODE        0x7017DE25  (LCALL, 0 args)
//   ?DEFAULT_ERROR_HANDLER 0x7017E3D2  (XCALL from ?LIB_ERROR's tail)
//
// Derivation: docs/Project2/DERIVATION.md. Status/validation:
// docs/Project2/REPORT.md.
//
// Design (trio as one unit): ?LIB_ERROR's native path runs the whole
// signal setup — install, latch, code store, old-buffer free, message
// alloc+copy (via the validated emu_rt::i_freew / emu_rt::i_alloc at
// staged machine state), then the ?DEFAULT_ERROR_HANDLER body — down
// to the O?SIGNAL LCALL boundary, and composes with O?SIGNAL through
// the native registry exactly as the emulated LCALL would: if
// O?SIGNAL has a registered translation (Project 1), it is called as
// plain C++ from the exact dispatch state; otherwise the whole call
// falls back to emulation at entry, before any side effect. The
// registry IS the cross-project interface; no separate rt:: signature
// is needed.
//
// Every wrapper starts with the nested-span rule: a dispatch that
// fires while machine.rt_pending_return != 0 is inside another
// routine's emulated-fallback span; it returns its entry address
// WITHOUT re-arming so the nest stays one span on both engines.
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace rt {
// Cross-project contract with Project 1 (O?SIGNAL) — see REPORT.md §3
// and the weak default in lib_error.cpp: pure reads only; true iff the
// signal walk (from [wsb-0x40]) would find a handler for this code.
// Project 1's strong definition replaces the weak conservative default
// (false = fall back whole) at link time.
bool signal_has_handler(hw::Machine& machine, int32_t code);
} // namespace rt

namespace emu_rt {
uint32_t lib_error(hw::Machine& machine);
uint32_t lib_error_code(hw::Machine& machine);
uint32_t default_error_handler(hw::Machine& machine);
} // namespace emu_rt
