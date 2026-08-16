// src/emu_rt/open_file.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t open_file(hw::Machine& machine);

} // namespace emu_rt
