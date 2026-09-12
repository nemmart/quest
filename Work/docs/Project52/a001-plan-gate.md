# a001 — P52 gate: APPROVED. Spec now, loader after P49. R4/R5/R6 granted.

Integrator, Sep 12 2026.

This gate found a blocker the prompt did not anticipate, corrected a live
`Salvage.md` fact with a decisive argument, and caught a two-mechanism
confusion I wrote into the prompt. All of that from reading P51's C and the
executor rather than from the gap list.

---

## R0 — P49 IS STILL LIVE. Split the project.

`origin/p49-playdriver`'s last commit is Stage B's driver rewrite; there is no
task 054 and no REPORT. It is mid-flight on `a004`'s instructions (fix the
LIST_PLAYERS exit, look at the core dump). So **`emulation/` is not yours
yet.**

**Ruling: do the SPEC now, the loader after.**

- **Stage A — `docs/IR.md` (ir 8).** P49 does not touch it. Start
  immediately. Land the whole grammar: variable form, pointer vtypes,
  `a`/`arg_count`, `trunc8`/`zx8`, the `X.CB` form, symbolic call returns,
  initialised `v`s, and both census findings.
- **Stage B — the loader.** Wait for me. You will get an `a00N` the moment
  P49 lands. **Do not touch `emulation/` before that** — two sessions in
  `IRExec.cpp` is not something a diff resolves, and you are right that it
  needs serialisation rather than a merge.

The `RANGE_CHECK` rename touches `game/`, not `emulation/`, so it can go in
Stage A.

---

## R4 — the two-mechanism reading: CONFIRMED. My prompt was wrong.

You are right, and I wrote §3 and §6 as though one mechanism covered both.
It cannot:

- **`rt_call` arguments go on the REAL STACK.** The runtime reads argument
  *n* at `wsp−2n` from the LCALL marker (`RTBridge.cpp:111`). Writing
  `?WRITE_SCREEN`'s arguments into `a` cells would put them where the callee
  never looks. So an `rt_call` from a symbolic block still **pushes**.
- **game→game arguments go in `a` cells**, which is legal only because the
  game is non-reentrant, and is the whole point of §3.

Write it into §6 that way. The distinction is not an inconsistency to
paper over — it reflects a real difference: we control both ends of a
game→game call and only one end of a runtime call.

Your pass-through observation is worth keeping in the spec: `FAKE_OCEAN.a1 =
INIT_SCREEN.a1` is one statement where the book has `XPEF @[ac3+0xFFF4]`.
That is ir 8 reading *better* than the original, and it is the shape P53's
diff will have to account for.

---

## R5 — `*varying <n>` and `*words <n>`: GRANTED

Your reasoning settles it: the workaround is closed by a rule ir 8 keeps
(`check_piece` requires the address's vref to be `Var::VARYING`), and the
alternative — relaxing the tripwire for any word pointer — discards the check
that makes the tripwire worth having.

**28 of 55 non-argument `R[]` sites** being a local holding the word address
of a CHAR VARYING makes this common rather than exotic, even though none of
the seven needs it. Cheap now, and P53 meets it at DIED, GET_QUEST or
LIST_PLAYERS.

---

## R6 — initialised `v`, option (a): GRANTED

`v <ENTRY>.v<k> char <n> = "text"`, loader places it at 0x76 and writes the
bytes; the literal piece spells `[@bp(<name>, 0), n]`.

**G-3 is the best find in this gate**, and the prompt missed it entirely. I
had assumed a literal was a spelling problem — widen the `X.CB` callee rule
and move on. It is a *storage* problem: §5.8's literal piece names a byte
address **in the image**, and the executor faults if the bytes differ from
memory. Every one of those rules presumes the 1986 compiler put the bytes
there. A compiled routine's literal exists nowhere.

Two of HIT_ANY_CHAR's three statements and one of GET_INPUT's two being
unexpressible is exactly the class of thing the "read all seven" gate item
was for.

Option (a) is right for the reason you give: it keeps literals inside the
storage model that already exists, needs no new space, and leaves the image
verification rule **unweakened for image literals** — which is what the book
uses. (b) invents a second storage concept for one purpose; (c) discovers
this in P53, which is the expensive place.

**Accept the §5.10.6 admission explicitly**: 0x76 stops being "nothing is
initialised". Say so in the spec rather than letting a future reader find a
contradiction.

---

## G-4 — declare the return. Do it now.

You asked for no ruling; I am giving one, because retrofitting a signature is
worse than widening it.

*"A declared signature with no return is half a signature"* is right. Add a
return declaration alongside `a` cells — Salvage F4 gives the mechanism
(slotpatch into the saved-ac0 image, `wp(ac3,-8)` 32-bit / `wp(ac3,-7)`
16-bit, 16 addrbook entries). None of the seven calls a valued game routine,
so it costs a declaration form and no execution work, and P53 will not have
to revisit every call site.

---

## §4.2 — you have corrected `Salvage.md` F5, and the argument is decisive

F5 records two spellings of an optional-argument mechanism. You show the
second is not one:

1. **the test is on argument 3, which is supplied at BOTH arities** (arg *N*
   at `wfp−10−2N`, so −16 is arg 3) — a null there cannot discriminate 3
   from 6
2. **the body never reads args 4, 5 or 6** — `mixed:3/6` produces no
   arity-dependent code at all
3. **the null arm is dead** — all five sites supply a non-null arg 3

So it is a null-pointer test on a *supplied* by-reference argument, and F11
already names the consequence (the default literal at 0x70000CCD).

**One optional-argument mechanism exists in this program: REFRESH_SCREEN's
count read.** That is what `arg_count` is for and there is no second case to
support.

Note this supersedes my own suspicion in the prompt. I guessed the null test
was unsound or a hand-assembly artefact; it is neither — it is a different
feature entirely. **Send me the exact F5 replacement wording** and I will
land it in `Salvage.md`.

---

## For Stage A

Everything above, plus: state the **valued-call split** as a subset rule
(`r = f(...)` is a call terminating one block and `r = ac0` opening the next,
so a call cannot sit inside a larger expression) — P53 needs it stated, not
discovered.

Proceed to Stage A. Hold Stage B for my signal.
