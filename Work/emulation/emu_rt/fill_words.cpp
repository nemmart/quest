// src/emu_rt/fill_words.cpp
#include "fill_words.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../debug/SymbolTable.hpp"
#include "../types/PLIError.hpp"

namespace emu_rt {
using namespace hw;

uint32_t fill_words(Machine& machine) {
  uint32_t peek_slot=Machine::copy_segment(
    static_cast<uint32_t>(machine.wsp),
    static_cast<uint32_t>(machine.wsp)-1);
  int argc=machine.memory->read_word(peek_slot)&0x7FFF;

  if(argc!=2 && argc!=3) {
    return machine.symbols->address_for_name("?FILL_WORDS");
  }

  EagleIntegration ei(machine);
  Memory& mem=*machine.memory;
  // arg1 = dest address (wide), arg2 = word count
  uint32_t dest_addr=ei.arg_wide(1);
  int32_t word_count=static_cast<int32_t>(ei.arg_wide(2));
  int32_t fill_value=0;
  if(argc==3)
    fill_value=static_cast<int32_t>(mem.read_word(ei.arg_addr(3))&0xFFFF);

  for(int32_t i=0; i<word_count; i++)
    mem.write_word(dest_addr+static_cast<uint32_t>(i), fill_value&0xFFFF);

  return ei.wrtn_void();
}

} // namespace emu_rt
