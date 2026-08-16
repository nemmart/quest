// src/emu_rt/get_shared_page.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t get_shared_page(hw::Machine& machine);

} // namespace emu_rt
