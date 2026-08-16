// src/emu_types/Location.cpp
#include "Location.hpp"
#include "../hw/Memory.hpp"

namespace emu_types {

int16_t Location::get_x() const {
  return static_cast<int16_t>(memory.read_word(addr) & 0xFFFF);
}

int16_t Location::get_y() const {
  return static_cast<int16_t>(memory.read_word(addr + 1) & 0xFFFF);
}

void Location::set_x(int16_t v) {
  memory.write_word(addr, static_cast<uint32_t>(v) & 0xFFFF);
}

void Location::set_y(int16_t v) {
  memory.write_word(addr + 1, static_cast<uint32_t>(v) & 0xFFFF);
}

} // namespace emu_types
