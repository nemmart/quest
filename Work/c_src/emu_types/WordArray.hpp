// src/emu_types/WordArray.hpp
#pragma once
#include "../types/WordArray.hpp"
#include <cstdint>

namespace hw { class Memory; }

namespace emu_types {

class WordArray : public types::WordArray {
public:
  WordArray();
  WordArray(hw::Memory& mem, uint32_t word_addr, size_t capacity);
  WordArray(hw::Memory& mem, uint32_t word_addr, size_t size, size_t capacity);

  size_t size() const override;
  size_t capacity() const override;
  int32_t operator[](size_t pos) const override;
  void set_word(size_t pos, int32_t val) override;
  void set_size(size_t len) override;

private:
  hw::Memory* memory;
  uint32_t word_addr;
  size_t current_size;
  size_t max_capacity;
};

} // namespace emu_types
