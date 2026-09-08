// src/emu_rt/syscall.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

// Native syscall handler, registered at address 0x30000000.
// Called from XCALL/LCALL interception in EagleStack.
// Returns a PC: return_address+1 (success), return_address (error),
// or 0x30000000 (not handled — fall through to emulated dispatch).
uint32_t syscall_handler(hw::Machine& machine);

} // namespace emu_rt
