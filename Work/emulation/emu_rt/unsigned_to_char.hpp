// src/emu_rt/unsigned_to_char.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t unsigned_to_char(hw::Machine& machine);

} // namespace emu_rt
