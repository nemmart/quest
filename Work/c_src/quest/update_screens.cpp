// src/quest/update_screens.cpp
#include "update_screens.hpp"
#include "../types/Context.hpp"
#include "SharedData.hpp"
#include "Player.hpp"
#include <cstdlib>
#include <memory>

namespace quest {

// Viewport grid: column-major, 9 columns × 11 rows of 32-bit cells.
// Base at signed word offset 0x7D9D (-611) from the player record.
// Column stride: 22 words (11 rows × 2 words per cell).
static constexpr uint32_t VIEWPORT_BASE=0x7D9D;
static constexpr int32_t COL_STRIDE=22;

void update_screens(types::Context& ctx, int32_t x, int32_t y, int32_t viewport_value) {
  int32_t num_players=ctx.shared->num_players();
  if(num_players<1) return;

  for(int32_t i=1; i<=num_players; i++) {
    std::unique_ptr<quest::Player> player=ctx.shared->player(i);

    // Check horizontal distance
    int32_t px=player->get_x();
    int32_t dx=x-px;
    if(dx<0) dx=-dx;
    if(dx>4) continue;

    // Check vertical distance
    int32_t py=player->get_y();
    int32_t dy=y-py;
    if(dy<0) dy=-dy;
    if(dy>5) continue;

    // Player i can see (x, y). Compute viewport cell indices.
    int32_t col=x-px+5;   // range 1..9
    int32_t row=y-py+6;   // range 1..11

    // Write the tile data to the player's viewport grid
    uint32_t vp_offset=VIEWPORT_BASE+static_cast<uint32_t>(col*COL_STRIDE+row*2);
    player->write_wide(vp_offset, viewport_value);
  }
}

} // namespace quest
