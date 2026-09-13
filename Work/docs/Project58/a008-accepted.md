# a008 — P58 ACCEPTED and CLOSED. Stop recommendation granted.

Integrator, Sep 13 2026. Merged. Four reopenings, and each one earned itself.

| | a002 (first) | **a007 (final)** |
|---|---:|---:|
| **proven** | 1,006 sites / 2,065 ops | **1,092 / 2,233** |
| layout-backed | 549 / 1,067 | 481 / 932 |
| **asserted only** | 134 / 234 | **116 / 201** |
| **asserts to emit** | **1,326** | **453** |
| negative by construction | 0 | **0** |

---

## The stop recommendation is GRANTED, and for the reason you give

> **The floor is the shared page, and it is not a tool problem.**

27 of the 43 are record fields whose capacity is written by `NEW_USERS.PR` and
its siblings. No analysis of `QUEST.PR` reaches them, and
`shared_data_layout.h`'s `VARYING(32)` is an inferred layout, not a
declaration. The remaining 16 are named, small, and each says what it needs.

And the sentence that makes the residue acceptable rather than a hole:

> **Every one is a varying's length word (or arithmetic on one) with a runtime
> assert on it. None is a random word.**

Stopping here is right. What is left is a different project — shared-page
layout, possibly reading another program image — not more of this one.

## Corrections you made to me, both material

**a006 link 2 was wrong.** I said "exactly one routine fills `IN_BUFFER`". You
found **27 writers** — my grep missed the `LPEF [0x7000021C]` argument pushes
entirely, so TERRAIN ×13 and TERRITORY ×6 write it as a by-reference output
argument. The bound holds, but I had verified it from one writer out of 27,
and you re-derived rather than inherited. That is the third time a prompt of
mine has asserted something false and been caught by re-derivation.

**And the finding on the way is bigger than the bound.** The
"record-field capacity" class is **mostly not the shared page at all** — it is
**constant tables in the program image** (familiar names at 0x70150A5A, spells
at 0x70150448, help lines, the shop table), indexed under the compiler's own
DERR guard, with every length word in `quest.mem` and **nothing in the book
writing them**. A class we had filed as data-sourced and unprovable is
constant data with a guarded index.

## Method — the record

Seven model fixes, and your own summary after a003 held all the way:

> **the tool's model, not its search, was the binding constraint**

Not one was a better algorithm. Every one was found by looking at a single
concrete site or object and asking why it did not fit — including two by the
user, from questions (*"is `IN_BUFFER` used for reading from the user?"*,
*"does `?READ_SCREEN` have a max length?"*) whose answers were four greps away.

`DIVX` modelled as clobbering ac3 when it writes only ac0/ac1 — hiding a whole
chain behind a divide — is the same shape in miniature.

**The reflex when a tier looks too large is to reach for more analysis. On this
project that reflex was wrong seven times out of seven.** That belongs in
METHOD and I will put it there.

---

## What I owe, recorded so it is not lost

**1. The `quest.assumptions` rows are NOT landed, and cannot be yet.**
`compiler/check_assumptions.py:54` refuses any row whose claim is not
`ptr-bit31-clear` (`len(f) != 5 or f[0] != "ptr-bit31-clear"`), and it is wired
into `tests/run_lowerc_difftest.sh`. Adding your rows would turn the suite red.
**The checker needs extending first** — a code change, owed to whoever next
holds `compiler/`. Your drafted rows are in this project's REPORT §11 and
stand as written.

**2. Nobody has emitted the 453 asserts.** Your boundary was
specify-not-emit, correctly — it means changing the lifter or the compiler.
`asserts.tsv` carries site, block, statement index, operand, tier, `needed`,
root, and the exact IR text, so the emitting project re-derives nothing.

Both are now the last open items of this project, and both are mine to route.

Nothing further for you. This is the most thorough proof work the record has,
and the three-tier result is a statement the project can stand behind.
