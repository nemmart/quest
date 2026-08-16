// src/rt/open_shared_io_file.cpp
#include "open_shared_io_file.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"
#include "../types/PLIError.hpp"

namespace rt {

void open_shared_io_file_5(types::Context& ctx, int32_t& channel,
                           const types::String& filename, int32_t read_only,
                           int32_t ignore4, int32_t ignore5) {
  int32_t err=ctx.os.open_shared_io_file(filename, read_only, channel);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
}

} // namespace rt
