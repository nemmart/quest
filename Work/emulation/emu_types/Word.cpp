// src/emu_types/Word.cpp
#include "Word.hpp"
#include "../hw/Memory.hpp"

namespace emu_types {

int16_t Word::get_value() const {
  return static_cast<int16_t>(memory.read_word(addr) & 0xFFFF);
}

void Word::set_value(int16_t v) {
  memory.write_word(addr, static_cast<uint32_t>(v) & 0xFFFF);
}

} // namespace emu_types
