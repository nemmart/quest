// src/emu_rt/udiv32.cpp
#include "udiv32.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../rt/udiv32.hpp"
#include "../types/PLIError.hpp"

namespace emu_rt {
using namespace hw;

uint32_t udiv32(Machine& machine) {
  EagleIntegration ei(machine);
  try {
    types::Context& ctx=*machine.native_context;
    uint32_t dividend=ei.arg_wide(1);
    uint32_t divisor=ei.arg_wide(2);
    uint32_t remainder;
    uint32_t quotient=rt::udiv32_3(ctx, dividend, divisor, remainder);
    machine.memory->write_wide(ei.arg_addr(3), static_cast<int32_t>(remainder));
    return ei.wrtn(quotient);
  } catch(types::PLIError& e) {
    return ei.throw_lib_error(e.signal_code);
  }
}

} // namespace emu_rt
