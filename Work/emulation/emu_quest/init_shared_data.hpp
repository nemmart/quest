// src/emu_quest/init_shared_data.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_quest {

uint32_t init_shared_data(hw::Machine& machine);

} // namespace emu_quest
