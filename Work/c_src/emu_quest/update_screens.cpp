// src/emu_quest/update_screens.cpp
#include "update_screens.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../quest/update_screens.hpp"
#include "../types/Context.hpp"

namespace emu_quest {
using namespace hw;

uint32_t update_screens(Machine& machine) {
  EagleIntegration ei(machine);
  types::Context& ctx=*machine.native_context;
  int32_t x=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(1))&0xFFFF);
  int32_t y=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(2))&0xFFFF);
  int32_t viewport_value=static_cast<int32_t>(machine.memory->read_wide(ei.arg_addr(3)));
  quest::update_screens(ctx, x, y, viewport_value);
  return ei.wrtn_void();
}

} // namespace emu_quest
