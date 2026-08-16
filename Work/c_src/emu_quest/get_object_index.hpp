// src/emu_quest/get_object_index.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_quest {

uint32_t get_object_index(hw::Machine& machine);

} // namespace emu_quest
