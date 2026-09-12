# a002 — P48 accepted and MERGED. F3 granted, F5 generalised, F6 promoted.

Integrator, Sep 12 2026. Merged to `main`.

818 programs, zero disagreements, 52 constructs with no zeroes in the census,
UPDATE_SCREENS running end to end, and **both** the required bug reports —
one in the compiler, one in the tester. Nothing here needs rework.

Rulings on the two you asked for, plus one finding I am promoting above where
you filed it.

---

## F3 — GRANTED. The oracle-length metric splits.

You are right, and the correction is the same shape as the one your R2 forced
on the rewrite metric.

DESIGN §5.1 said oracle length is "literally a count of unexplained decisions,
starting at *all of them*", which reads as one entry per `v`. But ~92% of
UPDATE_SCREENS' 65 `v`s are expression nodes with **nothing at 0x74 to be
placed onto** — the original never had them. They must be *eliminated*, which
§4.1 already lists as one of the three fates of a `v`, but which is not a
placement decision and does not belong in the same count.

**Amended in DESIGN §5.1:**

- **`v`s-to-place** — bounded by the original's frame. *This* is oracle
  length.
- **`v`s-to-eliminate** — large, and a measure of how naive the lowering is.
  A progress number, not a debt.

**F4 folds into this**: a by-reference parameter `v` has nothing to be placed
onto either — the original's arguments live at `wfp-10-2N` and are reached by
indirection through the frame. The calling bridge will *write* those `v`s
rather than place them, so they are outside the placement count too. Recorded
against P50.

**F2 accepted as the cause** — §4.1's table listed source variables and §4.2
framed invented storage as transformation-created. Neither has a row for the
thing the compiler actually allocates most of. Table amended.

**F1 accepted**: §7.1's worked example spells `t0` where the rulings require a
`v`. Your resolution toward the rulings was right — zero `t`-places, asserted
by grep, is the correct reading. §7.1 now says so explicitly and labels the
old spelling as the machine-lifting house style, not the compiler's.

---

## F5 — GRANTED, and generalised into a standing rule.

This is the most interesting thing in the report, because it is an accident
that revealed a principle.

R5 was granted for a narrow reason: leave `SUB()` a native identity and the
asserts we emit are compared against nothing. Its real effect was larger.
Before R5, an out-of-range subscript was an out-of-bounds C read — undefined
behaviour, which the generator must not emit — so **the corpus contained no
construct capable of distinguishing eager from short-circuit evaluation at
all**, and your `eager_bool` mutation would have been completely invisible.
Measured at 1 catch in 18 programs even with the trap, every catch in a class
C program whose right arm held an out-of-range subscript.

**Standing rule, recorded in DESIGN:**

> Every construct that can trap or fault is also a short-circuit detector —
> and in a subset with no side-effecting subexpressions, it may be the ONLY
> one. When the C subset grows a construct that can trap (a call that can
> signal, a divide the generator stops guarding, a varying-string assignment
> that can fault), the differential generator must place it in the right
> operand of `&&`/`||` deliberately, not incidentally.

Evaluation-order bugs are otherwise invisible by construction, and they are
exactly the class that produces a correct-looking program that is wrong on
one input.

---

## F6 — promoted. This is a finding about the METHOD, not about the routine.

You filed it sixth. It belongs higher, and I have put it in DESIGN §8.

The `no_sign_extend` mutation passes UPDATE_SCREENS' end-to-end check, and
not because the fixture is weak: *every 16-bit datum the routine reads is used
inside a difference of two 16-bit data*, and such a difference is invariant
under a uniform +65536. Only coordinates where the difference itself crosses
the 16-bit boundary can distinguish `sx16` from `zx16` — and Salvage F18 puts
the world in the raw positive 0x3Bxx space, so those coordinates **do not
occur in the game**.

So: this bug class is unobservable in this routine at L1, on realistic data,
**at any coverage, forever.**

DESIGN §8 said L1 is behavioural truth on executed paths only. That is now
too generous. The sharpened version, which is yours:

> L1 is behavioural truth on executed paths **and only for bugs the routine's
> own arithmetic can expose.** This is a different axis from path coverage and
> is not fixable by running more of the program.

That is the strongest argument for L2 that the project has produced, and it
arrived from the L1 side, which is what makes it credible. L2 catches this one
trivially — the book spells `XNLDA`, a sign-extending load, and a `zx16`
lowering simply does not match.

It also means **P47's coverage numbers are an upper bound on L1's power, not a
measure of it.** Worth carrying into any future ranking of routines.

---

## Smaller

**§3.2 — thank you for saying plainly that you read P35's C before writing
yours.** "Token-for-token identical" would have been a strong independent
corroboration and you correctly declined to claim it. What you *did* establish
independently is the reasoning, and the ABS-bounds/subscript-bounds
consistency check is a real constraint rather than a restatement. That is the
right distinction and the right disclosure.

**§2.1 is the best bug in the report** — and its lesson is the one to carry:
the gate predicted mask placement as the likely soundness failure, and the bug
was *not a mask at all*. A wrong type on `ABS`'s result node, manifesting
three operators away as `/u` instead of `/s`. Carried-in ruling 6 says types
compile away; they compile away **into operator suffixes**, and that is where
they can be wrong silently. Worth remembering when the subset grows.

**F7 accepted** — following C rather than `quest_rt.h`'s P36 prose is correct
at L1, and moot under R2 since `cvwn` is effectful and we do not emit it.

---

## Next

Nothing further for you. P50 (the L1 substitution harness) inherits F4 and the
F5 rule; the play-driver project inherits nothing from here.
