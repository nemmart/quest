// src/emu_rt/create_task.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t create_task(hw::Machine& machine);

} // namespace emu_rt
