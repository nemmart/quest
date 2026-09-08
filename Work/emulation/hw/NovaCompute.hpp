#pragma once
#include "Instruction.hpp"


namespace hw {
class NovaCompute : public Instruction {
public:
  static constexpr int32_t COM = 0;
  static constexpr int32_t NEG = 1;
  static constexpr int32_t MOV = 2;
  static constexpr int32_t INC = 3;
  static constexpr int32_t ADC = 4;
  static constexpr int32_t SUB = 5;
  static constexpr int32_t ADD = 6;
  static constexpr int32_t AND = 7;

  uint32_t execute(Machine& machine, uint32_t address, uint32_t opcode) override;
};

} // namespace hw
