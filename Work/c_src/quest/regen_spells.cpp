// src/quest/regen_spells.cpp
//
// REGEN_SPELLS — Per-turn spell timer management.
// Translated from IR (107 blocks, 708 lines).
//
// Called each turn. Two phases:
//   Phase 1: Regenerating spells (value <= -2) tick toward ready (-1).
//            If Healing Aura active, this phase runs twice (double regen).
//   Phase 2: Active spells (value > 0) decrement by 1.
//            When a spell expires (reaches 0), side effects fire and
//            the spell enters regen (value = regen_table[spell]).

#include "regen_spells.hpp"
#include "Player.hpp"
#include "SharedData.hpp"
#include "Spell.hpp"
#include "../types/Context.hpp"

namespace quest {

static constexpr uint32_t SPELL_BASE  = 0x7E8E;
static constexpr int      MAX_SPELLS  = 100;  // loop limit from IR
static constexpr uint32_t PLAYER_NUM_ADDR = 0x216;

// Spell regen times (from table at 0x70150765, indexed by spell number)
static const int16_t REGEN_TABLE[] = {
  0,     //  0 unused
  -15,   //  1 Lightning bolt
  -20,   //  2 Magic shield
  -17,   //  3 Create smoke screen
  -8,    //  4 Fahrenheit foiler
  -4,    //  5 Obtain knowledge
  -21,   //  6 Dumbfound attacker
  -8,    //  7 Activate cloak
  -10,   //  8 Healing aura
  -30,   //  9 Camouflage
  -20,   // 10 Invisibility
  -2,    // 11 Wield Thor's hammer
  -42,   // 12 Teleport
  -20,   // 13 Gaze into Crystal Ball
  -23,   // 14 Field of protection
  -31,   // 15 Eye of Argus
  -45,   // 16 Activate caduceus
  -65,   // 17 Strength of Atlas
  -65,   // 18 Wield the staff of Death
  -85,   // 19 Create a boat
  -95,   // 20 Summon pegasus
  0,     // 21 Call healer (one-shot)
  0,     // 22 Cause fear (one-shot)
  0,     // 23 Clear vision (one-shot)
  0,     // 24 Panic (one-shot)
  0,     // 25 Weaken opponent (one-shot)
  0,     // 26 Conjour storm (one-shot)
  0,     // 27 Summon sun (one-shot)
  -40,   // 28 Dispatch familiar
  -40,   // 29 Recall familiar
  -20,   // 30 Familiar clairvoyance
  -40,   // 31 Charm familiar
  -22,   // 32 Create food
  -66,   // 33 Create water
  -5,    // 34 Fire ball
  -35,   // 35 Run like a horse
  -43,   // 36 Polymorph
  -300,  // 37 Create fog
};
static constexpr int REGEN_TABLE_SIZE = sizeof(REGEN_TABLE) / sizeof(REGEN_TABLE[0]);

// Bit flag addresses (from IR: player_num * 686 * 16 + offset)
static constexpr int32_t BIT_FAMILIAR_ACTIVE = -9458;  // 0xDB12 (used by spell 7)
static constexpr int32_t BIT_HEALING_AURA    = -9450;  // 0xDB16 (spell 8 active flag)
static constexpr int32_t BIT_INVISIBILITY    = -9448;  // bit:DRAGON_SLAIN area (spell 10)
static constexpr int32_t BIT_PEGASUS_FOLLOW          = -9433;  // 0xDB27 (spell 20, pegasus follow)

// Player field for movement speed bonus (spell 35)
static constexpr uint32_t OFF_SPEED_BONUS = 0x7FA6;

// Player field for saved perception (spell 15 restore)
static constexpr uint32_t OFF_UNKNOWN_269 = 0x7E99;  // player.unknown_269 in IR

void regen_spells(types::Context& ctx) {
  int32_t player_num = ctx.shared->read_sd_word(PLAYER_NUM_ADDR);
  auto player = ctx.shared->player(player_num);

  // ═══ PHASE 1: Tick regenerating spells toward ready ═══
  // IR: check Healing Aura at offset SPELL_BASE + 8
  int16_t healing_aura_val = static_cast<int16_t>(
    player->read_word(SPELL_BASE + static_cast<uint32_t>(Spell::HEALING_AURA)));
  int regen_passes = (healing_aura_val > 0) ? 2 : 1;

  for (int pass = 0; pass < regen_passes; pass++) {
    for (int slot = 1; slot <= MAX_SPELLS; slot++) {
      int16_t value = static_cast<int16_t>(
        player->read_word(SPELL_BASE + static_cast<uint32_t>(slot)));
      // Only tick spells that are regenerating (value <= -2)
      // value == -1 means ready, value == 0 means not known
      if (value <= -2) {
        player->write_word(SPELL_BASE + static_cast<uint32_t>(slot),
                          static_cast<uint32_t>(static_cast<uint16_t>(value + 1)));
      }
    }
  }

  // ═══ PHASE 2: Decrement active spells, handle expiration ═══
  for (int slot = 1; slot <= MAX_SPELLS; slot++) {
    int16_t value = static_cast<int16_t>(
      player->read_word(SPELL_BASE + static_cast<uint32_t>(slot)));

    if (value == 0) continue;  // Not known

    // Decrement
    value--;
    player->write_word(SPELL_BASE + static_cast<uint32_t>(slot),
                      static_cast<uint32_t>(static_cast<uint16_t>(value)));

    if (value != 0) {
      // Spell still active or regenerating — check spell 14 special case
      if (slot == static_cast<int>(Spell::FIELD_OF_PROTECTION)) {
        // Field of protection: scan castle grid and update protection bits
        // IR: nested loop over 9 regions × 11 entries at offset 0x7D9D, stride 22
        // Sets or clears bit 0xDB12 based on castle ownership
        // This is complex — leave to emulated code for now
      }
      continue;
    }

    // ═══ Spell expired (value reached 0) — handle side effects ═══

    // Activate cloak — clear familiar active
    if (slot == static_cast<int>(Spell::ACTIVATE_CLOAK)) {
      player->write_bit(BIT_FAMILIAR_ACTIVE, 0);
    }

    // Healing aura — clear active flag, heal +1 HP
    if (slot == static_cast<int>(Spell::HEALING_AURA)) {
      bool aura_bit = player->read_bit(BIT_HEALING_AURA);
      if (!aura_bit) {
        // Heal player by 1, capped at max_hp
        // IR: current_hp = min(current_hp + 1, max_hp)
        int16_t hp = static_cast<int16_t>(player->get_current_hp());
        hp++;
        int16_t max_hp = static_cast<int16_t>(player->read_word(0x7E88 + 686));
        // TODO: max_hp offset uncertain, may need verification
        if (hp > max_hp) hp = max_hp;
        player->write_word(0x7E88, static_cast<uint32_t>(static_cast<uint16_t>(hp)));
      }
      player->write_bit(BIT_HEALING_AURA, 0);
    }

    // Invisibility — clear invisibility flag
    if (slot == static_cast<int>(Spell::INVISIBILITY)) {
      player->write_bit(BIT_INVISIBILITY, 0);
    }

    // Eye of Argus — restore perception from saved value
    if (slot == static_cast<int>(Spell::EYE_OF_ARGUS)) {
      int16_t saved = static_cast<int16_t>(player->read_word(OFF_UNKNOWN_269));
      player->write_word(0x7E8D, static_cast<uint32_t>(static_cast<uint16_t>(saved)));
    }

    // Summon pegasus — pegasus stops following
    if (slot == static_cast<int>(Spell::SUMMON_PEGASUS)) {
      player->write_bit(BIT_PEGASUS_FOLLOW, 0);
    }

    // Run like a horse — remove speed bonus
    if (slot == static_cast<int>(Spell::RUN_LIKE_A_HORSE)) {
      int16_t speed = static_cast<int16_t>(player->read_word(OFF_SPEED_BONUS));
      speed -= 2;
      player->write_word(OFF_SPEED_BONUS,
                        static_cast<uint32_t>(static_cast<uint16_t>(speed)));
    }

    // Set spell to regen timer
    int16_t regen = (slot < REGEN_TABLE_SIZE) ? REGEN_TABLE[slot] : 0;
    if (regen != 0) {
      player->write_word(SPELL_BASE + static_cast<uint32_t>(slot),
                        static_cast<uint32_t>(static_cast<uint16_t>(regen)));
    }
    // If regen == 0 (one-shot spells): value stays 0 = not known anymore

    // Field of protection — scan castle grid
    if (slot == static_cast<int>(Spell::FIELD_OF_PROTECTION)) {
      // IR: nested loop: for region 1..9, for entry 1..11
      //   Read [player_base * 22 * region + entry * 2 + SD_PTR + 0x7D9D]
      //   If value > some_player && value <= 10: clear bit 0xDB12 for that player
      // This interacts with castle ownership — complex, leave to emulated code
    }
  }
}

} // namespace quest
