// src/emu_quest/dist.cpp
#include "dist.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../quest/dist.hpp"
#include "../types/Context.hpp"
#include "../emu_types/Location.hpp"

namespace emu_quest {
using namespace hw;

uint32_t dist(Machine& machine) {
  EagleIntegration ei(machine);
  types::Context& ctx=*machine.native_context;
  // PL/I args: x1(1), y1(2), x2(3), y2(4) — adjacent pairs
  emu_types::Location a(*machine.memory, ei.arg_addr(1));
  emu_types::Location b(*machine.memory, ei.arg_addr(3));
  int32_t result=quest::dist(ctx, a, b);
  return ei.wrtn(static_cast<uint32_t>(result));
}

uint32_t distance_to_player(Machine& machine) {
  EagleIntegration ei(machine);
  types::Context& ctx=*machine.native_context;
  // PL/I args: x(1), y(2), player_num(3) — x,y adjacent pair
  emu_types::Location from(*machine.memory, ei.arg_addr(1));
  int32_t player_num=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(3))&0xFFFF);
  int32_t result=quest::distance_to_player(ctx, from, player_num);
  return ei.wrtn(static_cast<uint32_t>(result));
}

} // namespace emu_quest
