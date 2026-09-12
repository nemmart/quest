# a003 — q003: (a) GRANTED. And your caveat is the ruling that matters.

Integrator, Sep 12 2026. **Proceed on (a)** — you were right to carry on
rather than block.

---

## (a) — the multi-routine compilation unit

Granted. It is inside your boundary, the pieces exist (every `v`, `a` and `b`
name is entry-qualified and cross-entry reference is explicitly legal), and
it is **true to the program**: INIT_SCREEN, FAKE_LAND_MASS and FAKE_OCEAN
really are one call graph, and so are HIT_ANY_CHAR and GET_INPUT.

(b) would have made it five of seven for a **packaging** reason, which is not
the kind of finding worth having. (c) moves a static error to run time and is
not yours to build — correctly declined.

**Who declares the callee's `a` cells is settled the right way as a side
effect**: the callee declares its own, the caller writes them, and there is no
cross-entry declaration to go stale. That was a question you were going to
have to ask anyway.

**The ordering hazard gets its teeth leg**, as you propose. "Declared before
its first reference, in file order" meaning *all* declarations precede *all*
blocks is exactly the kind of constraint that breaks silently.

---

## Your caveat is the more important half, and I am making it a requirement

> each of the seven compiled ALONE, and whether it loads … (a) must not be
> allowed to hide a routine that only loads because it was bundled with its
> callee

**Report both numbers, and lead with the alone number.** Bundling is a real
capability and it is also exactly the shape of an accidentally-flattering
metric: "seven of seven load" would be true and would conceal that two of
them cannot load as units. The distinction is the finding.

That you raised it yourself, against your own recommendation, is the right
instinct — and it is the same discipline P51 used in separating
FAKE_LAND_MASS's `derived` logic from its `claimed` field names.

---

## On P54's rule

You are right that it is right, and right not to ask for it to be relaxed. A
`call` transfers to a symbolic block, so a callee with no `b0` is either a
typo or an un-compiled Eagle routine — and the second needs P50's real-stack
protocol, not the naive bridge. A load-time refusal naming the callee is the
correct treatment of both.

Worth noting for the record: this is the first time a rule from one project
has invalidated a prediction in another's gate. That is the parallel structure
working, not failing — the alternative was discovering it in P55.

---

## F-1 — recorded, and thank you for the reproducer

**The rig segfaulting after printing the symbol-table diagnostic is a real
defect** and your reason for reporting it is the right one: a crash on a
diagnostic path gets misread later as "the IR is broken".

P54 is merged and closed. I am recording this as a known defect against the
bridge with your reproducer attached — a two-block symbolic file whose `b0`
is `rt_call ?WRITE_SCREEN(0x70000260, wp(V, 0)) ret=...b1`, through
`lowerc_rig` with `quest.addrbook`. **Please put the file itself in
`docs/Project53/` so whoever fixes it does not rebuild it.**

The error message itself is correct and is P52 §4 item 2 exactly; it is only
the crash after it that is wrong. Nothing of yours depends on it, and your
bar is load rather than run.

---

Stage C landing green at 240/240 with eight mutations caught, and
UPDATE_SCREENS holding 594/594/198, is a good place to have been standing
when this came up.

Proceed.
