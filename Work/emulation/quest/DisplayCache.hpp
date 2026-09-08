// src/quest/DisplayCache.hpp
//
// Display cache for DISPLAY_INVENTORY — 27 contiguous words at 0x7FA8-0x7FC2
// within the player-strided shared data.
//
// Each field tracks the last-displayed value of a player stat.
// DISPLAY_INVENTORY compares current vs cached, only redraws on change.

#pragma once
#include <cstdint>
#include "Player.hpp"

namespace quest {

class DisplayCache {
public:
  explicit DisplayCache(Player& p) : player_(p) {}

  // ── Inventory items (10 slots) ──
  int32_t item(int slot) const       { return player_.read_word(BASE + static_cast<uint32_t>(slot)); }
  void set_item(int slot, int32_t v) { player_.write_word(BASE + static_cast<uint32_t>(slot), v); }

  // ── Stat fields ──
  int32_t intelligence() const       { return player_.read_word(0x7FB3); }
  void set_intelligence(int32_t v)   { player_.write_word(0x7FB3, v); }

  int32_t experience() const         { return player_.read_word(0x7FB4); }
  void set_experience(int32_t v)     { player_.write_word(0x7FB4, v); }

  int32_t hp() const                 { return player_.read_word(0x7FB5); }
  void set_hp(int32_t v)             { player_.write_word(0x7FB5, v); }

  int32_t vision() const             { return player_.read_word(0x7FB7); }
  void set_vision(int32_t v)         { player_.write_word(0x7FB7, v); }

  int32_t perception() const         { return player_.read_word(0x7FB8); }
  void set_perception(int32_t v)     { player_.write_word(0x7FB8, v); }

  int32_t wealth() const             { return player_.read_word(0x7FB9); }
  void set_wealth(int32_t v)         { player_.write_word(0x7FB9, v); }

  int32_t castle_count() const       { return player_.read_word(0x7FBA); }
  void set_castle_count(int32_t v)   { player_.write_word(0x7FBA, v); }

  int32_t arrows() const             { return player_.read_word(0x7FBB); }
  void set_arrows(int32_t v)         { player_.write_word(0x7FBB, v); }

  int32_t poison() const             { return player_.read_word(0x7FBC); }
  void set_poison(int32_t v)         { player_.write_word(0x7FBC, v); }

  int32_t quest_level() const        { return player_.read_word(0x7FBD); }
  void set_quest_level(int32_t v)    { player_.write_word(0x7FBD, v); }

  int32_t row6() const               { return player_.read_word(0x7FBE); }
  void set_row6(int32_t v)           { player_.write_word(0x7FBE, v); }

  int32_t wind() const               { return player_.read_word(0x7FBF); }
  void set_wind(int32_t v)           { player_.write_word(0x7FBF, v); }

  int32_t status() const             { return player_.read_word(0x7FC0); }
  void set_status(int32_t v)         { player_.write_word(0x7FC0, v); }

  int32_t display_bits() const       { return player_.read_word(0x7FC2); }
  void set_display_bits(int32_t v)   { player_.write_word(0x7FC2, v); }

  // ── Convenience: check-and-update, returns true if value changed ──
  bool changed(int32_t& cached, int32_t current) {
    if (cached == current) return false;
    cached = current;
    return true;
  }

private:
  static constexpr uint32_t BASE = 0x7FA8;
  Player& player_;
};

} // namespace quest
