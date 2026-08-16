// src/emu_rt/umul32.cpp
#include "umul32.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../rt/umul32.hpp"
#include "../types/PLIError.hpp"

namespace emu_rt {
using namespace hw;

uint32_t umul32(Machine& machine) {
  EagleIntegration ei(machine);
  try {
    types::Context& ctx=*machine.native_context;
    uint32_t a=ei.arg_wide(1);
    uint32_t b=ei.arg_wide(2);
    uint16_t overflow;
    uint32_t product=rt::umul32_3(ctx, a, b, overflow);
    machine.memory->write_word(ei.arg_addr(3), static_cast<uint32_t>(overflow));
    return ei.wrtn(product);
  } catch(types::PLIError& e) {
    return ei.throw_lib_error(e.signal_code);
  }
}

} // namespace emu_rt
