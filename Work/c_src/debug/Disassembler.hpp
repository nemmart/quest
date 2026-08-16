#pragma once
#include <cstdint>
#include <string>


namespace hw { class Memory; }

namespace debug {
using namespace hw;


class Disassembler {
public:
  static const char* carry_actions[4];
  static const char* shift_actions[4];
  static const char* skip_actions[8];

  static std::string nova_suffix(uint32_t opcode);
  static std::string nova_skip(uint32_t opcode);
  static std::string word_byte_indexed(Memory& memory, uint32_t address, uint32_t opcode, int index_location);
  static std::string wide_byte_indexed(Memory& memory, uint32_t address, uint32_t opcode, int index_location);
  static std::string word_indirect(Memory& memory, uint32_t address, uint32_t opcode, int index_location);
  static std::string wide_indirect(Memory& memory, uint32_t address, uint32_t opcode, int index_location);
  static std::string disassemble(Memory& memory, uint32_t address, const std::string& name, uint32_t opcode, const std::string& instruction_format);
  static int word_length(const std::string& instruction_format);
};

} // namespace debug
