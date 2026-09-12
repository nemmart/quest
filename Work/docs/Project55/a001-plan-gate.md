# a001 — P55 gate: APPROVED. Q1 (a) and Q2 (a). My prompt was wrong twice.

Integrator, Sep 12 2026. **Proceed to Part 2.**

---

## Q1 — (a) GRANTED: `ir2.book` is the target. `blocks.split` is provenance.

**My prompt was wrong.** It named `blocks.split` "the book's block partition.
The target". It is not the target — it is an **input** to the lifting that
produced the book, named with a sha256 in the book's own header.

DESIGN §2(b) obliges us to equal **the book's IR**. That is the artifact, and
it is the only thing a match can be against.

Your measurement is what makes this decidable rather than a preference: the
verdict is *opposite* under the two partitions — "canonical form wrong,
systematic 1.7–2.5× divergence" against `blocks.split`, "roughly right for
five of seven" against `ir2.book` — and for the seven the entire difference is
**2 × DERR sites**, a construct where our `assert` lowering already agrees
with the book. Choosing `blocks.split` would have sent P56 to build rewrites
that split our `assert` back into a three-block skip chain **the book's own IR
does not have**. That is the clearest possible sign of a wrong target.

Report both columns in `BlockCensus.md` as you propose, and state the verdict
against `ir2.book`.

Your risk note is correctly stated and correctly bounded: `ir2.book` is a
*lifted* artifact, so a lifting bug becomes a target we match to. That is a
real exposure for P56, not for a project that only measures. Carry it into
the report so it is not lost.

## Q2 — (a) GRANTED: change the canonical lowering. Not the macro.

**I proposed the macro in the prompt and you found the better answer.**

The deciding argument is yours and it is the right one: **(a) is the only
option of the three whose soundness is checked by the existing differential
rig against gcc.** Loop rotation is an ordinary C compiler transformation —
nothing about PL/I leaks into the compiler, the input stays C, and the oracle
survives. (b) would touch every source file and rests on the weaker footing
you name. (c) violates §7.2a's escalation order, which puts a new rewrite
third.

**And DESIGN §6 sanctions this precisely**: the canonical form is revisable on
a **book-wide census**, never to close one routine's diff. 206 witnesses with
no exceptions is that census. This is the rule being used as designed, for the
first time.

---

## The loop census is the best measurement this project has produced

**One shape, 206/206, no exceptions:**

```
preheader:  ... ; cv = lo ;  WBR -> body        (jumps OVER the DO block)
DO block:   <load limit> ; XNDO cv, disp, limit  -> [body, exit]
body:       ... ; WBR -> DO block                (back edge; `continue` too)
```

Increment and test are **one instruction in one block entered only on the back
edge**; the limit is **reloaded every iteration**; the entry test is separate
and outside the loop.

Three things about how it was done, which matter as much as the result:

1. **You did the book-wide census FIRST**, rather than reasoning from the
   seven. That is what the prompt asked and what the attic did not do.
2. **You found the preheader properly** — as the unique CFG predecessor at a
   lower address — rather than by address adjacency. Adjacency gave 191/206
   with 15 apparent exceptions, **all of which were the heuristic failing, not
   a second shape**. You flagged that as "the exact error the attic made". It
   is, and a 191/206 result would have looked like a real distribution with a
   tail. That is the finding under the finding.
3. **P51's P4 note now has 206 witnesses instead of two.** A one-witness
   observation promoted to a fact by counting.

**The one loop question you have not closed** — whether entry-test presence
correlates exactly with `lo ≤ hi` being statically decidable — is correctly
named as Part 2 work and correctly not guessed at.

---

## On `ircmp.py`

Your four reasons are sound and "reuse the idea, not the code" is right.
Equivalence 1's canonical block order (DFS from entry, successors in
terminator order) is the correct way to align two partitions, and the rest of
`ircmp` carries an ir 6 parser and a slot-bijection dependency this
measurement does not have.

Noting for the record: **`ircmp.py` parses ir 6, the book is ir 7, we emit ir
8.** Two version gaps in a tool P56 will want. That is a real item for P56's
sizing and it belongs in your report.

Proceed.
