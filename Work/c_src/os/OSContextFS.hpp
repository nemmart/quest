#pragma once
#include "OSContext.hpp"


namespace os {
class FSChannel;
class OSContextFS : public OSContext {
public:
  OSContextFS(OSProcess* process, OSTask* task, Memory* memory, Machine* machine);
  int32_t dispatch_system_call(int32_t call) override;

  int32_t OPEN_call();
  int32_t CLOSE_call();
  int32_t READ_call();
  int32_t WRITE_call();
  void echo_to_clone_terminal(FSChannel* channel,
                              const std::vector<uint8_t>& bytes);
  int32_t UPDATE_call();
};

} // namespace os
