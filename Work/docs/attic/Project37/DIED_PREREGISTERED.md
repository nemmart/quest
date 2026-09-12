# DIED as an experiment with pre-registered outcomes

**Read this before translating DIED.**

DIED's value to this project is no longer "the capstone".  Four beliefs now
rest on a single witness each, and DIED is the only routine in the program that
can supply a second instance of any of them.  It is therefore a **falsifier**,
and it should be approached the way an experiment is: the predictions are
written down HERE, BEFORE the translation, so that "DIED agreed with the model"
is a result and not a description of how the model was tuned.

Rule: if DIED contradicts one of these, that is a **finding**, recorded as a
falsification (METHOD §11) — not a cue to adjust the rule until DIED matches.
Each prediction below names what would count as a refutation.

---

## The four, and what each predicts

### 1. R29a — bit ASSIGNMENT is set-then-undo (confidence B, one witness)

**Why it is stuck:** OWNS was supposed to be the second witness for the bit
rules and it was — for the bit *tests*.  All seven of its bit operations are
`WSZB`; it contains no `WBTO` and no `WBTZ` at all.  So R26/R27/R28/R29 gained
a second witness and R29a gained none.

**Predicts:** DIED's 5 `WBTO` and 15 `WBTZ` are the set-then-undo shape —
`WBTO` the destination unconditionally, then the source value's R14b sign test,
then `WBTZ` on the zero arm (70166376..82 is the motivating instance).

**Refuted if:** any bit assignment in DIED emits `WBTZ` first, or emits only one
of the pair, or branches before the `WBTO`.

### 2. R36 — the loop-invariant subscript hoist (confidence C, one witness)

**Why it is stuck:** neither P35 loop can witness it.  UPDATE_SCREENS' body
subscript IS the loop variable (R9a); REFRESH_SCREEN's body references no
table.  OWNS is the only instance.

**Predicts:** DIED has two `XNDO` loops.  In any loop whose body references a
table with a subscript that does not depend on the loop variable, the bound
check (R17) and the stride multiply appear in the loop head BEFORE the loop's
own initialisation, and the R9 temp store appears AFTER it.

**Refuted if:** a DIED loop computes an invariant subscript inside the body, or
emits the temp store before the loop init.  **Also weakly refuted if neither
DIED loop has an invariant body subscript** — in which case R36 stays at C with
no second witness anywhere in the program, and that should be stated rather
than quietly left.

### 3. R7c′ — the if-body pin, and specifically its RELEASE POINT (confidence C)

**Why it is stuck:** this is the most fitted rule in the set.  The pin itself
(an R13b body inherits the skip state and protects a value the continuation
still reads) has eight instances in OWNS and is reasonably solid.  The *release
point* — "when the subscript expression's stride multiply is done" — was chosen
because it is the boundary that makes OWNS' 6-vs-1 register split come out, and
nothing else yet requires it.

**Predicts:** in DIED, an R13b if-body that computes a bit address will avoid
the pinned register for the subscript load and the stride constant, and may
take it from the `*16` constant onward.

**Refuted if:** DIED avoids the pinned register for the whole body, or releases
it earlier.  Either would mean OWNS' split has a different cause and the rule
should be restated, not re-fitted.  **A refutation here is expected to be the
most likely of the four** — treat it as such rather than defending the rule.

### 4. The self-move at a join (RECORDED, not ruled — one instance)

**Why it is stuck:** RETURN_MESSAGE 70176FF5 opens the join of its message
diamond with `ac0 = ac0` (`WMOV 0,0`), a real self-move in the instruction
stream.  Both arms already leave the length in ac0.  RETURN_MESSAGE is staged,
so this cannot be tested there.

**Predicts:** at an if/else join whose two arms leave the same quantity in the
same register, DIED emits a redundant `WMOV r,r`.

**Refuted if:** DIED has such a join and emits nothing.  Then RETURN_MESSAGE's
self-move has a local cause and stays an oddity.

---

## Also pre-registered: the three predicted arm-path defects

CODEGEN_RULES §8.8 lists three sites that still ask "is there a later use?"
with a bare `j > i`, the same way R27 did before its amendment: R9's scaled
temp, R10's element-address temp, and R3's "free from its last use on".  They
were deliberately NOT fixed speculatively.

**Predicts:** DIED (or INIT_OBJ_TBL) references one table element from two arms
of one `if`, and at least one of the three produces a DIFF that the amendment
fixes.  If that happens it is a **prediction confirmed**, not a discovery.

**Also worth stating:** if DIED trips none of them, they stay listed and
unfixed.  A predicted defect that never fires is not a reason to fix it blind.

---

## What DIED cannot settle

- **R10's verdict.**  P36 already showed R10 as stated is falsified (70166110
  is a counter-instance with four later references and no element-address
  temp).  P36 also identified the discriminating variable as ac2 contention.
  R10 needs the loop to run, i.e. DIED translated far enough to reach both
  701660F8 and 70166110.
- **The GET_INPUT open question** — the temp ALLOCATION order for a multi-dummy
  call (docs/Project37/GetInputFinding.md §3).  What is needed is a call mixing
  a call-materialised argument with plain dummies.  DIED has six game→game
  calls and two `rt_call`s; **check whether any of them has a BITS()-like
  argument before assuming DIED cannot help.**  If one does, take it.
- **The two mixed-arity spellings** — CLOSED, not open.  A census showed the
  program contains exactly two `mixed:` routines, so no third instance exists
  and any binary discriminator fits.  Unfalsifiable here; do not reopen it.

## Housekeeping before DIED

- `game/routines/DIED.c` is P35's staged opening, corrected by P36 (argument 1
  is a CHAR VARYING by reference; argument 2 is never read).
- DIED needs the two constructs P37 has not built: the frame relocation
  (FIRE.1 is the small case and should come first — it is the routine that
  decides whether `#pragma fp ac2` is needed at all) and the arena twins.
- Pragma count is currently **0** and should be reported next to every match
  number regardless of its value.
