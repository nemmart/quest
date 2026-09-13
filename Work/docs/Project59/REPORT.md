# Project 59 — REPORT (Sep 13 2026)

**Site:** DISPLAY_INVENTORY, WCMV at `70167F05` (block `70167EF9`, statement 7),
`[@ac2, ac0] = [@0x701673AD:0, ac1]`, P58 tier `unknown`, reason `register`,
both operands.

**Verdict: PROVEN, by dataflow over a closed CFG.** At the site
**`ac0 = ac1 = 3` on every path** — not merely non-negative. The length word at
frame slot 6 is one of `{8, 11, 13, 14, 16}` when the site runs; the clamp
`30 − len` is therefore in `{22, 19, 17, 16, 14}`, the guard `3 ≥ 30 − len`
is never true, and the `ac1 = 3` arm is always taken. The CFG edge that skips
that arm (`70167EEF → 70167EF9` directly) is infeasible.

Tool: `compiler/onesite.py` (new file; reuses `countflow.py`'s parser, CFG
builder and dominators unchanged). Console output of record:
`docs/Project59/onesite.out`. Nothing executed; no artifact touched.

---

## 1. Both halves, re-derived from the book

### 1.1 The reduction — right on the range that matters, with one corner

Block `70167EEF` (verified against `emulation/quest.ir2.book` lines
31968–31973; the prompt's transcription is exact):

    ac2 = wp(ac3, 6)
    ac1 = 30 ; ac0 = 3
    ac1 = nsub(ac1, M16[wp(ac2, 0)])          ; 16-bit 30 − len, sign-extended (IR.md :483)
    goto [70167EF8, 70167EF9] (ac0 >=s ac1)   ; WSGE 0,1

`70167EF8` is `ac1 = ac0 ; goto [70167EF9] 0`. Label order: false = 0 / true = 1
(IR.md :149). Signedness: `EagleCompute.cpp:198` casts both operands to
`int32_t`, and `WUSGE` exists and was not used. So `ac1 = min(3, 30 − len)`
and the site's `ac0 = ac1` (block `70167EF9` statement 6).

`ac1 ≥ 0 ⟺ len ≤ 30` holds for `len ∈ [−32737, 32767]`. The one corner: for
`len < −32737` the 16-bit `nsub` wraps positive. Irrelevant here (len is
never negative) but the reduction as stated in the prompt is not an identity
over all 16-bit values.

Three independent corroborations of the operand order `30 − len`: the IR.md
definition, P58's own trace (`nsub(0x1E, {…})`), and the lifter's fold of the
first two appends' destination to `bp(ac3, 22)` = byte 14 + 8, i.e. it too
read len = 8 there.

### 1.2 `len ≤ 30` — and in fact `len ≤ 16` at the site

**What slot 6 is.** A `CHAR(30) VARYING` local: length word at word 6, data
at bytes 14..43 (words 7..21). Evidence: every clamped append in the routine
clamps against 30; the largest value the length ever reaches on any path is
22; and the next slot the routine uses is word 22 (`M16[wp(ac3, 22)]` at
`701674EC`), exactly where a 30-byte varying at word 6 ends. Slot 6 is reused
for unrelated strings elsewhere in the routine (object names in the
inventory loop, "North"/"South"/"East"/"West" near the end) — that is one
variable, not one string.

**The dominating assignment.** Block `70167E6E`:

    [@wp(ac3, 6), 8 varying] = [@0x701673B7:0, "Rings: \x0B"]

writes `len = 8`. It **dominates** the site (dominator tree from the WSAVS
entry `701674B5` over the whole 505-block routine). Every path to the site
passes it, and after its last visit stays inside the set of blocks on paths
`70167E6E → 70167EF9`. That set is 27 blocks, **acyclic**, and contains
**no call, no rt_call, no raw instruction, and no memory write outside four
recognised appends** (onesite.out §4). Its predecessors are `70167E5B` and
`70167E6D` only — both outside the region.

**The appends.** Five instances of one compiler idiom, `s = s || piece` into a
CHAR(30) VARYING:

| head | piece (literal) | k | dest as the lifter folded it |
|---|---|---:|---|
| `70167E8E` → `70167E98` (site `70167EA4`) | `"Ir "` @`701673B5` | 3 | `bp(ac3, 22)` — len was 8 |
| `70167EA7` → `70167EB1` (site `70167EBD`) | `"\x1CIr\x1D "` @`701673B1` | 5 | `bp(ac3, 22)` — len was 8 |
| `70167ECB` → `70167ED5` (site `70167EE1`) | `"Tr "` @`701673AF` | 3 | `@ac2` |
| **`70167EEF` → `70167EF9` (site `70167F05`)** | **`"On "` @`701673AD`** | **3** | **`@ac2`** |
| `70167F13` → `70167F1D` (site `70167F29`) | `"Ri "` @`701673AB` | 3 | `@ac2` |

(Literal text from `Disassembled/quest.dis` line 30538–30540; the first two
are mutually exclusive arms of one bit test, which is why both fold to the
same destination.)

Each append is exactly: `ac1 = min(k, 30 − len)`; `len' = len + ac1`;
copy `ac1` bytes to `bp(ac3, 14) + len`. Algebraically
`len' = min(len + k, 30)` — **the same capacity clamp as `assign_varying`'s
`min(len, cap)`**, in concatenation form. Two consequences:

1. **the length word is never written by the copy.** The destination is
   `2·(ac3+6) + 2 + len` bytes = word `7 + len/2` and up; with `len ≥ 0` it
   starts at word 7. The one thing that could put it below word 7 is a
   negative `len`, which is what is being proven — so this is an induction
   from the constant 8, not a free-standing fact;
2. **the invariant `0 ≤ len ≤ 30` is preserved by every append**, given the
   base `len = 8`.

**The exhaustive walk** (onesite.out §4, `--paths` for the listing): 432 CFG
paths from `70167E6E` to `70167EF9`; 352 of them take a clamp arm the running
length forbids (infeasible); the 80 feasible ones give

| appends before the site | len at site | ac1 at site | len after |
|---|---:|---:|---:|
| none | 8 | 3 | 11 |
| `E8E` or `ECB` | 11 | 3 | 14 |
| `EA7` | 13 | 3 | 16 |
| `E8E`, `ECB` | 14 | 3 | 17 |
| `EA7`, `ECB` | 16 | 3 | 19 |

The same walk settles the other two `unknown` appends in the routine:
`70167EE1` (len ∈ {8, 11, 13}, ac1 = 3) and `70167F29` (len ∈ {8, 11, 13,
14, 16, 17, 19}, ac1 = 3, len after ≤ 22). All five appends' operands are
`3` or `5` exactly.

### 1.3 The assumptions, checked (PROMPT "what you may assume")

| assumption | check | result |
|---|---|---|
| closed CFG | all 505 blocks attributed to the routine reachable from `701674B5`; zero edges from any other routine's block into them; zero other addrbook entries in `[701674B5, 70168516]`; zero mentions in `ON_ERROR_CATALOG.md`; no block ends in a raw instruction (nothing borrowed from `quest.blocks.split`) | **holds** |
| frame private: game calls | one game call, `DISTANCE_TO_PLAYER` at `701675A9`; pushes `R[ac3−12]` (the caller's own arg cell, passed through) and two pointers into the shared page (`M32[0x70000212] + …`). No frame slot escapes | **holds** |
| frame private: runtime calls | 45 rt_calls. `?UNSIGNED_TO_CHAR` always at slot 30 (writes ≤ 17 words, 30..46). `?WRITE_SCREEN$5` writes its row/col args only (`quest_rt.h:273`): slots 3, 4, 30, 32, 40, 42 — never 6. Slot 6 is passed once, at `70167F42`, as the *text* argument (const), after the site | **holds** |
| frame private: uplevel | no `DISPLAY_INVENTORY.N@` entries; it is not itself nested | **holds** |
| pointers loaded from memory do not alias the frame (P58 temptation 8) | still assumed. The bit-set stores `M16[ind(ac1) + …]` (WBTO/WBTZ) go through `M32[0x70000210]` (shared page); the one register-derived rt_call arg `R[ac3+42]` is a claimed stack temporary (`s@70167744.2`) and is the read-only text argument | **assumed, not proven — but irrelevant to this site**: every such store is outside the region, and inside the region there is nothing but the appends |

The last row matters for the tier: the proof needs the aliasing assumption
only for writes *between* `70167E6E` and the site, and there are none. So
the site is proven by dataflow **without** temptation 8.

---

## 2. What P58 missed — and it is systematic

P58's trace at the site (`countflow.py --dump-site 70167F05`):

    {3 | nsub(0x1E, {<opaque slot 6 clobbered: store M16[wp(ac2, 0)] @70167E98:2>
                   | <opaque slot 6 clobbered: store M16[wp(ac2, 0)] @70167EB1:2>
                   | <opaque slot 6 clobbered: WCMV destination (frame range unknown) @70167ED5:7>
                   | 8})}

So the slot model **already had the right reaching definitions** — the
constant 8 and the three intervening writers — and P58's `<opaque slot 6
clobbered>` is exactly those writers. Three model gaps turned a provable
site into `unknown`, each one a refusal to read something the IR says:

1. **A store through a register whose single reaching definition is
   `wp(ac3, 6)` in the previous block is reported as an opaque clobber**
   (`store M16[wp(ac2, 0)]`), although the same tool's register RD knows
   `ac2 = wp(ac3, 6)`. Resolved, it is a write of slot 6 with value
   `trunc16(sx16(M16[slot 6]) + ac1)`.
2. **The concatenation clamp is not a recognised capacity-preserving
   write.** `assign_varying`'s `min(len, cap)` is (P58 a003 fix 1); the
   append's `len + min(k, cap − len) = min(len + k, cap)` is the same thing
   and is not. This is the "`assign_varying` induction does not apply here"
   line of the PROMPT: it does apply — the appends *are* clamped assignments.
3. **`judge()` never signs a subtraction.** `cap − len` with `len ∈ [0, cap]`
   is non-negative; the tool has the fact (from 1+2) and no rule to use it.

The WCMV-destination clobber at `70167ED5:7` is the same miss from the other
side: the destination `bp(wp(ac3, 6), 2) + len` is *provenance-tagged* to
slot 6's own data area, so it can never cover the length word — the
structural argument P58 already makes for the base class ("a negative count
walks the pointer back over the length word"), applied to the destination.
P58's "frame range unknown" is an over-approximation, as the PROMPT
suspected; and the fix is not "bound the scratch buffer" but "the pointer
came from the slot itself".

**Is it systematic? Yes, and it is a class, not the whole tier.** A
program-wide scan for the append idiom (head block `ac2 = <ptr>; ac1 = CAP;
ac0 = K; ac1 = nsub(ac1, M16[wp(ac2, 0)]); goto [F, T] (ac0 >=s ac1)` with
the canonical 10-statement body) finds **13 sites**:

| routine | sites | cap | dest | P58 tier |
|---|---:|---:|---|---|
| DISPLAY_INVENTORY | 5 | 30 | slot 6 | 2 `yes` (the lifter folded len = 8 for it), **3 `unknown` — now proven (this report)** |
| TERRITORY | 2 | 9 | slot 6 | `unknown` |
| OBSERVE | 2 | 150 | slot 18 | `unknown` |
| DISPLAY_MAP | 1 | 80 | slot 20 | `unknown` |
| DISPLAY_SCREEN | 1 | 84 | slot 998 (k = len of slot 10, not a constant) | `unknown` |
| INIT_OBJ_TBL | 2 | 600 | shared-page record field (`wp(ac2, 126906)`), not the frame | `unknown` |

That is **11 of the 70 sites, 22 of the 130 operands, in the `unknown`
tier** — the same shape, the same discharge (a dominating constant-length
assignment, a region with nothing but appends, the clamp invariant). I
proved the three in DISPLAY_INVENTORY and did **not** prove the other eight;
they are listed in `onesite.out`-style detail only for this routine. Two
of them need more than this project's argument: DISPLAY_SCREEN's piece
length is another varying's length word (needs slot 10 ≥ 0 first), and
INIT_OBJ_TBL's destination is on the shared page, where frame privacy says
nothing (P58 §2.2's record-field class). The other six (TERRITORY, OBSERVE,
DISPLAY_MAP) are frame slots maintained by the same three idioms — literal
assignment, the 4-character `len = 4; M32[data] = literal` assignment, and
clamped append — and DISPLAY_MAP additionally by a single-character append
(`if len < 80 then data[len] := ch; len := len + 1`, block `7016567B`), a
fourth clamped form. `onesite.py` recognises the WCMV-literal assignment as
the dominating write and the multi-character append only; extending it to
the other two writer shapes is the named follow-up.

**The rest of the tier is not this.** The other 59 `unknown` sites are
P58 §2's other classes — buffer extents through unbounded copies (26),
record-field capacities, argument cells, products. This project says
nothing about them except by analogy: at this site the "unbounded copy" was
bounded by where its pointer came from, and that is worth trying on the 26
before believing they need frame layout.

**Ruling 1 (evidence vs proof):** not needed. The site does not rest on "the
game works"; it rests on the book.

**Ruling 2 (tier):** dataflow (a dominating write, an acyclic region, an
exhaustive walk) over a closed CFG. Not construction (the count is not a
literal), not a dominating guard (the site's own guard `3 ≥ 30 − len` gives
`≤ 3`, not `≥ 0`; a `min` needs both arms), not provenance.

---

## 3. What did not survive contact

- **"`wp(ac3, 6)` is written three times, all bare length stores (556, 622,
  917)."** It is written **fourteen** times in the routine (onesite.out §3:
  three literal assignments, six bare length stores — four in the inventory
  loop, two of the 4-character `len = 4; M32[data] = "East"` form — and
  five appends). The three the prompt found (plus a fourth it missed,
  `701678CE:1`) are inventory-loop writes that `70167E6E` kills on every
  path; they never reach the site. The writers that matter — the "Rings: " assignment and the appends —
  were not on the prompt's list at all.
- **"The `assign_varying` `min(len, cap)` induction does not apply here."**
  It is the whole proof. The appends are `min(len + k, 30)`.
- **"The inferred CHAR(30) capacity turned out to be a different slot in a
  different frame."** Slot 6 in this frame *is* CHAR(30) VARYING; the
  earlier readings' 30 was right and their reasons were wrong.
- **The prompt's "(0 − len + 19)" at line 646** is padding a CHAR(19)
  *target* (`[@ac2, 19 − len] = [@blanks, 19 − len]` after copying `len`
  bytes), not a second capacity for slot 6. That site is a separate
  obligation (`len ≤ 19` at `70167744`), on the inventory-loop path where
  slot 6 holds an object name copied from a record — P58's record-field
  class, not touched here.
- **The reduction `ac1 ≥ 0 ⟺ len ≤ 30`** is right where it matters and false
  at the 16-bit wrap corner (§1.1). And it is the wrong question: the site's
  count is not "non-negative", it is 3.
- **P58's `<opaque slot 6 clobbered: WCMV destination (frame range unknown)
  @70167ED5:7>`**: a real write, to words 7..21, never to word 6. Over-
  approximation confirmed, mechanism named (§2).

---

## 4. What would settle the eight siblings (not done)

Generalise `onesite.py`'s dominating-write search from "WCMV literal into
`wp(ac3, s)`" to the two other constant-length writer forms
(`ac0 = sub(ac0, ac0)`/`ac0 = 4` followed by `M16[wp(ac3, s)] = trunc16(ac0)`),
add the single-character append shape, and drop the acyclicity requirement
in favour of the invariant (`len ∈ [0, cap]` is preserved by every
recognised writer, so a loop of appends is fine). For DISPLAY_SCREEN, first
establish slot 10's length word by the same method. INIT_OBJ_TBL stays in
the record-field class. In `countflow.py` terms the fix is model-side, in the
spirit of a003/a004: (1) resolve store addresses through register RD, (2)
treat the concatenation clamp as a `min(·, cap)` write, (3) let `judge()`
sign `cap − W` when `W` is a slot with the clamp invariant.

---

## 5. Files

    docs/Project59/REPORT.md        this
    docs/Project59/onesite.out      tool output for the three DISPLAY_INVENTORY sites (all sections)
    compiler/onesite.py             new; usage: onesite.py [--routine R] [--site-block B ...] [--paths]

Boundaries: nothing in `emulation/**`, `game/**`, `docs/IR.md`,
`docs/Project58/**`, or `compiler/countflow.py` changed; nothing regenerated;
nothing executed.
