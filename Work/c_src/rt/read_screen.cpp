// src/rt/read_screen.cpp
#include "read_screen.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"
#include "../types/PLIError.hpp"

namespace rt {

void read_screen_3(types::Context& ctx, int32_t channel, types::String& result,
                   int32_t max_length) {
  int32_t err=ctx.os.read_screen(channel, result, max_length);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
}

} // namespace rt
