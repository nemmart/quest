// src/emu_rt/unsigned_to_char.cpp
#include "unsigned_to_char.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../debug/SymbolTable.hpp"
#include "../emu_types/VaryingString.hpp"
#include "../rt/unsigned_to_char.hpp"
#include "../types/Context.hpp"

namespace emu_rt {
using namespace hw;

uint32_t unsigned_to_char(Machine& machine) {
  // Peek at arg count before constructing EagleIntegration.
  uint32_t peek_slot=Machine::copy_segment(
    static_cast<uint32_t>(machine.wsp),
    static_cast<uint32_t>(machine.wsp)-1);
  int argc=machine.memory->read_word(peek_slot)&0x7FFF;

  if(argc!=1) {
    return machine.symbols->address_for_name("?UNSIGNED_TO_CHAR");
  }

  // Destination VARYING string address is in AC2 (set by XLEF before LCALL).
  // Read it before EagleIntegration, though EI doesn't modify ac[2].
  uint32_t dest_addr=static_cast<uint32_t>(machine.ac[2]);

  EagleIntegration ei(machine);
  types::Context& ctx=*machine.native_context;
  uint32_t value=ei.arg_wide(1);
  emu_types::VaryingString dest(*machine.memory, dest_addr);
  rt::unsigned_to_char_1(ctx, dest, value);
  return ei.wrtn_void();
}

} // namespace emu_rt
