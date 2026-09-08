#pragma once
#include "EagleInstruction.hpp"


namespace hw {
class EagleCompute : public EagleInstruction {
public:
  static constexpr int32_t WMOV=0, WCOM=1, WNEG=2, WXCH=3;
  static constexpr int32_t WADD=4, WSUB=5, WADC=6, WMUL=7, WDIV=8;
  static constexpr int32_t ZEX=9, SEX=10, CVWN=11;
  static constexpr int32_t WLSH=12, WASH=13, WINC=14;
  static constexpr int32_t WADI=15, WSBI=16, WLSI=17;
  static constexpr int32_t WAND=20, WIOR=21, WXOR=22;
  static constexpr int32_t WHLV=30, WMOVR=31;
  static constexpr int32_t ADDI=35, ANDI=36;
  static constexpr int32_t NADD=40, NSUB=41, NNEG=42, NMUL=43;
  static constexpr int32_t NADDI=44, NADI=45, NSBI=46;
  static constexpr int32_t WSEQ=50, WSNE=51, WSLE=52, WSLT=53;
  static constexpr int32_t WSGE=54, WSGT=55, WUSGE=56, WUSGT=57;
  static constexpr int32_t WSKBZ=60, WSKBO=61;
  static constexpr int32_t WBTZ=62, WBTO=63, WSZB=64, WSZBO=65, WLOB=66;
  static constexpr int32_t WSEQI=80, WSNEI=81, WSLEI=82, WSGTI=83;
  static constexpr int32_t NSANA=84, WSANA=85;
  static constexpr int32_t NLDAI=100, WNADI=101, WLSHI=102;
  static constexpr int32_t WLDAI=200, WADDI=201;
  static constexpr int32_t WANDI=203, WIORI=204, WXORI=205;
  static constexpr int32_t WUGTI=206, WULEI=207;
  static constexpr int32_t XNADD=300, LNADD=301, XNSUB=302, LNSUB=303;
  static constexpr int32_t XNMUL=304, LNMUL=305;
  static constexpr int32_t XNADI=306, LNADI=307, XNSBI=308, LNSBI=309;
  static constexpr int32_t XNDSZ=350, XNISZ=351, XWISZ=352;
  static constexpr int32_t XWADD=400, LWADD=401, XWSUB=402, LWSUB=403;
  static constexpr int32_t XWADI=404, XWSBI=405, XWMUL=406, LWMUL=407;
  static constexpr int32_t DIV=500, DIVX=501, WDIVS=502;

  int32_t XX, YY;

  void setup(uint32_t opcode, const std::string& name, const std::string& fmt, int32_t op) override;
  uint32_t execute(Machine& machine, uint32_t address, uint32_t opcode) override;
};

} // namespace hw
