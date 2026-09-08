// src/quest/owns.hpp
#pragma once
#include <cstdint>

namespace types { class Context; }

namespace quest {

// OWNS — check if a player owns an object.
// 2 LCALL args: player_num (1..10), object_type.
//
// Type namespace:
//   1-20   = equipment (Sword, Knife, Armor, ... Amulet)
//   21-29  = treasures (silver, amethyst, ... magic crystals)
//   101-118 = magic items (Crown, Scepter, ... Staff of Death)
//
// Phase 1: Scans 10-slot inventory for an exact type match.
// Phase 2: For 7 specific types that have persistent gameplay
//   effects (worn armor, catapult in tow, equipped rings/boots),
//   checks a dedicated per-player bit flag.
//
// Returns 0x8000 (inventory match) or 0xFFFF (bit flag set)
// if owned, 0x0000 if not owned.
int32_t owns(types::Context& ctx, int32_t player_num, int32_t object_type);

} // namespace quest
