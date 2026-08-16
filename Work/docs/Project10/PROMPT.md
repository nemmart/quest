# Project 10 — Stack-Claim Zeroing + the Garbage Probe (B1 fix)

Hi Claude! Solo session; the reviewer verifies. Implements Layering
ruling 8 (deliberate infidelity #4, user-ratified) and its
user-designed validation probe. PREREQUISITE: Project 9 must be
LANDED AND APPROVED first (its Generation-3 terminal rules are
assumed by the matrix below; check NextSession.md / SessionPlan for
its verdict before starting — if it has not landed, STOP and say so).

Read IN ORDER: docs/METHOD.md; docs/Layering.md ruling 8 (the ruling
+ rationale + probe spec — your charter); docs/Project8/REPORT.md §6
B1 (the finding this fixes); the archived DG manual pages'
conclusions in ruling 8 (hardware reserved, never zeroed — this is a
deliberate departure); hw/EagleStack.cpp (the claim sites);
docs/NextSession.md "IN FLIGHT" section.

## Scope 1 — the zeroing (shared instruction path, per-machine gated)

A "claim" = any wsp increase that exposes unwritten stack words.
Zero the claimed WORDS [old_top, new_wsp) at:

- **WSAVS / WSAVR** — after the 5-wide push, the frame_size*2 claim.
- **WSSVS / WSSVR** — after the 6-wide push, same claim.
- **WMSP with positive delta** (57 game uses + 6 RT) — zero
  [old_wsp, new_wsp).
- **STASP raising wsp** (defensive completeness — the known live use
  is the I.GOTO landing stub LOWERING wsp; a raise is a claim).
- Pushes (WPSH/XPSH/wide_push/XPSHJ/WSSVx image words) write their
  words — NOT claims, no zeroing.
Audit EagleStack for any wsp-increase site not listed; report any
found. Gate every site on a per-Machine `zero_claims` flag so the
probe can run asymmetrically. Tripwire comment at the WSAVS site:
this is infidelity #4, the manual says "reserving", ruling 8 is the
license, and the -zero=none attic is the bit-faithful escape.

## Scope 2 — the switches (Launch)

- `-zero=both|none|clone` — ONE switch (user ruling: the asymmetric
  configuration IS the experiment; no separate -probe flag).
  **both** (default) and **none** (pre-ruling attic, bisection
  escape, precedent -handler=mv): NORMAL FULL CHECKER, unchanged.
  **clone** (master unzeroed + clone zeroed) = the garbage probe,
  and it IMPLIES the probe checker: register-VALUE comparison
  relaxed; **pc, instruction counts (existing exemption rules),
  terminal/span flags, and syscall mediation stay armed** (the
  user's detector suite); a loud banner names the experiment so it
  can never be mistaken for normal operation. No other mode
  interactions.

## Scope 3 — validation

1. **Baseline regression, default launch (both-zeroed, full check)**
   — LOGIN-FAST SET ONLY (user policy: no multi-hour matrices):
   -handler=check {FAIL_OPEN, inject shape 2, :ABORT} +
   -handler=mv {FAIL_OPEN}. 0 divergences everywhere. Breadth
   (M-trigger, shapes 1/3, BAD_TOKEN) is reviewer spot-check
   territory, not yours. (Zeroing is
   symmetric: lockstep cannot see it — this proves no mechanical
   breakage, e.g. a zeroing loop miscounting words.)
2. **B1 vanish check**: `-handler=native` login — the Project-8 B1
   divergence (trap 7017FDED, ac0 stale) must be GONE. Run the full
   native LOGIN-FAST set (login, FAIL_OPEN, inject shape 2) and
   report exactly how far native-alone now gets
   (with P9 landed, both blockers are resolved — a fully green
   native matrix is possible and would be the headline; any residual
   failure is a new finding, characterize it precisely).
3. **The probe** (`-zero=clone`, -handler=check): scripted login +
   FAIL_OPEN smoke (two fast runs), then note in the REPORT that the long-duration free-play
   probe is the USER'S to run at leisure — document the exact
   launch line and what a hit looks like (pc fork / count skew /
   mediation mismatch, each named to its pc and syscall). Any hit
   during your scripted probes = a 1988 read-of-uninitialized bug
   located: capture it completely (it is a finding, possibly the
   project's first probe specimen beyond B1's ?WRITE).
4. **The attic**: `-zero=none` login regression green (the escape
   works).

## Deliverables

The gated zeroing + switches; docs/Project10/REPORT.md
(SharedProtocol format: claim-site audit table, matrix evidence per
METHOD §10, the native-alone status headline, probe launch-line
documentation, any probe specimens); Layering ruling 8 gets a dated
IMPLEMENTED note (one line — the reviewer integrates the rest).

## Gotchas

The standing set (NextSession.md): ~49s turns, login
CL/Claude/quest/Y/any/F, scratch QUEST/, stdbuf, drivers in
docs/Project1/, port-8781 zombies, ≥120s ESC waits, grep-warning
false positives, -handler flag semantics (Project8 REPORT §1).
Plus: zeroing writes through machine.memory in the shared path —
make sure the writes go through the same memory interface the
mirror/audit machinery sees, or the server-side page comparison
will teach you about it.
