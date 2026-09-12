# q004 — F-1 was MISATTRIBUTED. Correcting it before it hardens.

Worker session, Sep 12 2026. No decision needed from you; this is a factual
correction to a record `a003` made on my report, and it needs to be made now
rather than in the REPORT, because the record is already attached to a merged
project.

## What I got wrong

`q003` F-1 said the rig segfaults after printing the `rt_call` symbol-table
diagnostic. **That is wrong.** I saw one crash, in a bash call that had just
run two different IR files, and attributed it to the wrong one.

Measured properly:

| case | loads? | runs? | crash |
|---|---|---|---|
| `rt_call ?WRITE_SCREEN` out of a symbolic block | yes | no — correct diagnostic, P52 §4 item 2 exactly | **none, 40 of 40 runs clean, exit 1** |
| naive game→game `call` out of a symbolic block | **yes, completely** | no | **SEGFAULT, 40 of 40, exit 139, no diagnostic first** |

So the rt_call half of F-1 should be struck: that path behaves exactly as
designed and prints exactly the right thing. **The crash is real, is worse than
I described, and is on the other construct.**

Worse in three specific ways:

1. **It is 100% reproducible**, not intermittent as "I saw it once" implied.
2. **There is no diagnostic at all.** The rt_call path at least prints why it
   cannot proceed. The `call` path loads everything — four cells placed, three
   blocks placed, 0x76 mapped — and then dies silently.
3. **It is on P53's own path.** `call` out of a symbolic block is precisely
   what `a003` just granted me the multi-routine compilation unit to emit.

Both reproducers are now in `docs/Project53/f1-repro/` as you asked:
`call_symbolic_crash.{ir,vmap}` (the real defect) and
`rt_call_symbolic.{ir,vmap}` (kept, because "this construct loads and
diagnoses correctly" is worth having pinned too).

## Whose it is — I do not claim to know, and did not guess

The seam is between P48's rig, which builds ONE WSAVS-shaped frame for the
top-level routine and leaves the shadow call stack EMPTY
(`tests/lowerc_rig.cpp:119`, `:319`), and P54's bridge, which on `call` pushes
a frame and a `call_stack` entry. A rig written for call-free programs meeting
a bridge that calls is enough to explain it without either being wrong alone.
That is a hypothesis from reading two files, not a diagnosis, and I am labelling
it as one.

## Why this does NOT block me, and what I am doing instead

**My acceptance is "compiles and LOADS", and the load succeeds** — visibly, in
the loader's own diagnostics, before the crash. So Stage E's criterion is
intact.

What I cannot do is use the rig's EXIT CODE as the load check, since it is 139
for any file containing a `call`. I do not need a new rig flag for that: the
loader already prints `IRExec: loaded <n> IR blocks from <file>` on success and
`IRExec: REFUSE: …` on failure, so **the load check is a scan of that output,
and it stays inside `compiler/`** — no further claim on `emulation/tests/`
beyond the `w8` render you already granted. I mention it because asking for a
`--load-only` flag would have been the obvious move and I want it on the record
that it was not needed.

## The lesson, recorded against myself

METHOD §10 says a claim from a code read is not a finding. I made the adjacent
error: **a claim from a single unreproduced observation, reported with the
confidence of a measurement.** It cost you a wrong entry in a defect record.
What caught it was writing the reproducer file `a003` asked for — the crash did
not reproduce, which is the only reason I looked again. Shipping the reproducer
is not bookkeeping; it is the check.

I would rather have found this by running it 40 times before reporting, and
from here I will.
