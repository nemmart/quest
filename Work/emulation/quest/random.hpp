// src/quest/random.hpp
#pragma once
#include <cstdint>

namespace types { class Context; }

namespace quest {

// RANDOM — percentage-based probability check.
// 1 LCALL arg: threshold (1..100).
// Generates random(1,100) and returns 0x8000 (true) if the roll
// is strictly less than threshold, 0x0000 (false) otherwise.
// RANDOM(30) ≈ 29% chance of returning true.
int32_t random(types::Context& ctx, int32_t threshold);

} // namespace quest
