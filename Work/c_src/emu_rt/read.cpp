// src/emu_rt/read.cpp
#include "read.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../debug/SymbolTable.hpp"
#include "../emu_types/ByteArray.hpp"
#include "../rt/read.hpp"
#include "../types/Context.hpp"
#include "../types/PLIError.hpp"

namespace emu_rt {
using namespace hw;

uint32_t read(Machine& machine) {
  // Peek at arg count before constructing EagleIntegration.
  uint32_t peek_slot=Machine::copy_segment(
    static_cast<uint32_t>(machine.wsp),
    static_cast<uint32_t>(machine.wsp)-1);
  int argc=machine.memory->read_word(peek_slot)&0x7FFF;

  if(argc!=4 && argc!=6 && argc!=7) {
    // Fall back to emulated code — stack is untouched
    return machine.symbols->address_for_name("?READ");
  }

  EagleIntegration ei(machine);
  try {
    types::Context& ctx=*machine.native_context;
    int32_t channel=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(1))&0xFFFF);
    uint32_t buf_byte_addr=ei.arg_wide(2);
    int32_t length=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(3))&0xFFFF);

    emu_types::ByteArray buffer(*machine.memory, buf_byte_addr, 0, static_cast<size_t>(length));
    bool eof=false;

    if(argc==4) {
      rt::read_4(ctx, channel, buffer, eof);
    }
    else if(argc==6) {
      int32_t options=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(5))&0xFFFF);
      int32_t extra=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(6))&0xFFFF);
      rt::read_6(ctx, channel, buffer, eof, options, extra);
    }
    else {
      int32_t options=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(5))&0xFFFF);
      int32_t extra=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(6))&0xFFFF);
      int32_t record_number=static_cast<int32_t>(ei.arg_wide(7));
      rt::read_7(ctx, channel, buffer, eof, options, extra, record_number);
    }

    // Write actual length back to *arg3
    machine.memory->write_word(ei.arg_addr(3), static_cast<uint32_t>(buffer.size())&0xFFFF);
    // Write EOF flag to *arg4 (present for both 4-arg and 6-arg)
    machine.memory->write_word(ei.arg_addr(4), eof ? 0x8000u : 0u);

    return ei.wrtn_void();
  } catch(types::PLIError& e) {
    return ei.throw_lib_error(e.signal_code);
  }
}

} // namespace emu_rt
