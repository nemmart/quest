// src/emu_quest/owns.cpp
#include "owns.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../quest/owns.hpp"
#include "../types/Context.hpp"

namespace emu_quest {
using namespace hw;

uint32_t owns(Machine& machine) {
  EagleIntegration ei(machine);
  types::Context& ctx=*machine.native_context;
  int32_t player_num=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(1))&0xFFFF);
  int32_t object_type=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(2))&0xFFFF);
  int32_t result=quest::owns(ctx, player_num, object_type);
  return ei.wrtn(static_cast<uint32_t>(result));
}

} // namespace emu_quest
