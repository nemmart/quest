// src/emu_rt/random_number.cpp
#include "random_number.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../emu_types/SharedRandomState.hpp"
#include "../rt/random_number.hpp"
#include "../types/Context.hpp"

namespace emu_rt {
using namespace hw;

static constexpr uint32_t SD_PTR_ADDR=0x70000210;
static constexpr uint32_t SEED_OFFSET=0x28;

uint32_t random_number(Machine& machine) {
  EagleIntegration ei(machine);
  types::Context& ctx=*machine.native_context;

  // Lazy init: create SharedRandomState on first call.
  // By this point INIT_SHARED_DATA has run and SD_PTR is valid.
  if(!ctx.random_state) {
    uint32_t sd_ptr=static_cast<uint32_t>(machine.memory->read_wide(SD_PTR_ADDR));
    ctx.random_state=new emu_types::SharedRandomState(*machine.memory, sd_ptr+SEED_OFFSET);
  }

  int32_t lower=static_cast<int32_t>(ei.arg_wide(1));
  int32_t upper=static_cast<int32_t>(ei.arg_wide(2));
  int32_t result=rt::random_number_3(ctx, lower, upper);
  return ei.wrtn(static_cast<uint32_t>(result));
}

} // namespace emu_rt
