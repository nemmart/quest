// src/emu_quest/display_magic.cpp
#include "display_magic.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../quest/display_magic.hpp"
#include "../types/Context.hpp"

namespace emu_quest {
using namespace hw;

static constexpr uint32_t OUT_CHAN_ADDR = 0x70000260;

uint32_t display_magic(Machine& machine) {
  EagleIntegration ei(machine);
  types::Context& ctx = *machine.native_context;
  int32_t channel = static_cast<int32_t>(
      machine.memory->read_word(OUT_CHAN_ADDR) & 0xFFFF);
  quest::display_magic(ctx, channel);
  return ei.wrtn_void();
}

} // namespace emu_quest
