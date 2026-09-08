// src/emu_rt/get_current_time.cpp
#include "get_current_time.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../debug/SymbolTable.hpp"
#include "../rt/get_current_time.hpp"
#include "../types/Context.hpp"
#include "../types/PLIError.hpp"

namespace emu_rt {
using namespace hw;

uint32_t get_current_time(Machine& machine) {
  uint32_t peek_slot=Machine::copy_segment(
    static_cast<uint32_t>(machine.wsp),
    static_cast<uint32_t>(machine.wsp)-1);
  int argc=machine.memory->read_word(peek_slot)&0x7FFF;

  if(argc!=3) {
    return machine.symbols->address_for_name("?GET_CURRENT_TIME");
  }

  EagleIntegration ei(machine);
  try {
    types::Context& ctx=*machine.native_context;
    int32_t seconds=0, minutes=0, hours=0;
    rt::get_current_time_3(ctx, seconds, minutes, hours);
    // arg1 = hours, arg2 = minutes, arg3 = seconds
    machine.memory->write_word(ei.arg_addr(1), hours&0xFFFF);
    machine.memory->write_word(ei.arg_addr(2), minutes&0xFFFF);
    machine.memory->write_word(ei.arg_addr(3), seconds&0xFFFF);
    return ei.wrtn_void();
  } catch(types::PLIError& e) {
    return ei.throw_lib_error(e.signal_code);
  }
}

} // namespace emu_rt
