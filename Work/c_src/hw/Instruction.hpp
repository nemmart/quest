#pragma once
#include <cstdint>
#include <string>
#include <stdexcept>
#include <cstdio>


namespace hw {
class Machine;
class Memory;

class Instruction {
public:
  std::string name;
  std::string instruction_format;
  int32_t oper = -1;  // "operator" is a C++ keyword

  Instruction() = default;
  virtual ~Instruction() = default;

  virtual void setup(uint32_t opcode, const std::string& name, const std::string& instruction_format, int32_t oper) {
    this->name = name;
    this->instruction_format = instruction_format;
    this->oper = oper;
  }

  virtual uint32_t execute(Machine& machine, uint32_t address, uint32_t opcode) {
    throw std::runtime_error("Instruction subclass must override execute " + name);
  }

  std::string disassemble(Memory& memory, uint32_t address, uint32_t opcode);
  void debug_before_execution(Machine& machine, uint32_t address, uint32_t opcode);
  void debug_after_execution(Machine& machine, uint32_t address, uint32_t opcode);

  // Segment helpers — pure bit manipulation, no Machine dependency
  static uint32_t get_segment(uint32_t address) {
    return (address >> 28) & 0x07;
  }

  static uint32_t set_segment(uint32_t segment, uint32_t address) {
    return (address & 0x0FFFFFFF) | (segment << 28);
  }

  static uint32_t copy_segment(uint32_t segment_address, uint32_t address) {
    uint32_t segment = (segment_address >> 28) & 0x07;
    return (address & 0x0FFFFFFF) | (segment << 28);
  }

  static uint32_t get_byte_segment(uint32_t address) {
    return (address >> 29) & 0x07;
  }

  static uint32_t set_byte_segment(uint32_t segment, uint32_t address) {
    return (address & 0x1FFFFFFF) | (segment << 29);
  }

  static uint32_t copy_byte_segment(uint32_t segment_address, uint32_t address) {
    uint32_t segment = (segment_address >> 29) & 0x07;
    return (address & 0x1FFFFFFF) | (segment << 29);
  }
};

} // namespace hw
