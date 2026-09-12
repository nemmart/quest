# Project 55 — REPORT: BLOCK STRUCTURE, MEASURED BEFORE MATCHING

Worker session, Sep 12 2026, branch `p55-blockcensus`. Gate
`q001-plan-gate.md`; rulings `a001` (Q1(a), Q2(a)), `a002` (Q3(a)).
The census itself is `BlockCensus.md`; this is what it means and what it cost.

**Nothing was modified.** Written: `docs/Project55/*`,
`compiler/blockcensus.py`, `compiler/blockcmp.py` (new files, boundary 1).
Not touched: `lower_c.py`, `game/**`, `emulation/**`, `docs/IR.md`,
`docs/Project44/DESIGN.md`, any artifact. Nothing executes; this compared two
texts. `docs/attic/` was not opened.

---

## 1. The outcome against the prompt's criteria

| required | delivered |
|---|---|
| per routine, our block count against the book's, **every difference classified** | BlockCensus §1, §2. Seven routines, eight classes, zero unclassified blocks |
| **a verdict on the canonical block form** | BlockCensus §3: **roughly right on the partition, wrong in three lowerings**, each with a book-wide census |
| the loop-shape census, and whether our `for` matches | BlockCensus §2.2: 206 sites, **one shape, no exceptions**; ours does not match |
| every place our C is wrong, with disassembly evidence | **none found** — §4, and I think that is a real result rather than a failure to look |
| every place I was tempted to change the C and did not | §6, three of them |
| a recommendation for P56 with application counts | §7 |

---

## 2. The prompt's premise was wrong, and finding that out was most of the value

The prompt named `quest.blocks.split` "the book's block partition. The target."
It is not the target; it is an **input** to the lifting that produced the book,
named with a sha256 in the book's own header. The two differ by 4,502 blocks
(18,009 → 13,507).

For the seven, the entire difference is `2 × DERR sites` — five routines for
five, no residue — and it is a construct where **our `assert` lowering already
agrees with the book's IR.** Measuring against `blocks.split` would have
charged us ~122 blocks for being right, and sent P56 to build rewrites that
split our `assert` back into a three-block skip chain the book's own IR does
not have.

The verdict is *opposite* under the two partitions: "canonical form wrong,
systematic 1.7–2.5× divergence" against `blocks.split`, "roughly right for five
of seven" against `ir2.book`. a001 ruled `ir2.book`. Both columns are in the
census, as ruled.

**Why this was catchable at all:** the prompt asked for a rough count at the
gate, before budget was committed. The rough count is where the discrepancy
showed. A project that had gone straight to classification would have spent
its budget classifying 122 blocks of nothing.

---

## 3. What the differences are made of

The one-line version: **merge and split account for most of it; three classes
account for the rest; and none of those three is reachable by merge or split.**

Normalising both sides by merge-to-fixpoint turns a raw spread of +31…−2 into
−3…+16, with six of seven inside ±3 and HIT_ANY_CHAR structurally identical.
What survives:

- **loop shape** — 206/206 witnesses for a shape ours does not emit
- **two-armed vs one-armed conditional** — 807:199 book-wide; 36/36 for
  compare-against-zero
- **comparison materialisation** — 8 sites in FAKE_LAND_MASS, `8 × 2 = 16`
  blocks, exactly the residual; 36 book-wide

**Statement placement — the class the prompt calls the expensive one and warns
is most likely to match while being wrong — does not occur.** Zero sites across
the seven. That is worth more than the three findings above, because it means
no rewrite P56 builds from this census needs the all-paths precondition.

### 3.1 Merge-to-fixpoint was the right instrument, and here is the argument

Counting blocks says how many differ, not what the difference is. Aligning
block-to-block needs a correspondence that does not exist when the counts
differ. Normalising **both** sides under §6's merge rule and comparing what
survives answers the actual question — *is this difference cheap?* — because
merge is by construction the cheap rewrite. If the reduced graphs agree, the
difference was merge/split and nothing else.

It also produces the merge/split counts as a byproduct rather than as a
separate estimate, and it needs no slot bijection (q001 §1) because nothing in
it reads a statement.

---

## 4. Where our C is wrong: nowhere, and why I believe that

The prompt calls these "the most valuable lines in the report" and I have none.
Three things make me think that is a result rather than a miss:

1. **The one routine that looked like a C problem was not one.** FAKE_LAND_MASS
   at +31 was the project's only real outlier and the obvious hypothesis was
   that our reading was wrong. It closed exactly — 8 idiom sites × 2 blocks —
   against a machine idiom, with the C's `|` being the correct reading and
   P51's §2.2 having already recorded the three sites from reading alone.
2. **Statement placement is zero.** If our C said something structurally
   different from the routine, computations would sit in different blocks.
   None do.
3. **The residues are uniform, not per-routine.** A wrong reading is local; a
   wrong lowering is systematic. All three surviving classes are systematic —
   every loop, every conditional, every materialised comparison — which is the
   signature of the compiler, not of the C.

This is the escalation order of DESIGN §7.2a running in the direction it was
designed to run: suspect the C first, and the C was exonerated by measurement
rather than by assertion.

**The residual risk, stated:** `ir2.book` is a *lifted* artifact, so a lifting
bug becomes a target we match to (a001). Nothing in this project is exposed to
that — it only measures — but P56 is, and the exposure is largest exactly where
this census says "the book branches and we do not", since that is a claim about
the lifting's block partition.

---

## 5. DESIGN §6 — four corrections

1. **§6 names the wrong target** (§2 above). `blocks.split` is provenance.
2. **§6 makes block COUNT the signal, and count is the wrong signal.**
   PICK_X_Y is 20/20 against `blocks.split` and −2 against the target: an exact
   count match sitting on a mismatch in both directions. The tripwire should be
   the merge-reduced CFG, not the integer.
3. **§6's merge/split pair is not closed under the differences that exist.**
   All three surviving classes are edge-changing; merge and split both preserve
   the edge set. As written, a reader concludes the loop gap is cheap. It is
   not.
4. **§6 assumes both partitions are the same kind of object.** The book's is a
   **machine** partition — every Eagle skip is a 2-way terminator, so a skip
   chain becomes a run of 1-instruction blocks (50 of FAKE_LAND_MASS's 84 in
   `blocks.split`). Ours is a source-construct partition. The ir 2 lifting
   already folds one family of skip chains (DERR) and not others. **The
   hypothesis I carried into Part 2 — that the residual divergence would be
   mostly this — was wrong**: after the DERR fold the remaining machine-skip
   effect is 8 sites in one routine, not a systematic term. Recorded because I
   flagged it at the gate as the thing Part 2 was for, and Part 2 said no.

---

## 6. Temptations recorded

The prompt says recording the temptation is worth as much as resisting it.

1. **Rewriting the seven's `for` loops as `goto`/label chains.** `Goto` and
   `Label` are already in the subset, so I could have matched the book's loop
   CFG in the source, today, with no compiler change and no ruling. It is the
   textbook fitting move: blocks line up and the C becomes a **worse** reading,
   since the original says `DO` and a goto chain says less than a `for` does.
   Declined; it became Q2, and the shape change is now justified by a
   206-witness census and applied uniformly by the compiler.
2. **Splitting PICK_X_Y's two extra blocks to reach 18/18.** Two blocks from an
   exact match on the routine with a 64/64 statement match behind it (P35).
   Declined; −2 is a datum. It turned out to be one merge-class block and one
   loop-shape block, which is worth more than the round number.
3. **Deleting our `ABS/pos` copy block to match the book's one-armed skip.**
   The most tempting of the three, because the block *looks* redundant — it
   holds `v18 = v17` and nothing else. Declined, and the reason is the
   interesting part: **it is redundant only after copy coalescing, and it is
   not removable by merge even then**, because its successor has two
   predecessors. Treating it as a local tidy-up would have hidden a systematic
   lowering finding (807:199 book-wide) behind one routine's diff — which is
   the precise thing §6's census clause exists to prevent.

---

## 7. Recommendation for P56

**Two lowering changes, one rewrite, one tool decision.** Application counts
are over the whole book where I have them, and over the seven where noted.

| # | change | kind | applications |
|---|---|---|---|
| 1 | `for` → guarded do-while shape (preheader jumps into body; increment+test in one block on the back edge; limit reloaded) | **lowering** (DESIGN §6 census clause) | 206 loops book-wide; 10 in the seven |
| 2 | conditional with a short arm → one-armed skip rather than a two-armed diamond | **lowering** (same clause) | 807 one-armed sites book-wide; 36/36 for compare-against-zero; 17 two-armed diamonds in the seven |
| 3 | comparison materialisation (`|`/`&` of comparisons → compare-skip building 0/1) | **rewrite**, oracle-driven | 36 book-wide; 8 in the seven, all FAKE_LAND_MASS |
| 4 | block merge / block split | **rewrite**, DESIGN §6 as written | 16 merges + 55 splits over the seven |

**On #1 and #2 being lowering changes rather than rewrites.** Both are ordinary
C compiler transformations — loop rotation and one-armed branch emission — so
nothing about PL/I leaks into the compiler, the input stays C, and **gcc
remains the differential oracle**, which is the whole soundness argument. A
grammar extension to `lower_c.py` is not proposed and would not be acceptable.
Both are sanctioned by §6's book-wide-census clause and by nothing else; neither
may be justified from a routine's diff.

**On #3 NOT being a lowering change.** P51 §2.2 measured the same PL/I `|` on
the same kind of operands emitted both ways by the 1986 compiler — jumps in
FAKE_OCEAN, materialised in FAKE_LAND_MASS. The source does not determine it,
so it belongs to the oracle, like register binding. Making it a lowering rule
would be inventing a 1986 compiler's mind, which is the move §7.2a forbids.

**Rule count: 4 changes, of which 2 are lowering and 2 are rewrites.** Against
§7.2a's ~20-rewrite target that is two entries, with application counts in the
hundreds. One rule applied 206 times is a compiler model working.

**Sizing note for P56: `ircmp.py` parses ir 6, the book is ir 7, we emit ir 8.**
Two version gaps in the tool P56 will want for statement-level work, plus the
slot bijection (DESIGN §5.2) which is still unbuilt and which statement-level
comparison — unlike this census — does need.

### 7.1 Left open, deliberately

**The loop entry test.** Whether the guard tracks `lo ≤ hi` being statically
decidable is not settled (BlockCensus §2.2). It matters for #1: our lowering
cannot know whether `lo ≤ hi`, so it must emit the guard unconditionally and
will emit one where the book did not in ~115 of 206 loops. Either constant
folding removes it or it is a per-loop residual P56 should expect. **Settling
it needs a real dominator analysis, not the adjacency-and-predecessor
heuristics used here.**

---

## 8. Method notes — two heuristics that lied, both caught

Recorded because the prompt's warning is about exactly this failure mode.

1. **Address adjacency for the loop preheader gave 191/206 with 15
   exceptions.** All 15 were the heuristic failing, not a second shape: the
   block physically preceding the DO block was an unrelated jump-out. Finding
   the preheader properly — the unique CFG predecessor of the body at a lower
   address — gave **206/206**. A 191/206 result would have looked like a real
   distribution with a tail, and a tail is exactly what invites a rule with two
   witnesses. This is the attic's error and it was one measurement away.
2. **"The book's ABS is always 2 blocks" — asserted from 14/36 sites,
   retracted in the same session.** The other 22 sites are the same *shape*
   with a different *idiom* (`NNEG`, and `WSUB` for MAX-against-0). The claim
   that survives is about arm length, not about ABS, and it is stronger for
   being narrower: 36/36.

Both were caught by checking a number before writing it down rather than after.
Neither reached a document. The general form: **a census is only as good as its
detector, and the detector needs its own witness count.**

## 9. Files

Branch `p55-blockcensus`; `q*.md` also on main. Written:
`compiler/{blockcensus,blockcmp}.py`,
`docs/Project55/{q001-plan-gate,BlockCensus,REPORT}.md`.

**To re-run the census:**

```
compiler/blockcensus.py --ir <compiled.ir> [--routine NAME ...]
compiler/blockcmp.py   --pair ENTRY=<compiled.ir> [--pair ...]
```

Compiling the seven is P53's line: `compiler/lower_c.py <sources> --routine A
--entry A [...]`, with INIT_SCREEN and HIT_ANY_CHAR in multi-routine units.
