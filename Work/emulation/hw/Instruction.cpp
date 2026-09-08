#include "Instruction.hpp"
#include "Memory.hpp"
#include "../debug/Disassembler.hpp"
#include <cstdio>




namespace hw {
using namespace debug;

std::string Instruction::disassemble(Memory& memory, uint32_t address, uint32_t opcode) {
  if (name.empty())
    throw std::runtime_error("Instruction name not set");
  if (instruction_format.empty()) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "[%08X]  %04X", address, opcode);
    throw std::runtime_error(std::string("InstructionFormat for opcode ") + buf + " (" + name + ") not set");
  }
  return Disassembler::disassemble(memory, address, name, opcode, instruction_format);
}

void Instruction::debug_before_execution(Machine& machine, uint32_t address, uint32_t opcode) {
}

void Instruction::debug_after_execution(Machine& machine, uint32_t address, uint32_t opcode) {
}

} // namespace hw
