// src/quest/display_inventory.cpp
//
// DISPLAY_INVENTORY — right-side panel update with display cache.
// Translated from IR output (DISPLAY_INVENTORY.ir, 3798 lines).
//
// Screen layout (0-indexed rows, verified from IR):
//   Col 30: Row 0 = INVENTORY header, Rows 1-10 = items
//   Col 50: Row 0 = Strength, Row 1 = Wealth, ... Row 13 = Wind

#include "display_inventory.hpp"
#include "Player.hpp"
#include "PlayerFlag.hpp"
#include "DisplayCache.hpp"
#include "SharedData.hpp"
#include "GameNames.hpp"
#include "../types/Context.hpp"
#include "../types/StdString.hpp"
#include "../types/OperatingSystem.hpp"
#include "../rt/write_screen.hpp"
#include <string>

namespace quest {

// Game field offsets
static constexpr uint32_t OFF_WIND_DIRECTION = 0x1EFB8;
static constexpr uint32_t OFF_WIND_STATUS    = 0x10221C;

// Screen constants (from IR: all verified)
static constexpr int32_t OPTIONS = 0x0800;

static std::string num_str(int32_t value) { return std::to_string(value); }

static std::string pad(const std::string& s, size_t n) {
  if (s.size() >= n) return s.substr(0, n);
  return s + std::string(n - s.size(), ' ');
}

static void ws5(types::Context& ctx, int32_t ch,
                const std::string& text, int32_t row, int32_t col) {
  types::StdString s(text);
  int32_t r = row, c = col;
  rt::write_screen_5(ctx, ch, s, r, c, OPTIONS);
}

static void ws2(types::Context& ctx, int32_t ch, const std::string& text) {
  types::StdString s(text);
  rt::write_screen_2(ctx, ch, s);
}

void display_inventory(types::Context& ctx, int32_t player_num, int32_t channel) {
  auto player = ctx.shared->player(player_num);
  auto cache = player->get_display_cache();

  // ═══ INVENTORY HEADER (row 0, col 30) ═══
  if (cache.hp() == static_cast<int32_t>(0xFFFF)) {
    ws5(ctx, channel, "   INVENTORY   ", 0, 30);
  }

  // ═══ INVENTORY ITEM LOOP (col 30, rows 1+) ═══
  int32_t item_row = 0;

  for (int slot = 1; slot <= 10; slot++) {
    int32_t item_type = player->get_inventory(slot - 1);
    if (item_type == 0) continue;

    item_row++;
    cache.set_item(slot, item_type);

    const char* n = item_name(item_type);
    std::string name = n ? n : "Unknown";

    // Quest object compass: when carrying quest object (type 18) and
    // quest_level==15, compute direction arrow to quest target.
    // IR blocks: 70167574-7016765b (~42 blocks)
    if (item_type == 18 && player->get_quest_level() == 15) {
      // Read quest target object position from OBJ_PTR table
      // IR: reads [player+0x4] (quest target index), then
      //     calls DISTANCE_TO_PLAYER, computes dx/dy, determines direction
      // TODO: Full compass logic requires DISTANCE_TO_PLAYER + direction calc
      // For now, display item name without compass suffix
    }

    // Familiar (type 108): special handling with FAMILIAR_ACTIVE check
    // IR blocks: 70167542-70167572
    // When familiar is active, display name comes from creature table
    // TODO: familiar name lookup from creature table

    ws5(ctx, channel, pad(name, 19), item_row, 30);
  }

  // Clear remaining rows below items
  for (int row = item_row + 1; row <= 10; row++) {
    ws5(ctx, channel, pad("", 19), row, 30);
  }

  // ═══ ROW 0, COL 50: "Strength N [wizard]" ═══
  {
    int32_t hp = player->get_current_hp();
    if (hp != cache.hp()) {
      cache.set_hp(hp);

      std::string class_suffix;
      int32_t cls = player->get_player_class();
      if (cls >= 1 && cls <= 5) {
        class_suffix = " [" + std::string(CLASS_NAMES[cls - 1]) + "]";
      }

      ws5(ctx, channel, pad("Strength " + num_str(hp) + class_suffix, 30), 0, 50);
    }
  }

  // ═══ ROW 1, COL 50: "Wealth N" / "Thy purse is empty" ═══
  {
    int32_t wealth = player->get_wealth();
    if (wealth != cache.wealth()) {
      cache.set_wealth(wealth);
      if (wealth > 0)
        ws5(ctx, channel, pad("Wealth " + num_str(wealth), 30), 1, 50);
      else
        ws5(ctx, channel, pad("Thy purse is empty", 30), 1, 50);
    }
  }

  // ═══ ROW 2, COL 50: "Experience level N" ═══
  {
    int32_t exp = player->get_experience();
    if (exp != cache.experience()) {
      cache.set_experience(exp);
      ws5(ctx, channel, pad("Experience level " + num_str(exp), 30), 2, 50);
    }
  }

  // ═══ ROW 3, COL 50: "Intelligence level N" ═══
  {
    int32_t intel = player->get_intelligence();
    if (intel != cache.intelligence()) {
      cache.set_intelligence(intel);
      ws5(ctx, channel, pad("Intelligence level " + num_str(intel), 30), 3, 50);
    }
  }

  // ═══ ROW 4, COL 50: "Vision N, Perception N" ═══
  {
    int32_t vis = player->get_vision();
    int32_t perc = player->get_perception();
    if (vis != cache.vision() || perc != cache.perception()) {
      cache.set_vision(vis);
      cache.set_perception(perc);
      ws5(ctx, channel,
        pad("Vision " + num_str(vis) + ", Perception " + num_str(perc), 30),
        4, 50);
    }
  }

  // ═══ ROW 5, COL 50: "Quest level N" ═══
  {
    int32_t quest = player->get_quest_level();
    if (quest != cache.quest_level()) {
      cache.set_quest_level(quest);
      ws5(ctx, channel, pad("Quest level " + num_str(quest), 30), 5, 50);
    }
  }

  // ═══ ROW 6, COL 50: "Number of castles N" / "You have no castles yet" ═══
  {
    int32_t castles = player->get_castle_count();
    if (castles != cache.row6()) {
      cache.set_row6(castles);
      if (castles > 0)
        ws5(ctx, channel, pad("Number of castles " + num_str(castles), 30), 6, 50);
      else
        ws5(ctx, channel, pad("You have no castles yet", 30), 6, 50);
    }
  }

  // ═══ ROW 7, COL 50: "Dragons slain N" / "You havn't slain a dragon yet" ═══
  {
    int32_t dragons = player->get_dragons_slain();
    if (dragons != cache.castle_count()) {
      cache.set_castle_count(dragons);
      if (dragons > 0)
        ws5(ctx, channel, pad("Dragons slain " + num_str(dragons), 30), 7, 50);
      else
        ws5(ctx, channel, pad("You havn't slain a dragon yet", 30), 7, 50);
    }
  }

  // ═══ ROW 8, COL 50: Rings ═══
  {
    std::string rings = "Rings: ";
    bool any_ring = false;
    if (player->has_player_flag(PlayerFlag::SIGNET_RING))   { rings += "Ri "; any_ring = true; }
    if (player->has_player_flag(PlayerFlag::ONE_RING))      { rings += "On "; any_ring = true; }
    if (player->has_player_flag(PlayerFlag::TELEPORT_RING)) { rings += "Tr "; any_ring = true; }
    if (player->has_player_flag(PlayerFlag::INVIS_RING))    { rings += "Ir "; any_ring = true; }

    if (any_ring)
      ws5(ctx, channel, pad(rings, 30), 8, 50);
    else
      ws5(ctx, channel, pad("", 30), 8, 50);
  }

  // ═══ ROW 9, COL 50: "Arrows - N" + "Poison - N" ═══
  {
    int32_t arrows = player->get_arrows();
    int32_t poison = player->get_poison();
    if (arrows != cache.arrows() || poison != cache.poison()) {
      cache.set_arrows(arrows);
      cache.set_poison(poison);
      if (arrows > 0)
        ws5(ctx, channel, "Arrows - " + num_str(arrows), 9, 50);
      else
        ws5(ctx, channel, pad("", 15), 9, 50);

      if (poison > 0)
        ws2(ctx, channel, "  Poison - " + num_str(poison));
      else
        ws2(ctx, channel, pad("", 15));
    }
  }

  // ═══ ROW 10, COL 50: Activity status ═══
  // IR blocks: 70168081-701680ee (activity flags), 701680f4-70168146 (carrying)
  {
    std::string activity;

    if (player->has_player_flag(PlayerFlag::ACTIVITY_FLYING))
      activity = "flying on a pegasus";
    else if (player->has_player_flag(PlayerFlag::ACTIVITY_EXPLORING))
      activity = "exploring a cave";
    else if (player->has_player_flag(PlayerFlag::ACTIVITY_AT_HOME))
      activity = "at home in your castle";
    else if (player->has_player_flag(PlayerFlag::ACTIVITY_SAILING))
      activity = "sailing a boat";
    else if (player->has_player_flag(PlayerFlag::ACTIVITY_CARRYING)) {
      // IR blocks 70168107-70168117: reads status_field, divides by 10
      // to get creature type, looks up name from table at 0x70150A5A
      // (16-word entries: length + string data). Builds "carrying [name]".
      int32_t status = player->read_word(0x7F56);  // player status_field
      int32_t creature_idx = status / 10;
      // Look up creature name from table (0x70150A5A, 16-word stride)
      uint32_t entry = 0x70150A5A + static_cast<uint32_t>(creature_idx) * 16;
      int32_t name_len = ctx.shared->read_sd_word(entry);
      if (name_len > 0 && name_len <= 30) {
        // Read creature name string
        std::string creature;
        uint32_t str_addr = entry + 1;
        for (int i = 0; i < name_len; i += 2) {
          uint32_t w = ctx.shared->read_sd_word(str_addr + static_cast<uint32_t>(i / 2));
          char hi = static_cast<char>((w >> 8) & 0xFF);
          char lo = static_cast<char>(w & 0xFF);
          if (hi >= ' ' && hi < 0x7F) creature += hi;
          if (i + 1 < name_len && lo >= ' ' && lo < 0x7F) creature += lo;
        }
        activity = "carrying " + creature;
      } else {
        activity = "carrying";
      }
    }

    if (!activity.empty())
      ws5(ctx, channel, pad("You are " + activity, 30), 10, 50);
    else
      ws5(ctx, channel, pad("", 30), 10, 50);
  }

  // ═══ ROW 11, COL 50: Armor / Boots / Special items ═══
  {
    ws5(ctx, channel, pad("", 30), 11, 50);

    if (player->has_player_flag(PlayerFlag::MAGIC_BOOTS))
      ws2(ctx, channel, "Boots");

    if (player->has_player_flag(PlayerFlag::ARMOR)) {
      if (player->has_player_flag(PlayerFlag::MAGIC_BOOTS))
        ws2(ctx, channel, ", ");
      ws2(ctx, channel, "Armor");
    }

    // IR blocks 70168335-70168388: three special item variants
    // Block 70168335 checks 0xDB29, block 7016834D checks SPECIAL_ITEM
    // The three cases are mutually exclusive display outputs
    if (player->read_bit(0xDB29) && !player->has_player_flag(PlayerFlag::SPECIAL_ITEM)) {
      ws2(ctx, channel, "Dark Lord's Staff");
    } else if (player->has_player_flag(PlayerFlag::SPECIAL_ITEM)) {
      if (player->read_bit(0xDB29))
        ws2(ctx, channel, "Sceptor of Loric");
      else
        ws2(ctx, channel, "A magic shard");
    }
  }

  // ═══ ROW 12, COL 50: Catapult ═══
  {
    if (player->has_player_flag(PlayerFlag::CATAPULT))
      ws5(ctx, channel, pad("You have a catapult in tow", 30), 12, 50);
    else
      ws5(ctx, channel, pad("", 30), 12, 50);
  }

  // ═══ ROW 13, COL 50: Wind direction + status ═══
  // IR blocks: 7016841c-701684d6
  // Format: "Wind from North - Sunny" (direction 1-4, status 1-4)
  {
    static const char* WIND_STATUS[] = { "", "Sunny", "Cloudy", "Rainy", "Stormy" };

    int32_t wind = ctx.shared->read_sd_word(OFF_WIND_DIRECTION);
    int32_t status_val = ctx.shared->read_obj_word(OFF_WIND_STATUS);

    const char* dir;
    switch (wind) {
      case 1:  dir = "North"; break;
      case 2:  dir = "East";  break;
      case 3:  dir = "South"; break;
      default: dir = "West";  break;
    }
    const char* status = (status_val >= 1 && status_val <= 4)
                         ? WIND_STATUS[status_val] : "";
    std::string line = "Wind from " + std::string(dir) + " - " + status;
    ws5(ctx, channel, pad(line, 30), 13, 50);
  }
}

} // namespace quest
