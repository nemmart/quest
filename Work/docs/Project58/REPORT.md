# Project 58 — REPORT (Sep 13 2026)

Result of record: `docs/Project58/CountProof.md`. Ledgers: `sites.tsv`
(default, sound policy), `sites-infercaps.tsv` (a001 Q2 policy (a)),
`asserts.tsv` (the deliverable a001 asked for). Tool: `compiler/countflow.py`
(new; `--selftest` passes). Boundaries kept: nothing in `emulation/**`,
`compiler/lower_c.py`, `game/**`, `docs/IR.md`, `docs/StringModel.md`, DESIGN
or any artifact was changed; no assert was emitted; nothing executed.

---

## 1. The answer in one table

| tier | sites | operands | what discharges it |
|---|---:|---:|---|
| proven by construction | 998 | 2,047 | nothing to do |
| proven by a dominating guard (a001 Q3, reported separately) | 8 (18 operands, 12 sites touched) | 18 | nothing to do |
| conditional on named length words — the assert class | 611 (637 under policy (a)) | 1,171 | `asserts.tsv`, tiers `base-class` (223 rows) and `cond` (948 rows) |
| unproven | 72 (46 under policy (a)) | 130 | `asserts.tsv`, tier `unknown` (130 rows); the assert still discharges it at runtime |
| negative by construction | **0** | 0 | — |

The counts are signed and the direction is decided by them (PROMPT §Why): at
1,006 sites the copy is forward by proof; at the other 683 it is forward iff
the site's assert holds — the same condition, stated once, checked at runtime.

---

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

## 8. Files

    docs/Project58/q001-plan-gate.md      the gate (unchanged after a001)
    docs/Project58/CountProof.md          the result of record
    docs/Project58/REPORT.md              this
    docs/Project58/sites.tsv              3,366 operand rows, default (sound) policy
    docs/Project58/sites-infercaps.tsv    the same under a001 Q2 policy (a)
    docs/Project58/asserts.tsv            1,319 rows: site, operand, tier, needed, exact assert text
    docs/Project58/countflow.out          console output, default policy (the per-site lists)
    docs/Project58/countflow-infercaps.out
    compiler/countflow.py                 the Stage A tool (new file)
