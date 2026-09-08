// src/emu_types/Wide.cpp
#include "Wide.hpp"
#include "../hw/Memory.hpp"

namespace emu_types {

int32_t Wide::get_value() const {
  return static_cast<int32_t>(memory.read_wide(addr));
}

void Wide::set_value(int32_t v) {
  memory.write_wide(addr, static_cast<uint32_t>(v));
}

} // namespace emu_types
