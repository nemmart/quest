// src/emu_rt/create_ipc_file.cpp
#include "create_ipc_file.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../debug/SymbolTable.hpp"
#include "../emu_types/VaryingString.hpp"
#include "../rt/create_ipc_file.hpp"
#include "../types/Context.hpp"
#include "../types/PLIError.hpp"

namespace emu_rt {
using namespace hw;

uint32_t create_ipc_file(Machine& machine) {
  uint32_t peek_slot=Machine::copy_segment(
    static_cast<uint32_t>(machine.wsp),
    static_cast<uint32_t>(machine.wsp)-1);
  int argc=machine.memory->read_word(peek_slot)&0x7FFF;

  if(argc!=3) {
    return machine.symbols->address_for_name("?CREATE_IPC_FILE");
  }

  EagleIntegration ei(machine);
  try {
    types::Context& ctx=*machine.native_context;
    emu_types::VaryingString filename(*machine.memory, ei.arg_addr(1));
    int32_t local_port=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(2))&0xFFFF);
    int32_t ignore3=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(3))&0xFFFF);
    rt::create_ipc_file_3(ctx, filename, local_port, ignore3);
    return ei.wrtn_void();
  } catch(types::PLIError& e) {
    return ei.throw_lib_error(e.signal_code);
  }
}

} // namespace emu_rt
