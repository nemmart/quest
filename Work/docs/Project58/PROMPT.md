# Project 58 — PROVING THE STRING COUNTS

## GOAL

**Establish what can be PROVEN about the counts passed to `WCMV`, `WCMP` and
`WBLM`, and characterise precisely what cannot.**

The obligation, as it stands:

> at every string site, `ac0 ≥ 0` and `ac1 ≥ 0`

**It is already known to be FALSE**, and that is the starting point rather than
a problem: `DISPLAY_INVENTORY 7016816B` runs `cmp` with a source length of −1,
because the record field holds `0xFFFF` and the compiler reads length words
**sign-extended** (P31 Census F-B1). So the real question is not *is it true*
but **what is the true statement, and what does it rest on.**

This is a **proof and census project**. You write no compiler code and change
no artifact.

**Success = `docs/Project58/CountProof.md`:** for every one of the ~1,689
sites, which evidence class it falls in, what is proven, and — for the residue
— what would have to be true and how it could be established.

---

## Why this matters

Every rewrite touching a string statement inherits these counts. The signs
decide:

- **direction** — `EagleString.cpp:69–70` takes `dst_direction` and
  `src_direction` from the two counts **independently**, so all four
  combinations are expressible. Forward is *not* structural.
- **blank fill vs truncation** — the loop is bounded by `dst_count` alone
  (`while(dst_count != 0)`), so `ac0` governs how many bytes are written
- **the residues** — `ac1 = len − sgn(len)·t`, `ac3 = src + sgn(len)·t`

`docs/Project29/StringsDesign.md` asserts *"Quest uses only forward copies at
string sites"* — **that is one session's reading, not a proof**, and it is
exactly the single-witness shape this project has been burned by repeatedly
(see `docs/attic/README.md`).

**Precedent: `docs/Indirection.md`.** The `R[]` precondition looked like a wall
and turned out to be provable at 1,008 of 1,035 sites by construction, with the
remaining 27 settled by tracing three single writers. **That is the standard.**
But do not assume this one lands the same way — the distribution is worse (§3).

---

## Context of record

| path | why |
|---|---|
| `emulation/hw/strings/EagleString.cpp` | **the implementation.** `copy()` :60–90, `residues_after_copy()` :152–163, `compare()` :~190–205, `block_move()` :111–130 |
| `docs/IR.md` §5.8 | the string statements and their documented residues |
| `docs/Project29/StringsDesign.md` | the claims you are testing, especially the tail-split argument |
| `docs/Project31/Census.md` §10 F-B1 | **the known counterexample**, in full |
| `docs/Indirection.md` | the model for this kind of proof |
| `emulation/quest.assumptions` | where a provenance-based fact goes, and the requirement that it be CHECKABLE |
| `emulation/quest.ir2.book`, `quest.mem` | the corpus and the initialised data |
| `docs/StringModel.md` | the parked model these counts constrain |

**Do not read `docs/attic/`.**

---

## The distribution — measured, and worse than `R[]`'s

Of the `WCMV`/`WCMP` sites, classified by the two count operands:

| class | sites | prospects |
|---|---:|---|
| both counts literal constants | **5** | provable by construction, trivially |
| at least one count a register | **154** | dataflow over the defining statement |
| **a `varying` length** | **593** | **the length word's VALUE — runtime data** |
| other (literal sources, mixed constants) | **463** | mostly a literal's byte count — should be provable |

Plus `WBLM`'s 12, which take a single `k`.

**Confirm this classification before relying on it** — it is a rough cut from a
regex and the residue class is large.

### The 593 are the project

A `varying` length comes from a **length word in memory**, so its sign is a
property of the DATA, not of the code. `DISPLAY_INVENTORY` is exactly this case
going wrong.

**The induction that might work:** every length word is written by
`assign_varying`, which stores `min(len, cap)` — and `cap` comes from a
declaration, so it is non-negative. If every *write* is non-negative, every
*read* is. Base case: initialised data in `quest.mem`.

**And the counterexample tells you where it breaks:** `DISPLAY_INVENTORY` reads
a field that **is not a string** as a varying. So the induction holds over
length words that are genuinely length words, and fails where a site reads
arbitrary data through a varying accessor.

**So the shape of the true statement is probably:** *non-negative except at
sites that read a non-string field as a varying — and here is the enumeration
of those sites.* **Produce that enumeration.**

---

## Stage A — build the reaching-definitions tool FIRST

**You cannot prove 1,689 sites by reading them.** P2's 154 register sites and
most of the 463 need, for a given site, *which blocks define `ac0` and `ac1` on
the paths reaching it*. That instrument does not exist. Build it before
proving anything.

`emulation/tools/dataflow.py` exists from an earlier project — **report at the
gate what it actually does** and whether it is extensible or should be
replaced.

Two things make this tractable, and one is a trap:

- **the state space is tiny** — four accumulators and a carry, and P52's census
  found only ac2/ac3 ever serve as an address base (19,344 uses, zero on
  ac0/ac1)
- **THE TRAP: string statements define ac0–ac3 INVISIBLY.** `[@a, n] = piece`
  names two locations and silently redefines four accumulators and `c`
  (`residues_after_copy`, `EagleString.cpp:152–163`); `cmp` redefines all four;
  `words(...)` redefines ac1/ac2/ac3. **A naive reaching-definitions pass will
  trace THROUGH them and give wrong answers.** This is P56's F4, which found
  82% of `LDAFP`s exist because of these residues. The tool must treat every
  string statement as a definition of the registers it actually writes.

**The tool is infrastructure, not scaffolding.** DESIGN §5.2's slot bijection
needs the same analysis over `LDAFP`, and every future binding precondition
needs it. Build it to be reused, and give it its own correctness check —
ruling 1 applies to the tool as much as to the proofs.

---

## The closure lemma — verify it, then use it

**If `ac0` and `ac1` are non-negative going IN to a `WCMV`, they are
non-negative coming OUT.**

With `n > 0`, `len > 0`, `t = min(n, len)` (`residues_after_copy`):

```
ac0 = 0                                    ≥ 0
ac1 = len − sgn(len)·t = len − min(n, len) ≥ 0
```

**Verify this against the implementation rather than taking it from this
prompt**, including the `t = 0` and `len = 0` edges, and state it for `cmp` and
`block_move` too.

### Why it matters: it collapses the chains

The tail-split idiom does a copy, then uses the residue `ac1` as the count for
the next copy. With closure, **you do not need to trace the chain** — establish
non-negativity at its HEAD and it carries to every link by induction.

So the proof restructures:

1. **find the chain heads** — sites whose counts come from somewhere other than
   a previous string statement's residue
2. **prove those** — constants, literal byte counts, varying reads (the 593)
3. **the rest follows by closure**

And it narrows what Stage A's tool must answer: not *trace the full definition
chain*, but **does this count come from a string residue, or from outside?**
Much cheaper to build and to check.

It also explains why StringsDesign's tail-split claim felt right — the idiom
really is safe, for this reason. It was simply never stated as a closure
property with the arithmetic behind it. **That is the difference between a
reading and a proof, and it is what this project is for.**

---

## What to establish



**P1 — the constant sites.** Prove them. Report the count. Trivial, and it
fixes the classification.

**P2 — the register sites (154).** Trace each count to its definition. Prove
non-negative where you can. StringsDesign's tail-split claim — *"a negative
count appears only in the tail-split idiom as the compiler's remaining-room
arithmetic, and is positive when the WCMV runs"* — is a **reachability
argument made by reading**. Re-derive it mechanically or mark it as one
witness.

**P3 — the varying sites (593).** Attempt the induction. Enumerate every site
where a non-string field is read as a varying. **That enumeration is the
project's most valuable output**, because it is the exception list every later
rewrite must respect.

**P4 — `WBLM` (12).** Single count `k`, all constants in the book as far as we
have seen. Confirm, and confirm the self-overlapping fills (`src = dst − 1` or
`− 2`) are all forward.

**P5 — segment containment.** `copy()` **throws** if source or destination
crosses a segment (`EagleString.cpp:76, 82`). Nobody has stated this as an
obligation. Is it provable, or is it another runtime property?

**P6 — the dead-residue facts.** Already measured for `cmp`: across all 40
sites, `ac0`, `ac2`, `ac3` are never read before redefinition — only `ac1`.
**Verify that independently** (it is mine, from a short script) and extend it:
which residues are live after `WCMV` and after `WBLM`? P56's F4 found **82% of
`LDAFP`s exist because of these residues**, so the answer decides how much
binding precision is recoverable.

---

## Rulings

1. **"We have never seen one" is not a proof.** Say which tier each claim
   reaches: **proven by construction**, **proven by dataflow**, **provable by
   provenance** (like `quest.assumptions`' three statics), or **unproven**.
   An honest *unproven* is worth more than a confident assertion.
2. **The known counterexample is a fact to characterise, not to explain away.**
   If the induction needs an exception list, produce the list.
3. **Dynamic evidence proves executed paths only.** The battery reaches ~15.9%
   of statements, and coverage deltas under a few hundred statements are noise
   (`NextSession.md`). Instrumenting `copy()` is legitimate as *corroboration*
   and is not a proof.
4. **A fact that survives goes in `emulation/quest.assumptions`**, which
   requires each row to be **checkable** — state what would falsify it.

---

## Part 1 — PLAN GATE

`docs/Project58/q001-plan-gate.md`, push to main, **STOP**. Report:

1. **What `emulation/tools/dataflow.py` does**, and your plan for the Stage A
   tool — extend or replace, with reasons, and how you will check it is right
2. **The closure lemma verified** against `EagleString.cpp`, including the
   edges, and stated for `cmp` and `block_move`
3. **How many sites are chain heads** versus fed by a residue — this is the
   number that sizes the whole project
4. **Your corrected classification** of the sites, with method
5. **Your approach to the 593**, and whether the induction looks sound
6. **A first pass at the exception list** — how many sites read a non-string
   field as a varying?
7. **What you expect to be unprovable**, before you try. Pre-registered, so it
   is scoreable — see `docs/Project55/REPORT-2.md` for why this matters

STOP. Wait for `a001`.

---

## Part 3 — Report

`docs/Project58/CountProof.md` — the per-class proof, the exception
enumeration, the tier of every claim. `docs/Project58/REPORT.md` — what you
could not prove and what it would take, your prediction against the result,
recommended rows for `quest.assumptions`, and the temptation register (standing
practice since P55: every place you wanted to call something proven and did
not).

---

## Boundaries — BINDING

1. **You may WRITE:** `docs/Project58/**`, and analysis tooling in `compiler/`
   (**new files only**).
2. **Do NOT modify** `emulation/**` (including `quest.assumptions` — recommend
   rows, do not write them), `compiler/lower_c.py`, `game/**`, `docs/IR.md`,
   `docs/StringModel.md`, `docs/Project44/DESIGN.md`, or any artifact.
3. **Nothing executes**, except instrumentation you build for corroboration —
   and say plainly that it corroborates rather than proves.

---

## Coordination

`docs/Project58/q00N-short-title.md`, push to **main**, then **STOP and tell
the user**. You own `q*.md`; the integrator owns `a*.md`. Every question states
what you found, the decision needed, the options with your read, and **your
RECOMMENDATION**. SOP: `docs/INTEGRATOR.md` §10.

## Delivery

**Push to `p58-countproof` at every stage boundary.** One `Work.tgz` with the
final report.
