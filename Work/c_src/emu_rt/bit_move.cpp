// src/emu_rt/bit_move.cpp
#include "bit_move.hpp"
#include "../hw/EagleIntegration.hpp"
#include "../hw/Machine.hpp"
#include "../debug/SymbolTable.hpp"
#include <cstdio>

namespace emu_rt {
using namespace hw;

static bool test_bit(Memory& mem, int32_t word_addr, int32_t bit) {
  uint32_t addr=static_cast<uint32_t>(word_addr)+(static_cast<uint32_t>(bit)>>4);
  uint32_t mask=0x8000>>(bit&0x0F);
  return (mem.read_word(addr)&mask)!=0;
}

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

uint32_t bit_move(Machine& machine) {
  fprintf(stderr, "B.MOVE intercepted\n");
  uint32_t peek_slot=Machine::copy_segment(
    static_cast<uint32_t>(machine.wsp),
    static_cast<uint32_t>(machine.wsp)-1);
  int argc=machine.memory->read_word(peek_slot)&0x7FFF;

  if(argc!=6) {
    return machine.symbols->address_for_name("B.MOVE");
  }

  EagleIntegration ei(machine);
  Memory& mem=*machine.memory;

  // B.MOVE uses WPSH calling convention (values, not XPEF pointers).
  // Stack layout (arg_addr returns raw values):
  //   arg_addr(6) = source word addr
  //   arg_addr(5) = source bit offset
  //   arg_addr(4) = dest bit count (capacity for padding computation)
  //   arg_addr(3) = dest word addr
  //   arg_addr(2) = dest bit start
  //   arg_addr(1) = total bit count
  int32_t src_addr=static_cast<int32_t>(ei.arg_addr(6));
  int32_t src_bit=static_cast<int32_t>(ei.arg_addr(5));
  int32_t dest_bit_count=static_cast<int32_t>(ei.arg_addr(4));
  int32_t dest_addr=static_cast<int32_t>(ei.arg_addr(3));
  int32_t dest_bit=static_cast<int32_t>(ei.arg_addr(2));
  int32_t total_count=static_cast<int32_t>(ei.arg_addr(1));

  // copy_count = min(dest_bit_count, total_count)
  // padding    = max(0, total_count - dest_bit_count) + 1
  int32_t copy_count=(dest_bit_count<total_count) ? dest_bit_count : total_count;
  int32_t diff=total_count-dest_bit_count;
  if(diff<0) diff=0;
  int32_t padding=diff+1;

  // Copy bits from source to dest
  for(int32_t i=0; i<copy_count; i++) {
    if(test_bit(mem, src_addr, src_bit))
      set_bit(mem, dest_addr, dest_bit);
    else
      clear_bit(mem, dest_addr, dest_bit);
    src_bit++;
    dest_bit++;
  }

  // Zero-pad remaining dest bits
  for(int32_t i=0; i<padding; i++) {
    clear_bit(mem, dest_addr, dest_bit);
    dest_bit++;
  }

  return ei.wrtn_void();
}

} // namespace emu_rt
