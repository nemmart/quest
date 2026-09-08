// src/emu_rt/recreate_file.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t recreate_file(hw::Machine& machine);

} // namespace emu_rt
