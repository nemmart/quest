// src/emu_types/WordArray.cpp
#include "WordArray.hpp"
#include "../hw/Memory.hpp"

namespace emu_types {

WordArray::WordArray()
  : memory(nullptr), word_addr(0), current_size(0), max_capacity(0) {}

WordArray::WordArray(hw::Memory& mem, uint32_t addr, size_t cap)
  : memory(&mem), word_addr(addr), current_size(cap), max_capacity(cap) {}

WordArray::WordArray(hw::Memory& mem, uint32_t addr, size_t size, size_t cap)
  : memory(&mem), word_addr(addr), current_size(size), max_capacity(cap) {}

size_t WordArray::size() const {
  return current_size;
}

size_t WordArray::capacity() const {
  return max_capacity;
}

int32_t WordArray::operator[](size_t pos) const {
  return static_cast<int32_t>(memory->read_word(word_addr+static_cast<uint32_t>(pos)));
}

void WordArray::set_word(size_t pos, int32_t val) {
  memory->write_word(word_addr+static_cast<uint32_t>(pos), val);
}

void WordArray::set_size(size_t len) {
  current_size=len;
}

} // namespace emu_types
