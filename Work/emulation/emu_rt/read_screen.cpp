// src/emu_rt/read_screen.cpp
#include "read_screen.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../debug/SymbolTable.hpp"
#include "../emu_types/VaryingString.hpp"
#include "../rt/read_screen.hpp"
#include "../types/Context.hpp"
#include "../types/PLIError.hpp"

namespace emu_rt {
using namespace hw;

uint32_t read_screen(Machine& machine) {
  uint32_t peek_slot=Machine::copy_segment(
    static_cast<uint32_t>(machine.wsp),
    static_cast<uint32_t>(machine.wsp)-1);
  int argc=machine.memory->read_word(peek_slot)&0x7FFF;

  if(argc!=3) {
    return machine.symbols->address_for_name("?READ_SCREEN");
  }

  EagleIntegration ei(machine);
  try {
    types::Context& ctx=*machine.native_context;
    int32_t channel=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(1))&0xFFFF);
    emu_types::VaryingString dest(*machine.memory, ei.arg_addr(2));
    int32_t max_length=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(3))&0xFFFF);
    rt::read_screen_3(ctx, channel, dest, max_length);
    return ei.wrtn_void();
  } catch(types::PLIError& e) {
    return ei.throw_lib_error(e.signal_code);
  }
}

} // namespace emu_rt
