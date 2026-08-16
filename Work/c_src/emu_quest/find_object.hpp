// src/emu_quest/find_object.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_quest {

uint32_t find_object(hw::Machine& machine);

} // namespace emu_quest
