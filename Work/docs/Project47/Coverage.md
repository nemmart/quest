# Project 47 — Coverage: what the play battery actually executes

Added deliverable (a001). Sep 12 2026. Source of every "reached" claim is
stated; none of it is ground truth.

**Provenance.**
- **047b** = `results/047b-p33b-arena-temps`, the accepted reference battery
  (16 legs: fo/m/inj/abort/derr/play ×{book, stock, emu, st}, k1fo, inj3,
  forced). Coverage is the union over all legs of the `first execution of
  block` lines in `*.err`, mapped to nodes by `callgraph.py --coverage`.
  Block-level; a block "executed" means every statement in it executed at
  least once (basic blocks), so statement coverage here is exact for
  executed blocks and zero for the rest. **Caveat carried from NextSession:
  the play legs end by SIGTERM before the driver's post-auto-move keys reach
  the command prompt** (Project33/FINDINGS_SUMMARY §2), so play-leg coverage
  is partial by construction.
- **Aug-22 live play** = `docs/Project14/M4A_ROLLCALL.md`, one human session
  under lockstep, names only (no block data), 44 of the then-101 live
  entries; 39 families.

## 1. The headline

| | battery 047b | live play Aug 22 | union |
|---|---|---|---|
| nodes reached (of 80) | **41** | 39 | **44** |
| translatable leaves reached (of 20) | **16** | 16 | 16 |
| statements in executed blocks | **8,305 / 53,588 = 15.5%** | — | — |
| blocks executed | 2,090 / 13,507 = 15.5% | — | — |

Node counts overstate what is verified: a node counts as reached when one
block runs. By statements the battery executes **15.5% of the book**, and
only 5 nodes are executed to completion (DIST, DISTANCE_TO_PLAYER,
HIT_ANY_CHAR, RANDOM, THIEF — all tiny). The large routines the battery
does reach are executed at 20–50%: START_TURN 45%, MOVE_PLAYER 18%,
DISPLAY_INVENTORY 64%, GET_QUEST 13%.

**36 nodes (23,282 statements, 43% of the book) are executed by neither
source.**

Live play and the battery reach different things. Live-only: ATTACK, CAST,
STORE (the player fought and cast; the driver does neither). Battery-only:
AUTO_MOVE, C_A_LISTENER, INIT_SHARED_DATA, LIST_PLAYERS (partially, 52 of
1,058 statements — the roll-call listed it as unexercised).

## 2. Per node (`--coverage`)

Sorted by statement coverage. `%` is statements-in-executed-blocks over the
node's statements.

| node | h | kind | 047b blocks | 047b stmts | % | Aug-22 live play |
|---|---|---|---|---|---|---|
| DIST | 0 | game | 2/2 | 19/19 | 100 | yes |
| DISTANCE_TO_PLAYER | 0 | game | 3/3 | 30/30 | 100 | yes |
| HIT_ANY_CHAR | 1 | game | 4/4 | 10/10 | 100 | yes |
| RANDOM | 0 | game | 5/5 | 17/17 | 100 | yes |
| THIEF | 0 | game | 1/1 | 2/2 | 100 | yes |
| INIT_SCREEN | 1 | game | 31/32 | 133/134 | 99 | yes |
| UPDATE_SCREENS | 0 | game | 17/18 | 71/72 | 99 | yes |
| TERRITORY | 0 | game | 79/85 | 354/364 | 97 | yes |
| SQR31?3 | 0 | library | 11/14 | 84/89 | 94 | — |
| REFRESH_SCREEN | 0 | game | 10/14 | 53/61 | 87 | yes |
| DISPLAY_SCREEN | 1 | game | 180/224 | 1019/1230 | 83 | yes |
| GET_INPUT | 0 | game | 4/5 | 18/22 | 82 | yes |
| INIT_SHARED_DATA | 1 | game | 16/20 | 71/90 | 79 | — |
| INIT_OBJ_TBL | 0 | game | 9/17 | 134/173 | 77 | yes |
| AUTO_MOVE | 1 | game | 42/62 | 104/140 | 74 | — |
| FIND_OBJECT | 0 | game | 35/55 | 119/183 | 65 | yes |
| READ_IN | 0 | game | 5/6 | 11/17 | 65 | yes |
| DISPLAY_INVENTORY | 1 | game | 285/505 | 1339/2086 | 64 | yes |
| TERRAIN | 0 | game | 66/107 | 174/274 | 64 | yes |
| LOGON | 1 | game | 56/87 | 192/309 | 62 | yes |
| TOWER_ATTACK | 4 | game | 69/114 | 242/418 | 58 | yes |
| FAKE_LAND_MASS | 0 | game | 35/64 | 137/255 | 54 | yes |
| QUEST | 8 | unit | 104/220 | 409/850 | 48 | yes |
| OWNS | 0 | game | 28/50 | 73/152 | 48 | yes |
| BEING_ATTACK | 2 | game | 205/443 | 731/1600 | 46 | yes |
| START_TURN | 7 | game | 370/878 | 1252/2755 | 45 | yes |
| MOVE | 1 | game | 37/143 | 163/615 | 27 | yes |
| SIGNAL_TURN | 3 | game | 27/150 | 119/483 | 25 | yes |
| LOCK_FILE | 1 | asm | 7/45 | 20/99 | 20 | yes |
| REGEN_SPELLS | 0 | game | 21/107 | 79/402 | 20 | yes |
| MOVE_PLAYER | 5 | game | 184/1023 | 624/3392 | 18 | yes |
| C_A_LISTENER | 0 | unit | 2/4 | 2/12 | 17 | — |
| REPORT | 4 | game | 18/43 | 57/370 | 15 | yes |
| FAKE_OCEAN | 0 | game | 8/60 | 30/224 | 13 | yes |
| GET_QUEST | 2 | game | 25/203 | 120/901 | 13 | yes |
| DEFEND | 4 | game | 26/231 | 106/895 | 12 | yes |
| CATAPULT | 4 | game | 11/100 | 43/410 | 10 | yes |
| STORMS_AT_SEA | 2 | game | 5/48 | 16/164 | 10 | yes |
| LIST_PLAYERS | 2 | game | 25/254 | 52/1058 | 5 | — |
| MOVE_FAMILIAR | 2 | game | 7/126 | 27/609 | 4 | yes |
| TAKE | 2 | game | 15/292 | 49/1176 | 4 | yes |
| ALCHEMIST_HOME | 3 | game | 0/199 | 0/871 | 0 | — |
| ALLY_PLAYER | 2 | game | 0/106 | 0/297 | 0 | — |
| ATTACK | 4 | game | 0/1141 | 0/4369 | 0 | yes |
| BACKPACK | 2 | game | 0/138 | 0/502 | 0 | — |
| BARGAIN | 1 | game | 0/214 | 0/842 | 0 | — |
| BOAT | 6 | game | 0/194 | 0/745 | 0 | — |
| CAST | 3 | game | 0/703 | 0/2534 | 0 | yes |
| CASTLE_INVENTORY | 2 | game | 0/172 | 0/598 | 0 | — |
| CAVE_ATTACK | 3 | game | 0/361 | 0/1503 | 0 | — |
| CLONE_SUNDAR | 1 | game | 0/47 | 0/331 | 0 | — |
| CREATE_MAP | 1 | game | 0/225 | 0/909 | 0 | — |
| DIED | 3 | game | 0/73 | 0/495 | 0 | — |
| DISPLAY_CAVE | 0 | game | 0/61 | 0/502 | 0 | — |
| DISPLAY_FLASK | 2 | game | 0/21 | 0/99 | 0 | — |
| DISPLAY_MAGIC | 2 | game | 0/40 | 0/204 | 0 | — |
| DROP | 3 | game | 0/437 | 0/1638 | 0 | — |
| FIRE | 3 | game | 0/307 | 0/1305 | 0 | — |
| GET_OBJECT_INDEX | 0 | game | 0/63 | 0/186 | 0 | — |
| HELP | 2 | game | 0/196 | 0/609 | 0 | — |
| KILL_PLAYER | 2 | game | 0/104 | 0/395 | 0 | — |
| KNIGHT_ATTACK | 1 | game | 0/114 | 0/491 | 0 | — |
| LOOK | 3 | game | 0/33 | 0/127 | 0 | — |
| MOVE_IN_CAVE | 3 | game | 0/267 | 0/1118 | 0 | — |
| OBSERVE | 3 | game | 0/279 | 0/1044 | 0 | — |
| OP_EDIT | 3 | game | 0/735 | 0/3812 | 0 | — |
| OP_HELP | 2 | game | 0/6 | 0/31 | 0 | — |
| PICK_X_Y | 0 | game | 0/18 | 0/64 | 0 | — |
| PLACE_PLAYER | 1 | game | 0/88 | 0/399 | 0 | — |
| REPOSITION | 2 | game | 0/4 | 0/11 | 0 | — |
| RETURN_MESSAGE | 0 | game | 0/8 | 0/30 | 0 | — |
| SEIGE | 4 | game | 0/324 | 0/1230 | 0 | — |
| SPYGLASS | 2 | game | 0/63 | 0/205 | 0 | — |
| STORE | 2 | game | 0/320 | 0/1241 | 0 | yes |
| TAKE_OVER_CASTLE | 3 | game | 0/143 | 0/721 | 0 | — |
| TERRAIN_HELP | 0 | unit | 0/12 | 0/100 | 0 | — |
| TERRITORY_MAP | 2 | game | 0/344 | 0/1492 | 0 | — |
| TRANSPORT_TERRAK | 1 | game | 0/29 | 0/140 | 0 | — |
| UPDATE_USER_DATA_FILE | 1 | game | 0/24 | 0/74 | 0 | — |
| WRITE_OBJECT | 1 | game | 0/30 | 0/162 | 0 | — |
## 3. The partition

Three sets. "L1-reachable now" means a substituted C routine would execute
under the current battery and so has a behavioural oracle. "L1 after driver
fix" is my read of what the owed driver work (NextSession: keys after the
auto-move never reach the prompt) would add. "L2-only under the current
battery" is everything else: no behavioural oracle exists for it until the
driver can produce that game state.

### 3.1 L1-reachable now — 41 nodes

DIST, DISTANCE_TO_PLAYER, HIT_ANY_CHAR, RANDOM, THIEF, INIT_SCREEN,
UPDATE_SCREENS, TERRITORY, READ_IN, DISPLAY_SCREEN, INIT_SHARED_DATA,
GET_INPUT, SQR31?3, REFRESH_SCREEN, AUTO_MOVE, LOGON, FIND_OBJECT, TERRAIN,
TOWER_ATTACK, DISPLAY_INVENTORY, OWNS, FAKE_LAND_MASS, INIT_OBJ_TBL,
C_A_LISTENER, QUEST, BEING_ATTACK, START_TURN, REPORT, MOVE, REGEN_SPELLS,
SIGNAL_TURN, MOVE_PLAYER, LOCK_FILE, FAKE_OCEAN, GET_QUEST, DEFEND,
CATAPULT, STORMS_AT_SEA, LIST_PLAYERS, MOVE_FAMILIAR, TAKE.

Weak oracles inside this set (under 25% of statements, or a single block):
REPORT 15%, STORMS_AT_SEA 10%, MOVE_FAMILIAR 4%, TAKE 4%, LIST_PLAYERS 5%,
CATAPULT 10%, DEFEND 12%, GET_QUEST 13%, FAKE_OCEAN 13%, LOCK_FILE 20%,
REGEN_SPELLS 20%, MOVE_PLAYER 18%. A leaf in this list with 13% coverage
(FAKE_OCEAN) is a worse first target than one at 98% (UPDATE_SCREENS).

### 3.2 The 36 never-executed nodes, by what it would take

| group | nodes | stmts | what reaches them |
|---|---|---|---|
| **A. menu keys from the command prompt** | HELP, OBSERVE, LOOK, DISPLAY_MAGIC, SPYGLASS, BACKPACK, CASTLE_INVENTORY, DROP, ALLY_PLAYER (+ their private callees GET_OBJECT_INDEX, DISPLAY_FLASK, PLACE_PLAYER, TERRITORY_MAP, UPDATE_USER_DATA_FILE) | 14 / **7,474** | Direct START_TURN callees. **The driver bug.** The driver already sends O / D / L / H; they do not land. Fixing the key sequence is the whole cost. Some need trivial state (an item to DROP, another player to ALLY) — the scripted approach in FINDINGS_SUMMARY covers that. |
| **B. combat and casting** | BARGAIN, CLONE_SUNDAR, REPOSITION, TRANSPORT_TERRAK, CREATE_MAP, KNIGHT_ATTACK, FIRE | 7 / 4,029 | ATTACK (4,369) and CAST (2,534) themselves *were* reached in live play, so the state is producible; the driver never fights or casts. Needs a combat script — an encounter is a dice roll away from a move, so this is driver work plus patience, not a new mechanism. |
| **C. caves** | CAVE_ATTACK, MOVE_IN_CAVE, DISPLAY_CAVE | 3 / 3,123 | Walk to a cave and enter. The Aug-22 player could not find one; a script with the world data could. Content-state work. |
| **D. death and kill** | DIED, KILL_PLAYER, PICK_X_Y | 3 / 954 | Die, or log in as a record flagged killed-off (QUEST 7015C31F: PLAYER K=−377 set → DIED → REPOSITION → PICK_X_Y — the "new character" path of a001 D3). **A one-record fixture in the user data file**, cheaper than dying. |
| **E. castles, boats, alchemist** | SEIGE, TAKE_OVER_CASTLE, BOAT, ALCHEMIST_HOME | 4 / 3,567 | Deep content: a siege needs a castle and an army; BOAT is h=6 and needs a boat. Genuinely hard to script; likely the last reached. |
| **F. operator mode** | OP_EDIT, OP_HELP, WRITE_OBJECT | 3 / **4,005** | Gated by the operator flag at 0x7000021A (QUEST 7015C2BB). A login fixture, like D; then OP_EDIT (3,812 statements, the second-largest node) is a menu to script. |
| **G. fatal exit** | RETURN_MESSAGE | 1 / 30 | Only on the fatal paths (F11). A scripted "inj"-style leg that breaks a file open would reach it; it never returns, so its L1 oracle is the terminal path handling (METHOD §13). |
| **H. no inbound edge** | TERRAIN_HELP | 1 / 100 | Nothing reaches it (CallGraph §6). L2-only, possibly dead. |

Sum: 36 nodes, 23,282 statements.

### 3.3 The partition, summarised

| set | nodes | stmts | share of book |
|---|---|---|---|
| L1-reachable now by the battery (any block) | 41 | 22,162 | 41% — but only 8,305 (15.5% of the book) actually executed |
| reached only in the Aug-22 human session (ATTACK, CAST, STORE) | 3 | 8,144 | 15% — no scripted leg reaches them |
| L1 after the driver fix (A) | +14 | +7,474 | +14% |
| L1 after cheap fixtures (D killed-off login, F operator login, G fatal leg) | +7 | +4,989 | +9% |
| L1 after combat/cave scripting (B, C) | +10 | +7,152 | +13% |
| L1 only after deep content scripting (E) | +4 | +3,567 | +7% |
| L2-only, possibly dead (H) | 1 | 100 | 0.2% |

## 4. My read (a001 point 4)

**The driver bug is the single biggest item and it is hygiene-sized, but
it is not the gating item for L1 as a whole — the battery's shallowness
inside the routines it does reach is.**

Three observations:

1. **Group A is the driver bug and nothing else**: 7,474 statements, 14
   nodes, all one key sequence away from the prompt. That is the cheapest
   14% of the book there is, and it should be done before the first
   handler-bearing or menu routine is chosen. It is hygiene: the driver
   already sends the keys.
2. **Groups D and F are login fixtures**, not play: a killed-off record and
   an operator record in the user data file turn 4,989 statements from
   "unreachable" into "reached every leg". D also happens to be the path to
   PICK_X_Y. Same weekend as the driver fix.
3. **The number that should worry the design is 15.5%, not 41/80.** Of the
   22,162 statements in nodes the battery touches, it executes 8,305 (37%). Once
   the first few leaves are done, L1's oracle for anything sizeable
   (START_TURN 45%, MOVE_PLAYER 18%) is a fraction of the routine, and L1
   accepts on executed paths only (DESIGN §8). So the real gating item for
   the strategy is the standing "statements never executed by any leg"
   verdict line NextSession already asks for — L1 needs that number to be
   reported per routine at the moment the routine is substituted, and it
   needs a driver that plays enough turns to push it up. The driver fix is
   the first step of that, not the whole of it.

Groups B, C and E (14,742 statements, 27%) are real content-state work
and will take scripted play with knowledge of the world data. None of it
blocks the first ten routines in Order.md; all of it blocks L1 for ATTACK,
CAST and the castle line, which are the biggest subtrees in the program.
DESIGN §8's L1/L2 pairing is the right answer for those — they will be
L2-first routines whether anyone likes it or not — and the design should
say so.
