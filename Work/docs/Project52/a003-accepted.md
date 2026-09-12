# a003 — P52 ACCEPTED and MERGED. ir 8 is live.

Integrator, Sep 12 2026.

129 self-test cases green with a teeth leg for every refusal, both artifacts
loading unchanged through the new loader, calls validating without executing
and saying so by name, and the sizing scored honestly against the gate. The
project did what it was scoped to do.

---

## What I want on the record

**F3 is the most instructive finding, and it is about method.** A bare
aggregate name being refused (R1) meant a string address now arrives in three
shapes, and `check_piece` assumed one — so **three teeth legs LOADED that
should have refused**, and the self-test caught it rather than a reading.

A teeth leg that passes when it should fail is the exact failure this project
has now recorded six times: P41's three, the battery's permanently-set
`FAILED` marker, 052's stale `want` values, and this. **It is the first time
one was caught by machinery rather than by a person noticing.** That is what
building the teeth in from the start buys, and it is the argument for keeping
that discipline in P53.

**F1 was my error, and the replacement is better than the rule I asked for.**
I specified a blanket literal ban to enforce the bit-31 invariant; it would
have refused the book **660 times**. Your replacement does not merely narrow
it — it *derives* the guarantee: every address an ir 8 program can produce
comes from `wp()`, `bp()` or a cell name, all three masked into ring 7, so
bit 31 is clear by construction. The scoped refusal is a belt on a guarantee
that already holds, and it costs the book 0.

**F2's compatibility window is the right shape.** Accepting an `ir 7` header
iff the file declares no `v` and no `a`, measured on both artifacts, checked
both ways in the self-test. That is how a version bump avoids forcing a
regeneration.

**The sizing prediction worth carrying forward** is the one you flagged: the
executor came in *under* (~13 lines against ~25) because the `wp`/`bp`
overload and the width/sign decision live in the **parser**, so `Ctx` never
consults the declaration table. That is a structural property, not luck, and
it should hold for whatever ir 9 adds.

---

## What remains, stated as you stated it

**Calls load and validate; they do not execute.** The LCALL replica with a
0x77 return is P50's build or P53's, and nothing in ir 8 pretends otherwise.

**The difftest is gated expected-red until P53.** Acceptable as a resting
state only because it is *declared* and has an owner. P53 clears it.

---

## For P53, inherited from this project

1. **Ten of fifteen routine files fail `g++ -fsyntax-only`** (q002 §3) — and
   UPDATE_SCREENS is missing `#include "declarations.h"` while its six
   siblings have it. The native view is one of P48's two differential
   oracles; fix this before running any corpus.
2. The eight granted `compiler/` lines are done; the rest of the rename's
   consequences are P53's.
3. The valued-call split is a **subset rule** now written into §5.10.6 — a
   call cannot sit inside a larger expression.

Nothing further for you. `docs/IR.md` grew 1,144 → 1,630 lines and remains
normative and self-contained, which was the constraint that mattered.
