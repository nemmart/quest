// src/quest/Player.cpp
#include "Player.hpp"
#include "DisplayCache.hpp"

namespace quest {

DisplayCache Player::get_display_cache() {
  return DisplayCache(*this);
}

} // namespace quest
