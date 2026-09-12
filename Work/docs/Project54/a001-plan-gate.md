# a001 — P54 gate: APPROVED. All seven rulings granted. M2's correction matters.

Integrator, Sep 12 2026. **Proceed.**

This gate did precisely what it was asked to: took four claims I made from a
design conversation and checked them line by line. **One held half**, and the
half that failed is the one that would have cost the most.

---

## M2 — the correction

I said "an LCALL replica already exists". You found it replicates the
**caller's** half — `wide_push((psr<<16)|argc)`, `ac[3] = pc`, `ovr = 0`,
`call_stack->call(...)` — **and nothing else**, and specifically **not the
callee's WSAVS** (`EagleStack.cpp:419–438`). A symbolic callee has no WSAVS
instruction to execute.

So the bridge needs a WSAVS replica the design never mentioned. That is
exactly the class of omission "verify rather than trust" exists for, and it
would otherwise have surfaced as a mysterious frame corruption mid-build.

**M3's two qualifiers are equally load-bearing**, particularly the second: a
native return sets `native_break`, so `Machine::run` **ends the batch** at the
0x77 continuation and the block runs on the *next* `run_steps`. That is a
correctness-neutral but test-shaping fact — the self-test must drive
`run_steps` in a loop — and under lockstep it is a rendezvous at a pc the
master does not have, which is P50's problem and correctly flagged rather than
absorbed.

**And the two facts the claims did not state are the best work in the gate.**
`?RANDOM_NUMBER` being a *logging stub* absent from `translation_table`, so an
`rt_call` falls through to the emulated body at 7017DE33 and the "result in
ac0" is really **F4's slotpatch restored by WRTN** — that reframes what
"valued rt_call" means. And `emu_rt::random_number` being dead code
(referencing a `machine.native_context` that `Machine.hpp` does not declare,
and not on the link line) is the kind of thing that would have been assumed
live by the next person to look.

---

## Rulings

**R1 — GRANTED.** `call <ENTRY>` transfers to `<ENTRY>.b0`; refuse a naive
call whose callee has no `b0`, and refuse calling un-compiled Eagle code from
a symbolic block. You are right that the latter needs the real-stack protocol
and the M4a area writes, which is P50's. **I will land the IR.md §6 line** —
send me the wording. An `entry` marker is more grammar than the problem needs.

**R2 — GRANTED, (a).** argc = 0. Nothing was pushed, WRTN pops nothing,
`arg_count` carries the arity. Your argument decides it: the shape difference
*is* the point of naive, and tombstones would buy a shape nothing reads at L1
while making the broken-bridge teeth harder to state. The original's shape is
a **rewrite** to reintroduce, exactly as with pure-vs-effectful operators.

**R3 — GRANTED.** The `IRExec::rt_registry_override` seam, eight lines, inert
when unset. Building an `OSProcess` with a `.PR` file and the FS layer is the
wrong weight for a self-test, and a seam that is null in the emulator cannot
change its behaviour.

**R4 — GRANTED.** Duplicate the dispatch tail with a citation. Refactoring
`EagleStack`'s LCALL/XCALL/LJSR arms is a change inside the strict surface for
no gain, and the strict surface is what the whole project rests on. Twenty
duplicated lines with a comment naming the original is the cheaper risk.

**R5 — GRANTED.** New file plus script; `vform_selftest` leg 3b flips from
asserting the refusal to asserting execution, and the case count moves by one.
Say the new number in the REPORT so the 129 → N change is not a mystery later.

**R6 — the runner is NOT deployed. Do not queue a battery.**

`bin/runner.sh` is fixed in the repo and the box still runs the old version,
so a restart can wipe an in-flight attempt's results. The disk was filled and
cleared today, and **task 054 is running right now** — queueing 055 behind it
risks the overlap path that destroyed 053's results twice.

Do as P52 did: the two-artifact load probe, a local in-container leg if one
fits, and say plainly in the REPORT that the battery is outstanding. I will
queue it once 054 lands and the runner is deployed.

**R7 — GRANTED.** A test-native `?RANDOM_NUMBER` with the LCG copied from
`rt/random_number.cpp` plus a host oracle. Your justification is the right
one and I want it in the REPORT: **the seed comparison is still real** — same
memory word, advanced the same number of times by the same recurrence — so it
still catches a bridge that loses or duplicates a call, which is the failure
the test exists for. The emulated 7017DE33 path is a lockstep matter.

---

## One thing for the REPORT

You have made M2's correction, M3's qualifiers, and the two unstated facts
into a better account of the calling mechanism than the design had. **Collect
them in one section** rather than leaving them in a verification table — P50's
harness design and P55's matching work both need this, and neither will think
to read a gate's §1.

Proceed.
