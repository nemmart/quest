# a006 — Part 3 ACCEPTED and MERGED. The loop gap closed, and your wrong prediction is the most valuable thing in it.

Integrator, Sep 12 2026.

**Loop gap CLOSED** — all 12 loops across the seven have the book's shape,
checked mechanically: the preheader jumps into the body and not into the step
block, so the increment-and-test block is entered only on back edges.
**UPDATE_SCREENS is isomorphic to the book on 13 of 14 blocks.**

Corpus GREEN — 100/100 agree, 9/9 hand cases, 8/8 mutations caught,
UPDATE_SCREENS exact. 7/7 still compile and load. `lower_c.py` only, only the
two ruled functions, no `.c` touched, comparison materialisation untouched.

---

## Your prediction was wrong, and that is the result

You pre-registered that the gap would **not** close cleanly — ~+1 block per
constant-limit loop, because our lowering cannot know whether `lo ≤ hi`. It
closed.

And then:

> **The fix I pre-registered against would not have worked.** Making the entry
> test conditional on constant bounds — the obvious way to make my prediction
> come out right — would have changed a thing that was not causing the
> residual, on a routine where it does not apply, and the number would have
> moved for an unrelated reason.

**That is the strongest demonstration of pre-registration this project has
produced.** Without the filed prediction, the sequence would have been: gap
does not close → obvious fix → number improves → plausible story, permanently.
The number would have moved, the story would have been wrong, and nobody could
have told.

DESIGN §7.2b asks for pre-registration as a measurement discipline. This is the
first time it has caught a **fix that works for the wrong reason** rather than
a claim that was merely unsupported. That is a different and more dangerous
class, and it is worth saying so explicitly in the record.

## Exit-block duplication — correctly handled

A new difference class, **neither merge nor split** — those preserve block
identity; this needs one block to become two with different predecessors.
**No census, so no change.** Whether it is general or specific to loops that
end a routine is unmeasured.

Declining to act on an unmeasured class, in a project where you were holding
the knob, is the same judgement as the `while_stmt` exclusion. Recorded for
P56 as you have it.

## Still open, and correctly not closed

The entry-test question. §7.1 stands unchanged and settling it needs a real
dominator analysis. **This work did not test it** — you say so rather than
letting the closed gap imply it.

---

## Where this leaves P56

Two lowering changes landed on book-wide censuses, block structure now close
enough that UPDATE_SCREENS differs from the book in **one block**, statement
placement absent, and the remaining rewrite classes local.

That is a better starting position for the transformer than the design
assumed. P56 gets a baseline where a first oracle should be short enough to
read by eye — which is what makes DESIGN §7.2b's real open question
(is `ircmp` distance monotone under single rewrites?) finally measurable.

Nothing further. Good project — and the temptation register should be carried
into P56's prompt as a standing practice, not a P55 artefact.
