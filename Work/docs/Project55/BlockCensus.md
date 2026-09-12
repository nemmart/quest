# Project 55 — BLOCK CENSUS

Worker session, Sep 12 2026, branch `p55-blockcensus`. Rulings `a001` (Q1(a),
Q2(a)), `a002` (Q3(a)). **Nothing was modified.** Two new read-only tools,
`compiler/blockcensus.py` and `compiler/blockcmp.py`. `docs/attic/` was not
opened.

**Target: `quest.ir2.book`** (a001 Q1(a)). `quest.blocks.split` is provenance
and is reported alongside, as ruled.

---

## 1. The per-routine table

Ours from `compiler/lower_c.py` at HEAD — five alone, INIT_SCREEN and
HIT_ANY_CHAR in the compilation units P53 §2 requires. All seven compiled
clean.

### 1.1 Raw counts

| routine | ours | **`ir2.book`** | **Δ** | `blocks.split` | Δ split | DERR sites |
|---|---:|---:|---:|---:|---:|---:|
| GET_INPUT | 5 | 5 | **+0** | 5 | +0 | 0 |
| HIT_ANY_CHAR | 4 | 4 | **+0** | 4 | +0 | 0 |
| PICK_X_Y | 20 | 18 | **−2** | 20 | +0 | 1 |
| UPDATE_SCREENS | 15 | 18 | **+3** | 26 | +11 | 4 |
| INIT_SCREEN | 27 | 32 | **+5** | 48 | +21 | 8 |
| FAKE_OCEAN | 47 | 60 | **+13** | 86 | +39 | 13 |
| FAKE_LAND_MASS | 33 | 64 | **+31** | 84 | +51 | 10 |
| **total** | **151** | **181** | **+30** | **273** | **+122** | 36 |

`blocks.split → ir2.book` folds exactly `2 × DERR sites` in every routine that
has any — 2, 8, 16, 26, 20 against 1, 4, 8, 13, 10 sites, five for five with no
residue. The cause is IR.md's ir 3 note: DERR clusters fold to
`assert(cond, "DERR nn @pc"); goto [K] 0` in a guard block. **Our compiler
already emits that `assert` for `RANGE_CHECK`.** The entire `blocks.split`
column is therefore a construct on which we agree with the book.

### 1.2 After normalising both sides by merge-to-fixpoint

DESIGN §6's merge — legal when A's only successor is B and B's only
predecessor is A — applied to fixpoint on **both** sides. Merge is the cheap
rewrite; what survives it is a difference merge cannot close.

| routine | our merges | book merges | ours reduced | book reduced | **ΔR** |
|---|---:|---:|---:|---:|---:|
| GET_INPUT | 2 | 2 | 3 | 3 | **+0** |
| HIT_ANY_CHAR | 3 | 3 | 1 | 1 | **+0** |
| PICK_X_Y | 4 | 4 | 15 | 14 | **−1** |
| UPDATE_SCREENS | 0 | 4 | 15 | 14 | **−1** |
| INIT_SCREEN | 4 | 12 | 23 | 20 | **−3** |
| FAKE_OCEAN | 2 | 17 | 45 | 43 | **−2** |
| FAKE_LAND_MASS | 1 | 16 | 32 | 48 | **+16** |
| **total** | **16** | **55** | **134** | **134** | **+0** |

**Merging closes most of it.** The raw spread of +31 to −2 becomes −3 to +16,
and six of seven land within ±3. HIT_ANY_CHAR reduces to a single block on both
sides and is **structurally identical** by canonical-DFS out-degree signature.

The totals agreeing at 134 is a coincidence of summation, not a result — the
per-routine residuals are what matter, and they do not all have the same sign.

---

## 2. Classification

### 2.1 The classes, and two the prompt's table does not have

| class | sites | blocks | verdict |
|---|---:|---:|---|
| **merge** (book coarser than us) | 16 | 16 | cheap, DESIGN §6 as written |
| **split** (book finer than us) | 55 | 55 | cheap, DESIGN §6 as written |
| **polarity** | not counted | 0 | not a structural difference (P46 F5); no attempt made to count it, and none needed |
| **statement placement** | **0** | **0** | **none found.** No difference in the seven required moving a computation across a block boundary |
| **loop shape** ★ | 10 loops | see §2.2 | **not closeable by merge/split** |
| **two-armed vs one-armed conditional** ★ | 17 ours / 20 book | see §2.3 | **not closeable by merge/split** |
| **comparison materialisation** ★ | 8 | **+16** | **not closeable by merge/split**; a rewrite, oracle-driven |
| **our C is wrong** | **0** | 0 | nothing found. See REPORT §4 |

★ = a class DESIGN §6 does not have. All three are edge-changing; merge and
split both preserve the edge set, so none of them is reachable from the other
side by any sequence of §6's two rewrites.

**Statement placement is zero, and that is the most reassuring line in the
census.** The prompt calls it "the expensive one" and warns it is the rewrite
class most likely to match while being wrong. It does not arise here.

### 2.2 Loop shape

Book-wide census, all 206 `XNDO`/`XWDO` sites (176 `XNDO`, 30 `XWDO`):

| fact | count |
|---|---|
| DO instruction is the block's last instruction (a 2-way terminator) | **206 / 206** |
| exactly 2 successors `[body, exit]` | **206 / 206** |
| first instruction is a limit **load** (`NLDAI` 124, `XNLDA` 53, `XWLDA` 29) | **206 / 206** |
| DO block is exactly 2 instructions | 192 (the other 14 are 3) |
| unique preheader at a lower address, ending in an unconditional jump **into the body**, over the DO block (`WBR` 204, `XJMP` 2) | **206 / 206** |

```
preheader:  ... ; cv = lo ;  WBR -> body        (jumps OVER the DO block)
DO block:   <load limit> ; XNDO cv, disp, limit  -> [body, exit]
body:       ... ; WBR -> DO block                (back edge; `continue` too)
```

Increment and test are **one instruction in one block entered only on the back
edge**. The limit is **reloaded every iteration** — P51's P4 note, now with 206
witnesses instead of two.

**Our `for` lowering does not match.** Ours is the textbook while shape:
`for/head` (test) → `for/body` → `for/step` → back to `for/head`. The book's
CFG carries a `preheader → body` edge ours does not have, and our `for/head`
has two predecessors (entry and step), so §6's merge precondition can never
fire on it.

**The entry test — NOT CLOSED, and the reason matters.** Whether the guard's
presence tracks `lo ≤ hi` being statically decidable cannot be settled by the
measurement I built. Two detectors gave two answers (86/206 and 73/206), and
the tight detector — a block whose two successors are exactly the preheader and
the loop's exit — found **zero**, because the guard's not-entered leg lands on
a *separate* block rather than on the loop's own exit (UPDATE_SCREENS: the
guard at 7017D635 exits to 7017D643, while the DO block exits to 7017D6A8; both
are `WRTN`). There is no clean structural marker, so a detector cannot separate
a real guard from an unrelated 2-way block. The correlation under the looser
detector is **115/124 constant-limit loops with no guard** and **64/82
memory-limit loops with one**, which is suggestive and is not a rule.

**This is recorded as open rather than reported as a distribution on purpose.**
A heuristic's output presented as a distribution is the attic's exact failure,
and the adjacency version of this same measurement already produced one false
tail in this project (§4 of the REPORT).

### 2.3 Two-armed vs one-armed conditional

Ours emits both arms of a conditional into a common `v`; the book skips over a
single-instruction arm and mutates in place.

Ours, UPDATE_SCREENS `ABS`:

```
b5 ABS/neg:  ac0 = v17 ; ac0 = 0 - ac0 ; v18 = ac0 ; goto [b7]
b6 ABS/pos:  ac0 = v17 ;                 v18 = ac0 ; goto [b7]      <-- a copy
b7 ABS/join: ...
```

The book, same site: `WSGE 2,2` skips a lone `WNEG 2,2`. Two blocks, no copy.

Per routine, over the merge-reduced graphs:

| routine | our 1-armed | our 2-armed | book 1-armed | book 2-armed |
|---|---:|---:|---:|---:|
| UPDATE_SCREENS | 0 | 3 | 2 | 1 |
| PICK_X_Y | 3 | 0 | 0 | 0 |
| GET_INPUT | 1 | 0 | 0 | 0 |
| HIT_ANY_CHAR | 0 | 0 | 0 | 0 |
| INIT_SCREEN | 0 | 3 | 2 | 1 |
| FAKE_OCEAN | 2 | 6 | 4 | 2 |
| FAKE_LAND_MASS | 0 | 5 | 12 | 1 |

**Book-wide:** of 3,515 two-way blocks in `ir2.book`, **807 are one-armed and
199 are two-armed** — 4:1 — with 2,509 not classifiable by arm length. And for
the compare-against-zero idiom specifically (`WSGE r,r`, IR.md §5.6),
**36 sites, and all 36 skip a ONE-instruction arm**: 14 `WNEG` and 8 `NNEG`
(ABS, narrow and wide), 8 `WSUB` (MAX against 0, per P51 §2 item 7), 4 `WBR`,
one each `WADC`/`WADD`. **None is a two-armed diamond.** Our lowering emits a
two-armed diamond at every one of these sites.

**Correction to my own first pass:** I initially wrote that the book's ABS "is
always 2 blocks, never 3" from 14/36 sites and had to retract it in the same
session — the other 22 are the same *shape* but a different *idiom*. The claim
that survives is about arm length, not about ABS.

### 2.4 Comparison materialisation — the FAKE_LAND_MASS outlier, closed exactly

`+16` post-merge, four times the next worst. **It is 8 sites × 2 blocks, and
the 8 is measured, not inferred.**

The idiom, from `Disassembled/quest.dis` 701697ef (the narrow read a002 Q3(a)
granted):

```
WADC 0,0;      701697ef
WSLE 1,2;      701697f0   <-- a SKIP: a 2-way terminator
WSUB 0,0;      701697f1   <-- the skipped arm, one instruction
```

Each materialised comparison operand is a compare-**skip** over a lone
`WSUB r,r`, building 0/1 into a register. Two blocks in the book; **zero in
ours**, because P48's a001 R2 makes the naive compiler emit pure operators and
our `|` of two comparisons carries no control flow at all.

Counted by the idiom's exact signature (`WADC r,r` / compare-skip / lone
`WSUB r,r`):

| routine | sites |
|---|---:|
| **FAKE_LAND_MASS** | **8** |
| FAKE_OCEAN | 0 |
| INIT_SCREEN | 0 |
| UPDATE_SCREENS | 0 |
| PICK_X_Y | 0 |
| **book-wide** | **36** |

**8 × 2 = 16 = the residual exactly.** The 8 sites are the three P51 §2.2
already recorded as materialised (701697EF..FF, 70169819..2E, 70169840..76),
and FAKE_LAND_MASS is the only one of the seven with the idiom — which is what
P51 said, from reading, before any of this was measured.

**This is not a lowering change and must not become one.** P51 §2.2 measured
that the *same* PL/I `|` on the *same* kind of operands is emitted both ways by
the 1986 compiler — as jumps in FAKE_OCEAN, materialised in FAKE_LAND_MASS.
The choice is not predictable from the source, so it belongs to the oracle,
exactly as register binding does. It is a **rewrite**, 36 applications
book-wide, and it is one rule.

---

## 3. The verdict on the canonical form

**Roughly right on the partition; wrong in three specific lowerings, each with
a book-wide census behind it.**

Not a systematic divergence. Merge and split account for 71 of the differences
across the seven and are the cheap rewrites §6 already sanctions; statement
placement — the expensive class — does not occur at all; two routines match
exactly and a third is structurally identical after merging.

But the residue is not noise, and it is not closeable by §6's pair:

1. **The `for` lowering is wrong**, on 206 witnesses with no exceptions. §6's
   own clause — revisable on a book-wide census, never to close one routine's
   diff — is satisfied. **Recommend changing it** (a001 Q2(a)).
2. **The conditional lowering emits both arms** where the book skips a
   one-instruction arm, 807:199 book-wide and 36/36 for compare-against-zero.
   **Recommend changing it**, on the same clause.
3. **Comparison materialisation is a rewrite, not a lowering change**, because
   the source does not determine it. 36 applications book-wide, one rule.

§6 itself needs four corrections; they are in REPORT §5.
