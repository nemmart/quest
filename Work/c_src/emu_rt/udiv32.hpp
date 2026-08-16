// src/emu_rt/udiv32.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t udiv32(hw::Machine& machine);

} // namespace emu_rt
