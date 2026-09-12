# Project 51 — CANDIDATE C

## GOAL

Derive and write **the C we believe is right** for seven routines, from the
disassembly.

That is the whole project. **No compiling, no matching, no running.** The
compiler will not support some of what you write; that is expected, and it is
this project's *output* rather than its problem.

**Success = seven `.c` files, each with a derivation and a confidence, plus a
measured list of what the compiler is missing.**

This is stage 1 of three:

| stage | project | what |
|---|---|---|
| **1** | **this** | candidate C, derived from the disassembly |
| 2 | next | extend the compiler to whatever stage 1 actually needed |
| 3 | after | compile, diff against the book, census the rewrite rules |

The split is deliberate. **Stage 1 tells stage 2 what to build**, instead of
the compiler being extended against a guess. And the derivation is the part
that cannot be rushed: everything downstream rests on the C being right.

---

## The routines

Work down the list. **Stop and report when budget runs short** — four
routines derived properly beats seven derived thinly.

| # | routine | stmts | why this one |
|---|---|---|---|
| 1 | **HIT_ANY_CHAR** | 10 | **P38 ABANDONED it at 8/10** rather than fit a cross-procedural register model on three sites of one callee. Stage 3's head-to-head against the old approach |
| 2 | **PICK_X_Y** | 64 | **CONTROL.** Matched 64/64 under the old translator, so the C is very likely right — stage 3 needs routines whose differences are *purely* rewrite distance |
| 3 | **GET_INPUT** | 22 | **P37 STAGED it** at the `BITS()` argument — the other case the new design should dissolve. Salvage F14 already has its frame and `?READ$6` signature |
| 4 | **UPDATE_SCREENS** | 72 | **CONTROL, already written and RUN by P48.** No re-derivation needed; include it in the confidence table as the calibration point |
| 5 | **INIT_SCREEN** | 134 | loop cohort — no exotic constructs |
| 6 | **FAKE_OCEAN** | 224 | loop cohort |
| 7 | **FAKE_LAND_MASS** | 255 | loop cohort |

**Why the loop cohort matters** despite being last: the attic's most refitted
rules were the loop ones — R36/R36′/R36a hoisting, R21c/R21d/R21e′ loop
registers and DO limits, four of which P40 amended in a single project. If a
small general rewrite set works anywhere, it shows there.

---

## Context of record

| path | why |
|---|---|
| `docs/Salvage.md` | **Read before the disassembly.** P45's verified facts: F1/F2 static link, F4 slotpatch, **F12 `X.CB` and BIT literals**, F13 CHARACTER is unsigned, **F14 GET_INPUT's frame + `?READ$6`**, F16 twin sizing, F17 record fields and bit numbering, **F18 no world-coordinate offset** (with its 0x3B73 correction) |
| `docs/Project28/RTConventions.md` | runtime call conventions — **and where you RECORD any you work out**, see the duty below |
| `docs/RTWorklist.md` | which runtime routines play reaches, with call counts |
| `Disassembled/quest.dis`, `quest.mem`, `quest.symbols` | the primary source |
| `docs/Project34/readable/` | the readable renderings — a reading aid, not authority |
| `game/declarations.json`, `declarations.h`, `quest_rt.h` | record layouts and the C spellings |
| `docs/Project48/REPORT.md` §3 | **the model for a derivation.** Read it before writing your first header |
| `compiler/lower_c.py` | what the compiler accepts today, so you can name the gaps precisely |
| `docs/Project47/CallGraph.md` | callers and callees per routine |

**Do not read `docs/attic/`.**

---

## Carried-in rulings

1. **RE-DERIVE. Do not inherit.** `game/routines/` holds P35–P43 output
   written by sessions fitting against the book. Stage 3 measures rewrite
   distance; if the C carries the old translator's shape, stage 3 measures
   the old translator. You may consult the old file **after** writing yours,
   and **must report whether it agreed and whether you had read it first** —
   P48 disclosed exactly this, and "token-for-token identical" is worth much
   less when it is not blind.
2. **Every file ships its DERIVATION in the header**: addresses, instruction
   readings, and above all the **cross-checks that constrain it**. P48's
   UPDATE_SCREENS header is the model — its bounds-consistency check (two ABS
   bounds against two subscript bounds) is a real constraint a wrong stride
   would break, worth more than three restatements.
3. **Every file carries a CONFIDENCE**: `verified` (ran, or matched under the
   old line), `derived` (cross-checked against an independent constraint),
   `claimed` (one reading, nothing corroborates it). Stage 3 weights its
   conclusions by this, so be hard on yourself.
4. **Write the C you believe is right, not the C you think will compile.** If
   the clear expression of a routine needs something `lower_c.py` lacks,
   **write it anyway and record the gap.** Bending the C to today's compiler
   would corrupt stage 3's measurement, which is the whole point.
5. **Never read `quest.ir2.book` to decide what to write.** It is stage 3's
   target; derive from `quest.dis` / `quest.mem`. Reading it to *check* a
   reading is allowed — say where you did.
6. **`SUB()` placement matters.** P48 found the original checks the guards but
   not the store, because it reuses a hoisted base. Where the original omits a
   check, omit it, and say why in the header.

---

## The RT documentation duty — BINDING

You will meet runtime routines nobody has documented. **When you work one
out, record it in `docs/Project28/RTConventions.md`**: entry address, inputs
(which register or slot holds what), outputs and which registers survive, the
call form, the game call sites, the evidence, and a confidence.

That file's scope was widened for this project to cover any `X.*` / `I.*` /
`O.*` / `D.*` helper, not only the 18 `?` routines.

**`X.CB` is the cautionary case.** It was worked out because GET_INPUT forced
it and recorded in `Salvage.md` F12 — ac2 = destination word address, ac0 =
byte pointer to the character form, ac1 = its length, undecorated
`LCALL [0x7017E708],0`. The next session to meet a BIT literal would have
re-derived it from nothing. **A convention that lives only in a C file's
header is a convention the project has not learned.**

---

## Part 1 — PLAN GATE

`docs/Project51/q001-plan-gate.md`, push to main, **STOP**. Report:

1. **Your reading method** — how you get from `quest.dis` to a C statement,
   and what you do when a reading is ambiguous.
2. **A first pass over all seven**, naming for each: the constructs it needs,
   which of those `lower_c.py` lacks, and which runtime routines you will have
   to work out. Rough is fine — I want the shape before you commit budget.
3. **HIT_ANY_CHAR in full**, as the worked example, with its derivation. Ten
   statements; deriving it at the gate proves the method and costs little.
4. **Anything in `Salvage.md` that does not survive contact with the
   listing.** P45 verified it, but F18 already carries one attic correction
   (0x3B73, not 0x3B77), so a second is not unthinkable — and a wrong fact
   there propagates into every routine.

STOP. Wait for `a001`.

---

## Part 2 — Build

One `.c` per routine in `game/routines/`, derivation in the header,
confidence stated. Record runtime findings in `RTConventions.md` **as you
go**, not at the end.

---

## Part 3 — Report

`docs/Project51/REPORT.md`:

- **the confidence table**: routine, confidence, and what constrains it
- **THE HANDOFF — the compiler gap list.** Every construct you wrote that
  `lower_c.py` cannot accept, with the routines needing it and how central it
  is. **This is stage 2's specification** and the most load-bearing section in
  the report
- runtime routines documented, and any you could not work out
- per re-derived routine: **did the old `game/routines/` file agree**, and had
  you read it first
- anything in `Salvage.md` you would correct
- where you are least confident, and what would settle it

---

## Boundaries — BINDING

1. **You may WRITE:** `game/routines/` files you author,
   `docs/Project28/RTConventions.md` (additive), `docs/Project51/**`.
2. **Do NOT touch** `compiler/**` (stage 2's), `emulation/**` (**P49 owns
   it**), `docs/Project50/**` (**P50 owns it**), `Disassembled/**`,
   `docs/IR.md`, `docs/Salvage.md` (recommend corrections in the report),
   `docs/Project44/DESIGN.md`, any artifact.
3. **Nothing compiles, nothing loads, nothing runs.** If you are invoking
   `lower_c.py`, you have left the project.
4. If a routine needs a fact nobody has established, that is a
   **STOP-and-report**, not a guess.

---

## Coordination — questions and the plan gate (BINDING)

Write `docs/Project51/q00N-short-title.md`, commit, **push to main**, then
**STOP and tell the user it is there.** Do not work ahead while a question is
outstanding. You own `q*.md`; the integrator owns `a*.md`. Every question
states what you found, the decision needed, the options with your read, and
**your RECOMMENDATION**. SOP: `docs/INTEGRATOR.md` §10.

## Delivery

**Push to `p51-candidate-c` at every stage boundary.** One `Work.tgz` with
the final report.
