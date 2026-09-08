// src/emu_quest/display_inventory.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_quest {

uint32_t display_inventory(hw::Machine& machine);

} // namespace emu_quest
