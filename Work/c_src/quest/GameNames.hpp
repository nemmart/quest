// src/quest/GameNames.hpp
#pragma once

namespace quest {

// Equipment names (types 1-20), indexed 0..19
constexpr const char* EQUIPMENT_NAMES[] = {
    "Sword",           // 1
    "Knife",           // 2
    "Armor",           // 3
    "Bow",             // 4
    "Climbing gear",   // 5
    "Shield",          // 6
    "Cross bow",       // 7
    "Catapult",        // 8
    "Battering ram",   // 9
    "Arrows",          // 10
    "Poison arrows",   // 11
    "Castle defences", // 12
    "Territory map",   // 13
    "Backpack",        // 14
    "Water skin",      // 15
    "Food rations",    // 16
    "Dragon slayer",   // 17
    "Holy Symbol",     // 18
    "Flask",           // 19
    "Amulet",          // 20
};

// Magic item names (types 101-130), indexed 0..29
constexpr const char* MAGIC_ITEM_NAMES[] = {
    "Crown",           // 101
    "Scepter",         // 102
    "Gold orb",        // 103
    "Great seal",      // 104
    "Signet ring",     // 105
    "Magic boots",     // 106
    "Transporter key", // 107
    "Magic cloak",     // 108
    "Spy glass",       // 109
    "Magic rope",      // 110
    "The One Ring",    // 111
    "Teleport ring",   // 112
    "Crystal ball",    // 113
    "Invis. ring",     // 114
    "Staff of Ra",     // 115
    "Thor's hammer",   // 116
    "Caduceus",        // 117
    "Staff of Death",  // 118
    "Chapter 1",       // 119
    "Chapter 2",       // 120
    "Chapter 3",       // 121
    "Chapter 4",       // 122
    "Chapter 5",       // 123
    "Chapter 6",       // 124
    "Chapter 7",       // 125
    "Chapter 8",       // 126
    "Chapter 9",       // 127
    "Master Volume",   // 128
    "Magic Sword",     // 129
    "Wizards scroll",  // 130
};

// Player class names (indices 1-5), indexed 0..4
constexpr const char* CLASS_NAMES[] = {
    "wizard",          // 1
    "cleric",          // 2
    "druid",           // 3
    "fighter",         // 4
    "barbarian",       // 5
};

// Get display name for an item type code.
// Returns nullptr for unknown types.
inline const char* item_name(int type) {
    if (type >= 1 && type <= 20)
        return EQUIPMENT_NAMES[type - 1];
    if (type >= 101 && type <= 130)
        return MAGIC_ITEM_NAMES[type - 101];
    return nullptr;
}

} // namespace quest
