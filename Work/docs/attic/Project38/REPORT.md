# Project 38 — Part 3 report

Branch `p38-routines`, from `main` at **a3e36ce** (the P38 prompt commit;
parent 9eb32d9, the P37 merge — the vintage the prompt asked for).  The
uploaded `Work/` was verified byte-identical to the repo tree before starting,
and `docs/Provenance.md` checked.  Nothing in `emulation/` or `Disassembled/`
was touched; no battery was run.

---

## 1. What this project delivers

**Routines finished this project: 0.  Running total: 4 of ~130.**

That is the number that matters and it did not move.  What moved is the model:
R33/R34 went from derived-but-unimplemented to implemented and validated, and
three beliefs in the ledger were corrected — one of them a rule that has been
carried since P35 and was never supported by its own evidence.

Nothing was staged.  Two routines were attempted and both were **abandoned
with a reason**, which is what the project's binding constraint asks for:

| routine | stmts | result | pragmas |
|---|---|---|---|
| PICK_X_Y | 64 | 64/64 primary, 41/41 folded (never regressed) | 0 |
| UPDATE_SCREENS | 72 | 72/72 primary, 44/44 folded (never regressed) | 0 |
| REFRESH_SCREEN | 61 | 61/61 primary, 43/43 folded (regressed to 59 mid-project, restored) | 0 |
| OWNS | 152 | 152/152 primary, 114/114 folded (never regressed) | 0 |
| **FIRE.1** @7016A3BD | 89 | **ABANDONED** — three unmodelled constructs. No match number. | — |
| **HIT_ANY_CHAR** @7016DE91 | 10 | **ABANDONED at 8/10**, 2 DIFF from one register choice, identity slot bijection | 0 |
| DISTANCE_TO_PLAYER | 30 | not attempted — floating point. Used as an R41 witness only. | — |
| INIT_OBJ_TBL | 173 | not attempted — needs the whole twins construct | — |
| DIED | 495 | **not reached** | — |
| TRANSPORT pair | — | not reached | — |

**Totals unchanged: 349/349 primary, 242/242 folded, 0 DIFF, 0 pragmas** across
the four matched routines.  `ircmp.py --selftest` PASS after every rule change.
All six routine sources compile clean under `gcc -std=c99` and `g++ -std=c++17`
with `-Wall -Wextra -Werror`.

**Pragma count: 0.**  As P37 predicted, the frame case needed none.

### 1.1 The four DIED pre-registrations

**NO WITNESS, all four.**  DIED was not reached, so R29a (bit assignment), R36
(the loop-invariant hoist), R7c′'s release point and the join self-move are
each recorded as **NO WITNESS** — not confirmed, not refuted.  The
pre-registration in `docs/Project37/DIED_PREREGISTERED.md` stands untouched and
is still valid for the next project.  Likewise the two live search targets (a
BITS()-shaped argument among DIED's eight call sites; R36's second witness) and
the three predicted arm-path defects: none tripped, because nothing ran.

---

## 2. Stage 1 — R33/R34 implemented (the project's one real deliverable)

`Regs` now models ac0–ac3.  The frame is an ordinary register content
`("fp", None)`; `emit()` resolves the `ac3` in a frame reference to wherever
the frame actually is, respelling the reference and appending an `LDAFP` into a
`pick_fp()` destination when it is in no register at all.  The `fp_dirty` flag
is gone — "the frame is not in ac3" is now just ac3's ordinary content (after a
WCMV it holds the source end).  `pick_fp()` is R7's cost order with ac3 as the
tie-break, per R33's "ac3 holds it by default"; the target is never coded as
ac2 (FrameRelocation.md §4).

The implementation is live code, not a no-op refactor: REFRESH_SCREEN's five
`LDAFP`s are emitted through the new path and it stays at 61/61.

Implementing it required one correction to R8 and produced one amendment to
R33 itself.  Both are in `compiler/CODEGEN_RULES.md` §9.

---

## 3. The rule ledger

### 3.1 R3b VOIDED — the most valuable result here

**P35's disjoint-temp-pools clause was never evidence at all.**  R3b said
*"scalar temps never take words that ever belonged to a string temp"*, resting
on REFRESH_SCREEN's 4/18/20-vs-6/22/34 slot pattern.  But REFRESH_SCREEN's
string temps are **live at every point where a scalar temp is allocated**, so
slot 6 was unavailable under a disjoint reading and under an overlapping one
alike.  The observation could not have come out the other way.  This is
METHOD §16's lesson applied to the ledger's own contents, and it caught a
belief that had been carried unchallenged through P36 and P37.

Its replacement, **R3b′ (conf B)**: the pools **overlap**, but a dead *string*
temp's words become available to a scalar temp only from a **later statement**
on.  Two witnesses, one for each side of the boundary:

- HIT_ANY_CHAR 7016DEAD puts the packed CHAR VARYING at `wp(ac3, 4)` — the slot
  the 30-byte prompt dummy held (4..19) until the `?WRITE_SCREEN` **two
  statements earlier** consumed it.  Disjointness predicts slot 20.
- REFRESH_SCREEN 70176B10 does *not* take slot 6 for the row temp although the
  CAT dummy there is dead — it died in **that same statement**.  It takes 18.
  Plain overlap predicts 6.

Note the contrast with R3's clause for scalar temps, which frees a slot "from
its last use on, *including within the statement that last uses it*".  A string
temp's words are released one statement later than a scalar temp's.  Getting
this wrong cost REFRESH_SCREEN two statements mid-project; both routines are
100 % on slots under R3b′, with identity bijections.

### 3.2 R41 — the base-register class {ac2, ac3}.  Three witnesses, conf B

An address or base load does **not** use R7's pick over all four registers.  It
allocates from a two-register class **{ac2, ac3}, ac2 preferred**; ac0 and ac1
are value registers and are not candidates however cheap they are.

| witness | instruction pair | what it shows |
|---|---|---|
| INIT_OBJ_TBL 7016DF68 | `ac2 = M32[wp(ac3, -14)]` / `ac3 = M32[wp(ac3, -14)]` | the **same argument load, same routine**, going to ac2 when ac2 is free and to ac3 when ac2 is live — with ac0 free at cost 0 both times |
| FIRE.1 7016A3C7 | `ac3 = M32[wp(ac3, -6)]` (+2 more) | ac2 live, ac1 free at cost 0 → ac3 |
| DISTANCE_TO_PLAYER 701687B5 | `ac0 = add(ac0, M32[0x70000210])` then `ac3 = ac0` (WMOV 0,3) | the cleanest: the address is *already in ac0* and is still moved to ac3, because ac2 holds a live value and ac0/ac1 are not in the class |

**R33 amended.**  Its phenomenon stands and is confirmed — the frame is not
pinned, it is displaced and re-materialised by `LDAFP`.  Its *mechanism* ("ac3
takes it when ac3 is the R7 pick") is contradicted: at FIRE.1's
`ac3 = M32[wp(ac3, -6)]` the costs are ac0 = 3, ac1 = 0, ac2 = 0, ac3 = 2, so
R7 picks ac1 and the book picks ac3.  The discriminator in all three routines is
only ever **ac2's occupancy**.

**R34 is now derived rather than fitted.**  P37 recorded it as a separate
B-confidence rule from two instances.  Under R41 it is a consequence: at
INIT_OBJ_TBL 7016DF68 ac0, ac1 and ac2 are all live and ac3 holds a live base,
so there is no register for the frame at all and saving ac3's occupant is the
only way to get one.  A rule that explains a previously hand-fitted rule as a
consequence is the strongest structural result in the model.

**Why the four matched routines could not have witnessed R41**: ac2 is free at
every base load in all 349 of their statements, so the class never reaches its
second member and R41 and R5 agree everywhere.  Their staying at 100 % is
*consistent with* R41 and is not evidence for it (§16); the three witnesses
above are.

### 3.3 R8d — the frame survives a join.  Conf A

R8's reset at a join clears cached values but **not** the frame.  Where the
frame is, is not knowledge about a value — it is the record of which `LDAFP`s
have executed, and a join executes none.

Evidence: every block of all four matched routines addresses `wp(ac3, d)` with
no preceding `LDAFP`, across 349 statements.  Measured the other way, when
`reset()` clears ac3 the frame becomes cost 0, `pick()` takes it immediately
and all four regress — PICK_X_Y 62/64, UPDATE_SCREENS 44/72, REFRESH_SCREEN
59/61, OWNS 107/152.

This is a genuine correction, not an implementation detail: R8 as written is
false of ac3, and the only reason P35–P37 never met the falsification is that
ac3 was pinned and so never passed through `reset()`.

**Corollary, NO WITNESS:** `restore()` (R8c) leaves the frame where the emitter
actually left it and discards the snapshot's opinion about ac3.  In all four
routines the frame is in ac3 on both sides of every edge, so nothing tests this.
Recorded as a modelling choice.

### 3.4 FP_COST — a fitted constant, with its experiment named

The frame needs a protection cost in R7's table.  `FP_COST = 2`.

- **Lower bound, established:** at 0 or 1 all four routines regress (figures
  above).  `FP_COST >= 2`.
- **Upper bound, NOT witnessed:** 2 and 3 give byte-identical output for all
  four.  They differ in exactly one case — a statement where ac0, ac1 and ac2
  are *all* live when a register is picked.  No such statement occurs in the
  four.  **The experiment that would pin it** is a routine containing one where
  the contested register is wanted for an ordinary value (INIT_OBJ_TBL's
  `WPSH 3,3` site has all three live but is covered by R34, so it does not
  separate them).

2 is the weaker of the two admissible claims: 3 would additionally assert that
ac3 is never taken while any other register is live, which has no witness.

### 3.5 OPEN — the caller's register state after a game→game call

**No rule recorded.**  Full detail in CODEGEN_RULES §9.5.  HIT_ANY_CHAR's only
divergence is `ac1 = 0x00020D0B` in the book against `ac0` in the translation.
A program-wide sweep of sites that load a packed immediate and store it to a
frame slot in the block immediately after a decorated game→game `call` finds
**15 sites: 12 use ac0, 3 use ac1, and all three ac1 sites call GET_INPUT**.

Two readings fit all fifteen:

1. **The caller models the callee's exit registers** — GET_INPUT leaves a live
   pointer in ac0 at both `ret` blocks, and Quest is one compilation unit so
   the compiler could know.  A large claim about the compiler on three call
   sites of one callee.
2. **The byte-pointer argument** — those three are also the only sites pushing
   `XPEFB` rather than `XPEF`.  Fits perfectly and has **no mechanism**, so
   §16 rules it out.

**What would separate them:** a site calling a different callee that also
leaves a live ac0 (reading 1 predicts ac1, reading 2 predicts ac0), or an ac1
site whose callee leaves ac0 dead.  Neither exists in the fifteen.

### 3.6 Two defect fixes (not rules)

- The routine-end path dropped an empty final block even when a preceding
  `rt_call` still named it as its continuation.  `start()` already guarded this
  with `is_target`; the end path did not.  Found by HIT_ANY_CHAR, whose last
  statement is an `rt_call` followed by a bare `ret` block.
- `&c` for a byte-wide local now emits the byte address `bp(ac3, 2*slot)`
  (XPEFB), not a word pointer.

---

## 4. Standing checks

`compiler/crossings.py` was **moved from `docs/Project37/`** — METHOD §16
already named the `compiler/` path, and §16 makes it a check run at the start of
every routine-adding session, so it belongs with the standing tools rather than
in a project directory.  Its `ROOT` and usage line were corrected; §16 needed no
edit.  P37's REPORT.md and InadmissibleEvidence.md still quote the old path and
were left as written, being the historical record of P37.

Output at the start of this project, unchanged from P37: LOCK_FILE ↔
UNLOCK_FILE mutual (the program's one hand-assembly unit), the two one-way R40
pairs CREATE_MAP → DISPLAY_MAP and TRANSPORT_TERRAK → TRANSPORT_SUNDAR, **no
third assembly unit.**

**Correction to the prompt, accepted by the user:** "the five existing routines
must stay at 349/349" — 349 is **four** routines.  GET_INPUT and RETURN_MESSAGE
are staged with no match number and cannot be part of a no-regression
invariant.  Four were held.

---

## 5. Blocking constructs for the next project

Ordered by how many routines they unblock, which is estimated, not measured.

1. **The static link / uplevel access** (`docs/Project38/FIRE1_ABANDONED.md`
   §2).  There is no way to write "a variable of the enclosing procedure" in
   the C subset: `quest_rt.h` has no notation, `declarations.json` describes
   statics and tables but not another procedure's frame, and
   `gen_declarations.py` therefore cannot generate one.  The frame-layout fact
   is already established independently (M4aDesign.md's restore image,
   ON_ERROR_CATALOG §B) — what is missing is a source spelling and the codegen
   rule for when the link is reloaded from `wp(fp, -6)`.  The addrbook's `.N@`
   nested entries run all through the program (FIRE alone has `.1`, `.2`,
   `.3`), so this is the largest of the three.
2. **The twins / arena** — `LEN(v)`, claim/release sizing, `WMSP`/`STASP`/
   `LDASP`, variable-length `WCMV`, and the `dyn` frame.  None of it is in the
   translator today.  This is INIT_OBJ_TBL's whole cost and is why it was not
   the cheap Stage 1 validator the prompt expected; it remains fully designed
   in P36/P37 and unbuilt.
3. **Floating point** — `WFLAD`, `FRDS`, `FMS`, `FAS`, `WFFAD`, plus the
   `SQR31` runtime call.  Blocks DISTANCE_TO_PLAYER (30 statements, otherwise
   trivial) and anything doing geometry.

Three smaller ones, each currently a single witness: the `COM.#` skip form (a
16-bit test against −1), `WADC r,r` as the constant −1, and §3.5's open
question above.

## 6. How many routines could be attempted today

**Honest estimate: few — and this project is why I would not guess higher.**
The subset now covers frame slots and both temp pools, the register model
including the base class, DO loops, IF shapes, DERR folds, bit references,
string statements, indexed stores, rt_calls and game→game calls.  A routine
needing only those can be attempted.

But a sweep for routines that displace the frame found **79 of 130**, and the
two smallest of them that I opened — DISTANCE_TO_PLAYER at 30 statements and
HIT_ANY_CHAR at 10 — needed floating point and an underivable register rule
respectively.  Both looked trivial from the outside.  The census cannot tell
you which constructs a routine contains, so any number I give here would be the
kind of claim §16 exists to prevent.  What can be said: **the next project's
throughput is bounded by the twins and the static link, not by the register
model**, which is now in much better shape than it was at the start of P38.
