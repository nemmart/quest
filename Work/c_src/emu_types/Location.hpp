// src/emu_types/Location.hpp
#pragma once
#include "../types/Location.hpp"

namespace hw { class Memory; }

namespace emu_types {

// Memory-backed Location. Reads/writes go through hw::Memory to
// the shared data region. Constructed with the word address of the
// x coordinate; y is at addr + 1.
//
// Lightweight (reference + address) — cheap to construct and pass.

class Location : public types::Location {
public:
  Location(hw::Memory& mem, uint32_t addr) : memory(mem), addr(addr) {}

  int16_t get_x() const override;
  int16_t get_y() const override;
  void set_x(int16_t v) override;
  void set_y(int16_t v) override;

private:
  hw::Memory& memory;
  uint32_t addr;  // word address of x field
};

} // namespace emu_types
