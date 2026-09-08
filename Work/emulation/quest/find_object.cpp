// src/quest/find_object.cpp
#include "find_object.hpp"
#include "../types/Context.hpp"
#include "SharedData.hpp"
#include "Player.hpp"
#include <cstdlib>
#include <memory>

namespace quest {

// Object table (OBJ_PTR region, stride 9 per object)
static constexpr uint32_t OBJ_X_OFFSET=0x2CE7;
static constexpr uint32_t OBJ_Y_OFFSET=0x2CE8;
static constexpr uint32_t OBJ_COUNT_OFFSET=0x2CEE;
static constexpr int32_t OBJ_STRIDE=9;

// Region table (OBJ_PTR region, stride 22 per region)
static constexpr uint32_t REGION_TILE_OFFSET=0xFCC16;
static constexpr uint32_t REGION_XMIN_OFFSET=0xFCC28;
static constexpr uint32_t REGION_YMIN_OFFSET=0xFCC29;
static constexpr uint32_t REGION_XMAX_OFFSET=0xFCC2A;
static constexpr uint32_t REGION_YMAX_OFFSET=0xFCC2B;
static constexpr int32_t REGION_STRIDE=22;

// Viewport layout (same as UPDATE_SCREENS)
static constexpr uint32_t VIEWPORT_BASE=0x7D9D;
static constexpr int32_t VP_COL_STRIDE=22;

// Player status bits (per-player, word 95 of player record)
static constexpr uint32_t BIT_PLAYER_INACTIVE=0xDB10;       // 95.0: SET = skip in searches
static constexpr uint32_t BIT_PLAYER_HAS_VIEWPORT=0xDB11;   // 95.1: SET = valid viewport data

void find_object(types::Context& ctx, int32_t x, int32_t y, int32_t& result) {
  // Phase 1: Player viewport lookup
  int32_t num_players=ctx.shared->num_players();
  for(int32_t i=1; i<=num_players; i++) {
    std::unique_ptr<quest::Player> player=ctx.shared->player(i);

    if(player->read_bit(BIT_PLAYER_INACTIVE)) continue;
    if(!player->read_bit(BIT_PLAYER_HAS_VIEWPORT)) continue;

    int32_t px=player->get_x();
    int32_t py=player->get_y();
    int32_t dx=x-px;
    if(dx<0) dx=-dx;
    if(dx>4) continue;
    int32_t dy=y-py;
    if(dy<0) dy=-dy;
    if(dy>5) continue;

    int32_t col=x-px+5;
    int32_t row=y-py+6;
    uint32_t vp_offset=VIEWPORT_BASE+static_cast<uint32_t>(col*VP_COL_STRIDE+row*2);
    result=player->read_wide(vp_offset);
    return;
  }

  // Phase 2: Object table search
  int32_t obj_count=ctx.shared->read_obj_wide(OBJ_COUNT_OFFSET);
  for(int32_t j=1; j<=obj_count; j++) {
    uint32_t base=static_cast<uint32_t>(j)*OBJ_STRIDE;
    int32_t obj_x=ctx.shared->read_obj_word(base+OBJ_X_OFFSET);
    if(obj_x!=x) continue;
    int32_t obj_y=ctx.shared->read_obj_word(base+OBJ_Y_OFFSET);
    if(obj_y!=y) continue;
    result=j;
    return;
  }

  // Phase 3: Region bounding box search
  int32_t region_count=ctx.shared->read_obj_word(REGION_YMAX_OFFSET);
  for(int32_t k=region_count; k>=1; k--) {
    uint32_t base=static_cast<uint32_t>(k)*REGION_STRIDE;
    int32_t x_min=ctx.shared->read_obj_word(base+REGION_XMIN_OFFSET);
    if(x<x_min) continue;
    int32_t x_max=ctx.shared->read_obj_word(base+REGION_XMAX_OFFSET);
    if(x>x_max) continue;
    int32_t y_min=ctx.shared->read_obj_word(base+REGION_YMIN_OFFSET);
    if(y<y_min) continue;
    int32_t y_max=ctx.shared->read_obj_word(base+REGION_YMAX_OFFSET);
    if(y>y_max) continue;
    result=ctx.shared->read_obj_wide(base+REGION_TILE_OFFSET);
    return;
  }

  result=0;
}

} // namespace quest
