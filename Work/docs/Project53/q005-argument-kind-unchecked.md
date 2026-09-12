# q005 — the argument pointer-KIND check is specified, commented, and absent

Worker session, Sep 12 2026. Found while testing the one thing in my Stage D
plan I had been describing to myself as "probably": whether the loader catches
a byte/word pointer mismatch at a call. It does not.

## What I found

`docs/IR.md` mandates the check twice:

- §5.1, REFUSED AT LOAD: *"a `call`/`rt_call` whose declared arity or whose
  argument pointer KINDS disagree with the callee's `a` cells"*
- §6: *"each `a` cell of pointer type must have been written by a preceding
  statement whose value is of the matching KIND"*

`IRExec.cpp` says so too, in two comments:

- `:887` — `naive_calls.push_back(...);   // arity/kind checked at end of load`
- `:1349` — *"a naive call's declared arity and its arguments' pointer KINDS
  are checked against the CALLEE's declarations"*

**The loop those comments describe checks arity, `arg_count`'s presence, and
the callee's `b0`. It contains no kind test at all** — the single occurrence of
"kind" in it is the word in the comment.

Measured, two files differing in one character:

    GET_INPUT.a1 = bp(HIT_ANY_CHAR.v0, 0)    ; a1 is *char — CORRECT
    GET_INPUT.a1 = wp(HIT_ANY_CHAR.v0, 0)    ; a WORD pointer into a BYTE cell

Both load: `IRExec: loaded 3 IR blocks`. The second is exactly the disagreement
§6 names.

**This is narrow, and I want to be precise about what is NOT broken.** The
§5.10.5 M-form tripwire works perfectly — adding `ac0 = M32[GET_INPUT.a1]` to
the same file refuses with a textbook message:

    REFUSE: tripwire: M32[GET_INPUT.a1] on a BYTE pointer (*char) — byte
    pointers take M8 only (docs/IR.md §5.10.5)

So kind is enforced where a pointer is USED and unenforced where one is PASSED.
P52's self-test has a leg for the first and, I assume, none for the second.

## Why it matters to me

§5.10.5 says the tripwire exists *"because it catches a compiler emitting the
wrong width or the wrong pointer KIND AT THE IR BOUNDARY, where it is loud,
instead of three projects later as a behavioural divergence."* Passing a
pointer at a call is where **this** compiler is most likely to get kind wrong:
HIT_ANY_CHAR hands GET_INPUT a `*char` built with `bp()` while every other
argument in the seven is a word pointer built with `wp()`, and the two spellings
differ by one letter.

I had planned to lean on the boundary check. It is not there.

## Decision needed

Whether I compensate in the compiler, and whether the loader gap gets scheduled.

## Options

- **(a) I extend my own emit-time tripwire to cover it, and report the loader
  gap for someone else to close.** My tripwire already consults every cell's
  declared vtype, so "a pointer `a` cell assigned a `wp()` must be a word
  pointer, a `bp()` must be `*char`" is a few lines in a file I own.
- **(b) Ask for the loader fix and wait.** It is `emulation/hw/IRExec.cpp` —
  P52's file, in a merged project. Not mine, and it would block Stage D.
- **(c) Do nothing and rely on the seven being read carefully.** No.

## RECOMMENDATION: (a), with the gap reported rather than quietly routed around

A compiler should not depend on a downstream checker anyway — that is the
argument I made for building the emit-time tripwire in Stage B, and it applies
unchanged here. (a) costs me a few lines and protects the exact construct most
likely to go wrong.

**But the gap should not be closed by my working around it**, which is why this
is a question and not just a note in the REPORT. Two things follow from it that
are not mine:

1. **`docs/IR.md` currently describes a check that does not run.** Either the
   loader gains it or §5.1/§6 should say it is not yet enforced. A spec that
   overstates its enforcement is worse than one that admits a gap, because the
   next compiler will lean on it exactly as I was about to.
2. **`IRExec.cpp`'s two comments assert it happens.** Those are more misleading
   than the spec, because they are at the site and read as a description of the
   code below them.

I am proceeding on (a) so Stage D is not blocked, as with q002 and q003.

## What I am NOT claiming

I have not checked whether `rt_call`'s argument kinds are validated at a
decorated `site=`, where a pushmap exists — that path is P28's and untouched by
this. My measurement covers the naive symbolic-call form only, which is the
form P53 emits.
