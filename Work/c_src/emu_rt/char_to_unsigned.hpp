// src/emu_rt/char_to_unsigned.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

// Integration wrapper for ?CHAR_TO_UNSIGNED
// Reads args from emulated stack, calls rt::char_to_unsigned_1 or _2
uint32_t char_to_unsigned(hw::Machine& machine);

} // namespace emu_rt
