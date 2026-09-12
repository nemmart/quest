# Project 55 — q001 PLAN GATE

Worker session, Sep 12 2026, branch `p55-blockcensus`. Nothing was modified;
one new file, `compiler/blockcensus.py` (~130 lines, read-only measurement).
`docs/attic/` was not opened. `quest.ir2.book` was opened for COMPARISON only
and nothing in it was used to decide what any `.c` should say.

**The headline, up front, because it changes the project's premise:**
`quest.blocks.split` and `quest.ir2.book` do **not** carry the same partition
— 18,009 blocks against 13,507, a 25% fold. The prompt names `blocks.split`
as "the book's block partition. The target". Measured against `blocks.split`
our lowering looks 1.7–2.5× too coarse; measured against `ir2.book` five of
seven routines are within ±5 and two match exactly. **Q1 asks which is the
target.** Every number below is given both ways.

---

## 1. How I will compare — `ircmp.py` does not serve, and the slot bijection is not needed

**The slot bijection is not needed. Nothing in a block census is a local.**
A block is identified on the book side by its head address and on ours by its
symbolic `<ENTRY>.b<digits>` name; the CFG is carried by the `n <succ>` lines
in `blocks.split` and by the `goto [X,Y] c` target lists in the IR. Neither
mentions a frame slot, so DESIGN §5.2's unbuilt `wp(r, d)` ↔ absolute
equivalence is out of scope. This is the one place the project is cheap, and
it is why measuring shape before statements is the right order.

**`ircmp.py` does not serve, for four reasons, and I am not proposing to fix
it:**

1. It parses **ir 6** (its own docstring; `readable.py`'s lexer). The book is
   **ir 7** and our compiler emits **ir 8**. Two version gaps.
2. It is **statement-level**. It reports MATCH/RENAME/DIFF per statement under
   seven equivalences. A census wants counts and classes, and a statement-level
   diff over a misaligned partition is exactly the noise the prompt says to
   avoid generating.
3. It needs the slot bijection (equivalence 2) that §1 just showed is
   unnecessary here — so using it imports a dependency the measurement does
   not have.
4. Its `--routine` path assumes both sides carry book-style numeric block
   addresses; ours are symbolic and unplaced.

**One piece of it is worth keeping and I will reuse the idea, not the code:**
equivalence 1's canonical block order (DFS from the routine entry, successors
in terminator order). That is the right way to put two partitions into
correspondence without either side's names, and Part 2 needs it to classify
merges and splits rather than only count them.

**What I built instead:** `compiler/blockcensus.py`. It reads `quest.addrbook`
for entry ranges, `quest.blocks.split` (or `quest.ir2.book`) for the book's
blocks, and a compiled `.ir` for ours, and prints per-entry counts. Part 2
extends it with the DFS correspondence and the classifier. New file, in
`compiler/`, inside boundary 1.

**Routine extents** come from the addrbook: an entry's range is
`[its address, the next entry's address)`, with commented-out `#` rows counted
as delimiters since they are still code. Checked: for all seven the first
block head equals the entry address exactly, and the last block in range is
well inside it. None of the seven has a nested `.N@` entry, so no
family-folding question arises for this set.

---

## 2. The first count — all seven, because it was cheap

Ours from `compiler/lower_c.py` at HEAD (5 alone, INIT_SCREEN and HIT_ANY_CHAR
in the compilation units P53 §2 requires). All seven compiled clean.

| routine | ours | `blocks.split` | Δ | `ir2.book` | **Δ (ir2)** | DERR sites | loops |
|---|---:|---:|---:|---:|---:|---:|---:|
| GET_INPUT | 5 | 5 | +0 | 5 | **+0** | 0 | 0 |
| HIT_ANY_CHAR | 4 | 4 | +0 | 4 | **+0** | 0 | 0 |
| PICK_X_Y | 20 | 20 | +0 | 18 | **−2** | 1 | 0 |
| UPDATE_SCREENS | 15 | 26 | +11 | 18 | **+3** | 4 | 1 |
| INIT_SCREEN | 27 | 48 | +21 | 32 | **+5** | 8 | 3 |
| FAKE_OCEAN | 47 | 86 | +39 | 60 | **+13** | 13 | 4 |
| FAKE_LAND_MASS | 33 | 84 | +51 | 64 | **+31** | 10 | 2 |

**The `blocks.split` column is almost entirely one effect, and it is not a
divergence.** `split → ir2` folds exactly `2 × (DERR sites)` blocks in every
one of the five routines that has any — 8, 2, 26, 20, 16 against 4, 1, 13, 10,
8 sites. Five for five, no residue. The cause is in IR.md's own ir 3 note:
DERR clusters fold to `assert(cond, "DERR nn @pc"); goto [K] 0` in a guard
block, 2,271 of 2,273 embeds gone. The machine spells a subscript check as a
three-block skip chain (two skip tests and a `DERR 17` block); the book's IR
already collapses it to one block holding an `assert`; **and `lower_c.py`
emits precisely that `assert` for `RANGE_CHECK` already.** The three
partitions are the machine's, the book IR's, and ours — and ours agrees with
the book IR's on this construct.

So measuring against `blocks.split` charges us ~105 blocks across the seven
for a construct where we are already right. That is the whole of the apparent
1.7–2.5× divergence for four of the five routines.

**What is actually left, against `ir2.book`:** two exact matches, one routine
where we are two blocks *coarser* than the book wants (PICK_X_Y, −2 → two
splits), two within +5, and **FAKE_LAND_MASS at +31, which is the real
finding and I cannot yet explain it.**

**My leading hypothesis for FAKE_LAND_MASS, offered as a hypothesis:** P51
§2.2 records that FAKE_LAND_MASS is the one routine whose booleans are
*materialised* (`WIOR`, `WIOR`, `WAND`, `MOV.L# 0,0,SZC`) rather than emitted
as jumps, at three sites. A materialised boolean still builds its 0/1 operands
out of **skip** instructions, so at the machine level it is skip-dense; and the
ir 2 lifting folds DERR chains only, not comparison chains. Our IR has pure
`|`/`&` operators and no branching there at all. 50 of FAKE_LAND_MASS's 84
`blocks.split` blocks are single-instruction, only 9 of them `WBR`. That fits,
but I have not tested it and it is not in the report until I have.

**Note what nearly misled me.** PICK_X_Y is 20/20 against `blocks.split` — an
exact count match — and is **−2** against the actual target, meaning it has
blocks we lack and we have blocks it lacks. Count equality is not structural
equality. §4 item 2 returns to this.

---

## 3. The loop census — method, and it is already done for the shape question

The prompt is right that this has burned the project, so I did the book-wide
census first rather than reasoning from the seven.

**Method, as run:**

1. Extract every block in `quest.blocks.split` whose instruction list contains
   an `XNDO`/`XWDO`. **206 sites** (176 `XNDO`, 30 `XWDO`).
2. Record, per site: the instruction's position in its block, the block's
   instruction count, the first instruction's opcode, and the successor pair.
3. Identify the **preheader** properly — as the unique CFG predecessor of the
   loop body that is not the DO block itself and sits at a lower address —
   rather than by address adjacency. (Address adjacency gave 191/206 and 15
   apparent exceptions; all 15 were the adjacency heuristic failing, not a
   second shape. Recording that because it is the exact error the attic made.)
4. Report the distribution.

**Result — one shape, 206 witnesses, no exceptions:**

| fact | count |
|---|---|
| DO instruction is the block's **last** instruction (a 2-way terminator) | **206 / 206** |
| DO block has exactly 2 successors `[body, exit]` | **206 / 206** |
| DO block is 2 instructions (`<load limit>; XNDO`) | 192; the other 14 are 3 |
| first instruction is a limit **load** (`NLDAI` 124, `XNLDA` 53, `XWLDA` 29) | **206 / 206** |
| a **unique preheader** exists, at a lower address, ending in an unconditional jump **into the body**, over the DO block (`WBR` 204, `XJMP` 2) | **206 / 206** |

The canonical DG PL/I `DO` is therefore:

```
preheader:  ... ; cv = lo ;  WBR -> body        (jumps OVER the DO block)
DO block:   <load limit> ; XNDO cv, disp, limit  -> [body, exit]
body:       ... ; WBR -> DO block                (back edge; `continue` too)
```

Increment and test are **one instruction in one block, entered only on the
back edge**. The limit is **reloaded every iteration** (P51's P4 note, now with
206 witnesses instead of two). The entry test is **separate and outside the
loop**: in UPDATE_SCREENS it is `WSLE 1,0` in the entry block, whose false leg
is the exit; where the bounds are statically decidable (`i = 1 TO 2`) there is
no entry test at all. **Whether that correlation is exact is the one loop
question I have not closed**, and it is Part 2 work: classify all 206 by
whether `lo ≤ hi` is decidable at compile time and check it against entry-test
presence.

**Our `for` lowering does not match, and — this is the load-bearing part —
the gap is NOT closeable by merge or split.** Ours is the textbook while
shape: `for/head` (test) → `for/body` → `for/step` → back to `for/head`. The
book's CFG has a `preheader → body` edge that ours does not have, and our
`for/head` has two predecessors (entry and step) so §6's merge precondition
(sole predecessor) can never fire on it. Merge and split preserve the edge
set; this needs **loop rotation**, which is an edge-changing rewrite and is
not in §6's pair. Hence Q2.

---

## 4. What looks wrong in DESIGN §6, now that there is output to compare

Four things, in the order I would want them ruled on.

1. **§6 names a target that is not the thing we must match.** "`quest.blocks.split`
   holds the book's partition, so a count mismatch surfaces immediately" — but
   obligation (b) in §2 is that our IR equals *the book's IR*, and the book's
   IR is `quest.ir2.book`, which has 4,502 fewer blocks. `blocks.split` is an
   **input** to `ir2.book` (it is listed with a sha256 in `ir2.book`'s own
   header), not the target. Using it as the tripwire makes the tripwire fire
   on 25% of the book for free. This is Q1.

2. **§6 makes block COUNT the signal, and count is the wrong signal.**
   PICK_X_Y is 20/20 against `blocks.split` and −2 against `ir2.book`. An
   exact count match can sit on top of a structural mismatch in both
   directions. §6's sentence should say the census is over the **CFG**, and
   the tripwire should be the canonical-DFS correspondence, not the integer.

3. **§6's merge/split pair is not closed under the differences that exist.**
   The loop shape (§3) needs rotation; no sequence of merges and splits
   produces it, because both preserve edges. §6 says the canonical form is
   revisable on a book-wide census — I have one, with 206 witnesses — so the
   design anticipated this. But as written, §6 presents merge and split as
   *the* block rewrites, and a reader would conclude the loop gap is cheap. It
   is not; it is either a lowering change or a third rewrite.

4. **§6 assumes the two partitions are the same kind of object.** They may not
   be. The book's is a **machine** partition — every Eagle skip instruction is
   a 2-way terminator, so a skip chain becomes a run of 1-instruction blocks
   (50 of FAKE_LAND_MASS's 84). Ours is a **source-construct** partition. The
   ir 2 lifting already folds one family of skip chains (DERR) and not others
   (materialised booleans). If the FAKE_LAND_MASS hypothesis in §2 holds, the
   residual divergence is entirely "branchless in ours, skip chain in the
   book", which is the same shape as the DERR case and is a *lifting*
   question, not a rewrite question. **I am not asserting this** — it is the
   thing Part 2 is for — but §6 does not currently have a class for it and the
   integrator should know it is coming.

---

## Decisions needed

### Q1 — Which partition is the target: `blocks.split` or `ir2.book`?

**What I found.** They differ by 4,502 blocks (18,009 → 13,507). For the seven,
the entire difference is `2 × DERR sites`, five routines for five, and it is
a construct where our `assert` lowering already agrees with the book's IR.
Against `blocks.split` the verdict reads "canonical form wrong, systematic
1.7–2.5× divergence". Against `ir2.book` it reads "roughly right for five of
seven, one outlier".

**Options.**

- **(a) `ir2.book` is the target; `blocks.split` is provenance.** Matches
  obligation (b). Costs nothing — `blocks.split` stays available and Part 2
  reports both columns. Risk: `ir2.book` is a *lifted* artifact, so a lifting
  bug becomes a target we match to. Mitigated by the fact that this project
  only measures.
- **(b) `blocks.split` is the target, per the prompt as written.** Faithful to
  the prompt. But it charges ~105 blocks across the seven against a construct
  we get right, and it would send P56 to build rewrites that split our
  `assert` back into a three-block skip chain the book's own IR does not have.
- **(c) Report both, decline to rule.** Honest, and the tables support it. But
  the project's deliverable is "a verdict on whether the canonical block form
  is roughly right or wrong", and that verdict is *opposite* under the two —
  so declining is declining to deliver.

**RECOMMENDATION: (a).** `ir2.book` is what §2(b) obliges us to equal;
`blocks.split` is an input to it, named with a sha256 in its header. Part 2
reports both columns in `BlockCensus.md` so the `blocks.split` view is never
lost, and the verdict is stated against `ir2.book`. If the integrator prefers
(b), say so now — it changes every classification and roughly triples Part 2.

### Q2 — The loop shape: lowering change, source macro, or a third rewrite?

**What I found.** One shape, 206/206, no exceptions (§3). Our `for` does not
match it, and merge/split cannot bridge it.

**Options.**

- **(a) Change the canonical `for` lowering in `lower_c.py`** (as a P56
  recommendation): emit `if (entry test) { preheader; do-body-while(step+test) }`.
  DESIGN §6 sanctions exactly this — "the canonical form is revisable… on a
  book-wide census" — and this is a book-wide census. Loop rotation is an
  ordinary C compiler transformation, so nothing about PL/I leaks into the
  compiler. Soundness is checked by the **existing** differential rig against
  gcc, which is the only one of the three options with that property. The
  input stays C; the oracle survives.
- **(b) A `DO(i, lo, hi)` macro in `quest_rt.h`** (the prompt's option). No
  compiler change; the source then *says* PL/I DO loop. `goto`/`Label` are
  already in the subset, so the macro has something to expand to. But it
  touches every source file with a loop, and under ruling 3 it is a change to
  the C whose justification would be "the blocks line up" unless the
  book-wide census is accepted as the independent justification — which I
  think it is, but it is the weaker footing of the two.
- **(c) Add loop rotation as a third block rewrite.** Keeps lowering and source
  untouched. But §7.2a's tripwire says a new rewrite is the *third* step, after
  the C is exonerated, and here the C is not the problem — the lowering is. It
  also pays the cost on every loop in the program forever.

**RECOMMENDATION: (a), with (b) as fallback.** Both are defensible under
ruling 3 because the justification is a 206-witness book-wide census and not a
diff on one routine. (a) is better because it changes one function in one file
instead of fifteen source files, and because its soundness is established by
the rig that already exists rather than by argument. Explicitly **not** a
grammar extension to `lower_c.py` — the input stays C either way and gcc
remains the oracle.

### Q3 — May Part 2 read `Disassembled/quest.dis` for the FAKE_LAND_MASS outlier?

**What I found.** +31 against `ir2.book`, three times the next worst, with a
hypothesis (§2) that needs the machine text to test. Ruling 4 requires that
any pressure on the C be justified from `quest.dis` and the routine's own
logic, not from `ir2.book` — so the disassembly is the *sanctioned* source
here, but it is not in the prompt's context-of-record table and I will not
open it unasked.

**Options.** (a) grant it, read-only, FAKE_LAND_MASS's range only;
(b) grant it for all seven; (c) withhold, and report the outlier unexplained.

**RECOMMENDATION: (a).** The narrow grant is enough to test the hypothesis and
keeps the census from becoming a re-derivation. If the hypothesis holds it is
a lifting finding, not a C finding, and no `.c` file is touched either way —
which is the outcome I expect and would report as such.

---

## Temptations recorded (prompt Part 3, gathered as they occur)

1. **Rewriting the seven's `for` loops as `goto`/label chains.** `Goto` and
   `Label` are already supported by `lower_c.py`, so I could have matched the
   book's loop CFG today, in the source, with no compiler change and no
   ruling. I did not. It is the textbook fitting move: it makes the blocks
   line up and makes the C a *worse* reading, since the original says `DO`
   and a goto chain says less than a `for` does. Q2 exists so that the shape
   change, if it happens, is justified by the 206-witness census and applied
   uniformly by the compiler rather than hand-written per routine.
2. **Splitting PICK_X_Y's two extra blocks to reach 18/18.** Two blocks from
   an exact match on the one routine with a 64/64 statement match behind it
   (P51). Not touched; −2 is a datum, and which two blocks and why is Part 2's
   classification job.

---

## Sizing

Part 2, under Q1(a) and Q3(a): extend `blockcensus.py` with the canonical-DFS
correspondence and the classifier (+~200 lines), classify seven routines,
close the loop entry-test question over 206 sites, test the FAKE_LAND_MASS
hypothesis, write `BlockCensus.md` and `REPORT.md`. Under Q1(b), roughly
triple — every DERR chain becomes a classified difference.

**STOP. Waiting for `a001`.**
