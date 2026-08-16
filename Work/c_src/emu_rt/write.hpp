// src/emu_rt/write.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t write(hw::Machine& machine);

} // namespace emu_rt
