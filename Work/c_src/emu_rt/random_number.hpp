// src/emu_rt/random_number.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t random_number(hw::Machine& machine);

} // namespace emu_rt
