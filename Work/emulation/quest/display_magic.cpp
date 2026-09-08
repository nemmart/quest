// src/quest/display_magic.cpp
//
// DISPLAY_MAGIC — Display spell status screen.
// Translated from IR (40 blocks, 364 lines).
//
// Shows each known spell with status:
//   value > 0:   "Effective for N more turn(s)"
//   value == -1:  "Ready for use"
//   value < -1:   "Regenerating for N more turn(s)"
//   value == 0:   (no spell, skip)

#include "display_magic.hpp"
#include "Player.hpp"
#include "SharedData.hpp"
#include "../types/Context.hpp"
#include "../types/StdString.hpp"
#include "../types/OperatingSystem.hpp"
#include "../rt/write_screen.hpp"
#include "../rt/read_screen.hpp"
#include <string>

namespace quest {

// Spell offset base within player data (from IR: [ac2+0x7E8E])
static constexpr uint32_t SPELL_BASE = 0x7E8E;

// Maximum spell index (from IR assert: WSGTI 0,37)
static constexpr int MAX_SPELLS = 37;

// Items per page before HIT_ANY_CHAR (from IR: WSEQI ac1,20)
static constexpr int PAGE_SIZE = 20;

static constexpr int32_t OPTIONS = 0x0800;

static const char* SPELL_NAMES[] = {
  "",                          //  0 (unused)
  "Lightning bolt",            //  1
  "Magic shield",              //  2
  "Create smoke screen",       //  3
  "Fahrenheit foiler",         //  4
  "Obtain knowledge",          //  5
  "Dumbfound attacker",        //  6
  "Activate cloak",            //  7
  "Healing aura",              //  8
  "Camouflage",                //  9
  "Invisiblity",               // 10 (sic — original spelling)
  "Wield Thor's hammer",       // 11
  "Teleport",                  // 12
  "Gaze into Crystal Ball",    // 13
  "Field of protection",       // 14
  "Eye of Argus",              // 15
  "Activate caduceus",         // 16
  "Strength of Atlas",         // 17
  "Wield the staff of Death",  // 18
  "Create a boat",             // 19
  "Summon pegasus",            // 20
  "Call healer",               // 21
  "Cause fear",                // 22
  "Clear vision",              // 23
  "Panic",                     // 24
  "Weaken opponent",           // 25
  "Conjour storm",             // 26
  "Summon sun",                // 27
  "Dispatch familiar",         // 28
  "Recall familiar",           // 29
  "Familiar clairvoyance",     // 30
  "Charm familiar",            // 31
  "Create food",               // 32
  "Create water",              // 33
  "Fire ball",                 // 34
  "Run like a horse",          // 35
  "Polymorph",                 // 36
  "Create fog",                // 37
};

static std::string pad(const std::string& s, size_t n) {
  if (s.size() >= n) return s.substr(0, n);
  return s + std::string(n - s.size(), ' ');
}

static void ws2(types::Context& ctx, int32_t ch, const std::string& text) {
  types::StdString s(text);
  rt::write_screen_2(ctx, ch, s);
}

static void hit_any_char(types::Context& ctx, int32_t channel) {
  // IR: writes "Hit any character to continue", calls GET_INPUT, writes newline
  ws2(ctx, channel, "Hit any character to continue");
  types::StdString input("");
  rt::read_screen_3(ctx, channel, input, 1);
  ws2(ctx, channel, "\r\n");
}

void display_magic(types::Context& ctx, int32_t channel) {
  int32_t player_num = ctx.shared->read_sd_word(0x216);  // PLAYER_NUM
  auto player = ctx.shared->player(player_num);

  // ═══ HEADER ═══
  // IR: 92-char string "SPELL ... | STATUS"
  ws2(ctx, channel,
    "SPELL                         | STATUS                        "
    "                            ");

  // ═══ SPELL LOOP ═══
  // IR: loop s[0x2]=1..100, XNDO limit=100
  int items_displayed = 0;

  for (int slot = 1; slot <= MAX_SPELLS; slot++) {
    // Read spell value from player + slot + SPELL_BASE
    int16_t value = static_cast<int16_t>(
      player->read_word(SPELL_BASE + static_cast<uint32_t>(slot)));

    if (value == 0) continue;

    items_displayed++;

    // Build status text
    std::string status;
    if (value > 0) {
      // IR: "Effective for N more turn(s)"
      status = "Effective for " + std::to_string(value) + " more turn(s)";
    } else if (value == -1) {
      // IR: "Ready for use"
      status = "Ready for use";
    } else {
      // IR: negate (value+1), "Regenerating for N more turn(s)"
      int turns = -(value + 1);
      status = "Regenerating for " + std::to_string(turns) + " more turn(s)";
    }

    // Build display line: spell_name (30 chars) + "| " + status (padded)
    std::string name = (slot >= 1 && slot <= MAX_SPELLS) ? SPELL_NAMES[slot] : "?";
    std::string line = pad(name, 30) + "| " + pad(status, 40);

    ws2(ctx, channel, line);

    // Page break every 20 items
    if (items_displayed == PAGE_SIZE) {
      hit_any_char(ctx, channel);
      // IR: writes 0x0C (form feed / clear screen)
      ws2(ctx, channel, "\014");
      items_displayed = 0;
    }
  }

  // IR: at end, if items_displayed != 20, call HIT_ANY_CHAR
  if (items_displayed != PAGE_SIZE) {
    hit_any_char(ctx, channel);
  }
}

} // namespace quest
