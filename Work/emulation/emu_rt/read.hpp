// src/emu_rt/read.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t read(hw::Machine& machine);

} // namespace emu_rt
