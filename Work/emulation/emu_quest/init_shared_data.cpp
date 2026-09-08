// src/emu_quest/init_shared_data.cpp
#include "init_shared_data.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../quest/init_shared_data.hpp"
#include "../types/Context.hpp"

namespace emu_quest {
using namespace hw;

uint32_t init_shared_data(Machine& machine) {
  EagleIntegration ei(machine);
  types::Context& ctx=*machine.native_context;
  quest::init_shared_data(ctx);
  return ei.wrtn_void();
}

} // namespace emu_quest
