# Project 56 — REPORT: THE FIRST MATCH

Worker session, Sep 12 2026, branch `p56-transformer`. Gate
`q001-plan-gate.md`; rulings `a001` (Q1(a), Q2(a), **Q3 overruled to (c)**).
The rule set is `Rewrites.md`; this is what it cost and what it means.

**Modified: nothing.** Written: `compiler/{irparse,irmatch,match_routine,
ldafp_census,monotonicity}.py`, `compiler/run_p56.sh`, `docs/Project56/*`,
`docs/Project56/oracles/HIT_ANY_CHAR.oracle`. Not touched: `lower_c.py`,
`game/**`, `emulation/**`, `docs/IR.md`, `docs/Project44/DESIGN.md`, any
artifact. No `.c` changed. Nothing executes; this compared two texts.
`docs/attic/` was not opened.

**To reproduce everything: `cd Work && sh compiler/run_p56.sh`.**

---

## 1. The outcome against the prompt's criteria

| required | delivered |
|---|---|
| **the seven ranked by residual difficulty** at operator granularity, and the simplest identified | q001 §1. **HIT_ANY_CHAR** — the only routine with a 1-1 block correspondence *and* an isomorphic CFG |
| **that routine matches the book exactly**, via an oracle and a rewrite set — or what stopped it | **It matches.** Distance 0, 4 blocks, 10 statements, modulo the one normalised prologue line (a001 Q1(a)) |
| **`Rewrites.md`** — every rule with precondition, justification, application count | `Rewrites.md`. **4 rules, 6 applications, oracle length 4** |
| **the monotonicity measurement** (§7.2b), pre-registered and reported either way | §4. **Monotone in both orders. My mechanism was wrong** |

---

## 2. The match

```
oracle length (placement decisions, DESIGN §5.1) : 4
oracle rewrite lines                             : 1
RESULT: MATCH — distance 0
```

All four blocks, statement for statement:

| | ours (transformed) | book (normalised) |
|---|---|---|
| b0 | `[@0x740056FA, 0x1E varying] = [@0x7016DE07:0, "\x0BHit any character to continue"]` | *identical* |
| | `rt_call ?WRITE_SCREEN(0x70000260, 0x740056FA)` | *identical* |
| b1 | `M32[0x74003F1C] = 0x740056F8:0` | *identical* |
| | `call 7016AA35 args=0x1` | *identical* |
| b2 | `ac1 = 0x20D0B` | *identical* |
| | `M32[0x740056FA] = ac1` | *identical* |
| | `rt_call ?WRITE_SCREEN(0x70000260, 0x740056FA)` | *identical* |
| b3 | `ret` | *identical* |

**The line worth looking at twice is `M32[0x74003F1C] = 0x740056F8:0`.** Our
side reaches it by placing `v0` at `frame+2` and taking `bp(v0, 0)`; the book
reaches it from `bp(ac3, 4)`. Those agree only because `bp` displacements are
**bytes** while `wp` displacements are **words** (IR.md §5.2), so the book's
`4` in `bp(ac3,4)` is frame word 2 and its `4` in `wp(ac3,4)` is frame word 4 —
two different objects that spell the same digit. Two independent derivations of
`0x740056F8` through that asymmetry is the single strongest piece of evidence
in the match that the slot bijection is right rather than fitted.

### 2.1 The comparator has teeth, and they are in the repro script

A comparator that always says MATCH is worthless, so five deliberate errors
were injected. All five are caught:

| injected error | result |
|---|---|
| `v0` misplaced by one word | 1 divergence, distance 1 |
| the `v2`/`v4` merge broken | 1 divergence, distance 2 |
| an illegal merge (overlapping live ranges) | **HARD ERROR** at the precondition |
| the R3 rewrite removed | 1 divergence, distance 2 |
| a placement omitted | 1 divergence, distance 2 |

---

## 3. What this match does NOT prove, measured rather than hedged

q001 §5.1 said HIT_ANY_CHAR exercises none of the big rewrite classes and that
closing it validates the rig rather than the transformer. That was a
qualitative claim. It is now a measured one, and it is **stronger than I
wrote**.

**Every soundness mutation `lower_c.py` can inject leaves HIT_ANY_CHAR's own
blocks byte-identical.**

| mutation | whole compilation unit | HIT_ANY_CHAR's blocks |
|---|---|---|
| `no_sign_extend` | differs | **identical** |
| `cmp_unsigned` | differs | **identical** |
| `bp_scaled` | differs | **identical** |
| `u16_unsigned`, `abs_argtype`, `eager_bool`, `shift_logical` | same | **identical** |
| `byte_as_word` | compile refused | — |

The mutations land in GET_INPUT, which shares the compilation unit and is not
compared. **HIT_ANY_CHAR cannot distinguish a single one of the eight
available soundness bugs**, because it contains no arithmetic, no comparison,
no sign extension and no byte/word conversion. It is two string assignments
and a call.

This is P48 F5's named residual in a new place: the thing under test contains
nothing capable of distinguishing right from wrong. **So the match proves the
rig works and proves almost nothing about the C.** It is still the right first
case — the prompt's argument stands — but the claim it supports is narrow and
should not be quoted wide.

Combined with `docs/Indirection.md`'s census (HIT_ANY_CHAR 0 `R[]` sites,
GET_INPUT 0), the first *two* cases test none of the program's largest rewrite.
**Rank 3 is where the transformer is first tested at all.**

---

## 4. Monotonicity — the prediction was wrong, and the mechanism is the finding

q001 §7 pre-registered five claims. Both orders were replayed.

```
ORDER A — shipped (placement first, R3 last)      8 → 6 → 5 → 4 → 3 → 2 → 0
          increases 0, plateaus 0
ORDER B — reordered (R3 first)                    8 → 5 → 5 → 4 → 3 → 1 → 0
          increases 0, plateaus 1
```

| claim | result |
|---|---|
| 1. placement rewrites strictly decrease distance | **correct** — every R1 step is −1 or −2 |
| 2. **R3 before its target is placed INCREASES distance by 1** | **WRONG** — it *decreases* it by 3 |
| 3. R3 after placement decreases by 2 | correct (−2) |
| 4. shipped order: 0 increases, 0 plateaus | **correct** (after §4.2) |
| 5. reordered: ≥1 increase | **WRONG** — 0 increases |

**Answer to DESIGN §7.2b: distance is monotone (non-strictly) under single
rewrites, in both orders tested. This is a walk, not a search.**

### 4.1 Why claim 2 was wrong, which is the part worth keeping

I reasoned: R3 turns one mismatched statement into two mismatched statements,
so distance goes 1 → 2. Every word of that is true and the conclusion does not
follow, because **I was counting mismatched statements and the metric counts
edits.**

Before R3, our block has 2 statements against the book's 3, so the edit
distance was already paying **an insertion** on top of a substitution. R3 makes
the counts agree, which removes the insertion, *and* the statement it
introduces (`ac1 = 0x20D0B`) is exactly right on the spot because the constant
is computed from our own declaration. So the step is −3, not +1.

The general form, and it inverts my prediction: **a rewrite that changes
statement count toward a longer target reduces distance rather than increasing
it.** Statement-count-changing rewrites are the *safe* ones for a walk, not the
dangerous ones. Everything §7.2b needs to worry about is elsewhere.

This is P55 Part 3's precedent repeating: the prediction was wrong, and the
ordering discipline I had pre-registered as the fix — "statement-count-
preserving rewrites first" — would have imposed a constraint against a hazard
that does not exist. Filing the prediction is what makes that visible instead
of leaving a plausible story standing.

### 4.2 The replay found a dead oracle line

`place GET_INPUT.a1 argcell` — in q001 §6's worked example — moved the distance
by **0** in both orders. R2 already derives the argument cell from the addrbook
(`wfp−10−2N`), so the oracle was supplying something the model explains.
Removed. **Oracle length 5 → 4.**

Worth stating because oracle length is DESIGN §5.1's headline metric and it is
supposed to fall as the model improves. Here it fell by 20% because a
measurement caught a line doing nothing — which is the metric behaving as
designed, and an argument for running the replay on every routine rather than
only where §7.2b is in doubt.

---

## 5. Two bugs in my own analysis, both caught by measurement

Recorded because in both cases the wrong version **still matched
HIT_ANY_CHAR**, which is the failure mode DESIGN §7 ruling 4 is about.

### 5.1 A must-analysis initialised at bottom

`frame_holders` joins with intersection and I initialised the out-states to the
empty set. On straight-line code that is invisible; on anything with a back
edge the loop predecessor contributes ∅ on the first pass and the intersection
can never grow again, so the analysis concludes the frame is never held
anywhere.

HIT_ANY_CHAR is straight-line, so **the match was unaffected and the bug was
invisible there.** It surfaced only when the normalisation was run across all
102 routines: 17 normalised clean. After the fix, 35.

The general lesson is the one P55 §8 states about detectors: running the tool
on the one case it was written for tells you nothing about whether it works.

### 5.2 DESIGN §5.2's inverted check false-positives on every record base

§5.2 says:

> If a `wp(r, d)` resolves against a base the analysis says is not the frame,
> while `d` lands in frame territory for that routine, then either our
> base-tracking is wrong or the original compiler emitted something odd. Either
> way it stops the build.

Implemented literally, it stopped the build on 57 routines. The first one I
looked at is not odd at all:

```
ac2 = M32[0x70000210]              ; SD_PTR — a record base, provably NOT the frame
ac0 = sx16(M16[wp(ac2, 43)])       ; a field of the shared-data page
```

Displacement 43 lands inside QUEST's frame territory, so the check fires. But
ac2 provably holds something else, and **a record base with a small positive
displacement is textually indistinguishable from a frame reference.** Salvage
F23 counts 19,344 base uses and most of them are exactly this.

**The check is only evidence when the base's provenance is UNKNOWN** — not when
it is provably something other than the frame. §5.2 as written does not make
that distinction and needs to. With the refinement (a second must-analysis of
"this register has a visible definition in this routine"), clean normalisations
go **35 → 63 of 102**, and the 22 remaining N2 refusals are genuine unknown-base
cases — the hazard §5.2 was actually pointing at.

---

## 6. What did not survive contact with DESIGN §7

q001 §8 filed four. All four stand, and the census and the implementation
sharpened two of them.

1. **F1 — §7 has no class for unlifted machine instructions.** 557 in the book,
   ≥1 in every one of the seven. a001 grants **normalisation of the target** as
   a fifth category and adopts the discipline: *a normalisation must check
   something, not merely delete*. N1 checks 130 WSAVS operands against the
   addrbook (100/102 agree, 0 mismatches); N3 checks 1,774 LDAFP sites and
   **refuses at 147 of them**.
2. **F2 — §7.2b's distance presupposes a block bijection** that exists for one
   routine of seven. `irmatch` refuses rather than reporting a number.
3. **F3 — §7.4 anchors our sites and says nothing about the book's.** N4 is the
   missing half and belongs in §7.4.
4. **F4 — §7's reasoning is over statement text and the string statements are
   lossy.** Now quantified: **1,463 of 1,774 LDAFPs (82%) exist because of the
   ac0–ac3 residues that IR.md §5.8 documents and the statement text does not
   name.** Any §7 transformation with a register precondition — binding above
   all — is exposed, and the exposure is silent.

**New, from §5.2 above: F5 — §5.2's inverted check needs a provenance
distinction it does not have.** Reported against §5.2 rather than §7, with the
measurement in §5.2.

**And one thing that DID survive and deserves saying:** DESIGN §5.1's claim
that by-reference parameters are written by the calling bridge rather than
placed is exactly right, and R2 is that bridge. It is the reason the oracle
lost a line in §4.2.

---

## 7. Temptation register

Carried from P55 as standing practice. Four, and the third is the one that
mattered.

1. **Reporting 383 N3 blockers instead of 147.** My first census detector
   counted `R[ac3 + -12]` — the book's spelling for argument slots, 939 of them
   per Indirection.md §2 — as a value read rather than a frame-relative base.
   It is a base, and N2 rewrites it. Widening the detector moved the number
   from 383 to 147. **Declined to publish the first figure**, and the general
   form is P55 REPORT §8's: a census is only as good as its detector, and a
   detector needs its own witness count before its output becomes a number in a
   document.
2. **Silently dropping the `@7016DE91 WSAVS` line.** It would have made the
   report say "matches exactly" with no qualifier. Held — a001 ruled the phrase
   in advance precisely so it could not be negotiated here, and the matcher
   prints the residual on every run rather than at the end.
3. **Keeping `place GET_INPUT.a1` because removing it felt like losing a
   result.** The most tempting, because oracle length 5 was already written into
   the gate and a line that does nothing still *looks* like understanding.
   Declined, and the metric improved by declining: a supplied fact that the
   model can derive is not oracle length, it is noise in the headline number.
4. **Relaxing the live-range check so it would not hard-error on routines with
   back edges.** The linear approximation would have "worked" on the next
   routine or two and been wrong silently on the first loop that reuses a
   temp — which is DESIGN §5.3's silent-corruption case. Held; it refuses, and
   a real liveness analysis is rank 3's first task.

---

## 8. Files

Branch `p56-transformer`; `q*.md` also on main. Written:

| file | what |
|---|---|
| `compiler/irparse.py` | one parser for ir 7 and ir 8; the def/use model, including §5.8's clobbers |
| `compiler/irmatch.py` | N1–N4, R1–R4, the bijection, the lockstep walk, the distance metric |
| `compiler/match_routine.py` | the driver |
| `compiler/ldafp_census.py` | a001 Q3(c) |
| `compiler/monotonicity.py` | DESIGN §7.2b |
| `compiler/run_p56.sh` | reproduces all of the above |
| `docs/Project56/oracles/HIT_ANY_CHAR.oracle` | 4 placements, 1 rewrite |

`compiler/ircmp.py` was **not ported** and not read beyond its header: it parses
ir 6, the book is ir 7, we emit ir 8, and a lockstep walk over a verified
bijection needs no alignment machinery. P55 reused the idea rather than the
code and this did the same.

---

## 9. For whoever is next

1. **Rank 2 is GET_INPUT and its equal block count is a coincidence** (q001
   §1.2). Its shape — a skip over a lone `WRTN` — has **one witness book-wide,
   and it is GET_INPUT.** No rule may be built for it. The obvious hypothesis
   ("the book duplicates returns rather than jumping to them") was censused and
   is **false**: the book jumps to a bare `ret` 67 times.
2. **Rank 3 needs a real liveness analysis** before R1's merge precondition can
   fire on it (§7 item 4), and it is the first case that tests the transformer
   at all (§3).
3. **R3's book-wide census was not run.** Sizing it needs a detector for the
   `WLDAI`/single-wide-store shape. Recorded as unmeasured rather than
   estimated.
4. **The 22 remaining N2 refusals are the real §5.2 hazard** and are the
   measure of how much of Salvage F23's two-register dataflow is still unbuilt.
5. **Run the monotonicity replay on every routine**, not only where §7.2b is in
   doubt. §4.2 is the argument: it is the cheapest available detector for
   oracle lines that supply what the model already explains.
