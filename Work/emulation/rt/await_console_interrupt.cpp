// src/rt/await_console_interrupt.cpp
#include "await_console_interrupt.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"
#include "../types/PLIError.hpp"

namespace rt {

void await_console_interrupt_0(types::Context& ctx) {
  int32_t err=ctx.os.await_console_interrupt();
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
}

} // namespace rt
