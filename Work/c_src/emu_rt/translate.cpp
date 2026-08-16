// src/emu_rt/translate.cpp
#include "translate.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../debug/SymbolTable.hpp"
#include <cstdio>

namespace emu_rt {
using namespace hw;

uint32_t translate(Machine& machine) {
  fprintf(stderr, "C.TRANS intercepted\n");
  uint32_t peek_slot=Machine::copy_segment(
    static_cast<uint32_t>(machine.wsp),
    static_cast<uint32_t>(machine.wsp)-1);
  int argc=machine.memory->read_word(peek_slot)&0x7FFF;

  if(argc!=6) {
    return machine.symbols->address_for_name("C.TRANS");
  }

  // C.TRANS uses WPSH calling convention (values, not XPEF pointers).
  // Result destination byte address is in AC0 at call time.
  uint32_t result_byte_addr=static_cast<uint32_t>(machine.ac[0]);

  EagleIntegration ei(machine);

  // Args pushed via 3x WPSH 0,1 (ac0=addr, ac1=len each time).
  // arg_addr returns raw wide values for WPSH-style calls.
  // Stack order (last pushed = arg_addr(1)):
  //   arg_addr(1) = from_len, arg_addr(2) = from_addr
  //   arg_addr(3) = to_len,   arg_addr(4) = to_addr
  //   arg_addr(5) = src_len,  arg_addr(6) = src_addr
  uint32_t from_addr=ei.arg_addr(2);
  int32_t from_len=static_cast<int32_t>(ei.arg_addr(1));
  uint32_t to_addr=ei.arg_addr(4);
  int32_t to_len=static_cast<int32_t>(ei.arg_addr(3));
  uint32_t src_addr=ei.arg_addr(6);
  int32_t src_len=static_cast<int32_t>(ei.arg_addr(5));

  // Read from/to char arrays from memory (byte-addressed)
  std::string from_chars, to_chars;
  from_chars.resize(from_len);
  to_chars.resize(to_len);
  for(int32_t i=0; i<from_len; i++)
    from_chars[i]=static_cast<char>(machine.memory->read_byte(from_addr+static_cast<uint32_t>(i)));
  for(int32_t i=0; i<to_len; i++)
    to_chars[i]=static_cast<char>(machine.memory->read_byte(to_addr+static_cast<uint32_t>(i)));

  // Source string as a temporary VaryingString-like read
  std::string src_str;
  src_str.resize(src_len);
  for(int32_t i=0; i<src_len; i++)
    src_str[i]=static_cast<char>(machine.memory->read_byte(src_addr+static_cast<uint32_t>(i)));

  // Build result using translation table
  uint8_t table[256];
  for(int i=0; i<256; i++)
    table[i]=static_cast<uint8_t>(i);
  size_t tlen=from_chars.size();
  if(to_chars.size()<tlen)
    tlen=to_chars.size();
  for(size_t i=0; i<tlen; i++)
    table[static_cast<uint8_t>(from_chars[i])]=static_cast<uint8_t>(to_chars[i]);

  // Write translated result to destination (byte-addressed)
  for(int32_t i=0; i<src_len; i++) {
    uint8_t ch=table[static_cast<uint8_t>(src_str[i])];
    machine.memory->write_byte(result_byte_addr+static_cast<uint32_t>(i), ch);
  }

  return ei.wrtn_void();
}

} // namespace emu_rt
