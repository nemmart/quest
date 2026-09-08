// src/quest/dist.cpp
#include "dist.hpp"
#include "../types/Context.hpp"
#include "../types/Location.hpp"
#include "SharedData.hpp"
#include "Player.hpp"
#include <cmath>
#include <memory>

namespace quest {

int32_t dist(types::Context& ctx, const types::Location& a, const types::Location& b) {
  double dx=static_cast<double>(a.get_x()-b.get_x());
  double dy=static_cast<double>(a.get_y()-b.get_y());
  return static_cast<int32_t>(std::sqrt(dx*dx+dy*dy));
}

int32_t distance_to_player(types::Context& ctx, const types::Location& from, int32_t player_num) {
  std::unique_ptr<quest::Player> player=ctx.shared->player(player_num);
  int32_t px=player->get_x();
  int32_t py=player->get_y();
  double dx=static_cast<double>(from.get_x()-px);
  double dy=static_cast<double>(from.get_y()-py);
  return static_cast<int32_t>(std::sqrt(dx*dx+dy*dy));
}

} // namespace quest
