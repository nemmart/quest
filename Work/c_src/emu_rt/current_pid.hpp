// src/emu_rt/current_pid.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t current_pid(hw::Machine& machine);

} // namespace emu_rt
