// src/emu_rt/open_shared_io_file.cpp
#include "open_shared_io_file.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../debug/SymbolTable.hpp"
#include "../emu_types/VaryingString.hpp"
#include "../rt/open_shared_io_file.hpp"
#include "../types/Context.hpp"
#include "../types/PLIError.hpp"

namespace emu_rt {
using namespace hw;

uint32_t open_shared_io_file(Machine& machine) {
  uint32_t peek_slot=Machine::copy_segment(
    static_cast<uint32_t>(machine.wsp),
    static_cast<uint32_t>(machine.wsp)-1);
  int argc=machine.memory->read_word(peek_slot)&0x7FFF;

  if(argc!=5) {
    return machine.symbols->address_for_name("?OPEN_SHARED_IO_FILE");
  }

  EagleIntegration ei(machine);
  try {
    types::Context& ctx=*machine.native_context;
    emu_types::VaryingString filename(*machine.memory, ei.arg_addr(2));
    int32_t read_only=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(3))&0xFFFF);
    int32_t channel=0;
    rt::open_shared_io_file_5(ctx, channel, filename, read_only==0 ? 1 : 0, 0, 0);
    machine.memory->write_word(ei.arg_addr(1), static_cast<uint32_t>(channel)&0xFFFF);
    return ei.wrtn_void();
  } catch(types::PLIError& e) {
    return ei.throw_lib_error(e.signal_code);
  }
}

} // namespace emu_rt
