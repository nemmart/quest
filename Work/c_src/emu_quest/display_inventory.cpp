// src/emu_quest/display_inventory.cpp
#include "display_inventory.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../quest/display_inventory.hpp"
#include "../types/Context.hpp"

namespace emu_quest {
using namespace hw;

// Output channel global at 0x70000260
static constexpr uint32_t OUT_CHAN_ADDR = 0x70000260;

uint32_t display_inventory(Machine& machine) {
  EagleIntegration ei(machine);
  types::Context& ctx = *machine.native_context;
  int32_t player_num = static_cast<int32_t>(
      machine.memory->read_word(ei.arg_addr(1)) & 0xFFFF);
  int32_t channel = static_cast<int32_t>(
      machine.memory->read_word(OUT_CHAN_ADDR) & 0xFFFF);
  quest::display_inventory(ctx, player_num, channel);
  return ei.wrtn_void();
}

} // namespace emu_quest
