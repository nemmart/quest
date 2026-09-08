// src/emu_rt/char_to_bit.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t char_to_bit(hw::Machine& machine);

} // namespace emu_rt
