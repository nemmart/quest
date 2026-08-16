// src/runtime/frames.hpp — Project 3 (docs/Project3/PROMPT.md)
//
// I.PROLOG (0x7017E733), I.EPILOG (0x7017E77D), I.GOTO (0x7017EC7C):
// condition-frame construction, teardown, and the non-local unwind.
// Derivation: docs/Project3/DERIVATION.md (the condition FRAME LAYOUT
// section there is the shared contract with Project 1 and O_ON.md).
//
// All three are LJSR routines with non-standard endings (dotted-helper
// convention, RTWorklist.md): I.PROLOG resumes at entry-ac3+4 (= LJSR
// pc+7), I.EPILOG performs its CALLER's WRTN (returns from the game
// routine that invoked it), and I.GOTO cuts the stack to a target frame
// and jumps to a label pc via the hidden landing stub at 0x7017EC9D.
// None of them builds a frame of its own (no WSAVS/WSSVS anywhere in
// the three bodies), so RTBridge's frame machinery does not apply;
// each wrapper replicates its body's exact register/memory footprint
// and ends with RTBridge::native_transfer.
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {
uint32_t i_prolog(hw::Machine& machine);
uint32_t i_epilog(hw::Machine& machine);
uint32_t i_goto(hw::Machine& machine);
}
