// src/emu_quest/thief.cpp
#include "thief.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../quest/thief.hpp"
#include "../types/Context.hpp"

namespace emu_quest {
using namespace hw;

uint32_t thief(Machine& machine) {
  EagleIntegration ei(machine);
  types::Context& ctx=*machine.native_context;
  quest::thief(ctx);
  return ei.wrtn_void();
}

} // namespace emu_quest
