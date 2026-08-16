#pragma once
#include "OSContext.hpp"


namespace os {
class OSContextShared : public OSContext {
public:
  OSContextShared(OSProcess* process, OSTask* task, Memory* memory, Machine* machine);
  int32_t dispatch_system_call(int32_t call) override;

  int32_t GSHPT_call();
  int32_t SSHPT_call();
  int32_t SOPEN_call();
  int32_t SCLOSE_call();
  int32_t SPAGE_call();
};

} // namespace os
