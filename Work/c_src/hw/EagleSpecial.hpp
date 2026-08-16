#pragma once
#include "Instruction.hpp"


namespace hw {
class EagleSpecial : public Instruction {
public:
  static constexpr int32_t WBLM=0, WCMV=1, WCMP=2, WCST=3;
  static constexpr int32_t WMESS=10, ENQT=11, DEQUE=12, ENQH=13;

  static int32_t direction(int32_t count);
  uint32_t execute(Machine& machine, uint32_t address, uint32_t opcode) override;
};

} // namespace hw
