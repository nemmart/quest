// src/emu_rt/close_file.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t close_file(hw::Machine& machine);

} // namespace emu_rt
