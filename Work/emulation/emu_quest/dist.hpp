// src/emu_quest/dist.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_quest {

uint32_t dist(hw::Machine& machine);
uint32_t distance_to_player(hw::Machine& machine);

} // namespace emu_quest
