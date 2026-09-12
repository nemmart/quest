# Project 47 — REPORT: the call graph and the translation order

Sep 12 2026. Branch `p47-callgraph` on `main` @ `18b3aa7`. Plan gate:
`q001-plan-gate.md` / `a001-plan-gate.md`. Deliverables:

| artifact | what |
|---|---|
| `compiler/callgraph.py` | builds the graph; `--check --leaves --callers --callees --depth --strata --frontier --unresolved --profile --coverage --edges --json` |
| `docs/Project47/CallGraph.md` | 80 nodes, 298 distinct call edges (695 sites), 63 intra-family calls, 26 signal + 26 goto + 1 spawn edges, strata, the unresolved list |
| `docs/Project47/Coverage.md` | added deliverable (a001): what the battery executes, the partition, my read on the gap |
| `docs/Project47/Order.md` | the first twelve routines, with reasons, risks and costs |

Boundary respected: `compiler/callgraph.py` and `docs/Project47/**` only.
Nothing regenerated; no C, no IR; `emulation/`, `game/`, `docs/IR.md`
untouched.

## 1. The number

**758 / 758 game call edges resolved — 100%.**

The unresolved remainder, enumerated: **none among edges.** One node has
no inbound edge of any class — **TERRAIN_HELP @7017CB3D** (100
statements, `nocall`): not called, not signalled, not spawned, its address
absent from `quest.dis` and `quest.mem`, never executed by any leg or the
Aug-22 session. Recorded as a finding (CallGraph §6), not chased.

Also 26/26 signal edges and 26/26 non-local gotos resolved, all
intra-family; 63/63 intra-family XCALLs; 0 register-indirect call forms in
the game range; 0 cycles.

Independent cross-check: `Disassembled/quest.callsites` (a different
tool) counts 754; the four extra are the sites whose targets are the two
routines the addrbook omits (SQR31?3 ×3, INIT_SHARED_DATA ×1), and
`--check` asserts the identity.

## 2. The leaf set, and what the battery reaches

**20 translatable leaves** (h=0, kind `game`) plus two `unit` nodes
reached by dispatch (C_A_LISTENER, TERRAIN_HELP):

DISPLAY_CAVE, DIST, DISTANCE_TO_PLAYER, FAKE_LAND_MASS, FAKE_OCEAN,
FIND_OBJECT, GET_INPUT, GET_OBJECT_INDEX, INIT_OBJ_TBL, OWNS, PICK_X_Y,
RANDOM, READ_IN, REFRESH_SCREEN, REGEN_SPELLS, RETURN_MESSAGE, TERRAIN,
TERRITORY, THIEF, UPDATE_SCREENS.

The 047b battery executes at least one block of **16 of the 20**. The
four it never touches: DISPLAY_CAVE, GET_OBJECT_INDEX, PICK_X_Y,
RETURN_MESSAGE. By statements, the battery executes 15.5% of the book
(8,305 / 53,588) and 41 of 80 nodes; 36 nodes (43% of the book) are
executed by neither the battery nor the Aug-22 human session. Coverage.md
partitions those 36 by what it would take to reach them.

## 3. Gate estimate vs final

| | q001 estimate | final | why different |
|---|---|---|---|
| resolved call edges | 758/758 | **758/758** | — |
| nodes | 80 | **80** | — |
| leaves | 20 (incl. SQR31?3) | **20 translatable + 2 units** | a001 D1 made SQR31?3 library and edge-free, so DIST, DISTANCE_TO_PLAYER and TERRITORY became leaves; SQR31?3 left the set |
| height of QUEST | 9 | **8** | same cause |
| cycles | none | **none** | — |
| signal / goto edges | 26 / 26, all intra-family | **26 / 26**, all intra-family | — |
| nodes reached by battery | 41/80 | **41/80** | — |
| leaves reached | 15/20 | **16/20** | the leaf set changed under D1 |
| unresolved | one node (TERRAIN_HELP) | **one node (TERRAIN_HELP)** | — |

The estimate held. The one thing I expected to move — attribution errors
under F10 interleaving — did not: every WSAVS in the game range is an
addrbook entry (132 = 130 + 2), so entry attribution is exact except
inside an interleaved range, and family attribution is exact everywhere.

## 4. Hazards against contact

All eight survived. Refinements worth recording:

- **H1/H2 (families, one level).** Confirmed by a second route: none of
  the 63 XCALL targets is outside the caller's family, and the link-load
  census reproduces F1 exactly (44 / 18 / 1). What the addrbook's
  `nested` flag on `#` lines *does* show is **ON-unit handler chains** —
  handlers established inside handlers (KILL_PLAYER → .1 → .2 → .3,
  OP_EDIT with three chains). That is not procedure nesting.
- **H3 (R40).** Both pairs merged; inbound edges land on either entry
  (CREATE_MAP 1 site, DISPLAY_MAP 2; TRANSPORT_TERRAK 2, TRANSPORT_SUNDAR
  1). The merged node's statement count (909 / 140) replaces the mis-sized
  per-entry figures of Census.md.
- **H4 (R39).** LOCK_FILE node marked `asm`; its RETURN_MESSAGE call at
  70169B82 carries `asm-origin` and is not treated as PL/I.
- **H5 (signal edges).** Cheaper than the hazard implied: every O.ON has
  its handler address two words before it. What §11 defers is the CFG
  consequence, not the edge.
- **H7.** 159 / 130 / 37 exactly.
- **H8 (register-indirect).** Runtime-only. Zero such forms in the game.
  The hazard was written on the assumption the game had analogues; it
  does not.

## 5. Findings

**F-A. INIT_SHARED_DATA @7015BE23 is a game routine with no addrbook
line.** 20 blocks / 90 statements, `WSAVS`, called once from QUEST at
7015C00B, calls RETURN_MESSAGE at 7015BE74 (the site Salvage F6 counts
among RETURN_MESSAGE's five callers), three `?OPEN_SHARED_IO_FILE` and
three `?GET_SHARED_PAGE` runtime calls. It executes on every leg (16 of
its 20 blocks in 047b). Being absent from the addrbook it has no 0x74
area and no WSAVS hijack — it runs on the MV stack, before the first
migrated entry. Probably harmless because it runs before any migrated
frame exists, and probably why nobody noticed. **Open item for the
addrbook's owner (P46)**; this project did not touch `quest.addrbook`
(a001 D1).

**F-B. The play report and the graph agree (a001 D3).** `NEW_USERS.PR` is
a separate 32 KB image whose symbol table has no PICK_X_Y; only
`QUEST.ST` does — creation lives elsewhere, but PICK_X_Y does not. All
eight REPOSITION sites and the PICK_X_Y site attribute to the family the
graph gives them, by nearest-WSAVS audit of `quest.dis`. The path the
user remembers is real and was under-described in q001: QUEST's startup
at 7015C31F reads PLAYER field K=−377 and, if set, calls DIED with the
literal `"This player was previously killed off!"` (0x7015BF25, 38 bytes)
→ REPOSITION → PICK_X_Y. A newly created record evidently logs in through
it. The battery never does: block 7015C323 executes 0×, its skip 7015C322
11×. So "PICK_X_Y runs when you create a new character" is a login path
inside QUEST.PR, not a different program, and not an attribution error.
It also names *a* fixture for reaching PICK_X_Y, DIED and REPOSITION under
L1: one killed-off record in the user data file.

**~~AMENDED at integration (a002)~~ — THE AMENDMENT ITSELF IS VOID, refuted by P49 (see `docs/Project49/a001-plan-gate.md`). Both START_TURN→DIED sites are starvation deaths; creation does not reach PICK_X_Y, and the class-prompt block already runs in 8 legs. F-B's original killed-off-flag route is neither confirmed nor refuted and stays open. The struck amendment read:** The common route is first character creation — START_TURN block
70178246 prompts for the character type and calls DIED (7017868D / 70178801)
→ REPOSITION → PICK_X_Y. DIED is "(re)initialise and place a character", not
death-only. So the fixture is **no fixture**: the driver types unused
initials and answers one prompt, reaching PICK_X_Y, REPOSITION,
PLACE_PLAYER, DIED and part of START_TURN. The graph was right throughout;
only the route chosen for the narrative was the uncommon one.

**F-C. Three non-local gotos land in a sibling piece's addrbook range**
(DROP.1 → DROP's range, ATTACK.4 → ATTACK.3's, KILL_PLAYER.1/.4 →
KILL_PLAYER.2's). These are F10 interleaving — the label is parent-body
code inside an ON-unit's range — not a second nesting level. Recorded in
CallGraph §5.3 so the next reader does not reach for the wrong
conclusion.

**F-D. The battery executes 15.5% of the book.** 41 of 80 nodes by any
block, but 8,305 of 53,588 statements; only five tiny routines run to
completion; the large reached routines run at 18–64%. Detail and
partition in Coverage.md.

**F-E. TERRAIN_HELP** has no inbound edge of any class (§1).

**F-F. START_TURN has 37 game callees** — the program is a command
dispatcher, wide rather than deep (§6).

## 6. My own read: is bottom-up tractable on this graph?

**Yes — and it is not the interesting question.**

Tractable: 80 nodes, 9 strata, no cycles, 20 leaves called from 1–27
families each. The replaced set is downward-closed by construction, the
marginal §9.3 cost of each next routine is zero, and `--frontier` gives
the next candidates mechanically. Twelve leaves in, the frontier offers 22
more. DESIGN §9.4 was a ruling made without the graph; the graph confirms
it as far as it goes.

Where it stops going:

1. **The leaves are small.** The twelve routines in Order.md total 1,768
   statements — 3.3% of the book. The program's mass is in the h≥2
   subtrees under START_TURN's 37 commands: ATTACK 4,369, OP_EDIT 3,812,
   MOVE_PLAYER 3,392, START_TURN 2,755, CAST 2,534. Bottom-up gives a
   correct order *within* each subtree and says nothing about *which*
   subtree, and depth is nearly useless as a tiebreak because the
   subtrees are all h=3–6.
2. **Coverage, not depth, is the binding constraint on L1.** DESIGN §8
   makes L1 primary and its oracle is gameplay. Of the frontier after the
   first twelve, half is never executed by any leg; the biggest subtrees
   (ATTACK, CAST, the castle line, OP_EDIT) are reached by no scripted leg
   at all. A bottom-up walk that ignores this arrives, quickly, at
   routines that can be built but not accepted. Order.md therefore ranks
   by (zero cost) first and (oracle fires, at how many statements) second,
   and defers PICK_X_Y — DESIGN's own first pick — on that ground.
3. **§9.3's cost is smaller than stated when the callee is never
   executed.** Un-checking a callee "loses verification of it for that
   run" — but a callee the battery never executes was never verified by
   that run. So the frontier can be relaxed: a routine whose unreplaced
   game callees are all never-executed may be translated at zero
   *effective* checker cost. After the first twelve that adds nine
   candidates (INIT_SHARED_DATA 79% covered, REPORT, DEFEND,
   STORMS_AT_SEA, ALCHEMIST_HOME, LOOK, REPOSITION, TERRITORY_MAP,
   UPDATE_USER_DATA_FILE). It is a small relaxation and an honest one;
   the un-checked callee should carry the same "L1-unverified" mark it
   already deserves.

What I would do with the ruling: keep §9.4 as the order *within* the
reached region, and add two sentences to DESIGN — that the first-routine
choice and every subtree choice are made on Coverage.md's numbers, and
that ATTACK, CAST, SEIGE/TAKE_OVER_CASTLE and OP_EDIT will be **L2-first
routines** under the current battery, because no behavioural oracle
exists for them until the driver can fight, cast, besiege or log in as an
operator. The driver fix (Coverage §3.2 A, +7,474 statements for one key
sequence) and the two login fixtures (D, F, +4,989) are worth more to L1
than the next ten routines, and should be scheduled ahead of routine #5.

## 7. Provenance and what was not done

- Every number is `callgraph.py` output over `main` @ `18b3aa7` artifacts;
  the D3 audit and the `quest.mem` literal read were by hand against
  `quest.dis`/`quest.mem`.
- Not done: no attempt to find what establishes TERRAIN_HELP (a001: do not
  chase); no re-run of any battery (a001 D2); no edit to `quest.addrbook`
  for INIT_SHARED_DATA (a001 D1); Census.md's caveat text not updated (not
  in boundary — the family statement counts in CallGraph §2 are the
  replacement figures).
- Statement counts are book statements in blocks owned by the family. They
  include embedded instructions (`@pc …` lines) as one statement each, so
  a float-heavy routine (DIST: 10 embeds of 19) counts its unlifted
  instructions.
