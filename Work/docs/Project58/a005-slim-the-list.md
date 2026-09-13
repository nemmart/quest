# a005 — REOPENING P58. P59 proved one of your `unknown` sites, and the miss is systematic.

Integrator, Sep 13 2026. **Read `docs/Project59/REPORT.md` first.** Then take
the class it opened and keep going.

---

## What P59 found

We picked **one of your 134 asserted-only sites at random** — DISPLAY_INVENTORY
`70167F05` — and asked whether it was genuinely unprovable. It is not.

**Verdict: PROVEN, by dataflow over a closed CFG.** Not merely non-negative:
`ac0 = ac1 = 3` on **every** path. The length word at slot 6 is one of
`{8, 11, 13, 14, 16}` when the site runs, so `30 − len ∈ {22, 19, 17, 16, 14}`,
the guard is never true, and the CFG edge that skips the clamp arm is
**infeasible**.

**Your slot model already had the right reaching definitions.** P59 quotes
your own `--dump-site` output: the constant 8 and the three intervening
writers were all there. `<opaque slot 6 clobbered>` *is* those writers, not
an unknown.

### The finding — and it is the one that matters

> **The concatenation clamp is `assign_varying`'s `min(len, cap)` in
> disguise.**

You model `assign_varying`'s `min(len, cap)` as capacity-preserving (a003 fix
1). The hand-written append computes

```
len + min(k, cap − len)  =  min(len + k, cap)
```

— **the same guarantee**, spelled out in instructions instead of hidden in a
library call. Your conclusion that "the `assign_varying` induction does not
apply here" was wrong, and so was mine: I reached it too, and wrote it into
P59's prompt as established fact. P59 was told to re-derive everything and did.

Three model gaps turned a provable site into `unknown`, **each a refusal to
read something the IR says**:

1. a store through a register whose single reaching definition is
   `wp(ac3, 6)` is reported as an opaque clobber — **though your own register
   RD knows `ac2 = wp(ac3, 6)`**. The tool does not join its two results.
2. the clamped append is not a recognised capacity-preserving write
3. (P59 §2's third gap — read it there)

---

## The work

**Slim the list.** P59 scanned program-wide for the append idiom and found
**13 sites**, of which **11 are in your `unknown` tier — 22 of the 130
operands**:

| routine | sites | cap | dest | status |
|---|---:|---:|---|---|
| DISPLAY_INVENTORY | 5 | 30 | slot 6 | 2 already `yes`; **3 proven by P59** |
| TERRITORY | 2 | 9 | slot 6 | unknown |
| OBSERVE | 2 | 150 | slot 18 | unknown |
| DISPLAY_MAP | 1 | 80 | slot 20 | unknown |
| DISPLAY_SCREEN | 1 | 84 | slot 998 | unknown — needs slot 10 ≥ 0 first |
| INIT_OBJ_TBL | 2 | 600 | shared-page record field | unknown — **frame privacy says nothing here** |

**Take the six that P59 did not prove and are in reach** (TERRITORY, OBSERVE,
DISPLAY_MAP). P59 names what is needed: they are frame slots maintained by
the same idioms, plus DISPLAY_MAP's fourth form — a single-character append
(`if len < 80 then data[len] := ch; len := len + 1`, block `7016567B`).
`onesite.py` recognises two writer shapes; **extend to the other two**.

**Leave the two that need more**: DISPLAY_SCREEN's piece length is another
varying's length word, and INIT_OBJ_TBL's destination is on the shared page.
Say what each would need.

**Then look for adjacent shapes.** The append idiom was found by pattern; the
question is whether the other 59 `unknown` sites hide a *different* invariant
maintained inline. P59 says the rest are your §2 classes — extents, record
capacities, argument cells, products — but that was a judgement from one
routine's vantage. **Check it.**

---

## Two things to carry

**Adopt P59's tool or its findings, do not rebuild.** `compiler/onesite.py`
reuses your parser, CFG builder and dominators unchanged. Merge what is useful
into `countflow.py` or drive both — your call, say which.

**Regenerate `asserts.tsv` and report the movement**, as with a003/a004. The
three P59 proved are already out; report where the rest land.

---

## The pattern, now five for five

| # | fix | effect |
|---|---|---|
| 1 | `assign_varying`'s length store is a WRITE of the count | 7 → 20 |
| 2 | algebraic `bytes(ptr) − 2·W = 2` instead of the syntactic matcher | 227 → 266 |
| 3 | wrap and `R[]` cases in the matcher | 266 → 286 |
| 4 | propagate non-negativity through the arithmetic | 909 → 104 `cond` |
| 5 | **the clamped append is capacity-preserving** | ? |

Your own words after a003: *"the tool's model, not its search, was the binding
constraint."* Five for five, and **every one was found by looking at a single
concrete site and asking why it did not fit** — never by a better algorithm.

The user's instinct each time was the same and was right each time: *the game
works, so we are probably not reading it carefully enough.* Worth stating in
the REPORT as a finding about method — because the reflex when a tier looks
too large is to reach for more analysis, and that has been the wrong reflex
every single time on this project.
