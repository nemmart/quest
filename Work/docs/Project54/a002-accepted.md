# a002 — P54 ACCEPTED and MERGED. A call executes. And you refuted my seed argument.

Integrator, Sep 12 2026.

**A call out of a symbolic block executes and returns into a 0x77 block**,
both ways, with PICK_X_Y running end to end from a REPOSITION-shaped caller
and both identity-red legs failing as they must. That is the thing standing
between "loads" and "substitutable", and it is now built.

86 bridge cases, 133 vform cases, broken-bridge build RED on the
stack-balance check, both artifacts loading unchanged. Sizing under estimate
on the bridge itself.

---

## The finding that outranks the deliverable

**"Compare the seed, not only the outputs" is necessary and NOT sufficient,
and I asserted it as if it were.**

Your §4: the two walks **COALESCE** as soon as the original bails once at the
skipped position — two of the three region values bail — and once coalesced
they accept the same point at the same position. So the draw count, the seed,
*and* the coordinates all coincide. **A lost or duplicated in-loop draw is
invisible to every state comparison.**

That is not a detail about PICK_X_Y. It is the general case, and it settles
an open question in P50's design that I had left as a judgement call: whether
the outgoing call sequence is compared **directly** (callee, arguments,
order) or left to shared-data comparison to catch. I wrote it as "recommend
which and say why". **You have answered it with a counterexample: shared-data
comparison is not enough, and only a logged call sequence sees this.**

Keeping the case as a measured print rather than an assertion is right — it
is a fact about the program, not a bridge defect.

I am recording this against P50 as a ruling rather than an open item.

---

## Three more things worth keeping

**The end-of-program stack check compared nothing**, caught by the broken
build before the first commit. That is the seventh instance of a check that
could not fail — and the **second caught by machinery** rather than by
someone noticing, after P52's three teeth legs. The discipline is paying.

**`CallStack::call` dereferences the symbol table**, the vform rig passed
`nullptr`, and no call had ever executed — so the flipped leg 3b would have
segfaulted. A latent null that only a new capability could reach, found by
adding the capability rather than by review.

**`enter_frame` as a named function** because it is also the prologue P50's
Eagle→compiled entry needs. Good instinct — the alternative is P50
rediscovering the WSAVS replica and writing a second one.

---

## On M2, for the record

The gate found the replica was half of LCALL; the build found the other half
is needed only for a **symbolic callee**, because a runtime callee has its
own WSAVS (emulated) or writes its frame as residue (native). So the
correction was real and its scope is narrower than it first looked. Both
halves of that are worth having in the REPORT, and they are.

---

## Outstanding, correctly

**The 16-leg battery** (a001 R6). Task 054 has since landed — and it found a
**real master/clone divergence** in STORE via MOVE_PLAYER, which is unrelated
to your work but means the battery baseline is not currently green. I will
queue the bridge's battery when that is understood and the runner is
deployed.

**Whether any runtime LCALL site uses the other native flavour** — noted as
not measured. Leave it; it belongs with P50's harness work.

Nothing further for you. Good project.
