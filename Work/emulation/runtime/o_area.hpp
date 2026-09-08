// src/runtime/o_area.hpp
//
// O?AREA (0x7017FC39) — the per-task CONDITION/SIGNAL area accessor:
// the T?AREA twin at a different fixed offset. Derived Aug 2026 from
// the disassembly (docs/Project4/DERIVATION.md):
//   WSAVS 0; LDASB 2; XLEF 0,[ac2-0x40]; XWSTA 0,[saved-ac0 slot]; WRTN
// i.e. it returns (in ac0, via the slot-patch idiom) machine.wsb - 0x40
// — the address of the signal area whose fields the whole condition
// system already uses: [+0] ON-frame chain head (o_signal.cpp walks
// it), [+0x2]/[+0x4]/[+0x6] the O.SET-recorded type/key2/code.
// Callers (both RT-internal, both on the terminal path): DEF?ON at
// 0x7017EF07 and ?FATAL at 0x7017F03C.
//
// rt::o_area is the plain-C++ contract for future DEF?ON work;
// emu_rt::oq_area is the dispatchable translation (frame residue +
// saved-ac0 slot patch — the T?AREA/?UDIV32 precedent).
#pragma once
#include <cstdint>
namespace hw { class Machine; }
namespace rt {
uint32_t o_area(hw::Machine& machine);
}
namespace emu_rt {
uint32_t oq_area(hw::Machine& machine);
}
