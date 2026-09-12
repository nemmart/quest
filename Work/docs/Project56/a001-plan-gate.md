# a001 — P56 gate: APPROVED. Q1 (a), Q2 (a), Q3 (c) — the one place I am overruling you.

Integrator, Sep 12 2026. **Proceed** with the rulings below.

---

## The hoped-for case does not exist, and saying so is the right answer

The prompt asked for 1-1 blocks **and** matching operators. **HIT_ANY_CHAR is
the nearest thing and it is 1/4 on operators.** You measured that and said it
plainly instead of stretching a routine into the frame I offered.

The residual is still what the prompt was after: **6 statement positions, every
one named**, 4 rewrite rules, 4 book-side normalisations, nothing structural.
That is a first case worth having.

## F1 is the finding, and §7 gains a fifth category

**557 raw `@addr OPCODE` lines the book carries and no rewrite can produce**,
with **≥1 in every one of the seven** — the entry `WSAVS`. As §7 is written the
seven are unmatchable for a reason that has nothing to do with the transformer.

Your argument that this makes **normalisation of the target** a *general*
category rather than a convenience for §5.2 is correct, and the prompt only
half-saw it: I introduced the idea for the slot bijection and noted it differs
in kind, without noticing a second independent instance was sitting in every
routine.

**And the discipline you propose is the part I want kept:**

> a normalisation must **check** something, not merely delete

N1 checking 130 `WSAVS` operands against the addrbook — 100/102 agreeing, 0
mismatches — is what separates a normalisation from "delete whatever does not
match". **That sentence is going into DESIGN §7 as the rule for the category.**

## Q1 — (a) GRANTED, and the phrase is ruled now as you asked

Normalise the book side, **checked** against the addrbook; report every other
raw line as a **named unmatched residual** with opcode and address.

**Ruled, so it is not negotiated in Part 3:** "HIT_ANY_CHAR matches the book
exactly" means **exactly, modulo the normalised prologue line, reported**. Any
report using the unqualified phrase is wrong.

(c) is (a) with the evidence deleted. (b) trades P53's standing "compiles and
loads" bar for a text match, to buy a line derivable from the addrbook anyway —
a bad trade, and you identified it as one.

## Q2 — (a) GRANTED

`docs/Project56/oracles/<ENTRY>.oracle`. DESIGN §7 says "alongside the C
source" and that is inside a boundary you may not write; the move is mechanical
when a project owns `game/**`.

## Q3 — (c). I am overruling your recommendation, and your own sentence is why

You recommended (a) — delete, with a report line — and added:

> if you want the census I will run it, and **I would rather be told to than
> decide to**

**Run it.** Three reasons, in order:

1. **You asked to be told.** A worker flagging that it does not want to make a
   call alone is information, and the right response is to make it rather than
   hand it back.
2. **It is cheap.** I measured it while writing this: **1,778 `ac<N> = wfp`
   statements book-wide**, all from `LDAFP`, one opcode, one grep. Not a
   project — an afternoon's measurement at most, and you already have the
   tooling.
3. **Deletion from the target is the strongest act available, and the analysis
   authorising it has a silent failure mode by your own §4.1.** Every other
   normalisation substitutes or checks; this one removes. One site in
   HIT_ANY_CHAR and *unmeasured elsewhere* is precisely the shape of claim this
   project has been burned by eight times.

**Your clobber rule stands and is binding** — every string statement counts as
a definition of `ac0`–`ac3`, so N3 never fires on an `LDAFP` doing real work.
The census is what tells us whether that rule is sufficient at 1,778 sites or
only at the one.

If the census says N3 is safe book-wide, you have a normalisation with a real
warrant instead of a single witness. If it does not, you have found something.

---

## For Part 2

Your plan is right, including **"then move to rank 3 without lingering, because
§5.1 is where the transformer proper is first tested and ranks 1–2 do not test
it."** That is the correct read of your own ranking — do not let two easy
closures stand in for evidence about the hard case.

**GET_INPUT's 62-to-22 statement ratio** is where the elimination burden is
first paid, and §1.2's one-witness structure must be handled **without
inventing a rule for it**.

Run the monotonicity replay in both orders and report against your prediction
either way — P55's Part 3 is the precedent, where the prediction was wrong *and*
the pre-registered-against fix would have moved the number for an unrelated
reason.

Proceed.
