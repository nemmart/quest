// src/quest/find_object.hpp
#pragma once
#include <cstdint>

namespace types { class Context; }

namespace quest {

// FIND_OBJECT — find what's at world position (x, y).
// 3 LCALL args: x, y, &result (output, wide).
// Three-phase search: player viewport → object table → region table.
// Writes result via the reference parameter.  Void function.
void find_object(types::Context& ctx, int32_t x, int32_t y, int32_t& result);

} // namespace quest
