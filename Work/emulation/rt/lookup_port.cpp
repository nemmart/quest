// src/rt/lookup_port.cpp
#include "lookup_port.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"
#include "../types/PLIError.hpp"
#include "../types/String.hpp"

namespace rt {

int32_t lookup_port_3(types::Context& ctx, const types::String& name, int32_t& output_pid, uint32_t ignore3) {
  int32_t err=ctx.os.lookup_port(name, output_pid);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
  return output_pid;
}

} // namespace rt
