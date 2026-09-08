// src/emu_rt/delay.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t delay(hw::Machine& machine);

} // namespace emu_rt
