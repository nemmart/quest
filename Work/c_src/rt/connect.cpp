// src/rt/connect.cpp
#include "connect.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"
#include "../types/PLIError.hpp"

namespace rt {

void connect_1(types::Context& ctx, int32_t pid) {
  int32_t err=ctx.os.connect(pid);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
}

} // namespace rt
