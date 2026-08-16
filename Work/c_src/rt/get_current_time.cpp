// src/rt/get_current_time.cpp
#include "get_current_time.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"
#include "../types/PLIError.hpp"

namespace rt {

void get_current_time_3(types::Context& ctx, int32_t& seconds,
                        int32_t& minutes, int32_t& hours) {
  int32_t err=ctx.os.get_current_time(seconds, minutes, hours);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
}

} // namespace rt
