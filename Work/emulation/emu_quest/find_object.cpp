// src/emu_quest/find_object.cpp
#include "find_object.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../quest/find_object.hpp"
#include "../types/Context.hpp"

namespace emu_quest {
using namespace hw;

uint32_t find_object(Machine& machine) {
  EagleIntegration ei(machine);
  types::Context& ctx=*machine.native_context;
  int32_t x=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(1))&0xFFFF);
  int32_t y=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(2))&0xFFFF);
  int32_t result=0;
  quest::find_object(ctx, x, y, result);
  machine.memory->write_wide(ei.arg_addr(3), static_cast<uint32_t>(result));
  return ei.wrtn_void();
}

} // namespace emu_quest
