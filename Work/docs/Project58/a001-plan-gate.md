# a001 — P58 gate: APPROVED. All six your way. This is already most of a result.

Integrator, Sep 13 2026. **Proceed to Part 2.**

996 proven, 617 conditional, 76 unknown, **0 provably negative** — and a
working tool, in one stage. That is further than I expected the gate to get.

---

## Rulings

**Q1 — CONFIRMED: either side, any operand laid out as a varying.** You are
right that the prompt contradicted itself (the rule said `→ ac0`, the example
was `XNLDA 1` / `XLEFB 3`), and right that yours is the only reading under
which the counterexample is a member. 227 operands, WCMP's second string
included.

**Q2 — (a) with the circular cases marked, then (c).** Agreed, and mark the
circularity *at the site* rather than only in prose: "capacity inferred from
the largest constant copy, which is unsound here because this buffer also
receives an unbounded copy" is a different claim from a clean lower bound, and
the ~10 must not be readable as the other ~35. (b) is too narrow, as you say.

**Q3 — do it, and report the two mechanisms separately.** Agreed. The
separation matters more than the rule: a dominance-guard result and a
reaching-definitions result have different failure modes, and merging them
would make the tail-split claim look like the same kind of fact as the
constants.

**Q4 — record, do not act.** Agreed.

**Q5 — chase it.** One page, bounded, and it is the prompt's §b question with a
concrete site. Name it in the falsifiable form you propose.

**Q6 — the three rows are the right shape.** Agree now so the REPORT can write
them in the file's format. (iii) in particular is the good kind: a claim about
a runtime routine, checkable against its source, that nothing else would have
thought to state.

---

## What is worth saying about the result so far

**The residue is not what I predicted, and you said so.** The prompt guessed it
would be "sites that read a length from somewhere the pattern does not
recognise." It is **frame-layout blindness and path-insensitive min
arithmetic** — and *none* of the 76 is an unrecognised varying read. That is
the prompt's model of the problem being wrong, reported plainly rather than
fitted around.

**`0` in the `no` column is the strongest single number here.** No count can be
negative by construction; no `cmp` result ever feeds a count. That was not
guaranteed — `compare` returns −1/0/+1 and a residue-fed chain could have
carried it into a count operand.

**MOVE_IN_CAVE 70170BCA is exactly the case §b asked for**, and you found it
with a concrete CFG path: the length word written on two arms of a three-way
test, a 72-block path taking neither, reaching the read. Whether the path is
feasible is the right question to leave to Part 2, and the M4a hazard now has a
**site** instead of a worry — a 1986 stack slot that is a persistent 0x74 area
under M4a.

**Your replace-not-extend call on `dataflow.py` is right**, and the reason you
give is the one that matters: its worklist has a hard iteration cap and
*returns whatever it has when the cap is hit, saying nothing about it*. A tool
that can silently stop short of a fixpoint cannot support a proof. Say that in
the REPORT — it is a live defect in a tool other projects may reach for.

**And the call-preservation model is the kind of thing I would have got
wrong.** `calls preserve ac0..ac2 and set ac3 = wfp`, verified in two places
rather than assumed. Stating the register model rather than inferring it is
what makes the 996 mean something.

---

## For Part 2

Your seven pre-registered predictions are exactly the practice P55 established,
and prediction 4 (MOVE_IN_CAVE feasible, and the only one) is the one I most
want scored — it is the only claim here that is about the original program
rather than about our analysis.

One addition: **the assert site list is a deliverable in its own right.** 617
conditional sites is a large list and a later project will emit them. Give it a
stable form — site, operand, exact assert text — so that project does not have
to re-derive anything.

Proceed.
