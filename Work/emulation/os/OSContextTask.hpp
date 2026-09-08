#pragma once
#include "OSContext.hpp"


namespace os {
class OSContextTask : public OSContext {
public:
  OSContextTask(OSProcess* process, OSTask* task, Memory* memory, Machine* machine);
  int32_t dispatch_system_call(int32_t call) override;

  int32_t TASK_call();
  int32_t REC_call();
  int32_t KILAD_call();
  int32_t DFRSCH_call();
  int32_t UIDSTAT_call();
};

} // namespace os
