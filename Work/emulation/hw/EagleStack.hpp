#pragma once
#include "Instruction.hpp"


namespace hw {
class EagleStack : public Instruction {
public:
  static constexpr int32_t XCALL=0, LCALL=1, WSAVR=2, WSAVS=3;
  static constexpr int32_t WSSVR=4, WSSVS=5, WRTN=6, WPOPB=7;
  static constexpr int32_t LDASP=20, STASP=21, LDAFP=22, STAFP=23;
  static constexpr int32_t LDASB=24, STASB=25, LDASL=26, STASL=27;
  static constexpr int32_t LDATS=80, STATS=81, ISZTS=82, DSZTS=83;
  static constexpr int32_t XPEF=90, LPEF=91, XPEFB=92, LPEFB=93;
  static constexpr int32_t XPSHJ=94, LPSHJ=95;
  static constexpr int32_t WMSP=100, WPSH=101, WPOP=102;
  static constexpr int32_t WFPSH=103, WFPOP=104, WPOPJ=110;
  static constexpr int32_t DERR=120;

  int32_t AA, XX;

  void setup(uint32_t opcode, const std::string& name, const std::string& fmt, int32_t op) override;
  uint32_t execute(Machine& machine, uint32_t address, uint32_t opcode) override;

private:
  uint32_t handle_overflow(Machine& machine, uint32_t address, uint32_t next_instruction);
};

} // namespace hw
