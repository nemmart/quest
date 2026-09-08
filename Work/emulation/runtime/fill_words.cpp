// src/runtime/fill_words.cpp
#include "fill_words.hpp"
#include "../hw/Machine.hpp"
#include "../hw/RTBridge.hpp"
#include "../hw/RTStubs.hpp"

namespace rt {

void fill_words_3(hw::Memory& memory, uint32_t dest, int32_t count, int32_t value) {
  // Faithful port of the original body: dest[0]=value, then the exact
  // WBLM loop with ac1=count-1, ac2=dest, ac3=dest+1 (see EagleSpecial).
  int32_t src_count, dir;
  uint32_t src, dst;

  memory.write_word(dest, static_cast<uint32_t>(value)&0xFFFF);
  src_count=count-1;
  dir=src_count>0 ? -1 : (src_count<0 ? 1 : 0);
  src=dest;
  dst=dest+1;
  while(src_count!=0) {
    memory.write_word(dst, memory.read_word(src)&0xFFFF);
    src_count+=dir;
    src=src-static_cast<uint32_t>(dir);
    dst=dst-static_cast<uint32_t>(dir);
  }
}

void fill_words_2(hw::Memory& memory, uint32_t dest, int32_t count) {
  fill_words_3(memory, dest, count, 0);
}

} // namespace rt

namespace emu_rt {

uint32_t fill_words(hw::Machine& machine) {
  hw::RTBridge bridge(machine);
  uint32_t dest;
  int32_t count;

  if(bridge.arg_count()!=2 && bridge.arg_count()!=3) {
    // Unexpected arity: fall back to emulation (runs the original body).
    hw::RTStubs::log_call(machine, "?FILL_WORDS", "(native-fallback)");
    return hw::RTStubs::entry_address("?FILL_WORDS");
  }

  hw::RTStubs::log_call(machine, "?FILL_WORDS", "(native)");
  // arg1 is a POINTER passed by reference: slot -> pointer variable ->
  // buffer (the body stores via the 0x8000 indirect bit). dest is the
  // pointer variable's value, i.e. one dereference of the slot.
  dest=static_cast<uint32_t>(bridge.arg_wide(1));
  count=bridge.arg_wide(2);
  int32_t value=bridge.arg_count()==3 ? bridge.arg_word(3) : 0;
  // Dead-stack fidelity: WSAVS image + the body's one local write
  // (XNSTA value,[ac3+0x2]).
  uint32_t frame=bridge.emulate_frame();
  bridge.write_frame_word(frame, 2, value);
  rt::fill_words_3(*machine.memory, dest, count, value);
  return bridge.native_return();
}

} // namespace emu_rt
