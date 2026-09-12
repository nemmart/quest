# P47 q001 — plan gate

Sep 12 2026. Tree: `main` @ `8cc9d83`. Nothing written yet except this
file; the numbers below come from a throwaway script outside the repo,
run over `quest.ir2.book`, `quest.addrbook`, `Disassembled/quest.dis`,
`Disassembled/quest.callsites`, `Disassembled/quest.symbols`, and the
`first execution of block` lines in `results/047b-p33b-arena-temps/*.err`.

## 1. Extraction method

Three edge sources, read from the book's **text** (the comment half of a
line carries the target name; the instruction half carries the address):

| source | form | count | target given by |
|---|---|---|---|
| decorated call | `call <tgt> args=… site=…` | 566 | `<tgt>` (26 of these are XCALLs to `.N@` entries the book reports as `symbol not found`; the address is still there) |
| embedded LCALL | `@site LCALL [0xADDR],n` | 159 | `0xADDR` — 155 game, 4 runtime (X.CB ×2, B.MOVE, C.TRANS) |
| embedded XCALL | `@site XCALL [pc+d] (0xADDR),0` | 37 | `0xADDR` in the parenthesis |
| `rt_call` | | 987 | ignored — runtime, hazard 6 |

Embedded LJSRs (130) are runtime intrinsics, not calls. Two of them are
edge-bearing and become **separate edge classes**, never merged into the
call graph (hazard 5):

- `LJSR O.ON` ×26 → **signal edge**, establisher → handler unit. Target is
  the `XLEF 2,[pc+d] (0xADDR)` immediately before the LJSR. In the book
  that XLEF has been lifted to an `ac2 = …` statement, so the address is
  taken from `quest.dis` at `site−2` and cross-checked against the book
  statement.
- `LJSR I.GOTO` ×26 → **non-local goto**, same `XLEF 2` target.
- `I.PROLOG` 18 / `I.EPILOG` 25 / `O.REVERT` 28 / `I.STOP` 7: no edge.

A fourth class, found while checking the two top-level `nocall` entries:
**task spawn** — QUEST `7015C390 WLDAI 0,0x70165713` feeding
`?CREATE_TASK` (CONSOLE_INTERRUPT.md). One edge, QUEST → C_A_LISTENER.

Cross-check: `Disassembled/quest.callsites` censuses 754 game-target
sites (566 CLEAN + 188 CLEAN-EMPTY). I count **758** = 754 + SQR31?3 ×3
(7015BD20, the PL/I sqrt library routine linked into the game segment) +
INIT_SHARED_DATA ×1 (7015BE23). Both are game-segment code that is **not
in the addrbook**; `callgraph.py` will add them as nodes from
`quest.symbols` so INIT_SHARED_DATA's own call to RETURN_MESSAGE
(7015BE74) has an owner.

Site → source node is by block owner: nearest addrbook entry at or below
the block pc, then family. Two blocks below 7015C005 (the first addrbook
entry) need the two synthetic nodes above.

## 2. Node definition

**Node = family** (`ATTACK`, `ATTACK.1@…`, … are one node), exactly as
`crossings.py` already does, with three adjustments:

1. **R40 pairs are one node with two entry points**: CREATE_MAP+DISPLAY_MAP,
   TRANSPORT_TERRAK+TRANSPORT_SUNDAR. Inbound edges to either entry land
   on the merged node; the entry hit is recorded on the edge.
2. **R39**: LOCK_FILE+UNLOCK_FILE one node, flagged `asm`, out of
   translation scope. Its *inbound* edges (LOCK_FILE ×2, UNLOCK_FILE ×1,
   from compiled callers) are real call edges and stay. Its *outbound*
   edge (the 3-arg RETURN_MESSAGE call at 70169B82, F6) is recorded but
   marked as assembly-origin, not PL/I.
3. **The 28 `#` unmigrated entries**: 26 are ON-unit bodies inside a
   family (`X.N@… nested,nocall`) — they are not nodes, they are signal
   targets inside their family's node. The other 2 are top-level:
   - **C_A_LISTENER** — node, reached only by the task-spawn edge.
   - **TERRAIN_HELP** (7017CB3D) — node with **no inbound edge of any
     class**. Not called, not signalled, not spawned, no data reference
     found in `quest.mem`; exists in the disassembly because
     `quest.symbols` names it. Either dead or reached through a path I
     cannot see. This is the one unresolved *reachability* item.

Result: 130 addrbook entries → 78 families → 76 after R40/R39 merges,
+ SQR31?3 + INIT_SHARED_DATA = **80 nodes**.

## 3. First measurement

| | |
|---|---|
| game call sites | **758** |
| statically unresolved | **0** — there are zero register-indirect `LCALL`/`XCALL`/`LJSR` forms in the game range (hazard 8 is a runtime-only phenomenon) |
| intra-family (parent→child, sibling→sibling XCALL) | 63 (= the 63 XCALL sites of F1/M3; all 63 land in the caller's own family) |
| cross-family sites | 695 |
| distinct family→family edges | 298 |
| signal edges | 26, all resolved, all establisher→unit **within the same family** |
| I.GOTO edges | 26, all resolved, all landing in the same family (three land in a *sibling* piece — DROP.1→DROP body, ATTACK.4→ATTACK.3 range, KILL_PLAYER.1/.4→KILL_PLAYER.2 range — which is F10's interleaving, not nesting) |
| task-spawn edges | 1 |
| leaves | **20** |

**Gate estimate of the resolved fraction: 758/758 = 100% of call
edges**, with the unresolved remainder being not a call edge but one
node (TERRAIN_HELP) with no inbound edge. I expect the final number to
be the same; the risk is in edge *attribution* (block ownership under
F10 interleaving), not in target resolution.

Leaves (20): C_A_LISTENER, DISPLAY_CAVE, FAKE_LAND_MASS, FAKE_OCEAN,
FIND_OBJECT, GET_INPUT, GET_OBJECT_INDEX, INIT_OBJ_TBL, OWNS, PICK_X_Y,
RANDOM, READ_IN, REFRESH_SCREEN, REGEN_SPELLS, RETURN_MESSAGE, SQR31?3,
TERRAIN, TERRAIN_HELP, THIEF, UPDATE_SCREENS. (THIEF is a leaf because
its body is dead — Plan.md known limitation 1.)

Height strata (leaf = 0, node = 1 + max over callees):

| h | n | nodes |
|---|---|---|
| 0 | 20 | the leaves above |
| 1 | 16 | AUTO_MOVE, BARGAIN, CLONE_SUNDAR, DIST, DISTANCE_TO_PLAYER, HIT_ANY_CHAR, INIT_SCREEN, INIT_SHARED_DATA, LOCK_FILE(asm), LOGON, MOVE, PLACE_PLAYER, TERRITORY, TRANSPORT_TERRAK, UPDATE_USER_DATA_FILE, WRITE_OBJECT |
| 2 | 16 | ALLY_PLAYER, CASTLE_INVENTORY, CREATE_MAP, DISPLAY_FLASK, DISPLAY_INVENTORY, DISPLAY_MAGIC, DISPLAY_SCREEN, HELP, KILL_PLAYER, KNIGHT_ATTACK, LIST_PLAYERS, MOVE_FAMILIAR, OP_HELP, REPOSITION, STORMS_AT_SEA, TAKE |
| 3 | 10 | ALCHEMIST_HOME, BACKPACK, BEING_ATTACK, CAST, DIED, GET_QUEST, MOVE_IN_CAVE, SPYGLASS, STORE, TERRITORY_MAP |
| 4 | 11 | CAVE_ATTACK, DEFEND, DROP, FIRE, LOOK, OBSERVE, OP_EDIT, REPORT, SIGNAL_TURN, TAKE_OVER_CASTLE, TOWER_ATTACK |
| 5 | 3 | ATTACK, CATAPULT, SEIGE |
| 6 | 1 | MOVE_PLAYER |
| 7 | 1 | BOAT |
| 8 | 1 | START_TURN |
| 9 | 1 | QUEST |

Roots (no callers): QUEST, C_A_LISTENER (spawned), TERRAIN_HELP (nothing).

## 4. Acyclic — yes

DFS over the 298 family edges finds **no cycle**, with or without the
R40/R39 merges. Consistent with Plan.md's non-reentrancy claim. The only
mutual pair in the program is LOCK↔UNLOCK (F8) and that is branch flow,
not calls.

## 5. Hazards against contact

All eight survive. Specifics:

- **H2 (one level)** — confirmed a second way: every one of the 63
  XCALL targets is a `.N@` entry of the caller's own family. No `.N@`
  entry is called from another family, and no `.N@` calls another
  family's `.N@`.
- **H7 (embed counts)** — 159 / 130 / 37 exactly.
- **H8** — zero indirect calls in the game; the note in Plan.md is about
  the runtime only. Not a gap.
- **H5** — signal edges resolve statically (the `XLEF 2` before every
  `O.ON`). What DESIGN §11 defers is the *CFG* consequences; the edges
  themselves are cheap.

## 6. One finding for the gate, not a hazard

**Play coverage vs. the leaf set.** Taking the union of `first execution
of block` over all 16 legs of the most recent full battery (047b): 2,090
distinct blocks, owned by **41 of 80 nodes**, and by **15 of the 20
leaves**. The five leaves the battery never executes:

- **PICK_X_Y** — DESIGN §13 project 6's named first routine. Its only
  caller is REPOSITION, which 047b never executes either: of REPOSITION's
  seven callers (ALCHEMIST_HOME, ATTACK, CAST, CAVE_ATTACK, DEFEND, DIED,
  MOVE_IN_CAVE) only DEFEND runs, and not down that path. M4A_ROLLCALL (Aug 22
  live play) lists it LIVE-UNEXERCISED too. Under L1 its oracle never
  fires. I will say so in Order.md; it may still be first on other
  grounds (it has a matched draft), but "leaf" alone does not make it a
  good first target.
- DISPLAY_CAVE, GET_OBJECT_INDEX, RETURN_MESSAGE (the fatal exit — only
  reachable by dying badly), TERRAIN_HELP.

Battery coverage is 047b's scripted legs plus its `play` leg, which is
killed mid-game; I will state the source of every "reached" claim in
Order.md rather than treat it as ground truth.

## Decision needed

None blocking. Two small ones, with my read:

1. **Do SQR31?3 and INIT_SHARED_DATA count as game nodes?** Options:
   (a) yes, both — they are game-segment code with call edges;
   (b) SQR31?3 is library (treat like `$N`, edge free under §9.3),
   INIT_SHARED_DATA is game. **Recommend (b)**: SQR31?3 is not Quest
   source and is not in the sync list's interest; INIT_SHARED_DATA is a
   real routine that the addrbook simply omitted (probably because it
   runs once before the first WSAVS-hijacked entry) and its omission is
   worth flagging to P46's owner, not silently fixing.
2. **Coverage source for Order.md.** Options: (a) 047b `.err` block
   traces as above; (b) rerun a battery on the runner to get fresh
   traces. **Recommend (a)** — it is in the tree, it is the current
   battery template, and re-running is P46's territory.

Plan for Part 2 unless told otherwise: `compiler/callgraph.py` with
`--leaves`, `--callers X`, `--callees X`, `--depth`, `--frontier`,
`--strata`, `--coverage <results-dir>`, `--unresolved`; then CallGraph.md
and Order.md from its output; REPORT.md last.
