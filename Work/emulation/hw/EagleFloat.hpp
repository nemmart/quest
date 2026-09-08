#pragma once
#include "EagleInstruction.hpp"


namespace hw {
class EagleFloat : public EagleInstruction {
public:
  static constexpr int32_t FTD=10, FTE=11;
  static constexpr int32_t FMOV=100, WFLAD=101, WFFAD=102;
  static constexpr int32_t XFLDS=103, LFLDS=104, XFSTS=105, LFSTS=106;
  static constexpr int32_t XFLDD=107, LFLDD=108, XFSTD=109, LFSTD=110;
  static constexpr int32_t FRDS=150, FHLV=151, FINT=152, FRH=153, FEXP=154, FSCAL=155;
  static constexpr int32_t FAS=200, FSS=201, FMS=202, FDS=203;
  static constexpr int32_t FAD=250, FSD=251, FMD=252, FDD=253, FCMP=254;
  static constexpr int32_t FSEQ=270, FSNE=271, FSGE=272, FSGT=273, FSLE=274, FSLT=275;
  static constexpr int32_t XFAMS=300, LFAMS=301, XFAMD=302, LFAMD=303;
  static constexpr int32_t XFMMS=304, LFMMS=305, XFMMD=306, LFMMD=307;

  int32_t XX, YY;

  void setup(uint32_t opcode, const std::string& name, const std::string& fmt, int32_t op) override;
  uint32_t execute(Machine& machine, uint32_t address, uint32_t opcode) override;
};

} // namespace hw
