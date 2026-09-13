# Project 58 — REPORT (Sep 13 2026)

Result of record: `docs/Project58/CountProof.md`. Ledgers: `sites.tsv`
(default, sound policy), `sites-infercaps.tsv` (a001 Q2 policy (a)),
`asserts.tsv` (the deliverable a001 asked for). Tool: `compiler/countflow.py`
(new; `--selftest` passes). Boundaries kept: nothing in `emulation/**`,
`compiler/lower_c.py`, `game/**`, `docs/IR.md`, `docs/StringModel.md`, DESIGN
or any artifact was changed; no assert was emitted; nothing executed.

---

## 1. The answer — three tiers (after a006; earlier tables follow)

| tier | operands | sites | what it rests on |
|---|---:|---:|---|
| **proven** — construction, a dominating guard, a slot invariant, or (a006) a length word bounded by an image table / a static's writer census | **2,189** | **1,069** | nothing to check |
| **layout-backed and asserted** — the base class and what is derived from it | **969** | **499** | one assert per root (258 rows) |
| **asserted only** — 126 `cond` + 82 `unknown` operands | **208** | **121** | the site assert |
| negative by construction | 0 | 0 | — |

Asserts to emit: **466** (258 base-class + 126 cond + 82 unknown); a005: 492,
a004: 516, a002: 1,326. Under policy (a): 107 asserted-only sites.

Earlier: a005 2,094 / 1,056 / 234 operands (1,022 / 542 / 125 sites);
a004 2,065 / 1,067 / 234; a002's verdict tiers 998 + 8 / 611 / 72 / 0.

## 2. What could not be proven, and what it would take

1. **Buffer extents (26 sites).** A compiler total stored to a frame slot, read
   back as the count, with an unbounded scratch copy between the store and the
   read that the slot model must assume may reach the slot. Policy (a) (extent
   := the largest constant copy into that buffer) resolves all 26 — **and all
   26 are circular**: the bound is inferred from copies into the very buffer
   whose unbounded copy is being bounded. q001 predicted a ~10/~35 split
   between circular and clean cases; there is no clean subset. Marked in
   `sites-infercaps.tsv` per row (`[extent INFERRED: …]`). What would settle
   it: the declared size of each scratch temporary, i.e. frame layout. The
   book does not carry it; `game/declarations.json` has it for the seven
   translated routines only. **A finding on the way:** the compiler REUSES
   temporary space — in ALCHEMIST_HOME the total slot `wp(ac3, 100)` is the
   first word of the scratch buffer at byte 200 that an earlier chain used —
   so "extent" is a temporal notion (liveness), not a spatial one; a
   layout-only fix would still be unsound there.
2. **Record-field capacities (9 sites + arms of 13).** `16 − len(name)`,
   `30 − len(spell)` padding arithmetic on PLAYER/OBJ record fields. True iff
   the field's length ≤ its declared capacity, which holds if every writer is
   an `assign_varying` (`min(len, cap)`) — every writer in every program that
   touches the shared page. Provenance-tier if someone establishes it; not
   established here.
3. **Argument cells (7 sites).** Counts or pointers pushed by the caller.
   Interprocedural; the tool stops at the routine boundary deliberately.
4. **Two products, one t-place, two loop-carried pointers.** Small, listed.

None of these is a varying read the base-class pattern missed. The prompt's
model of the residue was wrong and CountProof §3.5 says so.

**Segment containment (P5):** unproven, runtime-checked by the library's own
throw; not provable from the IR without frame/page bounds. Recorded as an
obligation with that tier.

---

## 3. Predictions (q001 §8) scored

| # | prediction | result | verdict |
|---|---|---|---|
| 1 | policy (a) resolves ≥ 40 of the 76 unknowns to `cond`, none to `yes` | 26 of 72 to `cond`, 0 to `yes` | **wrong on the count** (26 < 40); right that none became `yes`. The other clobbers are `M8[ac2]` cursor stores and cursors that trace through calls, which (a) does not touch |
| 2 | a dominance-guard rule resolves all ~16 SUBSTR remaining-room sites; the tail-split claim is re-derived as `cond` on the head | guards settled 8 sites fully (18 operands), 5 of them the SUBSTR shape via the compiler's own unsigned DERR bound; the other remaining-room sites are `cap − len(record field)` and a min over an unbounded arm — no guard exists for them. Tail splits: `cond` by closure, as predicted | **half right**: the mechanism worked where a guard exists; I over-counted where one does |
| 3 | ≥ 5 sites stay unknown: INIT_OBJ_TBL's argument cells (3), TAKE_OVER_CASTLE's products (2) | INIT_OBJ_TBL 6 unknown (4 argument cells, 2 padding), TAKE_OVER_CASTLE 2 products unknown (a third site there was guard-proven) | **right** |
| 4 | MOVE_IN_CAVE 70170BCA's write-free path is FEASIBLE (delta = 0) and it is the only genuine uninitialised read | **INFEASIBLE**: the zero case exits through "The book has no effect." before the read; both tests load the same unchanged word (single reaching definition of slot 3 at all four points). No uninitialised varying read exists | **wrong — and this is the one claim about the 1986 program, so it is the one that matters.** The tool found the path; only path conditions settle it, and the prompt was right that dataflow cannot |
| 5 | every static varying's length word is 0 or small positive at load | all six are 0 (`quest.mem`) | **right** |
| 6 | the induction will not be extended: no further base-class operand moves from `cond` to `yes` | 20 of 206 are `yes` through their writers, up from 7 at the gate — because modelling `assign_varying`'s length-word store as a WRITE of the count (IR.md 5.8) was a modelling fix, not interprocedural work | **wrong**, in the good direction; the induction still reaches one operand in ten |
| 7 | 7016816B stays base-class both sides, `cond`, the expected assert-tripper | unchanged | **right** |

Four right, three wrong. The two wrong ones that matter (1 and 4) were both
over-optimistic about what the tool would resolve and about the original
program being buggy; the pre-registration is what makes that visible.

**Prediction 6, re-scored after a003.** It said the base class would not grow
without interprocedural work. It has now grown twice, and both times by a
*modelling* fix rather than more analysis: (i) treating `assign_varying`'s
length-word store as a write of the count, 7 → 20 operands provable through
their writers; (ii) replacing the syntactic `bp(b, 2k+2)` matcher by the
algebraic identity `bytes(pointer) − 2·W = 2`, 227 → 266 base-class operands
(213 → 251 sites), all 39 moving from `cond`, none from `unknown`, none lost.
Two-for-two: the tool's model, not its search, was the binding constraint —
and a classifier that under-reports its strongest tier looks conservative
while it is discarding arguments (a003).

---

## 4. Recommended rows for `emulation/quest.assumptions` (not written — a001 Q6 agreed the shape)

Format: `<claim> <address|site> <evidence> <checker>` with FALSIFIED BY, as the
file's existing rows.

    varying-length-nonneg   <every base-class site in docs/Project58/asserts.tsv>   -   compiler/countflow.py
    #   Every operand laid out as a CHAR(n) VARYING (count = the 16-bit word at A, data at A+1;
    #   227 operands at 213 sites, on either side of WCMV/WCMP) has a non-negative length word
    #   when the statement runs.  Structural: a negative length walks the pointer back over the
    #   length word itself, which no compiler emits.  NOT an induction over writers — 111 of the
    #   206 by-form operands are written by a runtime callee, another routine, or a record field.
    #   CHECKED BY: the assert at the site (asserts.tsv, tier base-class).  KNOWN TRIPPER: 7016816B
    #   on the login path (PLAYER record field 0xFFFF, Census F-B1) — that is the mechanism working.
    #   FALSIFIED BY: a base-class assert firing anywhere but 7016816B; a new operand form the
    #   pattern does not recognise (countflow.py's pattern count changing).

    cmp-result-never-a-count   all 40 WCMP sites   -   compiler/countflow.py ('no' verdicts == 0)
    #   The −1/0/+1 residue in ac1 is consumed by the goto test of its own block at all 40 sites and
    #   never reaches a string count (reaching definitions, program-wide).  Matters because closure
    #   FAILS for cmp's ac1.  FALSIFIED BY: any 'no' verdict in sites.tsv; a 41st WCMP whose ac1
    #   is read by anything but its goto.

    unsigned-to-char-extent   ?UNSIGNED_TO_CHAR   runtime/unsigned_to_char.cpp:136   -
    #   Writes at most 17 words at the entry ac2 word address: one length word + k <= width <= 32
    #   digit bytes ("min(k,32): k<=width<=32 always").  Used by countflow.py to bound the callee's
    #   frame effect (a DEFINITE write of 17 words, not an upward clobber); 26 of the 128 frame
    #   varying reads rest on it.  FALSIFIED BY: a width argument > 32 accepted by the native body;
    #   the emulated body (7017DA75) writing beyond dest+17 in any trace.

And one fact that belongs in DESIGN §5.2 rather than in `quest.assumptions`
(a001 Q4: record, do not act): **`ac0 = 0` after a WCMV is a real dependency at
328 sites** — the compiler stores it as the zero row/column argument of
`?WRITE_SCREEN`, across an intervening `rt_call`; `ac3` after a copy is dead at
all 1,637. The binding precision P56's F4 asked about is: drop `ac3` always,
drop `ac0` at 1,309 sites, keep `ac1` at 44 and `ac2` at 455 — `sites.tsv` /
`countflow.py`'s `live_after` name the reader at each.

---

## 5. Who emits the asserts

**Recommended: the lifter project that owns `tools/lower.py` (P31/P32's
string emitter) or its successor**, not `lower_c.py`: the assert is a per-site
IR statement that must sit immediately before the string statement in the
book, and `asserts.tsv` is keyed by site pc / block / statement index — the
same keys `p31.tsv`/`p32.tsv` use. Emit the `base-class` and `cond` tiers
(`needed = yes`), skip `guard` (`needed = no`), and decide on `unknown` (130
rows) by whether a firing there should detach the clone; my recommendation is
to emit them too — a count the tool could not classify is exactly where a
loud stop is worth having. Minimal set: `countflow.py`'s slot RD can drop a
`cond` row whose length word was asserted earlier in the same block with no
intervening write; not done here, listed as an option.

---

## 6. A live defect in a tool other projects may reach for

`emulation/tools/dataflow.py` `forward_dataflow()` caps its worklist at
`3 × len(blocks)` iterations and **returns whatever state it has when the cap
is hit, without reporting it**. A dataflow result that may not be a fixpoint
cannot support a proof and, worse, looks like one. `countflow.py` iterates to
the fixpoint (a change-driven worklist; the book converges in a few passes).
Not modified (`emulation/` is not mine); recommend a follow-up either removes
the cap or makes hitting it a hard error.

---

## 7. The temptation register

Every place I wanted to call something proven and did not:

1. **"Calls preserve ac0..ac2"** — I wanted to take it from P56's
   measurement. Read WRTN in `EagleStack.cpp` and `RTBridge.cpp:88-90` instead;
   both restore, both set ac3 = wfp. Construction, cited.
2. **The closure lemma from the prompt.** Its statement (`n > 0, len > 0`)
   was weaker than the code: `ac1`'s sign depends on `len` alone. Re-derived
   from the code, then checked by exhaustion over small operands — and called
   that corroboration, because a transcription is not the C++.
3. **The 593.** I could not reproduce the prompt's number by any rule and was
   tempted to find one that gave it. Reported the recount and retired it.
4. **Slot tracing and calls.** The first slot model let game callees write
   nothing; it made 40 more sites look proven. Replaced with an upward clobber
   from every by-reference argument and a policy switch, and reported the
   sensitivity rather than the better number.
5. **Policy (a) as a proof.** All 26 resolutions are circular. They are in a
   separate ledger with the circularity in each row, not folded into the 611.
6. **The guard rule's memory caveat.** A traced fact about a record field is
   trusted across the guard-to-site interval without checking for a store to
   that field. Stated in the tool's docstring and CountProof §3.4; the rule is
   reported apart from dataflow for exactly this reason.
7. **MOVE_IN_CAVE.** The tool found a write-free CFG path and I predicted it
   feasible. The path condition says otherwise; the argument is by hand (two
   tests of one word, single reaching definition checked with the tool). Tier:
   dataflow + path condition, not "the tool proved it".
8. **"No pointer loaded from memory aliases the own frame."** Assumed in the
   slot model, stated in CountProof §2; a by-reference argument pointing back
   into the caller's own frame would break it. Not seen; not proven.
9. **`sx16(trunc16(x)) = x`** and products of non-negatives: assumed no 16-bit
   overflow. Stated.
10. **P6's "dead" residues.** `x = sub(x, x)` and `x = x` read the register.
    Called them value-independent rather than "not read", and counted them
    separately.
11. **Segment containment.** Wanted to argue frames ≤ 0xD89 words and counts ≤
    32 K make it impossible. That argument needs the page layout; left
    unproven with the library's throw as the check.
12. **The induction.** Twenty of 206 base-class operands are provable through
    their writers; it would have been easy to describe the base class as
    "mostly provable, asserted for safety". It is one in ten; the assert is the
    mechanism.

---

## 8. a003 reopening — the algebraic matcher (Sep 13 2026)

One bounded fix, no new proof work. `countflow.py` now decides the base class
by the linear identity `bytes(pointer) − 2·W = 2` over the traced operands
(`lin()` / `canon()`: `wp(b, d) = b + d`, `bp(b, d) = 2b + d`, `0xW:b = 2W + b`,
constant multiples fold, anything else an opaque term by canonical spelling;
a union-valued pointer must satisfy it in every member; a tree deeper than
400 nodes is a miss, never a match). The old matcher is kept as
`base_class_match_syntactic` only to measure the movement (`countflow.out`
§a003, which lists every moved operand with its `pointer − 2W`).

| | a002 | a003 |
|---|---:|---:|
| base-class operands / sites | 227 / 213 | **266 / 251** (286 / 268 after a004's wrap fix, §9) |
| moved `cond → base-class` | | 39 (WCMV src 36, WCMP s1 3) (59 after the wrap fix) |
| moved `unknown → base-class` | | 0 (a bare length read is always `cond`, never `unknown`) |
| previously matched, now lost | | 0 |
| `asserts.tsv` rows: base-class / cond / guard / unknown | 223 / 948 / 18 / 130 | **262 / 909** / 18 / 130 |
| per-site tiers (yes / guard / cond / unknown) | 998 / 8 / 611 / 72 | unchanged — a tier of the *justification*, not of the verdict |

The asserted expression at each of the 39 is the same as before; the tier
line in the message changes (`"P58 base-class …"`), which is what a later
reader trusts. Accidental matches: none found (CountProof §4.1). Self-test
extended with 7015C90D's shape, a near miss (`2W + 4`), a static, a frame
slot, and the `ac3*2 + 0xW:b` byte-pointer spelling.

## 9. a004 — propagate non-negativity through the arithmetic (Sep 13 2026)

The rule, as implemented (`Chains` in `countflow.py`): a `cond` operand is
DERIVED when its traced count is `yes` under `judge()` with every length-word
leaf `W` treated as non-negative for which a base-class ROOT exists that
(a) is the same statement or dominates the site within the routine (the
entry's dominator tree), and (b) is value-preserving: no statement on any
path from just after the root — on to the site, past it, and round any loop
back to the root — MAY WRITE `W` (frame word: the slot model's effects;
other word: a store to the same linear address, a string destination whose
range covers it, any call, any opaque raw instruction). A WCMV `ac1` residue
leaf is covered once that WCMV's own source count is established (closure);
the pass iterates to a fixpoint (2 passes). The operator set is `judge()`'s:
`+`, `×` of non-negatives, a diamond's union (min/max), `sx16(trunc16(x))`;
subtraction stays where it was.

**Movement** (of the 909 `cond` rows in a003's `asserts.tsv`):

| | operands |
|---|---:|
| derived from a dominating root — assert dropped | **785** (WCMV dst 490, src 295; 206 of them at the ROOT'S OWN SITE: the destination count of `[@ac2, LEN] = [@W, varying]` is the same `LEN`) |
| still `cond` — assert kept | **104** at 63 sites |
| distinct length words asserted at base-class roots | **169** |
| distinct length words behind the 104 | **56**, of which 39 ARE read as a varying somewhere in the same routine (a root exists but does not dominate, or a call / unbounded copy sits between); 17 are never read as a piece (`0x70000A4E + 2`, DISPLAY_CAVE — a static varying only ever appended to) |

So the "genuinely independent memory-sourced lengths" are **169 roots + 56
uncovered = 225 length words**, not 909 — and 169 of those carry the structural
argument. The integrator's guess of "low hundreds" was right.

Two corrections found on the way, both to a003's matcher, both sound-side
(misses, never false matches):

1. **32-bit wrap.** The linear forms compared constants as Python integers;
   `LNLDA 0,[ac3+0x70151F64]` lifts to `wp(ac3, −267051164)` and the matching
   `LLEFB` to `ac3*2 + 0x70151F65:0` — the identity `2W + 2` holds modulo 2^32
   (IR.md 5.1: host arithmetic wraps). Constants now wrap. **Base class 266 →
   286 operands**; a003's table is corrected in §8 below.
2. **`R[a]` is `M32[a]`** at every site (Indirection.md §3, the three
   `ptr-bit31-clear` rows). The tracer now reads `R[wp(wfp, −12)]` as the
   argument cell it is, so DIED's `sx16(M16[R[ac3 + -12]]) + 3` chain finds
   its root `[@M32[wp(ac3, −12)], varying]`.

**Dropping the asserts is safe** under exactly the conditions the rule
checks: the root's assert executes before the derived site on every path
(dominance, or the same statement — its assert precedes it), the length word
is the same value (value-preservation), and the arithmetic is monotone. The
one assumption carried over is `sx16(trunc16(x)) = x` (no 16-bit overflow of
a stored total; the totals are wide, `XWSTA`/`XWLDA`, so the assumption is
exercised only where a total was narrowed).

**Method — a004 §4, four for four.** Every enlargement of the strongest tier
in this project came from looking at ONE concrete site and asking why it did
not fit, and every fix was to the MODEL, not to the search:

| # | fix | effect |
|---|---|---|
| 1 | `assign_varying`'s length store is a write of the count | 7 → 20 operands provable through writers |
| 2 | algebraic `bytes(ptr) − 2·W = 2` | 227 → 266 base-class operands |
| 2′ | …with 32-bit wrap, and `R[] = M32[]` | 266 → 286 |
| 3 | propagate through `+`, `×`, `min`/`max` from dominating roots | 909 `cond` → 104; 785 asserts dropped |

The instinct when a tier is too big is to reach for more analysis; here the
right move was each time to notice the tool was refusing to see something the
IR already said. Recorded as the method finding a004 asked for: **a classifier
that under-reports its best tier is not conservative — it is discarding
arguments, and the discard is invisible in its output.**

## 10. a005 — the clamped append is `assign_varying` in disguise (Sep 13 2026)

**What was done.** P59's mechanism was merged into `countflow.py` (one tool;
`compiler/onesite.py` stays as the exhaustive path-walk corroboration for the
three DISPLAY_INVENTORY sites — its 80-path enumeration agrees with the
invariant). The new pass, per site and per frame slot read by the count:

1. **the nearest dominating DEFINITE write** of the slot (dominator tree of
   the routine entry; a same-block earlier statement counts);
2. **the region**: every statement on a path from that write to the site
   that does not pass the write again — including paths that go past the
   site and loop back to it;
3. **every writer of the slot in the region maps `[0, C]` into `[0, C]`**,
   `C` the clamp constant the routine subtracts the length from (32767 when
   it never clamps). Interval evaluation of the traced value with the slot
   read kept SYMBOLIC (a third tracer mode), plus three shapes named by
   P59: the clamped append `len + min(k, C − len)` rewritten to
   `min(len + k, C)`; the single-character append `len + 1` narrowed by the
   `len < C` guard on its edge; and a pointer difference `cursor − data`
   folded by the linear form. A string destination `data + len` is shown to
   write words `≥ data` under the same invariant, so it never covers the
   length word (P59's "the pointer came from the slot itself");
4. `judge()` then signs `C′ − len` (`C′ ≥ C`) and `len` itself.

Three model gaps closed on the way, all P59's §2: a store through a
register whose reaching definition is `wp(ac3, k)` is now an exact write of
slot `k` (the register RD and the slot RD are joined); a raw `LCALL`/`XCALL`
with **zero** arguments to a non-nested routine is not a frame clobber (151
of the 206 raw-call clobbers — the callee has no pointer into this frame;
nested routines, which reach the parent frame through the static link, and
`LJSR` (the O.ON/O.REVERT/I.PROLOG/I.EPILOG condition-system entries, which
write the caller frame at places `docs/O_ON.md` does not pin down) stay
opaque); `x & 0xFFFF` is the 16-bit store's own truncation.

**Movement.**

| | operands | sites |
|---|---:|---:|
| `unknown` → proven by invariant | 16 | 8: DISPLAY_INVENTORY 70167EE1 / 70167F05 / 70167F29 (P59's three, re-derived by the tool), OBSERVE 70173253 / 70173279, DISPLAY_MAP 7016566D; and TERRITORY 7017CE70 / 7017CEA7 by a constant fold (`9 − {0, 5}`, the `x − x` arm) |
| `cond` → proven by invariant (adjacent shape: a length word whose every writer in the region is a constant or a clamped append, no clamp at the site) | 13 | 6: OBSERVE 70172DED / 70172EBE, STORE.1 7017A176, TERRITORY 7017CE4D / 7017CE84 / 7017CEB0 (WCMP), 7017CEF1 / 7017CF04 |
| `asserts.tsv` rows: base-class / cond / derived / invariant / unknown | 276 / 102 / 780 / 25 / 114 | asserts needed **492** |

P59's table, scored: DISPLAY_INVENTORY 3 → **proven**; OBSERVE 2 →
**proven**; DISPLAY_MAP 1 → **proven** (the fourth writer form, the
single-character append, verified by the edge guard); TERRITORY 2 →
**proven** (not by the invariant — the `nsub(9, {5 | x − x})` was a constant
fold the tool had refused to make); DISPLAY_SCREEN 1 → **still unknown**:
its append's piece length is slot 10's length word, and slot 10 is
`min(len(slot 1042), 10)` where slot 1042's writers are totals of other
length words — it needs slot 1042's own invariant first, i.e. the pass run to
a fixpoint over several slots (the machinery is per site, per slot; making it
mutual is the next step, not done); INIT_OBJ_TBL 2 → **still unknown**: the
destination is a shared-page record field (`wp(M32[0x70000210], 0x1EFBA)`);
the invariant needs every writer of that field in every program, which frame
privacy does not give.

**Adjacent shapes — checked.** The remaining 64 `unknown` sites (114
operands), by what blocks each:

| cause | sites | is it an inline invariant? |
|---|---:|---|
| a total slot clobbered by a copy into a scratch buffer whose count is a STATIC varying's length (+k): DISPLAY_CAVE ×9 (`0x70000A4E`), ALCHEMIST_HOME, ATTACK.1/.5, DISPLAY_MAGIC, FIRE, FIRE.3, GET_QUEST ×4, LIST_PLAYERS.2 ×3, MOVE_FAMILIAR, MOVE_PLAYER ×2, REPORT ×2, SEIGE ×3, STORE.1, OP_EDIT.3 | 30 | **yes, of a static**: the static's length word is written only by literal assignments and clamped min shapes (`0x70000A4E`: 11 writers, all ≤ 80). A `StaticBounds` pass was built for exactly this (`countflow.py`, phase A/B) and bounds every writer — and is defeated by ONE statement, `[@0x7000021D:0, ac0] = …` at 70168117:20 in DISPLAY_INVENTORY: a copy into IN_BUFFER's data whose count is a chain total the slot model cannot bound, which — with no declared size for IN_BUFFER — the pass must treat as possibly reaching every static above 0x7000021D. What would settle all 30: **IN_BUFFER's capacity** (one declaration), or the compiler-correctness assumption that a copy into a variable's data never exceeds the variable (the temporal-extent point of a002, made spatial for statics because statics are not reused). Not assumed here |
| `M8[ac2]` cursor stores into a scratch chain that my slot model clobbers upward: OP_EDIT.2 ×4, DISPLAY_CAVE (part) | 5 | the same extent question, for a frame buffer |
| `capacity − len(record field)`: DISPLAY_MAGIC ×2, HELP.1, STORE.1 ×3, INIT_OBJ_TBL ×2 | 8 | a record-field capacity: the writers are in every program |
| argument cells (INIT_OBJ_TBL ×3, OP_EDIT.9, TERRAIN, TERRITORY ×3) | 8 | interprocedural |
| DISPLAY_SCREEN ×4 (slot 1042 / 998 / 1046 — the appends whose piece length is another varying's length) | 4 | yes — a MUTUAL invariant over three slots; the pass is per slot |
| products (TAKE_OVER_CASTLE ×2), a `t1` scratch (CAST.2, TERRITORY_MAP ×2), UNSIGNED_TO_CHAR's ac2 through a loop (SEIGE ×2), DISPLAY_INVENTORY 70168143 | 9 | small, listed |

So P59's judgement from one routine holds in shape but not in size: the
biggest remaining class IS an inline invariant — of a static, not a frame
slot — and it is blocked by one declaration this book does not carry, not by
analysis.

**Method — six for six** (a005 §4): the fixes were (1) the length store is a
write of the count, (2) the algebraic layout test, (3) its 32-bit wrap and
`R[] = M32[]`, (4) propagation through arithmetic, (5) the clamped append is
capacity-preserving, and on the way (6) a zero-argument raw call cannot reach
the frame. Each was found at one concrete site by asking why the tool did not
see what the IR said, and none was a better algorithm. The user's reflex — *the
game works, so we are probably not reading it carefully enough* — has been
right every time; the reflex to reach for more analysis when a tier looks too
large has been wrong every time. Recorded as the method finding.

## 11. a006 — IN_BUFFER's capacity, checked end to end (Sep 13 2026)

**The five links, verified — and link 2 is wrong twice.**

1. `quest.symbols`: IN_BUFFER 0x7000021C, next symbol OUT_CHAN 0x70000260 —
   0x44 words = 134 data bytes. **Holds** (cross-check only).
2. "Exactly one routine fills it." **False.** The tool's writer census
   (`StaticBounds`, which now sees game calls' by-reference arguments) finds
   **27 writers**: `?READ_SCREEN` ×1 (70176716); **TERRAIN ×13 and
   TERRITORY ×6** — IN_BUFFER is their CHAR(*) VARYING OUTPUT argument (arg 4
   / arg 3), pushed with a DESCRIPTOR argument (arg 9 / arg 6: words `0xB00,
   0x0000, 0x0084` at 0x7015BF19, 0x70160B17, … — twelve descriptors, all
   132 when IN_BUFFER is the target; the one 39 goes with a local buffer);
   and **DISPLAY_INVENTORY ×7** writing it directly — four literal
   assignments ("flying on a pegasus" 19, "exploring a cave" 16, "at home
   in your castle" 22, "sailing a boat" 14), `"carrying " || name` at
   70168117 (≤ 18, see the table finding below), `x − x = 0` at 70168146 —
   plus READ_IN.1's zero store. The grep that found two shapes missed the
   `LPEF [0x7000021C]` argument pushes (`M32[0x74009926] = 0x7000021C`).
3. The literal 132 at 7017670A → `M16[wp(ac3, 12)]` → arg 3. **Holds**; the
   tool traces the slot to the constant.
4./5. `OperatingSystem.cpp:284-296` sizes the buffer at `max_length` and
   floors; `read_screen.cpp` writes the count into the header in place.
   **Holds** (read, not re-derived: runtime C++ is outside this tool).

**And the bound survives**, because every one of the 26 other writers is
clamped to 132 or below: TERRAIN's single write (7017CA42) and TERRITORY's
three (7017CECC / 7017CF1A / 7017CF2D) are all the min shape
`{M32[wp(desc, 1)] | total}` against the descriptor's capacity word —
verified by hand from the traced values, encoded as `CALLEE_CAPPED_OUTPUT`
and applied at each call site with the descriptor's capacity read from
`quest.mem` (132) — and DISPLAY_INVENTORY's are ≤ 22.

So: **`0 ≤ len(IN_BUFFER) ≤ 132`, established from all 27 writers**, not from
one. The `quest.assumptions` row a006 drafted must say so:

    in-buffer-cap-132   0x7000021C   -   compiler/countflow.py (StaticBounds)
    #   IN_BUFFER (static CHAR VARYING, data at 0x7000021D) has length <= 132 whenever it is read.
    #   WRITERS (27, all bounded): ?READ_SCREEN @70176716 with max_length the literal 132 (block
    #   7017670A; the runtime sizes its buffer at max_length and floors a negative amount,
    #   emu_types/OperatingSystem.cpp:284-296); TERRAIN x13 (arg 4) and TERRITORY x6 (arg 3), each
    #   clamping its output to the caller's DESCRIPTOR capacity (arg 9 / arg 6; words 0xB00,0,0x84
    #   in the image), min shapes at 7017CA42, 7017CECC, 7017CF1A, 7017CF2D; DISPLAY_INVENTORY x7
    #   (four literals <= 22, "carrying " || familiar-name <= 18, x - x = 0); READ_IN.1 x1 (0).
    #   Cross-check: the object spans 0x7000021C..0x70000260 = 134 data bytes.
    #   FALSIFIED BY: a 28th writer (countflow.py's census changing); a descriptor with capacity
    #   > 132 passed with 0x7000021C; a ?READ_SCREEN max_length that is not the literal 132.

**The finding on the way, and it is the bigger one.** DISPLAY_INVENTORY's
`"carrying " || name` writer is bounded only because the name's length word
is `M16[idx·16 + 0x70150A5A]` — a **constant table in the program image**
(the familiar names: "Sara", "Gwendolyn", …, "Matilda"; 10 entries, 16 words
each), indexed under the compiler's own DERR guard `1 ≤ idx ≤ 10`, whose ten
length words are all in `quest.mem` (max 9), and which nothing in the book
writes (a constant-address write census: stores, string destinations, by-
reference arguments, callee write positions). The same shape is the spell
table at 0x70150448 (37 entries × 16 words, max 24), the help-line table at
0x70000272 (12-word records), the shop table at 0x7000058D (9-word records)…
**This is the "record-field capacity" class of §2 — most of it is not the
shared page at all; it is constant tables in the image, and the DERR guard
gives the index range.** A006's question ("is the bound a literal somewhere
nobody looked?") answered in the affirmative for the class, not just for
IN_BUFFER: `table_bound()` in `countflow.py` (§13 of its docstring).

**Movement.**

| | sites |
|---|---:|
| `unknown` → proven by an image-table bound (`C − len(table)` padding, and copy-out totals `len(table) + k` whose scratch copies no longer clobber the total slot): DISPLAY_CAVE ×4, ALCHEMIST_HOME, ATTACK.1, GET_QUEST ×3, LIST_PLAYERS.2, MOVE_PLAYER ×2, REPORT, SEIGE ×2, DISPLAY_MAGIC, HELP.1 … | **16** (64 → 48 unknown sites) |
| `cond` → proven by an image-table bound (the length word itself is a constant in the image) | 63 sites carry a table-bounded operand; asserted-only 125 → 121 sites |
| static length words bounded | IN_BUFFER ≤ 132, 0x70000A4E ≤ 80, 0x70000A78 ≤ 80 (a whole-varying copy of A4E), two zero statics |
| asserts needed | 492 → **466** |

Two more model fixes fell out (P59-style, one site each): `DIVX` writes ac0
and ac1 only (the tracer had it clobbering ac3, which hid a whole chain
behind a divide), and the lin/canon pair looped on non-arithmetic binary
operators (`&`), so any tree containing one was "too deep" and unmatched.

**What still blocks the statics.** 0x70000C34 / 0x70000AA2 / 0x70000C1E /
0x70000C44 (DISPLAY_SCREEN's message statics): a copy at 70167085:3 whose
count is a DISPLAY_SCREEN chain total — the same mutual-invariant knot as
DISPLAY_SCREEN's four unknown sites.

**The 48 remaining unknown sites** (default policy; 40 under policy (a)):
scratch-buffer copies of a PLAYER/OBJ **shared-page** record field (ALCHEMIST_HOME
7015CB5C, ATTACK.5, DISPLAY_CAVE ×4 via `M8[ac2]` cursors, FIRE ×2, GET_QUEST,
LIST_PLAYERS.2 ×3, MOVE_FAMILIAR, OP_EDIT.2 ×4, OP_EDIT.3, REPORT, SEIGE,
STORE.1) — the record's declared capacity is in `shared_data_layout.h`
(`name_length … VARYING(32)`) as an inferred layout, not a checked fact;
DISPLAY_SCREEN ×4 (mutual invariant); INIT_OBJ_TBL ×6, OP_EDIT.9, TERRAIN,
TERRITORY ×3 (argument cells — interprocedural); STORE.1 ×3 (`16 −
len(record field)` on the shared page); TAKE_OVER_CASTLE ×2 (products);
CAST.2, TERRITORY_MAP ×2 (`t1`); DISPLAY_MAGIC 70166536. The next literal
nobody looked at is probably the PLAYER record's name field: `VARYING(32)`
per the layout header, and every writer of it in the book is an
`assign_varying` — the same census, on the shared page, with the
NEW_USERS.PR caveat.

**Method, seven for seven** (a006 §4) — recorded. This one was three
questions and four greps, and the tool then found that the answer was
incomplete (27 writers, not 1) and still true.

## 12. Files

    docs/Project58/q001-plan-gate.md      the gate (unchanged after a001)
    docs/Project58/CountProof.md          the result of record
    docs/Project58/REPORT.md              this
    docs/Project58/sites.tsv              3,366 operand rows, default (sound) policy
    docs/Project58/sites-infercaps.tsv    the same under a001 Q2 policy (a)
    docs/Project58/asserts.tsv            1,226 rows: site, operand, tier, needed, root, exact assert text (a006: 466 needed; 711 derived, 25 invariant, 18 guard need none)
    docs/Project58/countflow.out          console output, default policy (the per-site lists)
    docs/Project58/countflow-infercaps.out
    compiler/countflow.py                 the Stage A tool (new file)
