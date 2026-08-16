// src/emu_rt/translate.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t translate(hw::Machine& machine);

} // namespace emu_rt
