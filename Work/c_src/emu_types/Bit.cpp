// src/emu_types/Bit.cpp
#include "Bit.hpp"
#include "../hw/Memory.hpp"

namespace emu_types {

bool Bit::get_value() const {
  uint32_t word = memory.read_word(addr);
  return (word >> (15 - bit)) & 1;
}

void Bit::set_value(bool v) {
  uint32_t word = memory.read_word(addr);
  uint32_t mask = 1u << (15 - bit);
  if (v)
    word |= mask;
  else
    word &= ~mask;
  memory.write_word(addr, word);
}

} // namespace emu_types
