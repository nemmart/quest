// src/emu_types/Bit.hpp
#pragma once
#include "../types/Bit.hpp"
#include <cstdint>

namespace hw { class Memory; }

namespace emu_types {

// Memory-backed Bit. Reads/writes a single bit through hw::Memory.
// Data General convention: bit 0 = MSB (0x8000), bit 15 = LSB (0x0001).

class Bit : public types::Bit {
public:
  Bit(hw::Memory& mem, uint32_t word_addr, int bit_pos)
    : memory(mem), addr(word_addr), bit(bit_pos) {}

  bool get_value() const override;
  void set_value(bool v) override;

private:
  hw::Memory& memory;
  uint32_t addr;  // word address
  int bit;        // bit position (0 = MSB)
};

} // namespace emu_types
