// shared_data_layout.h — Reconstructed Quest SHARED_DATA_FILE layout
//
// Naming convention:
//   named fields   — identified purpose
//   unknown_NNN    — accessed by game code (static scan or runtime), purpose TBD
//   reserved_NNN   — NO accesses found in static scan OR runtime diagnostic
//
// Verified: disassembly scan (QUEST + QUEST_SERVER), runtime Memory
// diagnostic (catches WCMV block copies, pointer-based access, etc.)
//
// Header: 44 words.  Player stride: 686 words.
// PL/I signed displacement → record offset: signed + 642

#pragma once
#include <cstdint>

// Coordinate pair — x immediately followed by y.
// Functions like DIST, FIND_OBJECT, UPDATE_SCREENS receive
// pointers to Location fields (passing &loc.x and &loc.y
// as separate PL/I arguments that are adjacent in memory).
struct __attribute__((packed)) Location {
    int16_t x;
    int16_t y;
};

// Per-catapult data (stride 4, 10 slots per player)
struct __attribute__((packed)) CatapultSlot {
    Location pos;                  // world position
    int16_t hp;                    // strength remaining (0 = destroyed)
    int16_t ammo;                  // shots remaining
};

struct __attribute__((packed)) Player {  // 686 words = 1372 bytes

    // === IDENTITY (offsets 0..50) ===
    int16_t unknown_00[13];        //   0..12   accessed at offsets 0, 12
    int16_t player_id;             //  13       process/session ID (40 hits)
    int16_t unknown_14[3];         //  14..16   accessed
    int16_t name_length;           //  17       VARYING(32) length
    int16_t name_data[16];         //  18..33   "Yankee Foxtrot" + spaces
    int16_t password_length;       //  34       VARYING(32) length
    int16_t password_data[16];     //  35..50   "My Password" + spaces

    // === STATUS BIT FLAGS (offsets 51..52) ===
    int16_t status_bits_0;         //  51       INACTIVE, DRAGON_SLAIN, boots, catapult
    int16_t status_bits_1;         //  52       ARMOR, rings, special items

    // === POSITION (offsets 53..54) ===
    Location pos;                  //  53..54   world coordinates

    // === VIEWPORT TILES (offsets 55..252) ===
    int16_t viewport_tiles[198];   //  55..252  99 int32_t tiles

    // === INVENTORY (offsets 253..262) ===
    int16_t inventory[10];         // 253..262  type codes

    // === CORE STATS (offsets 263..270) ===
    int16_t intelligence;          // 263  "Intelligence level N"
    int16_t experience;            // 264  "Experience level N"
    int16_t current_hp;            // 265  "Strength N" (HP)
    int16_t max_hp;                // 266  cap for Healing Aura
    int16_t vision;                // 267  "Vision N"
    int16_t perception;            // 268  "Perception N"
    int16_t unknown_269;           // 269  spell related
    int16_t wealth;                // 270  "Wealth N"

    // === CASTLE COUNT + SPELL TIMERS (offsets 271..309) ===
    int16_t unknown_271;           // 271
    int16_t castle_count;          // 272  "Number of castles N"
    int16_t spell_timers[37];      // 273..309

    // === SERVER-COMPUTED BLOCK (offsets 310..372) ===
    // QUEST_SERVER writes, QUEST reads (via WCMV block copy).
    // Static scan missed — no individual field instructions.
    int16_t unknown_310[63];       // 310..372

    // === SIEGE FIELDS (offsets 373..377) ===
    int16_t siege_field_0;         // 373
    int16_t siege_field_1;         // 374
    int16_t siege_field_2;         // 375
    int16_t siege_field_3;         // 376
    int16_t status_field;          // 377

    // === (offsets 378..385) ===
    int16_t unknown_378[8];        // 378..385

    // === CATAPULTS (offsets 386..425) ===
    struct CatapultSlot catapults[10];  // 386..425

    // === STRIDE-4 ARRAY (offsets 426..469) ===
    // Runtime: QUEST reads field_0 of 11 entries at stride 4
    // (426, 430, 434, ... 466).  Similar structure to catapults.
    int16_t unknown_426[44];       // 426..469

    // === (offsets 470..472) ===
    int16_t unknown_470;           // 470  2 hits
    int16_t unknown_471;           // 471  7 hits
    int16_t siege_castle_index;    // 472  0=none, 1-999=attacking, 10001+=defending

    // === CLIENT ARRAY (offsets 473..483) ===
    // Runtime: QUEST reads/writes 473..482, QS reads/writes 483.
    int16_t unknown_473[10];       // 473..482
    int16_t unknown_483;           // 483  server-side only

    // === (offsets 484..544) ===
    int16_t reserved_484[61];      // 484..544  NO runtime hits

    // === SERVER STATE (offsets 545..551) ===
    // Runtime: QUEST_SERVER writes all 7, QUEST reads 545.
    int16_t unknown_545[7];        // 545..551

    // === QUEST / SPELL STATUS (offsets 552..554) ===
    int16_t spell_status;          // 552
    int16_t quest_or_vision;       // 553
    int16_t castle_or_cache;       // 554

    // === DISPLAY CACHE EXTENSION (offsets 555..564) ===
    // Runtime: QUEST reads/writes all 10 (via WCMV).
    int16_t unknown_555[10];       // 555..564

    // === DISPLAY CACHE (offsets 565..580) ===
    int16_t cache_intelligence;    // 565
    int16_t cache_experience;      // 566
    int16_t cache_hp;              // 567
    int16_t unknown_568;           // 568
    int16_t cache_vision;          // 569
    int16_t cache_perception;      // 570
    int16_t cache_wealth;          // 571
    int16_t cache_castle_count;    // 572
    int16_t cache_arrows;          // 573
    int16_t cache_poison;          // 574
    int16_t cache_row5;            // 575
    int16_t cache_row6;            // 576
    int16_t cache_row13a;          // 577
    int16_t cache_row13b;          // 578
    int16_t cache_display_bits;    // 579
    int16_t cache_status;          // 580

    // === (offsets 581..646) ===
    int16_t reserved_581[3];       // 581..583
    int16_t unknown_584;           // 584  1 hit
    int16_t reserved_585[5];       // 585..589
    int16_t unknown_590;           // 590  3 hits
    int16_t reserved_591;          // 591
    int16_t unknown_592;           // 592  3 hits
    int16_t reserved_593;          // 593
    int16_t unknown_594;           // 594  4 hits
    int16_t reserved_595[3];       // 595..597
    int16_t unknown_598;           // 598  1 hit
    int16_t reserved_599;          // 599
    int16_t unknown_600;           // 600  2 hits
    int16_t unknown_601;           // 601  1 hit
    int16_t reserved_602[45];      // 602..646

    // === BACKPACK (offsets 647..656) ===
    int16_t backpack[10];          // 647..656

    // === FOOD + WATER (offsets 657..659) ===
    int16_t food_supply;           // 657  raw / 57 ≈ days remaining
    int16_t unknown_658;           // 658
    int16_t water_supply;          // 659  raw * 100 / 15 = fill %

    // === SERVER-WRITTEN FIELDS (offsets 660..663) ===
    // Runtime: QUEST_SERVER writes, QUEST reads.
    int16_t unknown_660[4];        // 660..663

    // === (offsets 664..680) ===
    int16_t reserved_664[17];      // 664..680  NO runtime hits

    // === TAIL FIELDS (offsets 681..685) ===
    // Runtime: scattered server writes, client reads.
    int16_t unknown_681;           // 681  server writes, client reads
    int16_t unknown_682;           // 682  server writes
    int16_t unknown_683;           // 683  client writes
    int16_t player_class;          // 684  1=wizard..5=barbarian
    int16_t unknown_685;           // 685  server writes
};

// Record stats: 369 named, 180 unknown, 137 reserved (686 total)

struct __attribute__((packed)) SharedDataFile {

    // === HEADER (44 words) ===
    int16_t unknown_header_00[40]; //   0..39
    int32_t random_seed;           //  40..41  RNG state (wide)
    int16_t unknown_header_42;     //  42      (value 100 in hex dump)
    int16_t num_players;           //  43

    // === PLAYERS ===
    struct Player players[10];     // [0..9] → PL/I players(1..10)

    // === STRIDE-6 TABLE (20000 entries) ===
    // Starts at word 6904 (immediately after players).
    // Factored base 0x1AF2, PL/I 1-indexed.
    // Fields per entry: +0 (main), +1, +2, +4 (bit flags)
    // Not declared here — accessed via raw OBJ_PTR arithmetic.
};
