// src/emu_rt/await_console_interrupt.cpp
#include "await_console_interrupt.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../rt/await_console_interrupt.hpp"
#include "../types/Context.hpp"
#include "../types/PLIError.hpp"

namespace emu_rt {
using namespace hw;

uint32_t await_console_interrupt(Machine& machine) {
  EagleIntegration ei(machine);
  try {
    types::Context& ctx=*machine.native_context;
    rt::await_console_interrupt_0(ctx);
    return ei.wrtn_void();
  } catch(types::PLIError& e) {
    return ei.throw_lib_error(e.signal_code);
  }
}

} // namespace emu_rt
