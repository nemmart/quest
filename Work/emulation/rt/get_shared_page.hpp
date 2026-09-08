// src/rt/get_shared_page.hpp
#pragma once
#include <cstdint>

namespace types { class Context; }

namespace rt {

// ?GET_SHARED_PAGE — map shared file pages into memory
void get_shared_page_4(types::Context& ctx, int32_t channel,
                       int32_t map_address, int32_t page_offset,
                       int32_t page_count);

} // namespace rt
