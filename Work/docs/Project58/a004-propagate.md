# a004 — One more fix: propagate non-negativity through the arithmetic.

Integrator, Sep 13 2026. Same shape as `a003` — a modelling fix, not more
search. **Third time; see §4.**

---

## The suspicion

**909 `cond` operands is too many to be 909 independent memory-sourced
lengths.** The suspicion is that we are not modelling the structure, not that
the compiler picked 909 values out of memory and used them as counts.

ALCHEMIST_HOME's block is the evidence. Its five string statements:

```
[@bp(ac3, 200), 60]                = literal                    ← 60, a constant
[@ac2, LEN]                        = record varying             ← base-class after a003
[@bp(ac3, 0x114), LEN + 60]        = [@bp(ac3, 200), LEN + 60]
[@ac2, 42]                         = literal                    ← 42, a constant
[@wp(ac3, 198), LEN + 102 varying] = [@bp(ac3, 0x114), LEN + 102]
```

`LEN + 60` and `LEN + 102` are **not values read from memory**. They are `LEN`
plus the literal bytes already copied — 60, then 60 + 42 = 102. The count is
*(a base-class length) + (a compile-time constant)*.

**So if `LEN ≥ 0` holds at the base-class site, `LEN + 60 ≥ 0` follows by
arithmetic.** Those `cond` rows are derivable from the site feeding them.

This is a concatenation chain: **one length load, and the arithmetic fanning
out from it.** There will be a few hundred such chains, not 909 independent
reads.

---

## The rule

A third propagation rule, alongside the two you already have (closure through
residues; `assign_varying` stores a non-negative count):

> **A count expression is non-negative if every leaf is either a non-negative
> constant or an operand already `yes`/`base-class`, and every operator is
> non-negativity-preserving** (`+`, `×`, `min`, `max` over non-negatives).

You already parse counts to trees, so this is a walk with a small operator
whitelist.

**Be strict about the operator set.** `−` is NOT non-negativity-preserving, and
the remaining-room shapes (`nsub(0x1E, …)`, `0 − len + 30`) are exactly
subtraction — those must stay where they are unless a guard covers them.
`sx16` of a proven-non-negative 16-bit value is fine; `sx16` of an arbitrary
load is not.

**And a base-class leaf carries a condition, not a proof.** `LEN + 60` is
non-negative *iff* `LEN` is, which the base-class assert checks. So the derived
site should be tiered as **depending on** its root's assert, not as `yes` —
and it then needs **no assert of its own**, because the root's already
discharges it. That is the win: fewer asserts, each covering a chain.

---

## What I want

1. **The rule implemented and the movement reported**: how many of the 909 are
   derivable, how many roots they trace to, and **the number of genuinely
   independent memory-sourced lengths** left. That last number is the one worth
   knowing — I would guess low hundreds.
2. **Tier the derived sites as chain-dependent**, naming the root site, and
   **drop their asserts** where the root's assert covers them. Regenerate
   `asserts.tsv`. If dropping is unsafe for a reason I have not seen, say so
   and keep them.
3. **A three-tier presentation in the REPORT**, because the current one lumps
   them: **proven** (2,047 + 18) / **layout-backed and asserted** (266) /
   **asserted only** (the residue). The user's objection is right — the base
   class has a *structural* argument (a negative count walks the pointer back
   over the string's own length word) and should not read as resting on its
   assert the way `cond` does.

## Not in scope

No new proof work beyond this rule. Do not revisit the 26 circular extents or
widen the guard rule.

---

## §4 — the pattern, three for three

Prediction 6 said the base class would not grow without interprocedural work.
It has now grown twice, both times by a **modelling** fix, and this is a third
of the same kind:

| # | fix | effect |
|---|---|---|
| 1 | `assign_varying`'s length store is a WRITE of the count | 7 → 20 operands |
| 2 | algebraic `bytes(ptr) − 2·W = 2` instead of the syntactic matcher | 227 → 266 |
| 3 | **this one** — propagate through non-negativity-preserving arithmetic | ? |

Your own words: *"the tool's model, not its search, was the binding
constraint."* Three for three.

All three were found the same way — **by looking at one concrete site and
asking why it did not fit** — and none needed a better algorithm. Worth stating
in the REPORT as a finding about method, because the instinct when a tier is
too big is to reach for more analysis, and that has been the wrong instinct
every time here.
