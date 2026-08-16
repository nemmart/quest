#pragma once
#include "OSContext.hpp"


namespace os {
class OSContextIPC : public OSContext {
public:
  OSContextIPC(OSProcess* process, OSTask* task, Memory* memory, Machine* machine);
  int32_t dispatch_system_call(int32_t call) override;

  int32_t CREATE_call();
  int32_t SERVE_call();
  int32_t ILKUP_call();
  int32_t ISEND_call();
  int32_t IREC_call();
  int32_t ISR_call();
  int32_t CON_call();
  int32_t DCON_call();

  void irec_return();
};

} // namespace os
