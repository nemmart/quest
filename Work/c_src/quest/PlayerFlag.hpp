// src/quest/PlayerFlag.hpp
#pragma once
#include <cstdint>

namespace quest {

// Player status bit flags. Values encode bit position relative to
// the start of status_bits_0 (Player record offset 51):
//   word index = value / 16  (0 = status_bits_0, 1 = status_bits_1)
//   bit index  = value % 16  (Data General convention: 0 = MSB)
//
// Add new flags lazily as discovered.

enum class PlayerFlag {
    // status_bits_0 (Player rec 51)
    INACTIVE        = 0,   // bit 0 — skip in searches
    HAS_VIEWPORT    = 1,   // bit 1 — valid viewport data
    FAMILIAR_ACTIVE = 7,   // bit 7 — familiar clairvoyance
    ACTIVITY_SAILING   = 8,   // bit 8 — sailing a boat
    ACTIVITY_FLYING    = 9,   // bit 9 — flying on a pegasus
    ACTIVITY_AT_HOME   = 10,  // bit 10 — at home in your castle
    DRAGON_SLAIN    = 12,  // bit 12
    MAGIC_BOOTS     = 14,  // bit 14 — type 106
    CATAPULT        = 15,  // bit 15 — type 8

    // status_bits_1 (Player rec 52)
    ARMOR           = 16,  // bit 0 — type 3
    SPECIAL_ITEM    = 17,  // bit 1 — Sceptor of Loric / magic shard
    TELEPORT_RING   = 18,  // bit 2 — type 112
    INVIS_RING      = 19,  // bit 3 — type 114
    ONE_RING        = 20,  // bit 4 — type 111
    SIGNET_RING     = 21,  // bit 5 — type 105
    ACTIVITY_EXPLORING = 22,  // bit 6 — exploring a cave
    ACTIVITY_CARRYING  = 24,  // bit 8 — carrying [creature]
};

} // namespace quest
