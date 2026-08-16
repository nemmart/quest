// src/emu_rt/write.cpp
#include "write.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../debug/SymbolTable.hpp"
#include "../emu_types/ByteArray.hpp"
#include "../rt/write.hpp"
#include "../types/Context.hpp"
#include "../types/PLIError.hpp"

namespace emu_rt {
using namespace hw;

uint32_t write(Machine& machine) {
  uint32_t peek_slot=Machine::copy_segment(
    static_cast<uint32_t>(machine.wsp),
    static_cast<uint32_t>(machine.wsp)-1);
  int argc=machine.memory->read_word(peek_slot)&0x7FFF;

  if(argc!=3 && argc!=6) {
    return machine.symbols->address_for_name("?WRITE");
  }

  EagleIntegration ei(machine);
  try {
    types::Context& ctx=*machine.native_context;
    int32_t channel=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(1))&0xFFFF);
    uint32_t buf_byte_addr=ei.arg_wide(2);
    int32_t length=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(3))&0xFFFF);

    emu_types::ByteArray buffer(*machine.memory, buf_byte_addr, static_cast<size_t>(length),
                                static_cast<size_t>(length));

    if(argc==3) {
      rt::write_3(ctx, channel, buffer, length);
    }
    else {
      int32_t options=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(4))&0xFFFF);
      int32_t extra=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(5))&0xFFFF);
      int32_t record_number=static_cast<int32_t>(ei.arg_wide(6));
      rt::write_6(ctx, channel, buffer, length, options, extra, record_number);
    }

    // Write actual length back to *arg3
    machine.memory->write_word(ei.arg_addr(3), static_cast<uint32_t>(length)&0xFFFF);
    return ei.wrtn_void();
  } catch(types::PLIError& e) {
    return ei.throw_lib_error(e.signal_code);
  }
}

} // namespace emu_rt
