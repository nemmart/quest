// src/emu_types/VaryingString.hpp
#pragma once
#include "../types/String.hpp"
#include <cstdint>

namespace hw { class Memory; }

namespace emu_types {

class VaryingString : public types::String {
public:
  VaryingString();
  VaryingString(hw::Memory& mem, uint32_t word_addr, size_t capacity = 512);

  size_t size() const override;
  char operator[](size_t pos) const override;
  void set_char(size_t pos, char c) override;
  void set_size(size_t len) override;

private:
  hw::Memory* memory;
  uint32_t word_addr;
  uint32_t byte_base;
  size_t max_capacity;
};

} // namespace emu_types
