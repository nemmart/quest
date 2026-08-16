// src/emu_quest/random.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_quest {

uint32_t random(hw::Machine& machine);

} // namespace emu_quest
