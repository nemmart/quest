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
property of the DATA, not of the code.

**The assertion worth aiming for** — and the shape of a good answer — is:

> A varying length word **written by `assign_varying`** is non-negative, because
> it stores `min(len, cap)` and `cap` comes from a declaration. So if every
> write is non-negative, every read is.
>
> **…except at these N enumerated sites**, each a named fact rather than a gap.

**Produce that enumeration.** It is the project's most valuable output, because
it is the exception list every later rewrite must respect.

Two distinct ways the induction can fail. **Both must be enumerated, and they
are different problems:**

#### (a) A site reads something that was never written as a string

This is the known counterexample and it is **not** a case of "a varying read
gives a negative number". Read F-B1 precisely:

```
ac1 = cmp([@record − 62, varying], [@IN_BUFFER, varying])   7016816B
```

The count reaches `ac1` by `XNLDA 1,[ac2+0x7FC2]` — the varying accessor
loading a length from memory. What sits at `record − 62` is a **game record
field**, not a string anything ever wrote as one, and it holds `0xFFFF` on the
login path.

So the induction is not wrong; the site is reading non-string data through a
string accessor. **Enumerate every site that does this.** If the list is small
and every member reads a game record field, that is a strong result.

#### (b) An UNINITIALISED varying is read

This breaks the induction's **base case**, not its step, and it may be the
larger problem.

A `CHAR(n) VARYING` local read before it is ever written holds whatever its
length word happened to contain. **And under M4a those slots are per-routine
global areas at 0x74 that PERSIST BETWEEN CALLS** — so it is not
garbage-once, it is the previous call's leftover.

Three sub-cases, and they need separating:

| case | how to find it | what it means |
|---|---|---|
| **written at load** | `quest.mem` holds the image; a static varying's initial length word is knowable | the base case, done properly — **check it** |
| **written on some paths, read on all** | dataflow: a varying read whose length word has **no dominating write** | the interesting one, and squarely Stage A's tool |
| **never written, read anyway** | no write anywhere in the routine | a bug in the original — but the original RAN, so either it does not occur or the leftover was always benign. **Say which** |

**And name the M4a interaction, because it is a real hazard nobody has
stated:** a slot that was originally STACK-allocated got a fresh value per
call; migrated to a global 0x74 area it now carries the previous call's
length. If the original relied on stack garbage being small and our global slot
holds a large leftover, behaviour differs. That is an emulator-level concern
rather than a matching one — **report it, do not chase it.**

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
6. **A first pass at BOTH exception lists** — how many sites read a non-string
   field as a varying (a), and how many read a varying with no dominating
   write (b)?
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
