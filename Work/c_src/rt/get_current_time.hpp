// src/rt/get_current_time.hpp
#pragma once
#include <cstdint>

namespace types { class Context; }

namespace rt {

// ?GET_CURRENT_TIME — returns time of day via pointer args.
void get_current_time_3(types::Context& ctx, int32_t& seconds,
                        int32_t& minutes, int32_t& hours);

} // namespace rt
