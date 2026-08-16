#pragma once
#include "EagleInstruction.hpp"


namespace hw {
class EagleGeneral : public EagleInstruction {
public:
  static constexpr int32_t XWLDA=10, XWSTA=11, LWLDA=12, LWSTA=13;
  static constexpr int32_t XNLDA=20, XNSTA=21, LNLDA=22, LNSTA=23;
  static constexpr int32_t XLEF=40, LLEF=41;
  static constexpr int32_t XLEFB=42, LLEFB=43, XLDB=44, XSTB=45, LLDB=46, LSTB=47;
  static constexpr int32_t WLDB=50, WSTB=51;
  static constexpr int32_t LPSR=90, CRYTO=91, CRYTZ=92;
  static constexpr int32_t XJMP=100, XJSR=101, XNDO=102, XWDO=103;
  static constexpr int32_t LJMP=110, LJSR=111, LNDO=112, LWDO=113, LDSP=114, WCLM=115;
  static constexpr int32_t WBR=200;
  static constexpr int32_t XCT=210;

  int32_t II, AA;

  void setup(uint32_t opcode, const std::string& name, const std::string& fmt, int32_t op) override;
  uint32_t execute(Machine& machine, uint32_t address, uint32_t opcode) override;
};

} // namespace hw
