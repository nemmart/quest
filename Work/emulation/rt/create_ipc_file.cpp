// src/rt/create_ipc_file.cpp
#include "create_ipc_file.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"
#include "../types/PLIError.hpp"

namespace rt {

void create_ipc_file_3(types::Context& ctx, const types::String& filename,
                       int32_t local_port, int32_t ignore3) {
  int32_t err=ctx.os.create_ipc_file(filename, local_port);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
}

} // namespace rt
