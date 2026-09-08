// src/emu_types/Wide.hpp
#pragma once
#include "../types/Wide.hpp"

namespace hw { class Memory; }

namespace emu_types {

// Memory-backed Wide. Reads/writes a single 32-bit value through
// hw::Memory at the given word address (occupies 2 consecutive words).

class Wide : public types::Wide {
public:
  Wide(hw::Memory& mem, uint32_t addr) : memory(mem), addr(addr) {}

  int32_t get_value() const override;
  void set_value(int32_t v) override;

private:
  hw::Memory& memory;
  uint32_t addr;
};

} // namespace emu_types
