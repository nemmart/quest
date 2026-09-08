// src/emu_rt/read_screen.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t read_screen(hw::Machine& machine);

} // namespace emu_rt
