// src/emu_rt/create_ipc_file.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t create_ipc_file(hw::Machine& machine);

} // namespace emu_rt
