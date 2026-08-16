# Quest Game Reference

Detailed reference for the Quest game, compiled from in-game help screens
and live gameplay observation.  Use alongside the disassembly for
interpreting data structures, offsets, and function behavior.

---

## Display Layout

The terminal (DG Dasher D215) shows a fixed layout:

```
  ______________________      INVENTORY           Strength NN [class]
 |                      |     (item list)         Thy purse is ...
 |                      |                         Experience level N
 |                      |                         Intelligence level N
 |    VIEWPORT          |                         Vision N, Perception N
 |       10 rows        |                         Quest level N
 |       22 cols (11x2) |                         Castle info
 |                      |                         Dragon info
 |                      |                         Rings:
 |                      |
  ----------------------
    Territory name
    City name
                                                  Weather / Wind -> direction
```

### Viewport

- 10 rows × ~22 columns (11 cells of 2 characters each)
- The player's 2-character initials are centered in the viewport
- The map scrolls as the player moves; a new row of terrain appears
  at the leading edge
- The D215 terminal has display modes: dim, underline, blink, bold
  (these render terrain with visual distinction but don't survive
  copy/paste)

### Cell Encoding

Every map cell is a **2-character pair**.  In the data tables, terrain
tiles are stored as 4 bytes: `[mode_start, char1, char2, mode_end]`
where mode bytes are D215 control codes:

| Byte Pair | Mode |
|-----------|------|
| 0E / 0F | Dim |
| 14 / 15 | Underline |
| 1C / 1D | Blink |
| 00 / 00 | Normal (no mode) |
| 20 / 20 | Space padding |

---

## Terrain Types

From in-game help:

| Symbol | Terrain | Notes |
|--------|---------|-------|
| `  ` | Level ground | Two spaces |
| `ff` | Forest | |
| `ww` | Shallow water | Drinkable source |
| `WW` | Deep water | |
| `mm` | Foothill | |
| `MM` | Mountain | |
| `x ` | Road | Single char + space |
| `++` | Ford | |
| `!!` | Cliff | |
| `""` | Swamp | |
| `()` | Cave | Enters cave mode |
| `. ` | Desert | Dot + space |
| `# ` | Catapult | Purchasable siege weapon |
| `%%` | Boat | |
| `[]` | Transporter | |
| `II` | Magic barrier | |
| `~~` | Magic fog | |

### Special Map Symbols

| Symbol | Meaning |
|--------|---------|
| `~~` | Shards of the Crystal of Ablion |
| `()` | Cave of the White Dragon |
| `t ` | The Dark Lord's staff |

### Castle Segments

Castles are drawn on the map using 2-character wall segments:

```
 /-  --  -\
 |          |
 \-  --  -/
```

Specific wall pieces: `/-`, `--`, `-\`, `\-`, `-/`, `|-`, `-|`,
`| `, ` |`, `|S` (store entrance?)

---

## Beings (NPCs/Monsters)

50 beings stored in parallel arrays starting at `BEING_NAMES`
(0x70000272).  Each being has: name, strength, moves, hit rate,
pursue range, pursue percent, surprise percent, spawn percent.

### Legendary Beings (Single-Letter Symbols)

These are unique named characters, likely quest targets:

| Symbol | Name | Realm |
|--------|------|-------|
| A | Giant | Albion |
| T | Wizard | Terrak |
| L | Lord | Loric |
| B | Banisher | Bristle |
| M | Mind Flayer | Maldork |
| C | Healer | Carrone |
| S | White Dragon | Sundar |

### White Dragon Variants

| Symbol | Name |
|--------|------|
| Wd | White Dragon Clone (×2) |
| Wd | White Dragon Mutant (×4) |

### Regular Beings (2-Character Symbols)

| Symbol | Name | Strength |
|--------|------|----------|
| dr | Dragon | high |
| og | Ogre | medium |
| go | Gorgon | medium |
| wi | Wizard | low |
| gr | Griffon | low |
| cy | Cyclops | medium |
| tr | Troll | low |
| bk | Black Knight | medium |
| hy | Hydra | medium |
| sp | Sphinx | medium |
| mi | Minotaur | medium |
| so | Sorceress | low |
| ha | Harpy | medium |
| kn | Castle Knight | low |
| bd | Black Dragon | high |
| pe | Pegasus | low |
| st | Stalker | medium |
| gi | Giant | medium |
| rd | Red Dragon | high |
| ad | Aqua Dragon | high |
| br | Black Rider | medium |
| rr | Red Troop Rider | low |
| dw | Dwarf | low |
| el | Elven Warrior | low |
| ki | King | medium |
| de | Demon | high |
| gh | Ghoul | high |
| bw | Black Wizard | high |
| ea | Elven Archer | medium |
| wo | Timber Wolf | low |
| cg | Citadel Guard | high |
| ow | Owl | low |
| eg | Eagle | low |
| du | Druid | medium |
| ba | Barbarian | high |
| rg | Red Guard | medium |
| bg | Black Guard | medium |

---

## Magic Items

18 magic items stored in `MAGIC_NAMES` (0x70000692):

| Symbol | Name |
|--------|------|
| Ri | Signet Ring |
| Or | Golden Orb |
| Cr | Crown |
| Sc | Scepter |
| Se | Great Seal |
| Bo | Magic Boots |
| Tk | Trans Key |
| Cl | Cloak |
| Sg | Spy Glass |
| On | One Ring |
| Mr | Magic Rope |
| Ca | Caduceus |
| Th | Thor's Hammer |
| Cb | Crystal Ball |
| Ws | Wizard's Scroll |
| Ir | Invisibility Ring |
| Tr | Teleport Ring |
| Sr | Staff of Ra |
| Sd | Staff of Death |

---

## Treasures

9 treasure types stored in `TREASURE_NAMES` (0x7000064A):

| Symbol | Name |
|--------|------|
| SI | Silver |
| AM | Amethyst |
| TO | Topaz |
| GO | Gold |
| SA | Sapphires |
| RU | Rubies |
| EM | Emeralds |
| DI | Diamonds |
| CR | Magic Crystals |

---

## Food and Water

### Food

- Food rations purchased at stores
- Last approximately 7 days (~420 moves at 60 moves/day)
- Consumed automatically from inventory or backpack
- Removed when fully consumed

### Water

- Must drink every 20 turns or strength weakens
- Drink by walking into any drinkable water source (shallow water,
  ford, etc.)
- Water skin can be purchased at stores (empty)
- Fill by moving through drinkable source while skin is in inventory
  (not backpack)
- Auto-consumed from inventory when timer hits
- Must be in inventory (not backpack) to drink from

---

## Player Commands

From in-game help:

| Key | Action | Game Function |
|-----|--------|---------------|
| Arrow keys | Move (N/S/E/W) | MOVE / MOVE_IN_CAVE |
| A | Attack | ATTACK |
| R | Release (drop) object | DROP |
| T | Take object from castle | TAKE |
| M | Auto-move | AUTO_MOVE |
| D | Display spell status | DISPLAY_MAGIC |
| G | Use spyglass | SPYGLASS |
| L | List players/allies/castles | LIST_PLAYERS |
| C | Cast non-attack spell | CAST |
| Q | Quit sieging castle | SEIGE |
| X | Rename castle (inside one) | CASTLE_INVENTORY or TAKE_OVER_CASTLE |
| K | Kill off player | KILL_PLAYER |
| F | Fire bow or crossbow | FIRE |
| P | Player alliances | ALLY_PLAYER |
| B | Backpack management | BACKPACK |
| O | Observe inventory | OBSERVE |
| ? | Territory maps | TERRITORY_MAP |
| ESC | Exit Quest | (main loop exit) |
| Erase Page | Refresh screen | REFRESH_SCREEN |

### Implicit Actions (auto-triggered)

- Store interaction — entering a store tile (STORE / BARGAIN)
- Being encounter — enemy in same tile (BEING_ATTACK / CAVE_ATTACK / KNIGHT_ATTACK / TOWER_ATTACK)
- Alchemist encounter — (ALCHEMIST_HOME)
- Thief encounter — (THIEF, currently a stub)
- Storms at sea — (STORMS_AT_SEA)

---

## Player Record Structure

Each player has a **686-word (0x02AE)** record in shared memory
(SD_PTR region).  Player N's record starts at:

```
SD_PTR + (N * 686)
```

Known fields from the display (offsets TBD during decompilation):

- Strength (current / max)
- Character class (Cleric, Wizard, Druid, Fighter, Barbarian)
- Purse (gold amount)
- Experience level
- Intelligence level
- Vision range
- Perception
- Quest level
- Castle ownership
- Dragon slain flag
- Rings held
- Position (X, Y)
- Food counter
- Water/drink counter
- Inventory slots
- Backpack slots

### Shared Memory Regions

| Pointer | Address | Contents |
|---------|---------|----------|
| SD_PTR | 0x70000210 | Player records, world state |
| OBJ_PTR | 0x70000212 | Object/item placement data |
| CAS_PTR | 0x70000214 | Castle data |

### Key Globals

| Symbol | Address | Description |
|--------|---------|-------------|
| PLAYER_NUM | 0x70000216 | Current player index |
| IN_BUFFER | 0x7000021C | Input buffer (varying string) |
| OUT_CHAN | 0x70000260 | Output channel |
| IN_CHAN | 0x70000262 | Input channel |
| CITY_NUM | 0x7000026C | Current city |
| TERR_NUM | 0x7000026E | Current territory |
| MOVES_LEFT | 0x70000270 | Remaining moves this turn |

---

## Data Array Addresses

### Being Arrays (indexed 0..49)

| Symbol | Address | Contents |
|--------|---------|----------|
| BEING_NAMES | 0x70000272 | 2-char display codes |
| BEING_STRENGTH | 0x7015088E | Base strength values |
| BEING_MOVES | 0x701508D2 | Movement rate |
| BEING_HIT_RATE | 0x70150916 | Combat accuracy |
| BEING_PERSUE_RANGE | 0x7015095A | Aggro radius |
| BEING_PERSUE_PERCENT | 0x7015099E | Chase probability |
| BEING_SURPRISE_PERCENT | 0x701509E2 | Ambush probability |
| BEING_PERCENT | 0x70150A26 | Spawn probability |
| PRINCESS_NAMES | 0x70150A6A | Associated quest names? |

### Object/Item Arrays

| Symbol | Address | Contents |
|--------|---------|----------|
| OBJECT_NAMES | 0x70000596 | Standard object display codes |
| TREASURE_NAMES | 0x7000064A | Treasure display codes |
| MAGIC_NAMES | 0x70000692 | Magic item display codes |
| PRICE | 0x70150416 | Store prices |
| WEIGHT | 0x70150426 | Item weights |
| MAGIC_WEIGHT | 0x7015043A | Magic item weights |

### Player Arrays

| Symbol | Address | Contents |
|--------|---------|----------|
| INITIAL_STRENGTH | 0x70150404 | Starting strength by class |
| PLAYER_MAX_STRENGTH | 0x7015040A | Max strength by class |
| PLAYER_MAX_INT | 0x70150410 | Max intelligence by class |
| PLAYER_SPELLS | 0x701506A8 | Per-class spell lists |
| ATTACK_SPELLS | 0x70150762 | Attack spell indices |
| SPELL_REGEN | 0x70150766 | Spell regeneration rates |
| SPELL_DURATION | 0x7015078C | Spell duration values |

### Map/World Arrays

| Symbol | Address | Contents |
|--------|---------|----------|
| STORE_NUM | 0x70151516 | Store locations |
| CAVES | 0x700007CA | Cave data |
| CAVE_CONTENTS | 0x70000942 | What's inside caves |
| CAVE_BEING | 0x70000A22 | Cave guardian being index |
| CAVE_BEING_STRENGTH | 0x70000A24 | Cave guardian strength |
| ALCHEMIST | 0x701507B2 | Alchemist data/positions |
| HOLY_COLORS | 0x70150B10 | Holy color associations |
| CAN_FLY | 0x70150B0A | Flyable terrain flag |
| CAN_BOAT | 0x70150B0C | Boatable terrain flag |
| CAN_CLIMB | 0x70150B0E | Climbable terrain flag |
| WEATHER | 0x70151582 | Weather strings |
