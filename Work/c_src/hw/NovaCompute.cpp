#include "NovaCompute.hpp"
#include "Machine.hpp"




namespace hw {
uint32_t NovaCompute::execute(Machine& machine, uint32_t address, uint32_t opcode) {
  uint32_t KKK = opcode & 0x07;
  opcode = opcode >> 3;
  uint32_t N = opcode & 0x01;
  opcode = opcode >> 1;
  uint32_t CC = opcode & 0x03;
  opcode = opcode >> 2;
  uint32_t SS = opcode & 0x03;
  opcode = opcode >> 2;
  opcode = opcode >> 3;  // skip OOO
  uint32_t YY = opcode & 0x03;
  opcode = opcode >> 2;
  uint32_t XX = opcode & 0x03;

  uint32_t c = static_cast<uint32_t>(machine.c) << 16;
  uint32_t src = machine.ac[XX] & 0xFFFF;
  uint32_t dst = machine.ac[YY] & 0xFFFF;

  switch (CC) {
    case 0: src = src | c; break;
    case 1: break;
    case 2: src = src | 0x10000; break;
    case 3: src = (src | c) ^ 0x10000; break;
  }

  switch (oper) {
    case COM: src = src ^ 0xFFFF; break;
    case NEG: src = (src ^ 0xFFFF) + 1; break;
    case MOV: break;
    case INC: src = src + 1; break;
    case ADC: src = (src ^ 0xFFFF) + dst; break;
    case SUB: src = (src ^ 0xFFFF) + dst + 1; break;
    case ADD: src = src + dst; break;
    case AND: src = src & (dst | 0x10000); break;
  }

  switch (SS) {
    case 0:
      c = (src >> 16) & 0x01;
      src = src & 0xFFFF;
      break;
    case 1:
      c = (src >> 15) & 0x01;
      src = (src & 0xFFFF) << 1 | ((src >> 16) & 0x01);
      break;
    case 2:
      c = src & 0x01;
      src = (src >> 1) & 0xFFFF;
      break;
    case 3:
      c = (src >> 16) & 0x01;
      src = ((src & 0xFF) << 8) | ((src & 0xFF00) >> 8);
      break;
  }

  if (N == 0) {
    machine.c = static_cast<int32_t>(c);
    machine.ac[YY] = static_cast<int32_t>(src);
  }

  switch (KKK) {
    case 0: break;
    case 1: return copy_segment(address, address + 2);
    case 2: if (c == 0) return copy_segment(address, address + 2); break;
    case 3: if (c == 1) return copy_segment(address, address + 2); break;
    case 4: if (src == 0) return copy_segment(address, address + 2); break;
    case 5: if (src != 0) return copy_segment(address, address + 2); break;
    case 6: if (c == 0 || src == 0) return copy_segment(address, address + 2); break;
    case 7: if (c == 1 && src != 0) return copy_segment(address, address + 2); break;
  }

  return copy_segment(address, address + 1);
}

} // namespace hw
