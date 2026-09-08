// src/emu_types/ByteArray.hpp
#pragma once
#include "../types/ByteArray.hpp"
#include <cstdint>

namespace hw { class Memory; }

namespace emu_types {

class ByteArray : public types::ByteArray {
public:
  ByteArray();
  ByteArray(hw::Memory& mem, uint32_t byte_addr, size_t capacity);
  ByteArray(hw::Memory& mem, uint32_t byte_addr, size_t size, size_t capacity);

  size_t size() const override;
  size_t capacity() const override;
  uint8_t operator[](size_t pos) const override;
  void set_byte(size_t pos, uint8_t val) override;
  void set_size(size_t len) override;

private:
  hw::Memory* memory;
  uint32_t byte_addr;
  size_t current_size;
  size_t max_capacity;
};

} // namespace emu_types
