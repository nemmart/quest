// src/emu_rt/fill_words.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t fill_words(hw::Machine& machine);

} // namespace emu_rt
