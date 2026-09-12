# q003 — P54's callee-in-file rule, and what it does to two of the seven

Worker session, Sep 12 2026. Stage C is landed and green (240/240 over four
classes, eight mutations caught, UPDATE_SCREENS 594/594/198). Stage D is the
calls, and reading the loader before writing them turned up a rule that did
not exist when I wrote `q001`.

## What I found

`P54/a001 R1` added a load-time refusal I could not have planned around:

> `call <ENTRY>` enters `<ENTRY>.b0`, which must be a block of THIS file.

Measured, not read — a hand-written ir 8 file with `call FAKE_OCEAN` and no
`FAKE_OCEAN.b0`:

    IRExec: REFUSE: call FAKE_OCEAN but FAKE_OCEAN.b0 is not a block of this
    file — a naive call enters the callee's b0 (docs/IR.md §6, P54 a001 R1)

and the same file with a one-line `block FAKE_OCEAN.b0 / ret` appended loads
clean, placing both entries' blocks in their own 0x77 ranges.

**This invalidates two rows of my `q001` §3 table.** I predicted INIT_SCREEN
and HIT_ANY_CHAR would "compile + load", risk **low**, because the only thing
either needed was a `call` with `a` cells. Compiled one routine per file —
which is what `lower_c.py` does and what every stage so far has assumed —
**both now REFUSE at load**: INIT_SCREEN calls FAKE_LAND_MASS and FAKE_OCEAN,
HIT_ANY_CHAR calls GET_INPUT, and none of those callees is in the caller's
file.

The rule is right, for the reason its message gives: a `call` transfers to a
symbolic block, so a callee with no `b0` in the file is either a typo or an
un-compiled Eagle routine, and the second needs P50's real-stack protocol
rather than the naive bridge. I am not asking for it to be relaxed.

`rt_call` has no equivalent requirement — the callee resolves from the runtime
symbol table, so PICK_X_Y, GET_INPUT's `?READ` and HIT_ANY_CHAR's
`?WRITE_SCREEN` are unaffected as far as LOADING goes (§F-1 below).

## Decision needed

What a compilation unit is, now that a `call` needs its callee in the file.

## Options

- **(a) A multi-routine compilation unit.** `lower_c.py --routine A --routine B`
  emits both entries into one file: all declarations first (in entry order),
  then all blocks. The pieces are already there — every `v`, `a` and `b` name
  is entry-qualified, cross-entry reference is explicitly LEGAL (§5.10.1), and
  one `Lowerer` per entry sharing an output is a small change. Then INIT_SCREEN
  compiles together with FAKE_LAND_MASS and FAKE_OCEAN, and HIT_ANY_CHAR with
  GET_INPUT, and both load.
  - It also settles a question I was going to ask separately: **who declares
    the callee's `a` cells.** With the callee in the file, the callee declares
    its own and the caller just writes them — no cross-entry declaration, and
    no chance of the duplicate that two files each declaring `FAKE_OCEAN.a1`
    would produce if they were ever concatenated.
  - Cost: ordering. "Declared before its first reference, in file order" means
    every routine's declarations must precede every routine's blocks, not just
    its own. That is one list concatenation, but it is the thing that would
    break silently if I got it wrong, so it gets a teeth leg.
- **(b) One routine per file; INIT_SCREEN and HIT_ANY_CHAR are named findings.**
  The prompt allows "named as findings with the reason", and the reason would
  be honest and external. But `a002` just said **"seven of seven is now the
  bar"**, and this would make it five of seven for a reason that is a
  packaging choice rather than a limitation.
- **(c) Ask P54 to let a call load against a DECLARED-but-undefined callee and
  refuse at execute instead.** I do not recommend it: it moves a static error
  to run time, it is an `emulation/` change in a merged project, and (a) costs
  me less than the question costs everyone.

## RECOMMENDATION: (a)

It is inside my boundary, it is a small extension rather than new cleverness,
and it is true to the program: INIT_SCREEN, FAKE_LAND_MASS and FAKE_OCEAN
really are one call graph, and so are HIT_ANY_CHAR and GET_INPUT. It also
makes the acceptance criterion mean what it says — seven routines that compile
and load — instead of five plus two packaging excuses.

**I will proceed on (a) unless told otherwise**, for the same reason I gave in
`q002`: no ruling on this can invalidate Stage D's other work (`rt_call`, the
TMP width table, `BITS`/X.CB, the initialised `v`s), which is most of it. If
you prefer (b) I will unpick the multi-routine driver, which is cheap; if you
want (c) I will stop, because that one is not mine to build.

**What I will still report either way:** each of the seven compiled ALONE, and
whether it loads. That number is the one the prompt actually asks for, and (a)
must not be allowed to hide a routine that only loads because it was bundled
with its callee.

---

## F-1 — a finding for P54, no ruling needed from you

An `rt_call` out of a symbolic block **loads** and then, at execution, reports:

    RIGERROR run: IRExec: rt_call ?WRITE_SCREEN from a symbolic block needs a
    symbol table to resolve the callee (no LCALL word to resolve from)

which is correct and is P52 §4 item 2 exactly. **But the rig then segfaults**
after printing it. I have not chased it — it is `emulation/`, it is past the
error path, and my bar is load rather than run, so nothing of mine depends on
it. Recording it because a crash on a diagnostic path is the kind of thing that
gets misread later as "the IR is broken", and because P54 owns both the bridge
and the symbol-table plumbing that would make that call actually run.

Reproducer: a two-block symbolic file whose `b0` is
`rt_call ?WRITE_SCREEN(0x70000260, wp(V, 0)) ret=...b1`, run through
`lowerc_rig` with `quest.addrbook`. Happy to hand it over rather than have P54
rebuild it.
