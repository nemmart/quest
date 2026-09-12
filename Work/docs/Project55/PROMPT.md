# Project 55 — BLOCK STRUCTURE: MEASURE BEFORE MATCHING

## GOAL

Compare the **block structure** of our compiled IR against the book's, for the
seven routines, and say what the differences are made of.

This is the coarsest possible signal and it comes first deliberately. **If
block structure does not line up, statement-level diffing is noise** — every
statement in a misaligned block reads as a difference. So before anyone writes
a rewrite rule, measure the shape.

**Success = `docs/Project55/BlockCensus.md`:** per routine, our block count
against the book's, and **every difference classified**. Plus a verdict on
whether the canonical block form (DESIGN §6) is roughly right or wrong.

**No rewrites are built in this project. No oracle format is designed.**
Measure, classify, recommend.

---

## Context of record

| path | why |
|---|---|
| `docs/Project44/DESIGN.md` | §6 (block structure fixed by lowering; merge/split are rewrites; the canonical form is revisable **only on a book-wide census**), §7.2a (the escalation order and the rule/application split) |
| `docs/Project53/REPORT.md` | what the compiler emits and its eight findings |
| `docs/Project51/REPORT.md` | the seven routines, their derivations and confidences |
| `compiler/lower_c.py` | the emitter — §1.5's control-flow lowering is what you are measuring |
| `emulation/quest.blocks.split` | **the book's block partition.** The target |
| `emulation/quest.ir2.book` | the book. **Read for COMPARISON only** |
| `docs/IR.md` | ir 8 |
| `compiler/ircmp.py` | the old comparator. Assess whether it serves; it has no slot bijection |

**Do not read `docs/attic/`.** Its loop rules are void and reading them will
bias the census — which is the one thing this project must not be.

---

## Carried-in rulings

1. **The canonical form is revisable only on a BOOK-WIDE CENSUS, never to
   close one routine's diff** (DESIGN §6). This is the rule the whole project
   turns on.
2. **Suspect the C first** (DESIGN §7.2a). A block difference is more likely
   our misreading than a compiler behaviour.
3. **Two activities must not be confused**, and the distinction is the
   project's main intellectual task:
   - **restructuring the C because our reading was wrong** — legitimate
   - **restructuring the C to make blocks line up** — **fitting**, unless the
     restructure is independently justified by the disassembly
   The test: *does the change make the C a better READING of what the routine
   does, or only a better match?*
4. **Do not read `quest.ir2.book` to decide what the C should say.** Read it
   to compare. If a difference makes you want to change the C, the
   justification must come from `quest.dis` and the routine's own logic.

---

## The measurement

For each of the seven: our block count and the book's, and for each
difference a **classification**:

| class | meaning | what it implies |
|---|---|---|
| **merge** | the book has one block where we have two, joinable by DESIGN §6's CFG rule (sole successor / sole predecessor) | a rewrite, cheap |
| **split** | the book has two where we have one | a rewrite, cheap |
| **polarity** | same blocks, `goto [X,Y] c` vs `goto [Y,X] !c` | not a structural difference at all (P46 F5) |
| **statement placement** | the same computation sits in a different block | **the expensive one — see below** |
| **our C is wrong** | the routine does something other than what we wrote | a finding, and the most valuable outcome |

### Statement placement deserves its own treatment

A statement in a different block is **not** a reordering. Reordering within a
block needs a dependence check; moving a statement across a block boundary
changes **which paths execute it**, so its precondition is that the statement
executes on all paths through both blocks. That is code motion, it is the
heaviest precondition in the design, and it is the rewrite class most likely
to match while being wrong.

**So where a placement difference can be removed by writing the C
differently, that is strongly preferred to a rewrite** — provided ruling 3's
test is met.

---

## The loop-shape census — do this properly, it has burned this project before

**Nobody on this project knows how DG PL/I emits a `DO` loop.** Where the
increment goes, where the test goes, whether the entry test is separate.

The attic's most-refitted rules were the loop rules — hoisting and loop
registers and DO limits, four amended in a single project — **because they
were inferred from one or two routines instead of counted across the book.**
Do not repeat that.

So: **census `XNDO`/`XWDO` loop shape across the whole book**, not just the
seven. Report the distribution. If every loop has one shape, that is a fact
with hundreds of witnesses. If the shape varies, say how and with what.

Then say whether our `for` lowering matches it. If it does not, recommend —
**do not build** — the change. One option already discussed: a `DO(i, lo, hi)`
macro in `quest_rt.h` expanding to whatever C shape lowers correctly, so the
source *says* PL/I DO loop, both views still compile, and `gcc` remains the
differential oracle. **A grammar extension to `lower_c.py` is NOT acceptable**
— it would stop the input being C and cost us the oracle, which is the entire
soundness argument.

---

## Part 1 — PLAN GATE

`docs/Project55/q001-plan-gate.md`, push to main, **STOP**. Report:

1. **How you will compare** — does `ircmp.py` serve, or do you need something
   simpler? Note DESIGN §5.2: the book spells locals `wp(r, d)` and ours are
   absolute, so the slot bijection is unbuilt. **For block structure you may
   not need it** — say whether you do.
2. **A first count for two or three routines**, roughest possible, so the
   shape of the answer is visible before budget is committed.
3. **The loop census method.**
4. **Anything in DESIGN §6 that looks wrong** now that there is real output to
   compare.

STOP. Wait for `a001`.

---

## Part 3 — Report

`docs/Project55/BlockCensus.md` and `REPORT.md`:

- the per-routine table and the **classification counts**
- **the verdict on the canonical form**: roughly right (a few merges) or
  wrong (systematic divergence)? This is what the project is for
- the loop-shape census and whether our `for` lowering matches
- **every place you think our C is wrong**, with the disassembly evidence —
  these are the most valuable lines in the report
- **every place you were tempted to change the C to make blocks line up and
  did not**, with why. Recording the temptation is worth as much as resisting
  it
- your recommendation for P56: which rewrites the census implies, with
  application counts

---

## Boundaries — BINDING

1. **You may WRITE:** `docs/Project55/**`, and analysis tooling in
   `compiler/` (new files only).
2. **Do NOT modify** `lower_c.py`, `game/**`, `emulation/**`, `docs/IR.md`,
   `docs/Project44/DESIGN.md`, or any artifact. **This project changes
   nothing; it measures.**
3. If the census says the canonical form should change, that is a
   **recommendation in the report**, not an edit.
4. Nothing executes. You are comparing two texts.

---

## Coordination

Write `docs/Project55/q00N-short-title.md`, commit, **push to main**, then
**STOP and tell the user it is there.** You own `q*.md`; the integrator owns
`a*.md`. Every question states what you found, the decision needed, the
options with your read, and **your RECOMMENDATION**. SOP:
`docs/INTEGRATOR.md` §10.

## Delivery

**Push to `p55-blockcensus` at every stage boundary.** One `Work.tgz` with the
final report.
