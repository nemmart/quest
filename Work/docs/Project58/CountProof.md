# CountProof.md — what is proven about the string counts, and what is not

**Status: RESULT of Project 58 (Sep 13 2026).** Every number here is produced by
`compiler/countflow.py` from `emulation/quest.ir2.book`; the per-operand ledger
is `docs/Project58/sites.tsv` (3,366 rows), the assert list is
`docs/Project58/asserts.tsv`, the console output `countflow.out`. Re-derive
with:

    python3 compiler/countflow.py --out sites.tsv --asserts asserts.tsv
    python3 compiler/countflow.py --infer-caps --out sites-infercaps.tsv      # a001 Q2 policy (a)

Tiers, as ruled (PROMPT ruling 1): **construction** (true by the shape of the
IR), **dataflow** (true by reaching definitions over the CFG, register and
frame-slot model of `countflow.py`), **guard** (true by a dominating
`assert`/branch, a001 Q3 — kept separate from dataflow), **provenance**
(rests on a fact about data or a runtime routine outside the CFG, checkable),
**unproven**. Nothing here executed; the closure lemma's exhaustive check is a
transcription and is called corroboration.

---

## 0. The obligation, restated

At every string site, the two counts (`ac0`, `ac1`; `WBLM`'s single `k`) are
non-negative. 1,689 sites: 1,637 WCMV, 40 WCMP, 12 WBLM; 3,366 count operands.

**Result** (a006's three tiers): **1,069 sites proven** (construction,
guards, slot invariants, image-table bounds); **499 sites layout-backed** — every non-proven count is a varying's
own length word or monotone arithmetic on such words, discharged by one assert
per root length word (169 words, 282 root rows); **121 sites asserted only**
(126 `cond` operands over length words the routine never reads as a piece
on a dominating path, plus 82 `unknown` operands); **0 sites where a count is
negative by construction.** No `cmp` result ever feeds a count. Verdict tiers
(a002): 998 / 8 / 611 / 72.

---

## 1. The closure lemma (tier: construction; corroborated by exhaustion)

From `emulation/hw/strings/EagleString.cpp`:

**WCMV** (`copy` :60-90; `residues_after_copy` :152-163), `t = min(|n|, |len|)`:

    ac0 = 0                          the loop runs dst_count to 0 in either direction
    ac1 = len − sgn(len)·t           len ≥ 0 ⇒ ac1 ≥ 0;  len < 0 ⇒ ac1 ≤ 0;  len = 0 ⇒ ac1 = 0
    ac2 = dst + n,  ac3 = src + sgn(len)·t,  c = (ac1 ≠ 0)

So after any WCMV, `ac0 ≥ 0` unconditionally and `ac1 ≥ 0 ⇔ len ≥ 0`. **The
destination count's sign never enters.** Edges: `len = 0` gives 0; `t = 0`
gives `len`; a negative `n` blank-fills backwards and still leaves `ac0 = 0`.

**WCMP** (:167-204): `ac0` = string-2 count at the stop, stepped by ±1 toward
0 and never past it, so `n ≥ 0 ⇒ 0 ≤ ac0 ≤ n`. `ac1 = result ∈ {−1, 0, +1}`:
**not** non-negative — a count fed by a compare result would be NO.

**WBLM** (:111-131; :206-210): `ac1 = 0`.

Corroboration: `countflow.py --selftest` transcribes the two loops and the
residue rule and runs every `(n, len) ∈ [−6, 6]²` — 169 cases, 0 disagreements.

**What closure buys.** A residue-fed count inherits the obligation of its head
and adds none. Measured: of 432 register operands, **18** are fed only by
residues (17 tail-split `ac1` after a WCMV; 1 WCMP second count fed by a WCMV
`ac0 = 0`). The chains are shallow; closure is a footnote, not the proof.

---

## 2. Register model the proof rests on (tier: construction, verified in the emulator)

- `acN = e` defines `acN`; string statements define exactly what the library
  writes (WCMV ac0..ac3+c, WCMP ac0..ac3, WBLM ac1..ac3).
- **Calls preserve ac0..ac2 and set ac3 = wfp**: `EagleStack.cpp` WRTN pops
  ac2, ac1, ac0 and sets `ac[3] = wfp`; `RTBridge.cpp:88-90` restores
  `saved_ac[0..2]` and sets `ac[3] = wfp` for native runtime bodies; WSAVS
  opens every routine with ac3 = wfp. Every predecessor-less block in the
  book opens with WSAVS except three after SYSCALLs / a DERR sink (reported).
- Raw instructions: float ops, WPSH, DERR write no accumulator; DIVX, WLOB,
  SYSCALL and a raw LCALL's unlifted arguments are OPAQUE (a trace through
  them is `unknown`, never `yes`).
- Frame slots `M32/M16[wp(ac3, k)]` are tracked per routine, with the
  effects of stores, varying assignments (length word := the count, IR.md 5.8
  P32 note), fills, runtime callees' writes-through
  (`docs/Project28/RTConventions.md` §64-80; `?UNSIGNED_TO_CHAR` ≤ 17 words at
  ac2, `runtime/unsigned_to_char.cpp:136`), game callees' by-reference
  arguments (upward clobber from each `M32[argslot] = wp(ac3, j)`), and raw
  instructions (clobber all). A pointer loaded from memory is assumed not to
  alias the routine's own frame.

---

## 3. Per-class proof

### 3.1 Constant and literal counts — 998 sites, tier: construction

| shape | sites | operands |
|---|---:|---:|
| both counts constant (WCMV 111, WCMP 2) | 113 | 226 |
| literal assignment, constant count (`[@a, 27 varying] = "…27 bytes…"`) | 870 | 1,740 |
| WBLM, constant `k` (8, 10, 90, 0xBE0, 0x167E) | 12 | 12 |
| `ac0` after a WCMV (= 0), `ac1` after WBLM (= 0), constant-folded arithmetic (`nsub(30, 8)`, `x − x`) | 3 | 69 (most at sites whose OTHER operand is not proven) |

A literal's count is its byte count (unescaped length of the quoted text);
a constant is ≥ 0 by the loader's grammar (0..32767). Every WBLM count is a
positive constant, so **all 12 fills are forward**; all 12 are the
self-overlapping smears (`src = dst − 1` ×5, `− 2` ×7) the book annotates —
well-defined because `block_move` steps one word at a time (IR.md 5.8).

### 3.2 The base class — 286 operands at 268 sites, tier: LAYOUT-BACKED, discharged by one assert per length word

An operand whose count is the 16-bit word at word address `W` and whose
pointer is byte `2W + 2` — the layout of a `CHAR(n) VARYING`, on either side
of the statement (a001 Q1). **The test is algebraic** (a003): with both
operands traced to their roots, `bytes(pointer) − 2·W = 2` must hold as a
linear identity (`wp(b, d) = b + d`, `bp(b, d) = 2b + d`, `0xW:b = 2W + b`,
constant multiples fold, everything else is an opaque term by canonical
spelling), however `W` and the pointer are written — computed bases, record
indexing through a multiply, by-reference pointers.

| how recognised | operands |
|---|---:|
| by form, `[@A, varying]` (the lifter's rule `string_sites.py:1696/2177`): WCMV src 160, WCMP s1 31, s2 15 | 206 |
| by dataflow + algebra, a fixed operand whose traced count is `sx16(M16[W])` and traced pointer satisfies the identity (modulo 2^32 — a004 §9): count spelled as an expression (`lenload-expr`) WCMV src 35, WCMP s1 2; count in a register WCMV src 38, WCMP s1 4, s2 1 | 80 |

(P58 as accepted in a002 had a SYNTACTIC matcher — a simple base register and
a constant `k` — and reported 227 / 213. It missed 39 operands whose length
word is a record field reached by index arithmetic, e.g. ALCHEMIST_HOME
7015C90D, `W = slot2·9 − 0xFEAF841`, pointer `2·(slot2·9) − 0x1FD5F080 =
2W + 2`. All 39 were `cond`; none was `unknown`; nothing previously matched
was lost. a003 §4.)

Address shapes of the 206: own-frame slot 128; static word 36 (`IN_BUFFER`
0x7000021C ×29, 0x70000C34 ×3, four others ×1); SD_PTR / OBJ_PTR / PLAYER-cache
record fields 18; a record through an argument pointer 10; by-reference `R[…]`
3; other computed 11.

**Why non-negative is the right assumption** (PROMPT): the length word sits
immediately below the data, so a negative count reads or writes the header
itself. No compiler emits that deliberately. **Why it is an assumption and not
a proof:** the length word's writer is, in 111 of 206 cases, a runtime callee
(`?READ`, `?UNSIGNED_TO_CHAR`), another routine through a by-reference argument,
or an unbounded copy; in 135 it is outside the routine's frame altogether
(statics, record fields, arguments — the algebraic matcher's 39 are all record
fields); only **20** of the 266 trace to a provable non-negative write on
every path (a literal or constant varying assignment). The induction "every
write is non-negative" is sound where it applies and applies to fewer than one
operand in ten. **The assert is the mechanism.**

The exact assert per operand is in `asserts.tsv` (tier `base-class`):

    assert((sx16(M16[<A>])) >=s 0, "P58 base-class <role> @<pc>")

emitted immediately before the statement, in its block. **`7016816B`
(DISPLAY_INVENTORY) is in this class on both sides** — its string 1 is
`sx16(M16[(sx16(M16[R[wp(ac3, -12)]]) * 686) + M32[0x70000210] - 62])`, a
PLAYER record field holding 0xFFFF on the login path — and is expected to
trip its assert. That is the test that the mechanism works (ruling 2).

**Static varyings (tier: provenance, checked against `Disassembled/quest.mem`).**
All six static length words the book reads (0x7000021C, 0x70000A78, 0x70000AA2,
0x70000C1E, 0x70000C34, 0x70000C44) are **0 at load**. Writers in the book:
0x7000021C — `?READ_SCREEN`, four varying assignments, three `M16` stores;
0x70000A78 and 0x70000C34 — one `M16` store each; 0x70000AA2, 0x70000C1E,
0x70000C44 — none (they stay 0). None is written by another program (they are
in the program's own data page, not the shared page).

### 3.3 Counts that are arithmetic on length words — DERIVED from their roots (a004), tier: layout-backed via dataflow

The compiler's precomputed concatenation total `len(x) + k` (k the literal
bytes appended), stored to a frame slot and reloaded (`XWLDA`), or held in a
register through a min diamond. `countflow.py` traces the slot and the
diamond; the result is `const + length word(s)`, non-negative iff each length
word is (tier: dataflow reduces it to the base-class fact). The 17 tail splits
are here: `ac1` after the previous copy, `≥ 0` iff that copy's source length
was — StringsDesign §2.4's "positive when the WCMV runs" re-derived as
**conditional on the head, by closure**, not as a fact.

**785 such operands are DERIVED** (`asserts.tsv` tier `derived`, `needed =
no`, the covering root(s) in the `root` column): every length-word leaf has a
base-class root at the same statement or dominating the site, with no
statement on any path from the root round to the root or on past the site
that may write the word, and the operators are `+`, `×`, a diamond's union
(min/max), `sx16(trunc16(x))`. 206 of them are the destination count of the
root's own statement (`[@ac2, LEN] = [@W, varying]`); the rest are the
concatenation totals `LEN + k`, the copy-outs, and the tail splits (`ac1` after
a WCMV whose source count was itself established — closure, iterated to a
fixpoint). The root's assert, e.g.

    assert((sx16(M16[wp(ac3, 60)])) >=s 0, "P58 base-class src-ac1 @70167FE0")

discharges the whole chain: `LEN + 11 ≥ 0` because `LEN ≥ 0`.

**104 operands at 63 sites stay `cond`** with their own assert: their length
word has no dominating root — 56 distinct words, 39 of them read as a varying
elsewhere in the routine (a root on another path, or a call / unbounded copy
between root and site), 17 never read as a piece at all (e.g. DISPLAY_CAVE's
static varying 0x70000A4E, only ever appended to as `len + 2`). These are
"asserted only" in REPORT §1's tiers — but note what they are: length words
plus constants, not arbitrary memory.

### 3.4 Dominating guards — 18 operands at 12 sites (8 sites fully), tier: GUARD

a001 Q3: reported separately from dataflow because its failure mode differs
(syntactic matching of a traced condition against a traced count; a record
field's value is trusted across the guard-to-site interval). Two rules fired:

- **the compiler's own DERR bounds check on a SUBSTR length** — `assert((19 −
  len) <=u 80)` in P27's fold — is an UNSIGNED upper bound, hence a
  non-negativity proof of `19 − len`: DISPLAY_INVENTORY 7016775F, TERRITORY_MAP
  7017B831/7017B866, OP_EDIT.9 7017597B, STORE.1 7017A15B;
- a branch on the register itself (`ac1 >=s 0` on the taken edge, the min
  diamond's `x <=s y` with `y` provable): QUEST 7015C0AD, ALLY_PLAYER.1
  7015D131, CAST.3 70162D72, KILL_PLAYER.2 7016E355/7016E3CA, LIST_PLAYERS.1
  7016ED28, TAKE_OVER_CASTLE 7017C380.

These need no assert (`needed = no` in `asserts.tsv`).

### 3.4b Slot invariants — 29 operands at 14 sites, tier: DATAFLOW (a005 / P59)

A count that reads a frame slot's length word — as `len`, or as `C − len`
after a clamp — is proven when the nearest dominating definite write of the
slot and every writer on every path from it to the site (and round any loop
back to the site) keep `len ∈ [0, C]`: literal and constant assignments, the
clamped append `len + min(k, C − len) = min(len + k, C)` (the same guarantee
as `assign_varying`'s `min(len, cap)`, written out in instructions — P59),
the single-character append `len + 1` under its `len < C` guard, a pointer
difference `cursor − data`. The string destinations in the region are shown
not to reach the length word under the same invariant. Sites: DISPLAY_INVENTORY
70167EE1 / 70167F05 / 70167F29 (P59's exhaustive walk agrees: the count is 3
on every path), OBSERVE 70172DED / 70172EBE / 70173253 / 70173279, DISPLAY_MAP
7016566D, STORE.1 7017A176, TERRITORY 7017CE4D / 7017CE84 / 7017CEB0 /
7017CEF1 / 7017CF04. No assert needed (`asserts.tsv` tier `invariant`).

Assumptions carried: frame privacy — a raw `LCALL`/`XCALL` with zero
arguments to a non-nested routine cannot write this frame; nested routines,
`LJSR` (the condition-system entries) and calls with arguments stay opaque; a
pointer loaded from memory does not alias the frame (temptation 8). None of
the 14 sites has such a statement in its region.

### 3.4c Image tables and static bounds — tier: PROVENANCE (a006)

A length word `M16[idx·stride + base]` where `base` names a table the 1986
compiler laid down as constants (the familiar-name table at 0x70150A5A, the
spell table at 0x70150448, the help lines at 0x70000272, the shop table at
0x7000058D) under the compiler's own DERR index guard is bounded by the
maximum over the reachable entries in `Disassembled/quest.mem`, provided no
statement in the book writes into the table (constant-address stores, string
destinations, by-reference arguments and callee write positions are all
censused). 63 sites carry such an operand. A static varying's length word is
bounded by the census of ALL its writers: IN_BUFFER ≤ 132 (27 writers —
REPORT §11), 0x70000A4E ≤ 80, 0x70000A78 ≤ 80. Assumptions: the image's
constant tables are not written through a pointer; no other program writes
the program's own data page.

### 3.5 The residue — 48 sites unproven (40 under policy (a)), tier: UNPROVEN

Complete per-site list: `countflow.out` §7 (default) and
`countflow-infercaps.out` §7 (policy (a)). By cause:

| cause | sites | what would establish it |
|---|---:|---|
| **a total slot clobbered by an unbounded scratch-buffer copy** (`[@bp(ac3, b), <expr>]`, `[@ac2, …]` with a cursor that traces through a call, or an `M8[ac2]` cursor store) | 26 | the buffer's extent. Policy (a) — extent := the largest constant copy into the same buffer — resolves all 26 to `cond`, and **every one of them is circular** (the bound is inferred from copies into the very buffer whose unbounded copy is being bounded); q001's guess of a ~10/~35 split was wrong — there is no non-circular subset. Each such row in `sites-infercaps.tsv` carries `[extent INFERRED: …]` in its reason. A sound extent needs the declaration (frame layout), which this book does not carry |
| **`capacity − length(record field)`** — padding arithmetic `16 − len(name)`, `30 − len(spell)`: DISPLAY_MAGIC ×2, HELP.1, STORE.1 ×4, INIT_OBJ_TBL 7016DF66/7016E005 | 9 | that the field's length never exceeds its declared capacity — true if every writer is an `assign_varying` (which stores `min(len, cap)`), a fact about the shared data page and every program that writes it. Tier would be provenance; not established here |
| **SUBSTR remaining room through a min of several arms** where one arm is an unbounded-copy residue or a callee: DISPLAY_MAP 7016566D, DISPLAY_SCREEN 70166FE5/70167127/701671FA/701672F4, DISPLAY_INVENTORY 70167EE1/70167F05/70167F29 (×3 need policy (a)), OBSERVE 70173253/70173279, TERRITORY 7017CE70/7017CEA7 | 13 | the same two facts as above, per arm; OBSERVE's `150 − (150 − residue)` needs the residue ≤ 150, i.e. the previous copy's source ≤ 150 |
| **an argument cell** (`M32[wp(wfp, −14)]`, `−18`, `−22`, `−28` — a caller-supplied count or pointer): INIT_OBJ_TBL ×4, OP_EDIT.9, TERRITORY 7017CF35, TERRAIN | 7 | interprocedural: the callers' pushed values |
| **a product** `len × 2 + 56`: TAKE_OVER_CASTLE ×2 | 2 | `len ≥ 0` (a base-class fact) AND no 16-bit overflow — the product is judged unknown because non-negativity of a product needs both factors' signs; the length word is the same base-class assumption |
| **a `t1` scratch** (WXCH-swapped count): CAST.2 70162B53 | 1 | a t-place model in the tracer (small) |
| **`?UNSIGNED_TO_CHAR`'s ac2 untraceable** (SEIGE ×2): its destination pointer comes through a loop | 2 | policy (a) resolves them |
| GET_QUEST ×4, MOVE_PLAYER ×2, OP_EDIT.2 ×2, DISPLAY_INVENTORY 70168143 — cursor stores / copies with an unknown frame range | 9 | as row 1 |

Every one of these is still discharged at runtime by the site assert in
`asserts.tsv` (tier `unknown`); the difference from `cond` is that the tool
cannot name *which* datum's sign the count depends on.

**None of the 72 is a `[@A, varying]` read the pattern failed to recognise.**
The prompt's expectation for the residue was wrong; the residue is frame-layout
blindness and min arithmetic over data the routine does not own.

---

## 4. Exception enumeration

### 4.1 Sites reading non-string data through a string accessor (PROMPT §a)

The base-class operands whose length word is a **record field** (18 by form,
plus 10 through an argument pointer, plus the 39 the algebraic matcher
recovered — OBJ and PLAYER fields indexed by a multiply): all are PLAYER / OBJ
record fields
read as varyings — name, title, spell and object strings the game keeps in
its records. `7016816B` is one. Whether such a field "was written as a string"
is a property of every program that writes the shared page; **this project
enumerates the readers (in `sites.tsv`, address shape `SD_PTR record field`,
`OBJ_PTR record field`, `PLAYER cache field`) and does not claim the writers.**
Every member is discharged by the base-class assert; `7016816B` is expected to
fire on the login path.

**Can the algebraic identity match by accident (a003 §3)?** Only a 16-bit
integer immediately followed by the bytes it counts satisfies `count =
M16[W]`, `data = 2W + 2` — which IS a varying, whatever the declaration called
it; the structural argument (a negative count walks the pointer back over the
count word) applies to any such pair. I inspected all 39 new matches: 37 are
`sx16(M16[record + index·stride + field])` with the pointer one word on
(OBJ/PLAYER name, title and spell fields); 2 are register counts loaded from a
PLAYER record with the pointer computed from the same record base (CAST.2
70162AAF, OP_EDIT.3 7017469C). No coincidental match found; a `t1`-scratch or
union-valued pointer never matches (the identity is required of every member).

### 4.2 Uninitialised varying reads (PROMPT §b)

Of the 128 `[@wp(ac3, k), varying]` reads of the routine's own frame:

| status | reads |
|---|---:|
| a DEFINITE write of the length word reaches on every path | **114** |
| a definite write is missing on some path, a MAY-write (callee, unbounded copy) covers it | **13** |
| **a path with no write of any kind** | **1** — MOVE_IN_CAVE 70170BCA, `wp(ac3, 10)` |

**MOVE_IN_CAVE 70170BCA — INFEASIBLE, tier: dataflow + path condition (by
hand, checked with the tool).** The length word at `wp(ac3, 10)` is written on
two arms of a three-way test on the strength delta `d = M16[wp(ac3, 3)]`:
`d < 0` → `"decreased by "` (70170B76); `d > 0` → `"increased by "`
(70170B84). The write-free path the tool found is `d = 0` at 70170B82 (taking
70170B83), then 70170B8F re-tests `d`: `d = 0` → 70170B93 `"The book has no
effect."` and the routine leaves WITHOUT reaching the read; `d ≠ 0` → the read.
Both tests load the same word, and the slot model shows a single reaching
definition of slot 3 (`70170B5B:2`) at 70170B66, 70170B82, 70170B8F and
70170BB8 — nothing writes it between. So the path needs `d = 0 ∧ d ≠ 0`: the
three-way is exhaustive and the zero case exits early. **No uninitialised
varying read exists in the book's own frames**, and the M4a "previous call's
leftover" hazard has no site. (q001 prediction 4 said feasible; it was wrong —
REPORT.md §3.)

The 13 may-write cases are all real writers (`?READ`'s buffer, `?UNSIGNED_TO_CHAR`'s
result, a game callee through a by-reference argument, a scratch copy) whose
extent the tool cannot bound; none is a candidate for "never written".

Static varyings: base case established from `quest.mem` (§3.2): all 0 at load.

### 4.3 `cmp` results — never a count

All 40 WCMP `ac1` residues are consumed by the `goto` test of the same block
(40/40 `goto … (ac1 == 0)` / `(ac1 != 0)`); `ac0` and `ac2` are READ after 3
compares but only by `x = sub(x, x)` (WSUB n,n) and `x = x` (WMOV n,n), whose
result does not depend on the value; `ac3` is never read.

---

## 5. Residue liveness — P6 (tier: dataflow)

First observing read on some path after the statement (a value-independent
read, `x = x` / `x = sub(x, x)`, does not count):

| after | ac0 | ac1 | ac2 | ac3 |
|---|---|---|---|---|
| WCMV (1,637) | **live 328** (dead 1,309) | live 44 (dead 1,593) | live 455 (dead 1,182) | dead 1,637 |
| WCMP (40) | dead 40 (3 value-independent reads) | live 40 — the `goto` test | dead 40 (2 value-independent) | dead 40 |
| WBLM (12) | — | live 1 (`add(ac1, ~ac1)`, the −1 idiom) | dead 12 | dead 12 |

What reads them: WCMV `ac0` — stored as a **zero** (`M16[wp(ac3, k)] =
trunc16(ac0)`, the row/column arguments of `?WRITE_SCREEN`, 250+ times, often
across an intervening `rt_call`, which preserves it) and `ac1 = ac0` copies;
WCMV `ac1` — 19 `goto` tests ("was the source exhausted?"), 17 `ac0 =
sub(ac0, ac1)` remaining-room computations (the tail splits), 4 pushed as an
argument; WCMV `ac2` — 383 as the next piece's destination cursor.

Consequence for binding precision: **`ac3` after a copy is dead everywhere;
`ac0` after a copy is a real dependency at one site in five** — a rewrite
that drops residues must keep `ac0 = 0` where `sites.tsv` says it is live (the
tool's `live_after` gives the exact reader).

---

## 6. Segment containment — P5 (tier: UNPROVEN, runtime-checked)

`copy()` and `residues_after_compare()` throw when a source or destination
byte crosses a 2^28 segment (`EagleString.cpp:76, 82, 181, 190`); `block_move`
likewise (:121). No site states it. It is not provable from the IR alone: every
address is frame-, static-, shared-page- or arena-relative and the bound is a
property of the loader's placement plus the counts' magnitudes (frames ≤ 0xD89
words; constant counts ≤ 32 K; the shared page's extent). The library's throw
is the check; lockstep would report it as a fault at the site. Recorded as an
obligation with this tier; no assert is proposed (it would duplicate the
library's own check).

---

## 6b. The three tiers, per operand and per site (a004)

| tier | operands | sites |
|---|---:|---:|
| proven (construction, guard 18, slot invariant 25, image-table / static bounds) | 2,189 | 1,069 |
| layout-backed and asserted (base-class + derived 711; 258 assert rows) | 969 | 499 |
| asserted only (`cond` 126 + `unknown` 82; 208 assert rows) | 208 | 121 |

A site is in the tier of its weakest operand. 466 asserts discharge
everything below "proven"; 711 derived, 25 invariant and 18 guard rows carry
`needed = no`. (a006; a005: 2,094 / 1,056 / 234, 492 asserts; a004: 2,065 /
1,067 / 234, 516.)

## 7. What every later rewrite may rely on

1. `ac0 = 0` after every WCMV; `ac1 ≥ 0` after a WCMV whose source length was
   ≥ 0; `ac1 = 0` after WBLM; `0 ≤ ac0 ≤ n` after a WCMP with `n ≥ 0`
   (construction).
2. At the 998 + 8 proven sites, both counts are ≥ 0 and the copy is forward
   (construction / guard).
3. At the 611 conditional sites and the 72 unproven, `asserts.tsv` gives the
   statement that makes the same true at runtime, loudly, at the site — 516
   asserts in all after a004; a derived operand is covered by the root(s) its
   row names.
4. No count is ever a compare result (dataflow, program-wide).
5. No varying read of an own-frame slot is reachable without a write of its
   length word (dataflow + one path-condition argument).
6. Direction: forward at every proven site; at every other site, forward iff
   its assert holds — which is exactly the same condition the assert states.
