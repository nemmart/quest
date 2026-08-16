// src/rt/random_number.hpp
#pragma once
#include <cstdint>

namespace types { class Context; }

namespace rt {

// ?RANDOM_NUMBER — linear congruential generator
// seed = fmod(seed * 31415933.0 + 14181771.0, 67108864.0)
// result = lower + int(seed * range / 67108864.0)
// State accessed via ctx.random_state.
int32_t random_number_3(types::Context& ctx, int32_t lower, int32_t upper);

} // namespace rt
