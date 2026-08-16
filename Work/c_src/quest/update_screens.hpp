// src/quest/update_screens.hpp
#pragma once
#include <cstdint>

namespace types { class Context; }

namespace quest {

// UPDATE_SCREENS — broadcast a viewport tile update to all nearby players.
// 3 LCALL args: x, y (world position), viewport_value (32-bit tile data).
// Void function.
void update_screens(types::Context& ctx, int32_t x, int32_t y, int32_t viewport_value);

} // namespace quest
