// src/emu_types/ByteArray.cpp
#include "ByteArray.hpp"
#include "../hw/Memory.hpp"

namespace emu_types {

ByteArray::ByteArray()
  : memory(nullptr), byte_addr(0), current_size(0), max_capacity(0) {}

ByteArray::ByteArray(hw::Memory& mem, uint32_t addr, size_t cap)
  : memory(&mem), byte_addr(addr), current_size(cap), max_capacity(cap) {}

ByteArray::ByteArray(hw::Memory& mem, uint32_t addr, size_t size, size_t cap)
  : memory(&mem), byte_addr(addr), current_size(size), max_capacity(cap) {}

size_t ByteArray::size() const {
  return current_size;
}

size_t ByteArray::capacity() const {
  return max_capacity;
}

uint8_t ByteArray::operator[](size_t pos) const {
  return static_cast<uint8_t>(memory->read_byte(byte_addr+static_cast<uint32_t>(pos)));
}

void ByteArray::set_byte(size_t pos, uint8_t val) {
  memory->write_byte(byte_addr+static_cast<uint32_t>(pos), val);
}

void ByteArray::set_size(size_t len) {
  current_size=len;
}

} // namespace emu_types
