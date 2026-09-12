# Project 47 — The translation order

Sep 12 2026. A judgement, not a sort. Inputs: `CallGraph.md` (height,
callees, callers), `Coverage.md` (does the battery execute it), `callgraph.py
--profile` (size, constructs), Salvage (which constructs exist), a001 (the
"PICK_X_Y first" ruling is withdrawn; the oracle must fire).

## 0. The criteria, in the order they were applied

1. **Zero §9.3 cost.** A leaf, or every game callee already replaced. All
   twelve below satisfy this at the point they are reached — bottom-up is
   respected, and no Eagle callee has to be un-checked for any of them.
2. **The L1 oracle fires.** Reached by the 047b battery, and by enough of
   its blocks that a wrong translation would be caught. A 100%-covered
   17-statement routine called 44 times is a better first target than a
   64-statement leaf called 0 times. Statement coverage is quoted for each.
3. **Constructs exist.** Salvage names what does not: floats (F-mnemonics),
   arena twins (`t@`/claim), bit fields (F17/P7), uplevel access (F1/F2),
   ON-units (DESIGN §11, hard-error), optional args (F5), and the calling
   bridge for any `rt_call` (P46 F7 / P48). A routine needing one of these
   is deferred until it exists — or is named as the pilot for it.
4. **Payoff.** Callers (how much of the frontier it opens) and heat (how
   often the oracle fires per leg).

"Checker cost" below is what leaves the sync list when the routine is
replaced: its own blocks (they are then verified by the rendezvous instead
of the lockstep). It is never a callee for these twelve.

## 1. The order

| # | routine | h | stmts | 047b cov | callers/sites | needs | why here |
|---|---|---|---|---|---|---|---|
| 1 | **RANDOM** | 0 | 17 | 17/17 (100%) | 16 / 44 | 16-bit `slotpatch` return (F4), `cvwn`, **bridge** for `?RANDOM_NUMBER` ×1 | the seed |
| 2 | **UPDATE_SCREENS** | 0 | 72 | 71/72 (99%) | 16 / 36 | DO loops ×2, record fields; **no rt_call** | matched draft; the bridge-free first if P48 is late |
| 3 | **GET_INPUT** | 0 | 22 | 18/22 (82%) | 27 / 77 | bridge for `?READ` (6 args), BIT literal via X.CB (F12), 144-byte CHAR buffer (F14), unsigned char (F13) | every keystroke of every leg |
| 4 | **HIT_ANY_CHAR** | 1→0 | 10 | 10/10 (100%) | 26 / 64 | bridge for `?WRITE_SCREEN` ×2, one located string | zero-cost once #3 is in |
| 5 | **FAKE_LAND_MASS** | 0 | 255 | 137/255 (54%) | 2 / 2 | DO loops ×4; no rt_call, no strings, no bits | pure loops over the map |
| 6 | **FAKE_OCEAN** | 0 | 224 | 30/224 (13%) | 2 / 2 | DO loops ×8; same class as #5 | weak oracle — see note |
| 7 | **INIT_SCREEN** | 1→0 | 134 | 133/134 (99%) | 10 / 19 | DO loops ×6; no rt_call | zero-cost after #5/#6; opens MOVE |
| 8 | **FIND_OBJECT** | 0 | 183 | 119/183 (65%) | 14 / 64 | bit test ×2 (P7 materialise), DO ×4; no rt_call | the bit-test pilot, small dose |
| 9 | **OWNS** | 0 | 152 | 73/152 (48%) | 13 / 93 | bit fields ×7 (F17), 16-bit slotpatch, DO ×2 | most-called routine; matched draft; the bit-field pilot |
| 10 | **REFRESH_SCREEN** | 0 | 61 | 53/61 (87%) | 7 / 13 | optional arg by marker word (F5, `mixed:0/1`), 5 strings, bridge for `?WRITE_SCREEN` ×5, bits ×2 | matched draft; the optional-arg pilot |
| 11 | **TERRAIN** | 0 | 274 | 174/274 (64%) | 5 / 13 | 9 args, 3 strings, `cvwn` ×2, byte pointers ×4; no rt_call | the wide-argument-list pilot |
| 12 | **TERRITORY** | 0 | 364 | 354/364 (97%) | 7 / 7 | **floats ×6 + SQR31?3** (library, edge-free), 11 strings | first float routine; 97% covered |

Twelve rather than ten because #5–#7 are one unit (INIT_SCREEN is not
zero-cost without both FAKE_* routines) and #12 is the float gate.

## 2. Each, in more words

**1. RANDOM.** h=0, 17 statements, 5 blocks all executed, called from 16
families at 44 sites, 147k hits in the Aug-22 session. It calls
`?RANDOM_NUMBER` once and stores a 16-bit result into the saved-ac0 image
(F4). It is first because it is the hardest thing to get wrong quietly: the
seed advances on every call, so a wrong translation diverges the *next*
random event in the master/clone pair, not just its own return value.
DESIGN §9.2's outgoing-call-sequence comparison is exercised by the first
routine. Risk: it needs the calling bridge (P48) for its one `rt_call` —
if the bridge is not ready, RANDOM waits and UPDATE_SCREENS goes first.
Checker cost: 5 blocks.

**2. UPDATE_SCREENS.** h=0, 72 statements, 17/18 blocks, 16 families / 36
sites, no runtime calls at all, two DO loops over record fields. It
matched the book register-exact through the void translator (Salvage
§3.6) so a draft exists and the shape is known. It is the routine that can
be substituted with the harness alone — no bridge, no strings, no bits.
Checker cost: 18 blocks.

**3. GET_INPUT.** h=0, 22 statements, 4/5 blocks, 27 families / 77 sites —
the most widely called routine in the program, and every scripted leg
passes through it on every key. Needs: the bridge for a six-argument
`?READ` (F14 lists the arguments), the `'001'B` literal rebuilt at run time
by X.CB (F12 — a runtime LCALL, not a game edge; it too needs the bridge),
a 144-byte CHAR buffer in the frame (F14), and `unsigned char` (F13, the
sign test at 7016AA63 is dead otherwise). A draft exists. Risk: the ?READ
bridge is the largest bridge instance of the first ten; if P48 delivers
`?RANDOM_NUMBER` and `?WRITE_SCREEN` first, GET_INPUT slips behind #4–#7.
Checker cost: 5 blocks.

**4. HIT_ANY_CHAR.** h=1 with GET_INPUT as its only game callee, so h=0 the
moment #3 lands; 10 statements, 4/4 blocks, 26 families / 64 sites. Two
`?WRITE_SCREEN` calls and one located string. Zero §9.3 cost after #3; a
draft exists. Checker cost: 4 blocks.

**5–7. FAKE_LAND_MASS, FAKE_OCEAN, INIT_SCREEN.** INIT_SCREEN (134
statements, 31/32 blocks, 10 families / 19 sites) calls exactly the two
FAKE_* routines; MOVE (h=1, battery-reached) calls the same two. All three
are pure: no runtime calls, no strings, no bits — DO loops over the map
arrays (4 / 8 / 6 loops). They are the best test of the compiler's loop
and array lowering with the harness doing nothing else. **FAKE_OCEAN's
oracle is weak: 8 of 60 blocks, 13% of statements execute** — the
battery's player never sails, so most of the ocean-drawing paths run zero
times. Say so on its L1 badge; do not skip it, because INIT_SCREEN is not
zero-cost without it. Replacing #5–#7 opens MOVE (h=1, 615 statements,
27% covered). Checker cost: 64 + 60 + 32 blocks.

**8. FIND_OBJECT.** h=0, 183 statements, 35/55 blocks, 14 families / 64
sites (42 of them from OP_EDIT). Two bit tests and four DO loops, no
runtime calls. It is the first routine to need a bit reference (P7:
`WSUB; WSZB; WADC; MOV.L#` materialise), and it needs only the *test*
form, twice — a small first dose of the bit construct before OWNS needs
the full field arithmetic. Checker cost: 55 blocks.

**9. OWNS.** h=0, 152 statements, 28/50 blocks, 13 families / 93 sites —
the most-called routine by sites, and a matched draft exists. It needs bit
fields properly (7 bit sites over `PLAYER.fm590`-class fields, F17), a
16-bit slotpatch return, two DO loops. It is the bit-field pilot, and it
comes after FIND_OBJECT so the bit *test* is already proven when the bit
*address arithmetic* is added. 48% coverage: the battery reaches it
through START_TURN/MOVE_PLAYER/TAKE but not through the ATTACK/CAST/SEIGE
sites (57 of its 93 sites are in never-executed families). Checker cost:
50 blocks.

**10. REFRESH_SCREEN.** h=0, 61 statements, 10/14 blocks, 7 families / 13
sites. Matched draft. Needs the optional-argument spelling by marker word
(F5, `mixed:0/1` — the only routine besides RETURN_MESSAGE that has one),
five located strings, five `?WRITE_SCREEN` calls, two bit sites. It is the
optional-arg pilot, placed after the bridge and strings are exercised by
#3/#4. Checker cost: 14 blocks.

**11. TERRAIN.** h=0, 274 statements, 66/107 blocks, 5 families / 13
sites, nine arguments (the widest argument list in the program), three
strings, two `cvwn`, four byte pointers, no runtime calls. It is the test
of argument marshalling at width; QUEST calls it once at startup so every
leg fires the oracle. Checker cost: 107 blocks.

**12. TERRITORY.** h=0, 364 statements, 79/85 blocks (97%), 7 families / 7
sites. Six float instructions plus a call to SQR31?3 (library, edge-free
under a001 D1, but it is an `LCALL` to a linked routine and needs the same
bridge treatment as a `$N` call). It is the first routine that cannot be
done without floats, and with 97% coverage it is the best float pilot
available — better than DIST/DISTANCE_TO_PLAYER, which are smaller but
identical in construct. If floats are not built by the time #11 is done,
TERRITORY waits and the frontier from #1–#11 (22 candidates,
`--frontier`) has plenty else.

## 3. Deferred, with the reason on each

| routine | h | stmts | 047b cov | why not in the first twelve |
|---|---|---|---|---|
| **DIST**, **DISTANCE_TO_PLAYER** | 0 | 19, 30 | 100%, 100% | Floats (5 F-mnemonics each) + SQR31?3. They are the hottest leaves in the program (62k / 443k hits in live play) and are otherwise perfect — they are the argument for building floats early, and they follow TERRITORY the day floats exist. |
| **READ_IN** | 0 | 17 | 65% | Has an ON-unit (READ_IN.1, signal edge 701766FA). DESIGN §11 hard-errors. It is the smallest handler-bearing routine in the program (7 parent statements + the unit) and is the natural **first ON-condition project** — but not before the CFG closure exists. |
| **INIT_OBJ_TBL** | 0 | 173 | 77% | 8 twin sites, 3 claim/release, 10 located strings — needs `s@` arena twins. Runs once per leg from QUEST; a good twin pilot, after twins exist. |
| **REGEN_SPELLS** | 0 | 402 | 20% | 7 bit sites, 14 DO loops, weak oracle (21/107 blocks). After OWNS proves bits, and after the driver plays more turns. |
| **PICK_X_Y** | 0 | 64 | **0%** | a001: never executed by any leg; reached only via QUEST 7015C31F (killed-off login) → DIED → REPOSITION, or by dying in play. A matched draft exists, so it costs nothing to *build* — but it cannot be *accepted* at L1 until the killed-off login fixture (Coverage §3.2 D) is a leg. Then it is a one-afternoon item. |
| **RETURN_MESSAGE** | 0 | 30 | 0% | The fatal exit (F11); never returns; only on error paths. A draft exists. Needs an injected-fault leg to fire; its oracle is the terminal-path handling (METHOD §13). |
| **GET_OBJECT_INDEX**, **DISPLAY_CAVE** | 0 | 186, 502 | 0%, 0% | Never reached (DROP/ATTACK/CAST/BOAT/OP_EDIT callers; the cave). Leaves with no oracle. DISPLAY_CAVE additionally has 5 twins and 54 strings. |
| **THIEF** | 0 | 2 | 100% | `WSAVS; WRTN;` — a stubbed routine (Plan.md limitation 1). Translating it (`return;`) proves nothing; do it as a harness smoke test if one is wanted. |
| **C_A_LISTENER**, **TERRAIN_HELP** | 0 | 12, 100 | 17%, 0% | Not called: task body and orphan respectively. Not translation targets in the §9.4 sense. |
| **LOCK_FILE** (+UNLOCK) | 1 | 99 | 20% | R39 hand assembly; out of scope by definition. |

## 4. What the order buys

After #1–#12 the replaced set is 12 leaves (1,768 statements, 3.3% of the
book — the leaves are small; the program's mass is in the h≥2 dispatch
subtrees). `--frontier` then offers 22 zero-cost candidates: BARGAIN,
CLONE_SUNDAR, DISPLAY_CAVE, DISPLAY_FLASK, DISPLAY_MAGIC, DIST,
DISTANCE_TO_PLAYER, GET_OBJECT_INDEX, INIT_OBJ_TBL, LIST_PLAYERS, MOVE,
MOVE_FAMILIAR, OP_HELP, PICK_X_Y, PLACE_PLAYER, READ_IN, REGEN_SPELLS,
RETURN_MESSAGE, TAKE, THIEF, TRANSPORT_TERRAK, WRITE_OBJECT. Of those,
only DIST, DISTANCE_TO_PLAYER, INIT_OBJ_TBL, LIST_PLAYERS, MOVE,
MOVE_FAMILIAR, READ_IN, REGEN_SPELLS, TAKE and THIEF are battery-reached — the frontier runs into Coverage.md's never-executed set
almost immediately, which is the point Coverage §4 makes.

The next things that matter more than the next routine: the driver fix
(Coverage §3.2 A, +14 nodes), the killed-off and operator login fixtures
(D, F), floats (unlocks 3 fully-covered leaves), and the ON-condition CFG
(unlocks READ_IN and, behind it, LOGON, AUTO_MOVE and every menu routine
with a handler).
