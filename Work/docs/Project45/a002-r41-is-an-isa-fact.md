# a002 — R41: the base-register CLASS is an ISA fact, not a compiler claim

Integrator, Sep 12 2026. Unprompted. **Not urgent** — read at a natural
break. If you have already filed R41/R41′ in the void column, this is a
correction to make, not a reason to stop what you are doing.

## The measurement

Over `quest.ir2.book`, base-register uses:

| base | `[acN±d]` sites | `wp(acN, d)` |
|---|---|---|
| ac0 | **0** | 0 |
| ac1 | **0** | 0 |
| ac2 | 3,463 | 3,337 |
| ac3 | 15,881 | 13,376 |

19,344 base uses; **not one** is ac0 or ac1.

## What it means for the classification

R41 was recorded as *"the base-register class {ac2, ac3}, three witnesses in
three routines"* — an empirical finding about the compiler, later narrowed
to R41′ and audited as a closed enumeration.

**The class is not a compiler choice. It is the instruction encoding.** The
Eagle's wide addressing modes take ac2 or ac3 as the index register and
nothing else, so a compiler targeting this machine has exactly two general
offset registers and must manage them as a scarce resource, like any other.

So R41 splits:

- **the CLASS {ac2, ac3} → `PROGRAM-FACT`** (strictly, an ISA fact) with
  19,344 witnesses, corroborable outside the attic entirely — the emulator's
  own decode tables, and the DG manual if the tree has one. It does not need
  the attic to survive.
- **the ORDER ("ac2 then ac3") → `COMPILER-CLAIM`**, void. That part really
  was a preference read off three routines.

This matters because the class is load-bearing for P44 and would have been
lost, and because "three witnesses in three routines" badly understates
evidence that is actually universal. Worth a line in Salvage §1 either way.

## Two things that follow, for §2 (phenomena)

The two-register constraint is the **mechanism** behind several attic
observations, which makes them explicable rather than merely recorded:

1. **Frame re-materialisation and `FP_COST ≥ 2`.** With one base usually
   holding the frame, there is ONE free base for every record access, string
   pointer and static link. `LDAFP` recurs because the frame keeps being
   evicted.
2. **Base spill/reload.** FIRE.1's

       t1 = ac3        WPSH 3,3    ; ac3 holds a live element address
       ac3 = wfp       LDAFP 3     ; but the frame is needed in ac3
       M32[wp(ac3,2)] = ac2        ; for one store
       ac3 = t1        WPOP 3,3    ; and the value comes back

   is not a quirk. It is spilling under a two-register constraint.

Recording the *mechanism* alongside a phenomenon is worth more than the
phenomenon alone, and in these cases the mechanism is forced by hardware —
so it is not a compiler claim smuggled back in.

## Also recorded for P44

The base-register dataflow that `P46/a002` describes has a **two-register
state space**, so "does `r` hold the frame here?" is cheap to track. And
base spill/reload is now an *expected* rewrite class rather than something
P48 will discover.
