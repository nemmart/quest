// src/quest/owns.cpp
#include "owns.hpp"
#include "../types/Context.hpp"
#include "SharedData.hpp"
#include "Player.hpp"
#include "PlayerFlag.hpp"
#include <memory>

namespace quest {

using quest::PlayerFlag;

// Object types that have dedicated ownership bit flags.
// These items have persistent gameplay effects (worn armor,
// catapult in tow, equipped rings/boots) that need to be
// checked frequently.
struct TypeFlag {
  int32_t object_type;
  PlayerFlag flag;
};

static constexpr TypeFlag TYPE_FLAGS[]={
  {  8, PlayerFlag::CATAPULT},
  {  3, PlayerFlag::ARMOR},
  {106, PlayerFlag::MAGIC_BOOTS},
  {105, PlayerFlag::SIGNET_RING},
  {112, PlayerFlag::TELEPORT_RING},
  {111, PlayerFlag::ONE_RING},
  {114, PlayerFlag::INVIS_RING},
};

int32_t owns(types::Context& ctx, int32_t player_num, int32_t object_type) {
  std::unique_ptr<quest::Player> player=ctx.shared->player(player_num);

  // Phase 1: scan 10-slot inventory
  for(int i=0; i<10; i++) {
    if(player->get_inventory(i)==object_type)
      return 0x8000;
  }

  // Phase 2: check bit flags for special items
  for(const TypeFlag& tf : TYPE_FLAGS) {
    if(object_type==tf.object_type) {
      if(player->has_player_flag(tf.flag))
        return 0xFFFF;
      else
        return 0x0000;
    }
  }

  // Unknown type — not owned
  return 0x0000;
}

} // namespace quest
