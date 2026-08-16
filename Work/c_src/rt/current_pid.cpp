// src/rt/current_pid.cpp
#include "current_pid.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"
#include "../types/PLIError.hpp"

namespace rt {

int32_t current_pid_0(types::Context& ctx) {
  int32_t pid;
  int32_t err=ctx.os.current_pid(pid);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
  return pid;
}

} // namespace rt
