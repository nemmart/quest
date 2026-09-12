# Project 56 — REWRITES

Every rule the first closure actually forced into existence, with its real
precondition and its real application count. Nothing here is speculative: a
rule is in this file only if removing it breaks the HIT_ANY_CHAR match, and the
teeth in `compiler/run_p56.sh` §4 demonstrate that for the two that are
oracle-supplied.

**Rule count: 4 rewrites.** Against DESIGN §7.2a's ~20 tripwire that is four
entries. **Applications: 6.** Oracle length: **4 placement decisions.**

Normalisations are counted separately and are *not* rewrites: they act on the
target, carry no soundness obligation, and a bug in one mismatches loudly
rather than breaking the proof. a001 grants the category and fixes its rule:

> **a normalisation must CHECK something, not merely delete.**

---

## The rewrites

### R1 — placement (`v` → address)

    place <v> frame+N | 0xW:b | <addr>

**Precondition.** For two `v`s placed on the same address, their live ranges
must be disjoint (DESIGN §5.3). Proved or **HARD ERROR** —
`compiler/irmatch.py:_check_merges`.

**A second precondition the first case forced, which DESIGN does not state.**
The live-range check is computed over a *linear* statement order, which is
sound only without back edges. `_live_range` therefore **hard-errors on any
routine containing one** rather than approximating. HIT_ANY_CHAR is
straight-line; rank 3 onward is not, and this is the first thing that has to
be built there.

**Justification.** Oracle-supplied by design (DESIGN §7.3). Oracle length is
the count of unexplained placement decisions and is the headline metric.

**Applications: 3** — `v0` → frame+2, `v2` → frame+4, `v4` → frame+4 (merged
onto `v2`). Plus `v1` → `0x7016DE07:0`, which R4 consumes.

| `v` | placed | what it is |
|---|---|---|
| `v0` | frame+2 | `ch`, CHAR(1), the even pair 2–3 (Salvage F22) |
| `v2` | frame+4 | the 30-byte varying temp |
| `v4` | frame+4 | the same temp — **merge** |
| `v1` | `0x7016DE07:0` | the literal, in the code image |

**One line came off this list during the work and the removal is the result.**
`place GET_INPUT.a1 argcell` was in q001 §6's worked example. The monotonicity
replay showed it moving the distance by **0**: R2 already derives the argument
cell from the addrbook (`wfp−10−2N` = `0x74003F1C`), so the oracle was
supplying something the model explains. **Oracle length 5 → 4.** This is the
metric working in the direction DESIGN §5.1 wants — it falls as the model
improves, and here it fell because a measurement caught a line doing nothing.

---

### R2 — the arg-cell bridge

    E.aN = X        ->  M32[<E's arg cell>] = X
    E.arg_count = n ->  deleted (folded into the call decoration)
    call E args=n   ->  call <E's entry address> args=n

**Precondition.** `E` is an addrbook entry; `n` agrees with the `args=` field.
The cell is `wfp − 10 − 2N` (IR.md §6), derived, never supplied.

**Justification.** DESIGN §5.1: by-reference parameter `v`s are outside the
placement count because the original's arguments live at `wfp−10−2N` with no
0x74 cell to lay them onto — **the calling bridge writes them.** This rule is
that bridge. It is not oracle-parameterised and therefore costs no oracle
length.

**Applications: 2** (the argument write and the `arg_count` fold).

---

### R3 — packed varying immediate

    [@wp(X,0), n varying] = [@bp(K,0), n]
        ->  acR = (n << 16) | <the bytes>
            M32[wp(X,0)] = acR

**Preconditions, all hard errors.** `n ≤ 2`; `K` is a compile-time constant
with a visible initialiser; `X`'s declared capacity `≥ n`; `acR` free.

**Justification.** The book stores a 2-byte varying as one wide immediate —
`ac1 = 0x00020D0B`, length 2 in the high half and `0D 0B` in the low. This is
**not** derivable from the source, so it belongs to the oracle, exactly as
register binding does.

**It was named before it was measured.** `game/routines/HIT_ANY_CHAR.c`'s own
header says: *"the second literal is a folded immediate … Stage 3 must name
this: CHAR constant <= 2 bytes -> WLDAI + one wide store"*. The P51 session
derived this rule from the disassembly; P56 confirmed it rather than inventing
it.

**Applications: 1** here. **Book-wide census not run** — sizing it needs a
detector for the `WLDAI`/single-wide-store shape and P56 did not build one.
Recorded as unmeasured rather than estimated.

---

### R4 — literal relocation

    [@bp(v,0), n]  ->  [@0xW:b, "<text>"]

**Precondition.** `v` is an initialised, read-only `char n` declaration, and
the oracle has placed it at a byte-pointer literal. The text comes from **our**
declaration's initialiser, not from the book — which is what makes the match at
that statement evidence rather than a copy.

**Justification.** String literals live in the code image, not in 0x74. This is
placement into a different address space plus a form change to §5.8's located
piece. q001 §5 predicted R4 "may collapse into R1"; it partly did — the address
is R1's, only the form change is R4's — but the form change is real, so the
rule stands.

**Applications: 1.**

---

## The normalisations (of the target)

| id | what | the check it performs | applications here |
|---|---|---|---:|
| **N1** | strip the entry `@<addr> WSAVS 0x00NN` | NN equals the addrbook frame column. **100/102 live entries agree, 0 mismatches**, 2 have no entry WSAVS | 1 |
| **N2** | `wp/bp(acN, d)` and `R[acN + d]` → absolute | acN provably holds the frame; unknown-provenance base in frame territory is a hard error | 4 |
| **N3** | delete `acN = wfp` left with no reader | **tested per site** — see below | 1 |
| **N4** | drop `site=`/`marker=`/`ret=`; map `goto` labels through the bijection | every label is in the bijection; successor structure is checked by the CFG isomorphism, not by statement text | 4 |

### N3 is conditional, and the census is why

a001 overruled q001's recommendation of "delete and report" and ordered the
census first. **It was right.** `compiler/ldafp_census.py`:

| | |
|---|---:|
| `acN = wfp` statements book-wide | **1,774** |
| ...on ac3 / ac2 | 1,683 / 91 |
| **N3-safe** (every reached use is a base spelling N2 rewrites) | **1,627 (91.7%)** |
| **BLOCKED** — acN is read as a VALUE | **147 (8.3%)**, in 21 of 102 routines |

The blockers are `ac1 = ac3` (the frame copied into a second base) and
`acX = add(acX, ac3)` (frame-relative arithmetic the lifter did not fold) —
DESIGN §5.2's two-register pressure, not an anomaly.

**So N3 tests and refuses; it never deletes on sight.** Shipping q001's
recommendation would have been wrong at one site in twelve and correct in
HIT_ANY_CHAR by luck.

**The census also confirmed the clobber rule is load-bearing:** 1,463 of 1,774
LDAFPs (82%) sit immediately after a folded string statement — they exist
*because* of the ac0–ac3 residues that IR.md §5.8 documents and the statement
text does not name.

**And it settled a fact that was being assumed.** 723 N3-safe defs have their
base uses after a call with no intervening definition. If calls clobbered
ac0–ac3 the book would reload the frame after every one and those 723 sites
could not exist. **Calls preserve ac0–ac3**, consistent with WSAVS saving
`ac0 ac1 ac2 wfp ac3|c` and WRTN restoring them.

---

## Named unmatched residual (a001 Q1(a))

HIT_ANY_CHAR: **one line**, reported by the matcher on every run.

    7016DE91  unlifted instruction: @7016DE91 WSAVS 0x0009;

Per a001's ruling, the claim is **"matches the book exactly, modulo the
normalised prologue line, reported"** — never the unqualified phrase.

Book-wide there are 557 such lines (159 `LCALL`, 130 `WSAVS`, 130 `LJSR`,
37 `XCALL`, 19 `DIVX`, 82 others). Only the entry `WSAVS` is normalised,
because only it has an independent check. Everything else is a residual.
