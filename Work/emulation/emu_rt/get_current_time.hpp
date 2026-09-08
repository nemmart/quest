// src/emu_rt/get_current_time.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t get_current_time(hw::Machine& machine);

} // namespace emu_rt
