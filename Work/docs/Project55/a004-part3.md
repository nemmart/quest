# a004 — P55 PART 3: land the two lowering changes. Your boundary is reversed for exactly two things.

Integrator, Sep 12 2026. P55 was scoped measure-only and delivered that.
**This extends it**: you have the census tooling, the 206-site loop data, the
807:199 conditional data and the per-routine baselines in hand, and a fresh
session would spend a third of its budget rebuilding them.

---

## Boundary change — read this first

Boundary rule 2 said **do NOT modify `lower_c.py`**. **That is reversed for
exactly the two changes below and nothing else.** `game/**`, `emulation/**`,
`docs/IR.md` and every artifact stay untouched, and you still change no `.c`.

---

## Scope — two lowering changes, both already ruled

**1. The `for` shape** (a001 Q2(a)). Emit
`if (entry test) { preheader; do-body-while(step+test) }` — the canonical DG
PL/I `DO` your census established:

```
preheader:  ... ; cv = lo ;  WBR -> body        (jumps OVER the DO block)
DO block:   <load limit> ; XNDO cv, disp, limit  -> [body, exit]
body:       ... ; WBR -> DO block                (back edge; `continue` too)
```

206 witnesses, no exceptions. The limit reloads every iteration; the entry
test is separate and outside the loop.

**2. The conditional's skipped arm.** The book skips a one-instruction arm
where we emit both — 807:199 book-wide, **36/36 for compare-against-zero**.

**NOT in scope: comparison materialisation.** You classified it as a
**rewrite, not a lowering change, because the source does not determine it** —
that distinction is right and it is the transformer's. Leave it.

---

## Your own census is the acceptance test

Re-run `blockcensus.py` / `blockcmp.py` across the seven and report
**before/after per routine, per class**. That is a cleaner signal than any
match attempt, and it is the whole reason for doing this as its own piece of
work.

State plainly whether the loop gap closed on all seven and whether the
conditional gap closed at the 36 compare-against-zero sites.

## Soundness comes from the existing rig, unchanged

**P48's differential corpus must stay GREEN.** Loop rotation is an ordinary C
compiler transformation — that it is checkable by the existing gcc oracle was
the deciding argument for (a) over the `DO()` macro, and it only holds if you
actually run it. If the corpus goes red, that is the finding and the change
stops.

---

## The one warning — the temptation inverts here

Until now you were measuring something you could not change. **Now you hold a
knob that moves the numbers you report.**

If a change closes the gap on one routine and opens it on another, **that is a
finding, not something to tune.** If the second lowering change needs a
condition to make all seven come out, that condition needs a book-wide census
of its own — §6's clause, the same standard you applied to earn these two.

Your three recorded temptations in Part 2 were all declines of things that
would have been unreviewable. This one is more dangerous because it is
*sanctioned* work: nobody would question a lowering tweak. **Keep the section
and add to it.**

---

## Deliver

Append to `docs/Project55/REPORT.md` (or `REPORT-2.md`, your choice): the two
changes as built, the before/after census, the corpus result, and the
temptation list. Push to `p55-blockcensus`.

If either change turns out to need something not in a 206- or 807-witness
census, **STOP and report** rather than widening it.
