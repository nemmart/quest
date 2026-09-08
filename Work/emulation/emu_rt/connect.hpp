// src/emu_rt/connect.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t connect(hw::Machine& machine);

} // namespace emu_rt
