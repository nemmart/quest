# Project 56 — THE TRANSFORMER, THE ORACLE, AND THE FIRST MATCH

## GOAL

Close **one routine**: naive ir 8 → rewrites → **exact match against the
book**.

Not seven. One, with the machinery general enough that the second is cheaper
than the first, and with the measurements that say whether this scales.

**Success:**

1. **One routine matches the book exactly**, via an oracle file and a rewrite
   set — or a report saying precisely what stopped it
2. **`docs/Project56/Rewrites.md`** — every rule, with its precondition, its
   justification, and its application count
3. **The monotonicity measurement** (DESIGN §7.2b), pre-registered and
   reported either way

---

## Why this is now tractable

P55 left the baseline much closer than the design assumed:

- **UPDATE_SCREENS is isomorphic to the book on 13 of 14 blocks**
- **statement placement — the code-motion class — does not occur at all**, so
  every remaining rewrite is **local**: a decision inside a statement or a
  block, with a precondition checkable from local dataflow
- merge and split account for 71 differences across the seven; two routines
  match exactly on the partition

**Start with UPDATE_SCREENS.** It is verified (it ran, P48 §3.1), it is 13/14,
and its C is the one P48 and P51 independently agree on. If it closes, do
PICK_X_Y next — known-good C, 64/64 under the old line, and it exercises calls.

---

## Context of record

| path | why |
|---|---|
| `docs/Project44/DESIGN.md` | **§2 the two obligations and the standing rule; §7 the four transformation classes, the tripwire, §7.2a's rule/application split, §7.2b's monotonicity pre-registration; §7.4 site addressing; §5.1 the `v`s-to-place / `v`s-to-eliminate split; §5.2 the slot bijection** |
| `docs/Project55/BlockCensus.md`, `REPORT.md`, `REPORT-2.md` | the baseline, the classification, and what is already closed |
| `docs/IR.md` | ir 8 |
| `compiler/ircmp.py` | **parses ir 6. The book is ir 7. We emit ir 8.** Two version gaps — see below |
| `compiler/blockcensus.py`, `blockcmp.py` | P55's tooling; the block-level correspondence is already solved |
| `docs/Salvage.md` | F4 slotpatch, F23 the two base registers, F12/F13/F14 |
| `emulation/quest.assumptions` | whole-program facts you may rely on |
| `emulation/quest.ir2.book` | **the target** |

**Do not read `docs/attic/`.** Its 45 rules are void and reading them will
turn a derivation into a recognition.

---

## The rewrites that are already identified

Not a specification — a starting list. Each needs its precondition stated and
its applications counted.

| rewrite | class | what is known |
|---|---|---|
| **`M32[M32[a]]` → `M32[R[a]]`** | elimination | **ONE rule, ~994 sites.** `quest.assumptions` + IR.md's census make the bit-31 precondition provable at every site |
| **pure → effectful** (`+` → `add`, …) | elimination | one rule, fires on nearly every arithmetic statement (P48 R2). Precondition: flag liveness |
| **merge / split** | block shape | 71 across the seven; CFG-shape preconditions (DESIGN §6) |
| **comparison materialisation** | selection | **36 applications, one rule** — P55 classified it a rewrite because the source does not determine it |
| **placement** (`v` → 0x74) | placement | §5.1: only `v`s-to-place count; expression nodes are eliminated, parameters are written by the bridge |
| **binding** (`v` → register) | binding | liveness precondition; this is where the attic's 45 rules tried to *predict* and the oracle *supplies* |

**Exit-block duplication** (P55 REPORT-2) is a new class with **no census**.
If you meet it, measure before acting.

---

## Two things that must be built first

**1. `ircmp` across three IR versions.** It parses ir 6; the book is ir 7; we
emit ir 8. Decide whether to port it or write a comparator for the ir 7/ir 8
pair, and say why. P55 found its statement-level machinery was not what a
census needed — it may or may not be what a match needs.

**2. The slot bijection** (DESIGN §5.2). The book spells locals `wp(r, d)`;
ours are absolute. It is **mostly derivable**, not supplied: "does `r` hold the
frame here" is reaching-definitions over a **two-register state space** (F23:
ac2 and ac3 carry all 19,344 base uses; ac0/ac1 carry none), with `LDAFP` as
the source.

**Consider normalising the BOOK side** — rewrite `M32[wp(ac3,0x22)]` to
`M32[0x740xxxxx]` where ac3 provably holds the frame. Then placement stays
literally "substitute one constant for another" and the naive side never
learns frame-relative spelling. Note the difference in kind: that is a
**normalisation of the target**, not a rewrite precondition, so a bug in it
mismatches loudly rather than breaking the proof.

**Hazard:** the frame is *not* always in ac3. FIRE.1 moves it between ac2 and
ac3 within a dozen instructions; uplevel access reaches a parent's frame
through the static link. A `wp(r, d)` against a provably non-frame base with a
frame-territory displacement should **stop the build**.

---

## Binding rulings

1. **The standing rule** (DESIGN §2): **a match failure is NEVER a reason to
   edit the compiler.** It means a missing rewrite or a wrong oracle. If
   closing a diff seems to need a compiler change, that is a soundness bug or
   a missing construct — a **finding**, fixed against the differential rig,
   never against the book.
2. **Escalation order** (§7.2a): an existing rewrite closes it → done; it does
   not → **suspect the C first**; only once the C is exonerated → a new
   rewrite, as a **reportable finding with its evidence**.
3. **Two metrics, not one** (§7.2a): **rewrite-RULE count is the tripwire**
   (~20 is the target; growth is a reportable event); **application count and
   oracle length are the progress metric** and are expected to be large. One
   rule applied 200 times is a compiler model working; 200 rules applied once
   each is fitting.
4. **Every precondition is a HARD ERROR.** A rewrite firing where its
   precondition does not hold voids the whole guarantee — and **would still
   produce a match**, which is the one failure mode with no signature.
5. **Oracle sites are construct-anchored** (§7.4), never keyed on IR position.
   Position-keyed sites make oracle files write-once the moment anything
   upstream changes.
6. **An unjustified-but-SOUND rewrite is permitted** (§7.2a). Do not
   reintroduce the attic's refusal rule. Soundness is the guard; plausibility
   is not.

---

## The monotonicity measurement — pre-register it

DESIGN §7.2b: **is `ircmp` distance monotone under single rewrites?** That is
what decides whether this is a *walk* or a *search*, and nobody has measured
it.

**Write your prediction before you run it.** Then, once an oracle closes,
replay it one rewrite at a time and record whether distance falls at every
step. Report either way.

P55's Part 3 is the precedent and the reason this matters: its prediction was
wrong, **and the fix it had pre-registered against would have moved the number
for an unrelated reason**. Filing the prediction is what made that visible.

---

## Part 1 — PLAN GATE

`docs/Project56/q001-plan-gate.md`, push to main, **STOP**. Report:

1. **The oracle file format**, with a worked example, and how sites are
   anchored (§7.4)
2. **The comparator decision** — port `ircmp` or write new, with reasons
3. **The slot bijection design**, and whether you are normalising the book
   side
4. **The minimum rewrite set** you expect UPDATE_SCREENS to need, from the
   13/14 baseline — a prediction, scoreable
5. **Your monotonicity prediction**
6. **Anything in DESIGN §7 that does not survive contact.** Every project
   since P46 has found something; §7 has never been built against.

STOP. Wait for `a001`.

---

## Boundaries — BINDING

1. **You may WRITE:** `compiler/**` (the transformer, the comparator, the
   oracle format), `docs/Project56/**`.
2. **Do NOT modify** `lower_c.py` (see ruling 1 — a match failure is not a
   reason), `game/**`, `emulation/**`, `docs/IR.md`,
   `docs/Project44/DESIGN.md`, or any artifact.
3. **You do not change the C.** If the C is wrong, that is a **finding** and a
   recommendation, not an edit.
4. Nothing executes. This is a text-to-text match.

---

## Part 3 — Report

`docs/Project56/REPORT.md` and `Rewrites.md`:

- **did the routine close**, and if not, exactly what stopped it
- **every rule**: precondition, justification, application count. Rule count
  against §7.2a's ~20
- **the monotonicity result** against your prediction
- **the oracle's length**, and what it says about what the model does not yet
  explain
- **the temptation register** — carried from P55 as standing practice. Every
  place you wanted to change the C, or add a rule to close one diff, and did
  not
- anything in DESIGN §7 that did not survive contact

---

## Coordination

`docs/Project56/q00N-short-title.md`, push to **main**, then **STOP and tell
the user**. You own `q*.md`; the integrator owns `a*.md`. Every question
states what you found, the decision needed, the options with your read, and
**your RECOMMENDATION**. SOP: `docs/INTEGRATOR.md` §10.

## Delivery

**Push to `p56-transformer` at every stage boundary.** One `Work.tgz` with the
final report.
