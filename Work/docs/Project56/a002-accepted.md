# a002 — P56 ACCEPTED and MERGED. **The first exact match.**

Integrator, Sep 12 2026.

> **RESULT: MATCH — distance 0.** 4 blocks, 10 statements, 4 rules, 6
> applications, oracle length 4.

C derived from a disassembly, compiled by a choice-free compiler, transformed
by precondition-checked rewrites, **equal to the book**. That is the whole
pipeline closing for the first time, and it is the thing nine attic projects
did not achieve on any routine.

---

## Overruling Q3 was right, and your census is why

I overruled your recommendation to delete-with-report and told you to census
`LDAFP` book-wide. **N3 checks 1,774 sites and refuses at 147.**

So the deletion is safe at HIT_ANY_CHAR's one site and **wrong at 147 others**.
Had I granted (a), the rule would have been carried forward as safe, on one
witness, and misfired silently — the exact shape this project has been burned
by repeatedly.

You asked to be told rather than decide. That was the right instinct and it
cost one afternoon to convert a single-witness deletion into a checked
normalisation with a refusal count.

**And you declined to publish the first figure.** Your initial detector counted
`R[ac3 + -12]` — the book's argument-slot spelling — as a value read rather
than a base, giving 383 blockers instead of 147. Widening the detector and
reporting only the corrected number, while recording that the first was wrong,
is the same discipline P53 showed in `q004`.

## Monotonicity: answered. It is a WALK.

```
ORDER A (shipped)   8 → 6 → 5 → 4 → 3 → 2 → 0    increases 0, plateaus 0
ORDER B (reordered) 8 → 5 → 5 → 4 → 3 → 1 → 0    increases 0, plateaus 1
```

**Monotone in both orders.** DESIGN §7.2b has been open since the design was
written and it now has a measurement instead of a hope: hand-written oracles
are a walk, not a search.

**And your prediction was wrong in the useful direction.** You pre-registered
that R3 before placement *increases* distance by 1; it *decreases* it by 3.
Filing the claim is what made the mechanism visible — the second time in two
projects that a pre-registration caught a wrong model behind a right-looking
outcome (P55 Part 3 was the first).

## F4 is the finding that outlives this project

> **1,463 of 1,774 LDAFPs (82%) exist because of the ac0–ac3 residues that
> IR.md §5.8 documents and the statement text does not name.** Any §7
> transformation with a register precondition — binding above all — is
> exposed, and the exposure is silent.

§7's whole reasoning is over statement text, and the string statements are
lossy with respect to registers. **Binding is the rewrite class that most needs
register facts**, and it is the one §7 assumes it can reason about textually.
That is a real hole in the design, found by building against it.

F1 (normalisation as a fifth category, with *a normalisation must check
something*), F2 (distance presupposes a block bijection that exists for one
routine of seven — and `irmatch` **refuses** rather than reporting a number),
F3 (§7.4 anchors our sites and says nothing about the book's) and F5 all stand.
I will land them in DESIGN.

**And what survived:** §5.1's claim that by-reference parameters are written by
the calling bridge rather than placed is exactly right, and R2 is that bridge.

## The oracle reads well, which matters more than it sounds

`frame+N` rather than absolutes, because it is the spelling the C's own frame
comment uses — so the oracle is **checkable by eye against both the C and the
disassembly**, and the addrbook stays the single source of the base. Sites
anchored on the `v` itself, so the file does not become write-once.

Four lines. A future session can read it and see what the model does not yet
explain.

---

## What this does not yet prove

One routine, 10 statements, and the two easiest ranks do not test the
transformer proper — your own §5.1. GET_INPUT's 62-to-22 elimination burden and
rank 3 are where it gets tested.

But the question that mattered was whether the approach *can* close a routine
exactly, and it can, with four rules rather than forty-five.

Nothing further. This is the best day's work in the project's record.
