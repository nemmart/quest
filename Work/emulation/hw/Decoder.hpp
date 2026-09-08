#pragma once
#include "Definition.hpp"
#include "Instruction.hpp"
#include "LEFInstruction.hpp"
#include <memory>
#include <vector>
#include <string>


namespace hw {
class Decoder {
public:
  // Opcode definition tables
  static std::vector<Definition> nova_general_opcodes;
  static std::vector<Definition> nova_io_opcodes;
  static std::vector<Definition> nova_lef_opcodes;
  static std::vector<Definition> nova_compute_opcodes;
  static std::vector<Definition> eclipse_mv_opcodes;

  // Fast lookup tables (populated by initialize())
  static Instruction* nova_general[12];
  static Instruction* nova_io[32];
  static Instruction* nova_compute[8];
  static Instruction* eclipse_mv[4096];
  static LEFInstruction lef_instruction;

  static uint32_t mask_for_match(const std::string& match);
  static uint32_t value_for_match(const std::string& match);
  static Instruction* instantiate(const std::string& class_name);
  static Instruction* find_opcode(const std::vector<Definition>& table, uint32_t opcode);
  static Instruction* slow_decode(bool lef_mode, uint32_t opcode);
  static Instruction* decode(bool lef_mode, uint32_t opcode);
  static void initialize();
};

} // namespace hw
