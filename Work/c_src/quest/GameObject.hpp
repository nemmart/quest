// src/quest/GameObject.hpp
#pragma once
#include <cstdint>

namespace quest {

// Game object type codes used in inventory slots and OWNS checks.
// Add new types lazily as encountered.
//
// Inventory namespace:
//   1-20   = Equipment
//   21-29  = Treasures
//   101-130 = Magic items
//
// World map tags are offset: treasures +380, magic +400.

enum class GameObject {
    // Equipment (1-20)
    SWORD           = 1,
    KNIFE           = 2,
    ARMOR           = 3,
    BOW             = 4,
    CLIMBING_GEAR   = 5,
    SHIELD          = 6,
    CROSS_BOW       = 7,
    CATAPULT        = 8,
    BATTERING_RAM   = 9,
    ARROWS          = 10,
    POISON_ARROWS   = 11,
    CASTLE_DEFENCES = 12,
    TERRITORY_MAP   = 13,
    BACKPACK        = 14,
    WATER_SKIN      = 15,
    FOOD_RATIONS    = 16,
    DRAGON_SLAYER   = 17,
    HOLY_SYMBOL     = 18,
    FLASK           = 19,
    AMULET          = 20,

    // Treasures (21-29)
    SILVER          = 21,
    AMETHYST        = 22,
    TOPAZ           = 23,
    GOLD            = 24,
    SAPHIRES        = 25,
    RUBIES          = 26,
    EMERALDS        = 27,
    DIAMONDS        = 28,
    MAGIC_CRYSTALS  = 29,

    // Magic items (101-130)
    CROWN           = 101,
    SCEPTER         = 102,
    GOLDEN_ORB      = 103,
    GREAT_SEAL      = 104,
    SIGNET_RING     = 105,
    MAGIC_BOOTS     = 106,
    TRANSPORTER_KEY = 107,
    MAGIC_CLOAK     = 108,
    SPY_GLASS       = 109,
    MAGIC_ROPE      = 110,
    ONE_RING        = 111,
    TELEPORT_RING   = 112,
    CRYSTAL_BALL    = 113,
    INVIS_RING      = 114,
    STAFF_OF_RA     = 115,
    THORS_HAMMER    = 116,
    CADUCEUS        = 117,
    STAFF_OF_DEATH  = 118,
    CHAPTER_1_EARTH = 119,
    CHAPTER_2_EARTH = 120,
    CHAPTER_3_EARTH = 121,
    CHAPTER_4_WIND  = 122,
    CHAPTER_5_WIND  = 123,
    CHAPTER_6_WIND  = 124,
    CHAPTER_7_FIRE  = 125,
    CHAPTER_8_FIRE  = 126,
    CHAPTER_9_FIRE  = 127,
    MASTER_VOLUME   = 128,
    MAGIC_SWORD     = 129,
    WIZARDS_SCROLL  = 130,
};

} // namespace quest
