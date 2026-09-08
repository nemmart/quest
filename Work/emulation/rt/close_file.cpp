// src/rt/close_file.cpp
#include "close_file.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"
#include "../types/PLIError.hpp"

namespace rt {

void close_file_1(types::Context& ctx, int32_t channel) {
  int32_t err=ctx.os.close_file(channel);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
}

} // namespace rt
