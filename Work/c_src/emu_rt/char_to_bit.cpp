// src/emu_rt/char_to_bit.cpp
#include "char_to_bit.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include <cstdio>

namespace emu_rt {
using namespace hw;

static void set_bit(Memory& mem, int32_t word_addr, int32_t bit) {
  uint32_t addr=static_cast<uint32_t>(word_addr)+(static_cast<uint32_t>(bit)>>4);
  uint32_t mask=0x8000>>(bit&0x0F);
  mem.write_word(addr, mem.read_word(addr)|mask);
}

static void clear_bit(Memory& mem, int32_t word_addr, int32_t bit) {
  uint32_t addr=static_cast<uint32_t>(word_addr)+(static_cast<uint32_t>(bit)>>4);
  uint32_t mask=0x8000>>(bit&0x0F);
  mem.write_word(addr, mem.read_word(addr)&~mask);
}

uint32_t char_to_bit(Machine& machine) {
  // X.CB — PL/I CHAR to BIT conversion.
  // Converts a string of '0'/'1' characters into a packed bit string.
  //
  // Register calling convention (no XPEF args):
  //   ac0 = source byte address
  //   ac1 = bit count (WINC+STATS+DSZTS loop runs ac1 times)
  //   ac2 = dest word address (bit 0 of this word = first dest bit)
  uint32_t src_byte_addr=static_cast<uint32_t>(machine.ac[0]);
  int32_t count=machine.ac[1];
  int32_t dest_word_addr=machine.ac[2];

  EagleIntegration ei(machine);
  Memory& mem=*machine.memory;

  for(int32_t i=0; i<count; i++) {
    uint8_t ch=static_cast<uint8_t>(mem.read_byte(src_byte_addr+static_cast<uint32_t>(i)));
    if(ch=='1')
      set_bit(mem, dest_word_addr, i);
    else if(ch=='0')
      clear_bit(mem, dest_word_addr, i);
    else {
      // CONVERSION error — signal via O.SCONVE → ?LIB_ERROR chain
      fprintf(stderr, "X.CB: CONVERSION error, char=0x%02X at offset %d\n", ch, i);
      return ei.throw_lib_error(0x00011611);
    }
  }

  return ei.wrtn_void();
}

} // namespace emu_rt
