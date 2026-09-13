# Project 58 — PROVING THE STRING COUNTS

## GOAL

**Establish what can be PROVEN about the counts passed to `WCMV`, `WCMP` and
`WBLM`, and characterise precisely what cannot.**

The obligation, as it stands:

> at every string site, `ac0 ≥ 0` and `ac1 ≥ 0`

**A negative count is already known to OCCUR**, so the question is not *is the
obligation true* but **what is the true statement, what discharges it, and what
does the residue rest on.**

`DISPLAY_INVENTORY 7016816B` runs `cmp` with a source length of −1: the record
field holds `0xFFFF` and length words are read **sign-extended** (P31 Census
F-B1, and there is a comment at `EagleString.cpp:30` about this very site).
**That site has the canonical varying layout** — it is a genuine string read of
shared data that happens to hold an implausible length, not a misuse of the
accessor. It is therefore discharged by the base class below, and is **expected
to trip its assert**.

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
| `emulation/hw/strings/EagleString.cpp` | **the implementation.** `copy()` :60–90, `residues_after_copy()` :152–163, `residues_after_compare()` :167, `block_move()` :111–130 |
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

#### The BASE CLASS — the `len → ac0, chars → ac2` pattern, discharged by assertion

**Do this first. It is probably most of the 593.**

A varying at word address `x` has its **length word at `x`** and its **data at
word `x+1`, i.e. byte address `2x + 2`**. So when a site loads the count from
`[x]` and sets the pointer to `2x + 2`, the count and the pointer are derived
from **one address** and the site is reading something laid out as a varying.

That relation is **mechanically recognisable**, and it is the base class.

**Why non-negative can be assumed there** — a structural argument, not an
aesthetic one. The length word sits immediately BELOW the data, so a negative
count walks the pointer backward from `2x+2` into the header:

- **destination**: the first bytes written land on the length word — the copy
  destroys the header `assign_varying` just wrote
- **source**: the first bytes read ARE the length word, then whatever is below

A negative count is self-destructive with respect to the string's own
structure. No compiler emits that deliberately.

**Discharge it by ASSERTION, not by argument.** Emit `assert(<count> >= 0)` at
the site (IR.md §3 has `assert(e)` / `assert(e, "message")`, never a
terminator). Then:

- the obligation is discharged **at runtime, on every path that actually
  runs** — stronger than a proof over the paths you analysed
- the assumption is **visible in the IR text** where a reader meets it
- every rewrite that wants to drop `sgn` or assume forward direction can rely
  on it
- and a violation **stops loudly at the exact site** instead of surfacing as a
  divergence somewhere else

**This is what makes the unprovable classes tractable.** A player name in
`SD_PTR` was written by another process, possibly in a prior session, possibly
by `NEW_USERS.PR` — a different program image. A record read from a file is the
same. **No induction over writes can ever reach those.** The pattern does not
care: it only says *this site reads something laid out as a varying*, and the
assert holds whatever produced it to the invariant.

**`DISPLAY_INVENTORY 7016816B` is expected to FIRE**, and that is the point. It
has the canonical pattern — `XNLDA 1,[ac2+0x7FC2]` for the count and
`XLEFB 3,[ac3+0xFF86]` for the pointer, with `ac3` a copy of `ac2`, so the
pointer is `2·(ac2−62) + 2` exactly — and the field holds `0xFFFF` on the login
path. So it is a genuine varying read of shared data that is not a plausible
length. **It is the test that the assert works**, not a counterexample to the
pattern.

**Report the pattern-match count at the gate.** That number, with the
chain-head count, sizes everything else.

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

## Stage A — build the reaching-definitions tool FIRST

**You cannot prove 1,689 sites by reading them.** For a given site you need
*which blocks define `ac0` and `ac1` on the paths reaching it*. That instrument
does not exist. Build it before proving anything.

`emulation/tools/dataflow.py` exists from an earlier project — **report at the
gate what it actually does** and whether to extend or replace it.

Two things make this tractable, and one is a trap:

- **the state space is tiny** — four accumulators and a carry, and P52's census
  found only ac2/ac3 ever serve as an address base (19,344 uses, zero on
  ac0/ac1)
- **THE TRAP: string statements define ac0–ac3 INVISIBLY.** `[@a, n] = piece`
  names two locations and silently redefines four accumulators and `c`
  (`residues_after_copy`, `EagleString.cpp:152–163`); `cmp` redefines all four;
  `words(...)` redefines ac1/ac2/ac3. **A naive pass will trace THROUGH them
  and give wrong answers.** This is P56's F4 — the reason 82% of `LDAFP`s
  exist. The tool must treat every string statement as a definition of the
  registers it actually writes.

**The tool is infrastructure, not scaffolding.** DESIGN §5.2's slot bijection
needs the same analysis over `LDAFP`, and every future binding precondition
needs it. Give it its own correctness check — ruling 1 applies to the tool as
much as to the proofs.

---

## The closure lemma — verify it, then use it

**If `ac0` and `ac1` are non-negative going IN to a `WCMV`, they are
non-negative coming OUT.**

With `n > 0`, `len > 0`, `t = min(n, len)` (`residues_after_copy`):

```
ac0 = 0                                    ≥ 0
ac1 = len − sgn(len)·t = len − min(n, len) ≥ 0
```

**Verify against the implementation rather than taking it from this prompt**,
including the `t = 0` and `len = 0` edges, and state it for `cmp` and
`block_move` too.

### It collapses the chains

The tail-split idiom does a copy, then uses the residue `ac1` as the count for
the next copy. With closure you **do not need to trace the chain** — establish
non-negativity at its HEAD and it carries to every link by induction.

So the proof restructures:

1. **find the chain heads** — sites whose counts come from somewhere other than
   a previous string statement's residue
2. **prove those** — constants, literal byte counts, and the base class above
3. **the rest follows by closure**

And it narrows what Stage A must answer: not *trace the full definition chain*,
but **does this count come from a string residue, or from outside?**

It also explains why StringsDesign's tail-split claim felt right — the idiom
really is safe, for this reason. It was never stated as a closure property with
the arithmetic behind it, **which is the difference between a reading and a
proof, and is what this project is for.**

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

**P3 — the varying sites (593). THE BASE CLASS COMES FIRST** — see above.
Match `len → ac0, chars → ac2` mechanically, emit the assert, report the count.

Then, for whatever does NOT match the pattern: attempt the induction, and
enumerate the residue. **That enumeration is the project's most valuable
output**, because it is the list every later rewrite must respect.

**Do not expect the residue to be the SD_PTR and file-read cases** — those
have the canonical pattern and are discharged by the base class. The residue is
sites that read a length from somewhere the pattern does not recognise.

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
   `DISPLAY_INVENTORY` has the canonical layout, so it is discharged by the
   base class and is **expected to trip its assert** — that is the test that
   the mechanism works, not a hole. If anything else needs an exception list,
   produce the list.
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
6. **THE PATTERN-MATCH COUNT** — how many of the 593 match `len → ac0,
   chars → ac2`? With the chain-head count this sizes the project
7. **A first pass at the RESIDUE** — of the 593, how many do NOT match the
   pattern, and what do those look like? Plus the uninitialised-varying
   question (§b): how many varying reads have **no dominating write**?
8. **What you expect to be unprovable**, before you try. Pre-registered, so it
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
2. **The Stage A tool goes in `compiler/` as a NEW FILE**, even if it borrows
   from `emulation/tools/dataflow.py`. Read that file, say what it does, copy
   what is useful — but **do not modify it**, because `emulation/` is not
   yours.
3. **Do NOT modify** `emulation/**` (including `quest.assumptions` — recommend
   rows, do not write them), `compiler/lower_c.py`, `game/**`, `docs/IR.md`,
   `docs/StringModel.md`, `docs/Project44/DESIGN.md`, or any artifact.
4. **You SPECIFY the asserts; you do not emit them.** The base class is
   discharged by an `assert` at each site, but emitting one means changing
   either the lifter (an artifact) or the compiler — neither is yours.
   **Deliver the site list and the exact assert text**; a later project emits
   them. Say in the REPORT which project should.
5. **Nothing executes**, except instrumentation you build for corroboration —
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
