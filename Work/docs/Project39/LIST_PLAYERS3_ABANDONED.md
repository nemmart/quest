# LIST_PLAYERS.3@7016F556 — abandoned, with the reason

Project 39, routine 1.  **No match number is quoted** (boundary 5: no match
number for a partial translation).  The intermediate figures below appear only
as the *before and after* of one pre-registered experiment, which is what they
are evidence for; they are not a score for the routine.

## 1. What it was chosen for, and what it turned out to be

The gate picked LIST_PLAYERS.3 as the first validator on the grounds that it
was the smallest use of the static link that was otherwise built already — one
uplevel read, and everything else machinery P36/P37 had landed.

**The static-link part of that was correct and is not why it was abandoned.**
`UP(LIST_PLAYERS, w13)` translates, the link loads into ac2 as an R41 base, and
the uplevel read matches the book.  What the gate missed is that the routine
carries *two* constructs the model did not have, neither of them the link.

## 2. R21d — the DO control variable may be a by-reference parameter (BUILT)

`for (*i = 1; *i <= lim; (*i)++)`.  Everything about R21's DO shape is
unchanged except that every reference to the control variable is INDIRECT
through the argument slot: `XNSTA 0,@[ac3+0xFFF4]` for the initialisation and
the indirect `XNDO` for the increment, rather than the direct forms.  A
by-reference parameter used as a subscript also needs a symbolic form, which is
a load THROUGH the argument slot.

Witness: LIST_PLAYERS.3 7016F55E.  **One witness — recorded at C.**

This is the gate's classification error in miniature and is worth stating as a
method point rather than a routine detail: the clean/blocked sweep that
produced "16 of 23" keyed on *statement-level* markers (runtime calls, floats,
arena twins, LDSP).  R21d is an *expression-level* gap, and **no marker-based
sweep can see that class**.  So 16 of 23 is an upper bound on a weaker basis
than it appeared at the gate, and it still does not promise that any of the 16
translate.

## 3. R21e — a DO limit that is an expression goes to a temp (TESTED, CONFIRMED)

This is the question the gate flagged as unresolved and refused to settle by
picking the reading that made the numbers work.  It resolved on evidence.

**The observation.** The book's loop head clobbers ac0 — which is holding the
just-computed player count — with the constant 1, and then RELOADS the count
from frame slot 2 into ac1:

    ac0 = sx16(M16[wp(ac2, 43)])      the count
    M16[wp(ac3, 2)] = trunc16(ac0)    ... stored to slot 2
    ac0 = 0x00000001                  the constant TAKES ac0
    M16[R[ac3 + -12]] = trunc16(ac0)  *i = 1
    ac1 = sx16(M16[wp(ac3, 2)])       the limit RELOADED
    goto [...] (ac0 <=s ac1)

UPDATE_SCREENS, whose limit is a declared local `n`, does the opposite: it
keeps the count in ac0 and gives the control variable ac1.  Reading slot 2 as a
user local reproduces UPDATE_SCREENS' behaviour here and contradicts the book.

**The hypothesis, stated before the test.** Slot 2 is not a user local but a
**DO-limit temp**: PL/I evaluates the TO expression once, and because a temp is
not a protected user variable, the initial constant is free to take the
register the limit was computed in, forcing the reload.

**The prediction, registered before running.** With the limit written as the
expression it is (`*i <= SD_PTR->player_count`) rather than as a named local,
(a) the constant takes ac0, (b) the limit is reloaded into ac1, (c) the loop
head matches.  A head that still diverged would kill the hypothesis.

**The result.** All three.  The loop head matches statement for statement, and
the routine moves 28.4 % -> 37.8 %.  The experiment could have come out the
other way (METHOD §16), so this is evidence and not consistency.

Witness: LIST_PLAYERS.3 7016F556..7016F565, against UPDATE_SCREENS
7017D635..7017D641 for the named-local case.  **Two routines, one on each side
of the distinction — recorded at B.**

## 4. Why it was abandoned anyway

R21e closed the question it was raised for and did not close the routine.  What
remains is an **R9/R10 divergence that has nothing to do with the static link**:
at its first bit reference the book does NOT create the scaled-subscript temp
at all —

    A: ac0 = mul(ac0, ac2); ac0 = add(ac0, 0xFFFFDB10); ac2 = sub(ac2, ac2)
    B: ac2 = mul(ac2, ac0); M32[wp(ac3, 4)] = ac2; ac2 = add(ac2, 0xFFFFDB10)

— and creates it later, at slot **6** rather than the slot 4 R3 predicts, while
recomputing the subscript from the parameter in between.  Both the *when* of
R10's temp and the *where* of R3's slot are wrong here, and the two interact.

Fixing that means adjusting R9/R10 against this one routine, which is the
fitting risk the project exists to avoid, and it would spend the budget the
user allocated to the FIRE pair — the mutual check that matters most for the
construct, because FIRE.1 and FIRE.2 must agree on their common parent's frame
layout *independently* or the disagreement is itself the finding.

**Abandoned with the reason.  Nothing staged.**  The source is kept at
`game/routines/LIST_PLAYERS.3.c` as the R21d/R21e witness, marked partial in
its header comment.

## 5. What the next project needs from this

- **R10's release point and R3's slot for a scaled-subscript temp under an
  indirect control variable.**  The specific question: why slot 6 and not 4,
  and why is the temp created at the second reference rather than the first?
  LIST_PLAYERS.3 is a small, self-contained case for it.
- The DIED pre-registrations remain untouched and NO WITNESS; R10 already
  stands at confidence C with a counter-instance recorded (P37), and this is a
  second counter-instance.
