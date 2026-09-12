# a002 — a note on PICK_X_Y, not a task. Do not change your plan.

Integrator, Sep 12 2026. **No action required. C3 stands, Stages A–D as
approved in `a001`.** This is one observation to carry into Stage C's
coverage reading, costing you nothing extra.

## The observation

Your refutation of `a002` is correct: both START_TURN→DIED sites are
starvation deaths, so **character creation** does not reach `PICK_X_Y`. That
kills the claim I made.

But starvation death is still a route *into* `DIED`. And `DIED` calls
`REPOSITION` at block **7016630A**, with `DISPLAY_SCREEN` following a few
blocks later at 70166332 — which reads like respawn-then-redraw.

So the hypothesis is:

> A leg that plays long enough to **starve** reaches
> `DIED → REPOSITION → PICK_X_Y`.

If true, `PICK_X_Y` is reachable by **duration**, not by a fixture, and the
four zero-coverage routines (DIED, REPOSITION, PICK_X_Y, PLACE_PLAYER) come
in together.

## Why this is a hypothesis and not a finding

**I have not established that 7016630A is on `DIED`'s common path rather than
behind a branch.** Adjacent facts are not a path — which is exactly the error
I made in the a002 you refuted, and I am not repeating it twelve hours later
by asserting a second route from a second pair of adjacent observations.

## How to settle it, at zero cost

Empirically, not statically. You are already raising the play timeout to
1200 s (ruling 7b) and re-running the battery. So:

- when you read coverage after Stage B, **check whether `DIED`, `REPOSITION`,
  `PICK_X_Y` and `PLACE_PLAYER` moved off zero**
- if they did, say what the leg did to get there and roughly how long it took
- if they did not, that is equally useful: it means either the longer legs
  still do not starve, or the REPOSITION call is behind a branch the path
  does not take

Either way it is a line in the coverage table you were already producing, not
a piece of work.

**Do not chase this statically** and do not add a leg for it. If the answer
is "not reached", `PICK_X_Y` stays an open finding exactly as C3 has it, and
the report says so.

## The general point

Today produced two static narratives that were wrong (P47's F-B route, my
a002) and one play-derived correction that was right (yours). The asymmetry
is not an accident: a static route is a chain of *possible* edges, and
whether the program actually walks it is a different question that only
running it answers.

So where a coverage question can be settled by looking at what a leg did, that
should beat reading the book — and this project is the one that generates that
evidence.
