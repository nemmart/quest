// src/emu_rt/create_task.cpp
#include "create_task.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../debug/SymbolTable.hpp"
#include "../rt/create_task.hpp"
#include "../types/Context.hpp"
#include "../types/PLIError.hpp"

namespace emu_rt {
using namespace hw;

uint32_t create_task(Machine& machine) {
  uint32_t peek_slot=Machine::copy_segment(
    static_cast<uint32_t>(machine.wsp),
    static_cast<uint32_t>(machine.wsp)-1);
  int argc=machine.memory->read_word(peek_slot)&0x7FFF;

  if(argc!=2) {
    return machine.symbols->address_for_name("?CREATE_TASK");
  }

  EagleIntegration ei(machine);
  try {
    types::Context& ctx=*machine.native_context;
    // arg1 points to descriptor: entry_point at offset 0 (wide)
    int32_t entry_point=static_cast<int32_t>(ei.arg_wide(1));
    int32_t stack_size=static_cast<int32_t>(ei.arg_wide(2));
    rt::create_task_2(ctx, entry_point, 0, stack_size);
    return ei.wrtn_void();
  } catch(types::PLIError& e) {
    return ei.throw_lib_error(e.signal_code);
  }
}

} // namespace emu_rt
