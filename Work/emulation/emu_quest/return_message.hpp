// src/emu_quest/return_message.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_quest {

uint32_t return_message(hw::Machine& machine);

} // namespace emu_quest
