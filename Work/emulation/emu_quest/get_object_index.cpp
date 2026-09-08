// src/emu_quest/get_object_index.cpp
#include "get_object_index.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../quest/get_object_index.hpp"
#include "../types/Context.hpp"

namespace emu_quest {
using namespace hw;

uint32_t get_object_index(Machine& machine) {
  EagleIntegration ei(machine);
  types::Context& ctx=*machine.native_context;
  int32_t index=0;
  quest::get_object_index(ctx, index);
  // arg1 is wide (32-bit) by reference — write result back
  machine.memory->write_wide(ei.arg_addr(1), static_cast<uint32_t>(index));
  return ei.wrtn_void();
}

} // namespace emu_quest
