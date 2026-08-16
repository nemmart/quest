// src/emu_types/Word.hpp
#pragma once
#include "../types/Word.hpp"

namespace hw { class Memory; }

namespace emu_types {

// Memory-backed Word. Reads/writes a single 16-bit value through
// hw::Memory at the given word address.

class Word : public types::Word {
public:
  Word(hw::Memory& mem, uint32_t addr) : memory(mem), addr(addr) {}

  int16_t get_value() const override;
  void set_value(int16_t v) override;

private:
  hw::Memory& memory;
  uint32_t addr;
};

} // namespace emu_types
