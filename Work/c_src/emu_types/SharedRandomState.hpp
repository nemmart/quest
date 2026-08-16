// src/emu_types/SharedRandomState.hpp
#pragma once
#include "../types/SharedRandomState.hpp"
#include "../hw/Memory.hpp"

namespace emu_types {

class SharedRandomState : public types::SharedRandomState {
public:
  hw::Memory& memory;
  uint32_t word_addr;

  SharedRandomState(hw::Memory& mem, uint32_t addr)
    : memory(mem), word_addr(addr) {}

  double get_state() const override {
    return static_cast<double>(memory.read_wide(word_addr));
  }

  void set_state(double state) override {
    memory.write_wide(word_addr, static_cast<int32_t>(state));
  }
};

} // namespace emu_types
