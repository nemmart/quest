// src/emu_quest/random.cpp
#include "random.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../quest/random.hpp"
#include "../types/Context.hpp"

namespace emu_quest {
using namespace hw;

uint32_t random(Machine& machine) {
  EagleIntegration ei(machine);
  types::Context& ctx=*machine.native_context;
  int32_t threshold=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(1))&0xFFFF);
  int32_t result=quest::random(ctx, threshold);
  return ei.wrtn(static_cast<uint32_t>(result));
}

} // namespace emu_quest
