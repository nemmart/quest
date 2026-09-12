# q006 — IR.md's own worked example does not load

Worker session, Sep 12 2026. No decision needed; a document fix that is not
mine. Raising it rather than working around it quietly, per a004 on q005.

## What I found

Both places IR.md spells an assignment to a `varying` cell use the BARE CELL
NAME as the destination:

- §5.10.4, the variable-form table:
  `[@QUEST.v3, 27 varying] = [@s@70166144.3, varying]`
- §5.10.10, the worked example:
  `[@QUEST.v2, 8 varying] = [@bp(QUEST.v3, 0), 11]`

**Neither loads.** Measured, §5.10.10's line verbatim in a minimal file:

    REFUSE: an aggregate cell (char/varying/words) has no CONTENTS —
    use wp(QUEST.v2, d) / bp(QUEST.v2, 0) or a string form: QUEST.v2

Change one thing — `[@wp(QUEST.v2, 0), 8 varying]` — and the same file loads.

## Why, and who is right

**The loader is right and the document is stale.** P52's own REPORT §7 F3
records the cause without noticing the consequence: `check_piece` was reworked
because "a string address now arrives in three shapes", and it now "resolves
through `wp`/`bp` and uses *which* wrapper it found as the word/byte evidence".
The bare name stopped being accepted then; §5.10.4's table and §5.10.10's
example were written before that rework and were not updated with it.

§5.10.5's tripwire table is consistent with the LOADER, not with the examples:
it says `[@<cell>, <n>]` refuses because "a fixed piece needs a BYTE pointer",
and the same logic makes a varying destination need a word pointer. So the
normative rule is right; two illustrations of it are wrong.

## Why it matters more than a typo

§5.10.10 is labelled "a symbolic-only program; the self-test's shape". It is
the thing a new reader copies. I copied it, and my first HIT_ANY_CHAR unit
refused at load for no reason of mine — which is cheap for me and would not be
cheap for someone who assumed their emitter was wrong.

It is also the **third** case in this project of the same shape: a normative
document describing something the implementation does not do (q005's argument
KIND check), or the implementation doing something the document does not
describe (P54 a001 R1's callee-in-file rule, which invalidated two rows of my
gate). Not a complaint — the parallel structure is why all three surfaced at
all. But it suggests IR.md's examples would benefit from being RUN rather than
read, the way the self-test runs its legs. Both of these examples are four
lines; a leg that loads §5.10.10 verbatim would have caught this.

## What I did

Emitted `[@wp(cell, 0), n varying] = [@bp(lit, 0), n]` with the reason and this
question number in a comment at the site, so the next reader meets the
explanation rather than the discrepancy. HIT_ANY_CHAR and its unit now load.

**RECOMMENDATION:** fix the two examples in IR.md to the `wp()` spelling, and
consider a self-test leg that loads §5.10.10 as written. Neither is mine —
IR.md is P52's and `tests/` beyond my two granted files is not in my boundary.

Nothing of mine is blocked.
