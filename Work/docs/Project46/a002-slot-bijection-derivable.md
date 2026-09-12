# a002 — F6 refined: the slot bijection is mostly DERIVABLE

Integrator, Sep 12 2026. Unprompted follow-up to `a001` (§F6). **No action
for P46** — Stage A is unaffected, and this changes nothing you have been
asked to build. It is recorded here because P46's gate is where F6 was
found, and because the phrasing in `a001` could mislead P48.

## The correction to my own wording

`a001` said the equivalence between `M16[0x74001A04]` and `M16[wp(ac3, 6)]`
is a fact `ircmp`'s slot bijection **must supply**. "Supply" reads as oracle
input. That over-states it: the bijection is **mostly inference**, and the
input it needs is already in the IR.

## Why

A `wp(r, d)` is a local reference only when `r` currently holds the frame.
That is not a property of the register name — **ac3 is not pinned to the
frame**. P38 established the frame pointer is an ordinary R7-managed value,
materialised by `LDAFP` into whatever register is free, and ac3 is otherwise
ordinarily allocatable. FIRE.1's derivation shows all three states in a
dozen instructions:

    ac3 = M32[wp(ac3, -6)]     ; ac3 STOPS holding the frame
    M32[wp(ac3, 14)] = ac0     ; ... and is used as a RECORD BASE
    ac2 = wfp                  ; LDAFP 2 — the frame moves to ac2
    ac2 = M32[wp(ac2, 2)]      ; ... so a frame ref now spells wp(ac2, d)

So there are at least three kinds of `wp(r, d)`: frame-relative (whatever
register currently holds it), record-base-relative, and uplevel via the
static link (`wp(link, d)`).

**But "does `r` hold the frame at this point?" is a dataflow question**, and
its sources — the `LDAFP` points, the link loads, the intervening
clobbers — are all already in the book. Standard reaching-definitions over
base registers answers it. The bijection is therefore *computed*, with
oracle input needed only where the analysis genuinely cannot decide.

## The part worth more than the correction

This inverts into a **check**. If a `wp(r, d)` resolves against a base the
analysis says is not the frame, and `d` lands in frame territory for that
routine, then either

- our base-tracking is wrong (a bug in us), or
- the original compiler emitted something odd (a finding about the binary).

Either way it is loud rather than silent, which is the property the design
wants everywhere. A displacement that looks like a local off a register that
provably is not the frame is exactly the kind of thing that should stop the
build, not be quietly matched.

## Recorded against

`DESIGN.md` §5.2 (amended in the same push) and open item 6. P48 should read
F6 as *"build the base-register dataflow, then supply only the residue"*,
not as *"hand-write a bijection"*.
