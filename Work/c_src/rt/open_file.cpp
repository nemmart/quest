// src/rt/open_file.cpp
#include "open_file.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"
#include "../types/PLIError.hpp"

namespace rt {

static constexpr int32_t DEFAULT_OPEN_OPTIONS=0x403A;

void open_file_2(types::Context& ctx, int32_t& channel, const types::String& filename) {
  int32_t err=ctx.os.open_file(filename, DEFAULT_OPEN_OPTIONS, channel);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
}

} // namespace rt
