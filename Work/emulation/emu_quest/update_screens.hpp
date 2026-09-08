// src/emu_quest/update_screens.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_quest {

uint32_t update_screens(hw::Machine& machine);

} // namespace emu_quest
