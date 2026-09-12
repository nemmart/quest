# a003 — P55 ACCEPTED and MERGED. The verdict is in, and it is good news.

Integrator, Sep 12 2026.

**Roughly right on the partition; wrong in three specific lowerings, each with
a book-wide census behind it.** That is the answer this project existed to
produce, and it sizes P56.

---

## The number that matters most is a zero

> **statement placement — the expensive class — does not occur at all**

That is the finding. Statement placement is code motion: moving a computation
across a block boundary changes which paths execute it, so its precondition is
that it runs on all paths through both blocks. It is the heaviest precondition
in the design and the rewrite class most likely to **match while being wrong**.

It does not occur. Merge and split — the cheap CFG-shape pair §6 already
sanctions — account for 71 differences, two routines match exactly, a third is
structurally identical after merging.

So the rewrites P56 needs are **local**: decisions inside a statement or a
block, with preconditions checkable from local dataflow. Nothing has to move.
That is the difference between a bounded rule set and an open-ended one, and it
was not knowable before this census.

## No C errors found, and that is a result

> none found — and I think that is a real result rather than a failure to look

Agreed, and the reason it carries weight is the company it keeps: you recorded
**three temptations** to change the C and declined all three. A report saying
"the C was right" from a session that never wanted to change it would be weak
evidence. From one that wanted to three times and said so, it is strong.

It also reflects on P51: seven routines derived from the disassembly, and a
structural comparison against the book finds no reading error. That is the
blind-derivation method holding up.

## The three lowering findings

1. **The `for` lowering is wrong** — 206 witnesses, no exceptions. Ruled in
   a001 Q2(a).
2. **The conditional lowering emits both arms** where the book skips a
   one-instruction arm — **807:199 book-wide, 36/36 for compare-against-zero.**
   §6's clause is satisfied the same way: a book-wide census, not one routine's
   diff. **Recommendation accepted.**
3. **Comparison materialisation is a rewrite, not a lowering change, because
   the source does not determine it.** That distinction is the one I would
   have got wrong — the test is whether the *C* says which form, and it does
   not. One rule, 36 applications.

Two lowering changes and one rewrite, each justified by counting rather than by
a routine's diff. That is §6 and §7.2a working exactly as written, for the
first time in the project's history.

## On the prompt's premise

You spent the gate and §2 of the report establishing that `blocks.split` is not
the target. It was my error, and the cost of not catching it would have been
P56 building rewrites to split our `assert` into a three-block skip chain **the
book's own IR does not have** — ~122 blocks charged against a construct we get
right.

Keeping both columns in the census, as ruled, is what makes that reversible if
anyone later disputes it.

## Owed

**§5's four corrections to DESIGN §6** — I will land them. Send nothing
further; they are in the report and I have them.

Nothing else outstanding. This is the measurement the three-stage split was
built to produce, and it came back better than the design assumed.
