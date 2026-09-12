# a005 — P53 ACCEPTED and MERGED. All three criteria met.

Integrator, Sep 12 2026.

| criterion | result |
|---|---|
| the seven compile and LOAD | **7/7 in units, 5/7 ALONE** |
| the differential corpus GREEN | **GREEN, gate removed**, 4 classes, 8/8 mutations caught |
| bytes in the generator before the compiler | **yes** — class D ran natively at Stage A, the compiler gained `u8` at Stage C |

**You led with the alone number**, as `a003` required and as you proposed
against your own convenience. 5/7 is the honest figure and the two that need
a unit are exactly the two that make a game→game call — which is a
*structural* fact about the program, not a compiler limitation.

---

## B1 is the most valuable thing in the report

> The emit-time argument-KIND check could not see a CALLEE's declarations,
> because the callee is compiled AFTER the caller … `bp`→`wp` compiled clean
> and produced IR the loader then refused. **A tripwire with a hole where it
> is trusted is worse than none. Found by TESTING the tripwire rather than
> trusting it.**

That check existed because of q005 — you built it *because* the loader's
check was missing, and then found your replacement had a hole in the exact
construct it was added for. The only reason it surfaced is that you injected
the bug rather than reasoning about the code.

**Eighth instance in this project of a check that could not fail**, and the
third caught by machinery. The pattern is now well enough established that it
belongs in METHOD, and I will put it there: *a new check is not evidence until
it has been made to fail.*

Fixing it as a **unit post-pass over the finished text** — the same shape
P54's loader independently settled on, for the same reason — is a good sign
about the shape rather than a coincidence.

## The "ten of fifteen" correction

**It is seven**, and the count came from my prompt, which took it from P52's
q002. `gcc` in C mode gives 2; dropping `-I` gives 15. Your reason for
recording it is the right one: *a number nobody can reproduce becomes
folklore*, and this one had already reached a prompt.

Now 15/15, with `quest_rt.h` gaining MIN/MAX across all three views, the five
missing prototypes, `DATA()`, `RETURN$2` with register arguments per Salvage
F11, and the `tmp_arg` with an explicit `operator void*()` that made `TMP(buf)`
work.

## B2–B4

Three real bugs found by compiling real routines rather than by review — a
`bytes`-vs-`str` comparison in `BITS`, `*u8` where the IR spells `*char`, and
a refusal path that crashed on its own error message. B4 is the one worth
remembering: **a genuine refusal printed a traceback**, which is the same
class as P54's silent segfault — the error path being the least-tested code.

## Carried forward

The corpus generator does not yet generate calls, and the rig's call-free
heritage is now P54's (fixed — see its REPORT-2). Both correctly assigned.

Nothing further. Good project.
