// src/emu_rt/umul32.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t umul32(hw::Machine& machine);

} // namespace emu_rt
