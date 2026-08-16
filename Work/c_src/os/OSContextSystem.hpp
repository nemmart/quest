#pragma once
#include "OSContext.hpp"


namespace os {
class OSContextSystem : public OSContext {
public:
  OSContextSystem(OSProcess* process, OSTask* task, Memory* memory, Machine* machine);
  int32_t dispatch_system_call(int32_t call) override;

  int32_t RECREATE_call();
  int32_t MEM_call();
  int32_t MEMI_call();
  int32_t GTOD_call();
  int32_t PNAME_call();
  int32_t RNGPR_call();
  int32_t ERMSG_call();
  int32_t DADID_call();
  int32_t RETURN_call();
  int32_t IXIT_call();
  int32_t INTWT_call();
  int32_t WDELAY_call();
};

} // namespace os
