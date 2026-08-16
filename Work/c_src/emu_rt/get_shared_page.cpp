// src/emu_rt/get_shared_page.cpp
#include "get_shared_page.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../debug/SymbolTable.hpp"
#include "../rt/get_shared_page.hpp"
#include "../types/Context.hpp"
#include "../types/PLIError.hpp"

namespace emu_rt {
using namespace hw;

uint32_t get_shared_page(Machine& machine) {
  uint32_t peek_slot=Machine::copy_segment(
    static_cast<uint32_t>(machine.wsp),
    static_cast<uint32_t>(machine.wsp)-1);
  int argc=machine.memory->read_word(peek_slot)&0x7FFF;

  if(argc!=4) {
    return machine.symbols->address_for_name("?GET_SHARED_PAGE");
  }

  EagleIntegration ei(machine);
  try {
    types::Context& ctx=*machine.native_context;
    int32_t channel=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(1))&0xFFFF);
    int32_t map_address=static_cast<int32_t>(ei.arg_wide(2));
    int32_t page_offset=static_cast<int32_t>(ei.arg_wide(3));
    int32_t page_count=static_cast<int32_t>(machine.memory->read_word(ei.arg_addr(4))&0xFFFF);
    rt::get_shared_page_4(ctx, channel, map_address, page_offset, page_count);
    return ei.wrtn_void();
  } catch(types::PLIError& e) {
    return ei.throw_lib_error(e.signal_code);
  }
}

} // namespace emu_rt
