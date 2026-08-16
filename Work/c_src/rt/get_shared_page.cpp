// src/rt/get_shared_page.cpp
#include "get_shared_page.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"
#include "../types/PLIError.hpp"

namespace rt {

void get_shared_page_4(types::Context& ctx, int32_t channel,
                       int32_t map_address, int32_t page_offset,
                       int32_t page_count) {
  int32_t err=ctx.os.get_shared_page(channel, map_address, page_count, page_offset);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
}

} // namespace rt
