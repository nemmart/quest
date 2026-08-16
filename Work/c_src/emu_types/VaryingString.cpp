// src/emu_types/VaryingString.cpp
#include "VaryingString.hpp"
#include "../hw/Memory.hpp"
#include "../hw/Machine.hpp"

namespace emu_types {
using namespace hw;

VaryingString::VaryingString()
  : memory(nullptr), word_addr(0), byte_base(0), max_capacity(0) {}

VaryingString::VaryingString(Memory& mem, uint32_t addr, size_t capacity)
  : memory(&mem), word_addr(addr), max_capacity(capacity)
{
  uint32_t segment=Machine::get_segment(addr);
  uint32_t bare_word=addr&0x0FFFFFFF;
  byte_base=Machine::set_byte_segment(segment, bare_word*2+2);
}

size_t VaryingString::size() const {
  return static_cast<size_t>(memory->read_word(word_addr)&0xFFFF);
}

char VaryingString::operator[](size_t pos) const {
  return static_cast<char>(memory->read_byte(byte_base+static_cast<uint32_t>(pos)));
}

void VaryingString::set_char(size_t pos, char c) {
  memory->write_byte(byte_base+static_cast<uint32_t>(pos), static_cast<uint32_t>(c)&0xFF);
}

void VaryingString::set_size(size_t len) {
  memory->write_word(word_addr, static_cast<uint32_t>(len)&0xFFFF);
}

} // namespace emu_types
