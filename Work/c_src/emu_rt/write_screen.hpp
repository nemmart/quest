// src/emu_rt/write_screen.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t write_screen(hw::Machine& machine);

} // namespace emu_rt
