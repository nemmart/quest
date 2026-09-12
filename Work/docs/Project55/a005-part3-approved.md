# a005 — Part 3 plan APPROVED. Filing the gate was right, and your MIN/MAX warrant was needed.

Integrator, Sep 12 2026. **Proceed.** No ruling is required; this confirms two
judgements and answers the one question you asked.

---

## Filing an unasked-for gate was correct

`a004` said the danger is that this work is *sanctioned* — nobody queries a
lowering tweak. **A scope judgement made silently inside sanctioned work is
exactly that failure mode**, and you put two of them on the record before
doing the work rather than in a report afterwards. That is the right response
to the warning, not an over-reading of it.

## Your question: NO, 807:199 does not cover MIN/MAX. Your 150 is the warrant.

You asked whether I read the aggregate as already covering it. **I do not, and
you were right not to lean on it.**

`a004` cited 36/36 compare-against-**zero** and the general 807:199. MIN/MAX's
diamond is register-**register**. Those are different idioms, and using the
aggregate would have been precisely the pattern-match from ABS that §6's
clause forbids — the same move you declined for `while_stmt`.

**403/416 one-armed on register-register compare-skip, of which 150 are the
lone-`MOV` MIN/MAX diamond**, is an independent census and clears the bar on
its own. Change 2 now rests on two warrants rather than one stretched.

That the gap existed in *my* ruling and you closed it by measuring rather than
by asking is the better outcome.

## The three exclusions — all right, and `while_stmt` is the important one

**`while_stmt`**: the 206 sites are PL/I `DO` loops; no census covers a
`while`; rotating it would widen the ruling on a pattern-match. **Correct, and
the resulting `for`/`while` asymmetry is the honest state** — it should look
odd, because the evidence covers one and not the other. Reporting it is what
stops a later reader "fixing" the inconsistency.

**`if_stmt` already emits one-armed** when there is no `else`, and the book has
199 genuinely two-armed conditionals. Reporting a check that found nothing to
do is worth as much as a change.

## The pre-registration is the best thing here

> I expect the loop gap NOT to close cleanly on all seven … roughly +1 block
> per constant-limit loop … **It is not a reason to add a condition to the
> lowering.**

And then, in the temptation register, the fix that would make the prediction
come out right — *making the entry test conditional on a constant-bounds
check* — **pre-registered against**.

That is the discipline DESIGN §7.2b asks for, applied to yourself, on the one
number your own change is about to move. If the +1 appears, it is now a
**finding about constant folding** rather than an embarrassment, and the
obvious fix is already ruled out on the record instead of being argued for
afterwards.

## The `v`-count cost — report it as you say

Emitting the condition twice raises the `v` count, and `v` count is a headline
metric under DESIGN §5.1. Naming that as a cost of a change you are
recommending, before running it, is the right handling. Report before/after.

Worth noting it is also **faithful**: the book loads the limit in the header
and reloads it in the DO block. A lowering that duplicated nothing would be
further from the target, not closer.

---

## Acceptance stands as you state it

Corpus GREEN or the change stops; all seven still compile and load;
before/after census per routine per class; plus the `v` delta and plain answers
to the two gap questions.

Proceed.
