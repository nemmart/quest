// src/runtime/p_defon.hpp
//
// P?DEFON (0x7017FD7A, 38 words) — the default handling of an
// UNHANDLED USER (positive-type) condition. Sole caller: DEF?ON at
// 0x7017EF22, on its [area+0x2] > 0 branch. Derivation:
// docs/Project4/DERIVATION.md §3.
//
// Semantics (args are PL/1 by-reference; slot numbering per RTBridge):
//   arg1 -> key2, arg2 -> code, arg3 -> type   (copies DEF?ON made
//   of the O.SET-recorded signal at [o_area+0x4/+0x6/+0x2])
//   type == 2 : LCALL C?INIT(arg1) — a no-op body — then return.
//   type == 6 : resignal O?SIGNAL(-1, key2, code)   (WADC 0,0 = -1)
//   else      : resignal O?SIGNAL( 6, key2, code)
//
// The resignal is a tail LCALL into O?SIGNAL, composed through the
// native registry exactly as ?DEFAULT_ERROR_HANDLER does
// (runtime/lib_error.cpp precedent): terminal-bound resignals are
// predicted on pure reads at entry and fall back whole.
#pragma once
#include <cstdint>
namespace hw { class Machine; }
namespace emu_rt {
uint32_t pq_defon(hw::Machine& machine);
}
