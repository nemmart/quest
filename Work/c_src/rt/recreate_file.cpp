// src/rt/recreate_file.cpp
#include "recreate_file.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"
#include "../types/PLIError.hpp"

namespace rt {

void recreate_file_2(types::Context& ctx, const types::String& filename,
                     int32_t ignore2) {
  int32_t err=ctx.os.recreate_file(filename);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
}

} // namespace rt
