// src/quest/dist.hpp
#pragma once
#include <cstdint>

namespace types { class Context; class Location; }

namespace quest {

// DIST — Euclidean distance between two points.
// PL/I: 4 args (x1, y1, x2, y2) passed as separate pointers.
// C++: two Location references.
int32_t dist(types::Context& ctx, const types::Location& a, const types::Location& b);

// DISTANCE_TO_PLAYER — distance from a point to a player's position.
// PL/I: 3 args (x, y, player_num).
// C++: Location for the point, player_num as value.
int32_t distance_to_player(types::Context& ctx, const types::Location& from, int32_t player_num);

} // namespace quest
