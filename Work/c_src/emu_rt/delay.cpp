// src/emu_rt/delay.cpp
#include "delay.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../rt/delay.hpp"

namespace emu_rt {
using namespace hw;

uint32_t delay(Machine& machine) {
  EagleIntegration ei(machine);
  types::Context& ctx=*machine.native_context;
  int32_t ms=static_cast<int32_t>(ei.arg_wide(1));
  rt::delay_1(ctx, ms);
  return ei.wrtn_void();
}

} // namespace emu_rt
