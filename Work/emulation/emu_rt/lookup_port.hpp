// src/emu_rt/lookup_port.hpp
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace emu_rt {

uint32_t lookup_port(hw::Machine& machine);

} // namespace emu_rt
