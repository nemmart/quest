// src/emu_rt/current_pid.cpp
#include "current_pid.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../rt/current_pid.hpp"
#include "../types/PLIError.hpp"

namespace emu_rt {
using namespace hw;

uint32_t current_pid(Machine& machine) {
  EagleIntegration ei(machine);
  try {
    types::Context& ctx=*machine.native_context;
    int32_t pid=rt::current_pid_0(ctx);
    return ei.wrtn(static_cast<uint32_t>(pid));
  } catch(types::PLIError& e) {
    return ei.throw_lib_error(e.signal_code);
  }
}

} // namespace emu_rt
