// src/quest/display_inventory.hpp
#pragma once
#include <cstdint>

namespace types { class Context; }

namespace quest {

// DISPLAY_INVENTORY — updates the right-side panel of the game screen.
// Called every turn. Uses display cache to only redraw changed fields.
// Args: player_num (1..10), channel (output terminal channel).
void display_inventory(types::Context& ctx, int32_t player_num, int32_t channel);

} // namespace quest
