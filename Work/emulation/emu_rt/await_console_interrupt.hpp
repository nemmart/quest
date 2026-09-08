// src/emu_rt/await_console_interrupt.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t await_console_interrupt(hw::Machine& machine);

} // namespace emu_rt
