# a003 — REOPENING P58. The pattern-matcher is syntactic where it should be algebraic.

Integrator, Sep 13 2026. P58 was accepted in `a002`. This reopens it for one
bounded fix, because the base class is under-reported and those are the sites
with the **strongest** argument.

---

## The defect

`countflow.py` recognises the base class as

> count = `sx16(M16[wp(b, k)])`, pointer = `bp(b, 2k + 2)`

— a **simple base register and a constant `k`**. That is a syntactic match, and
it misses every site where the address is computed.

### The case that found it — ALCHEMIST_HOME 7015C90D, currently `cond`

```
[@ac2, LEN] = [@bp((sx16(M16[wp(ac3,2)]) * 9), −0x1FD5F080), LEN]

LEN = sx16(M16[ (sx16(M16[wp(ac3,2)]) * 9) − 0xFEAF841 ])
```

Let `W = (slot2 × 9) − 0xFEAF841` be the length word's word address. Then the
data starts at word `W + 1`, i.e. byte `2W + 2`:

```
2W + 2 = 2(slot2×9) − 0x1FD5F082 + 2 = 2(slot2×9) − 0x1FD5F080
```

**which is exactly the source pointer.** It is the canonical varying layout —
length word immediately followed by characters — with a computed base. The
`2k+2` relation holds; the matcher cannot see through the multiply.

So this site has the full structural argument (a negative count walks the
pointer back over the length word itself) and is classified `cond`, in the tier
whose justification is weaker.

---

## The fix

**Make the test algebraic, not syntactic**: does

```
pointer_byte_address − 2 × (length_word_address) = 2
```

hold as an *expression identity*, however either side is spelled? That catches
every layout-conformant site regardless of index arithmetic — computed bases,
multiplies, record indexing, by-reference pointers.

Normalising both sides and comparing symbolically should do it; you already
parse statements to trees.

---

## What I want

1. **Re-run the classification** with the algebraic test and report the
   movement: how many operands go `cond → base-class`, and how many `unknown →
   base-class`.
2. **The asserts do not change in effect** — the same expression is asserted
   either way — but the **tier and its justification do**, and the tier is what
   a later reader trusts. Regenerate `asserts.tsv`.
3. **Say whether any site now matches the relation but should NOT be trusted**
   — a coincidental `2k+2` that is not a varying. I do not expect one; if the
   relation can be satisfied accidentally, that is worth knowing.
4. **Score it against q001's prediction 6**, which said the induction would not
   be extended. It already moved 7 → 20 through a modelling fix; this is a
   second modelling fix, and the pattern is now two-for-two that *modelling*
   beat *more analysis*.

## What I am NOT asking for

No new proof work, no re-litigating the 26 circular extents, no widening the
guard rule. **One fix, re-run, report the movement.**

---

## On how this was found

The user read a single `cond` row and asked why a length adjacent to its
characters was not the base class. It was — the matcher could not see it.

Worth recording as the lesson: **a classifier that under-reports its best tier
looks conservative and is not.** Every site it misfiles is a site whose
strongest available argument has been discarded, and nothing in the output says
so. Your own §2 admission — that the residue was "frame-layout blindness", not
unrecognised varying reads — was half right: there was also *matcher* blindness,
and it hid inside the `cond` tier where nobody would look for it.
