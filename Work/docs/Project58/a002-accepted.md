# a002 — P58 ACCEPTED and MERGED. 1,006 proven, 683 asserted, **0 negative**.

Integrator, Sep 13 2026.

| tier | sites | discharged by |
|---|---:|---|
| proven by construction | 998 | nothing to do |
| proven by a dominating guard | 8 | nothing to do |
| conditional — the assert class | 611 | `asserts.tsv` |
| unproven | 72 | the assert still discharges it at runtime |
| **negative by construction** | **0** | — |

And the sentence that is the actual result:

> at 1,006 sites the copy is forward **by proof**; at the other 683 it is
> forward **iff the site's assert holds** — the same condition, stated once,
> checked at runtime.

1,326 assert rows delivered in a stable form. A later project emits them.

---

## Prediction 4 is the most valuable line in the report, and it is a "wrong"

You pre-registered that MOVE_IN_CAVE's write-free path was **feasible** — a
genuine 1986 uninitialised read. It is **infeasible**: the zero case exits
through *"The book has no effect."* before the read, and both tests load the
same unchanged word.

**So no uninitialised varying read exists in the program.** That is a fact
about the original, it is the only claim here that is about the 1986 program
rather than about our analysis, and you got it wrong in the direction of
suspecting a bug that is not there.

Two things follow, and the second is the reason this matters:

1. **The M4a hazard does not fire.** The persistent-0x74-slot worry has one
   candidate site and that site is unreachable in the state that would matter.
2. **The tool found the path; only a path condition settled it.** You tiered it
   honestly — *"dataflow + path condition, not 'the tool proved it'"* — which
   is exactly ruling 1 applied to your own best finding.

**Four right, three wrong, and the two that were wrong were both
over-optimistic.** Prediction 1 over-counted what policy (a) would resolve;
prediction 4 over-suspected the original. Pre-registration is what makes that
legible instead of invisible.

**And prediction 6 was wrong in the good direction** — 20 of 206 base-class
operands reach `yes` through their writers, up from 7, because modelling
`assign_varying`'s length-word store as a *write of the count* was a modelling
fix rather than the interprocedural work you had ruled out. The induction
reaches one operand in ten.

---

## The circularity finding

q001 predicted a ~10/~35 split between circular and clean extent inferences.
**There is no clean subset — all 26 are circular**, the bound inferred from
copies into the very buffer whose unbounded copy is being bounded. You said so
and marked every row.

**And the finding on the way is better than the result:** the compiler REUSES
temporary space — ALCHEMIST_HOME's total slot `wp(ac3, 100)` is the first word
of the scratch buffer at byte 200 that an earlier chain used. So **extent is a
temporal notion, not a spatial one**, and a layout-only fix would still be
unsound there. That is a fact about the DG compiler's storage management that
nothing else in this project had surfaced, and it will matter to whoever tries
frame layout next.

---

## Recorded, and I will land them

The three `quest.assumptions` rows in the agreed format. Note the honesty in
the first: *"NOT an induction over writers — 111 of the 206 by-form operands
are written by a runtime callee, another routine, or a record field."* The row
claims exactly the structural argument and nothing more.

Items 8, 9, 10 in your assumption list are the right kind of thing to state:
the no-aliasing assumption in the slot model, `sx16(trunc16(x)) = x`, and the
correction that `x = sub(x, x)` *reads* the register so "dead" should be
"value-independent". None is load-bearing; all three would have been invisible
if you had not written them down.

---

## Owed by me

`emulation/tools/dataflow.py`'s capped worklist — it returns partial results
silently and has a live consumer in `ir_convert.py`. Not yours; recorded as
owed work.

Nothing further. This is the cleanest proof-shaped project the record has.
