// T?AREA (0x7017ED93) — per-task condition area accessor.
// Derived Aug 2026 from the disassembly:
//   WSAVS 0; LDASB 2; XLEF 0,[ac2-0x29]; XWSTA 0,[saved-ac0 slot]; WRTN
// i.e. it returns (in ac0, via the slot-patch idiom) the address
// machine.wsb - 0x29: the condition area sits at a fixed offset below the
// task's stack base, so each task has its own. rt::t_area is the FROZEN
// CONTRACT shared by the parallel translation projects (SharedProtocol.md):
// Project 2's native ?LIB_ERROR calls it directly; Project 3 owns the
// emu_rt wrapper (frame residue + saved-ac0 patch) and its validation.
#pragma once
#include <cstdint>
namespace hw { class Machine; }
namespace rt {
uint32_t t_area(hw::Machine& machine);
}
namespace emu_rt {
// Project 3: the dispatchable translation (frame residue + saved-ac0
// slot patch, ?UDIV32 precedent). rt::t_area above stays the frozen
// plain-C++ contract for Projects 1-2.
uint32_t t_area(hw::Machine& machine);
}
