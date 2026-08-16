// src/emu_rt/write_screen.cpp
#include "write_screen.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../debug/SymbolTable.hpp"
#include "../emu_types/VaryingString.hpp"
#include "../rt/write_screen.hpp"
#include "../types/PLIError.hpp"

namespace emu_rt {
using namespace hw;

uint32_t write_screen(Machine& machine) {
  // Peek at arg count before constructing EagleIntegration.
  uint32_t peek_slot=Machine::copy_segment(
    static_cast<uint32_t>(machine.wsp),
    static_cast<uint32_t>(machine.wsp)-1);
  int argc=machine.memory->read_word(peek_slot)&0x7FFF;

  if(argc!=2 && argc!=5) {
    // Fall back to emulated code — stack is untouched
    return machine.symbols->address_for_name("?WRITE_SCREEN");
  }

  EagleIntegration ei(machine);
  try {
    types::Context& ctx=*machine.native_context;
    int32_t channel;
    emu_types::VaryingString text;

    if(argc==2) {
      channel=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(1))&0xFFFF);
      text=emu_types::VaryingString(*machine.memory, ei.arg_addr(2));
      rt::write_screen_2(ctx, channel, text);
    }
    else if(argc==5) {
      channel=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(1))&0xFFFF);
      text=emu_types::VaryingString(*machine.memory, ei.arg_addr(2));
      int32_t row=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(3))&0xFF);
      int32_t col=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(4))&0xFF);
      int32_t options=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(5))&0xFFFF);
      rt::write_screen_5(ctx, channel, text, row, col, options);
      // If bit 0x200 set, write row/col back to caller
      if(options&0x0200) {
        machine.memory->write_word(ei.arg_addr(4), static_cast<uint32_t>(col)&0xFF);
        machine.memory->write_word(ei.arg_addr(3), static_cast<uint32_t>(row)&0xFF);
      }
    }

    return ei.wrtn_void();
  } catch(types::PLIError& e) {
    return ei.throw_lib_error(e.signal_code);
  }
}

} // namespace emu_rt
