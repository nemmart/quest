// src/emu_rt/bit_move.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t bit_move(hw::Machine& machine);

} // namespace emu_rt
