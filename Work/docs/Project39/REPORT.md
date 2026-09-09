# Project 39 — REPORT (written by the integrator at merge, Sep 9 2026)

*The P39 session ended at its tool budget after the FIRE.2 commit and
did not write a Part 3 report. This file is assembled from its four
commits, its abandonment doc, and the hand-off summary it gave the
integrator, so that later projects have a single place to read. Where it
records the session's own words they are its claims, verified where
noted.*

## Result

**Routines finished: 0. Running total: 4 of ~130** (PICK_X_Y,
UPDATE_SCREENS, REFRESH_SCREEN, OWNS — unchanged).

Verified at merge: four routines 349/349 primary, 242/242 folded, 0
DIFF; `ircmp.py --selftest` PASS; `gen_declarations.py` clean.

## What landed — the static link is BUILT and works

- **The C spelling** (gate ruling (c)): `UPLINK(P)` / `UP(P, name)` /
  `UPARG(P, k)` in `game/quest_rt.h`, with the rejected alternatives and
  the reasoning recorded in the header — including that the struct form
  (b) is the natural endpoint once parents are themselves reconstructed.
  `UPARG` needed its own justification: the parent's parameters are
  by-reference, so `*UPARG(P,k)` is a **triple** indirection (link → arg
  slot → datum), which is what FIRE.2's `XNLDA 1,@[ac2+0xFFF4]` does.
- **The witness guard is enforced, not merely intended.**
  `gen_declarations.py` grows a `PARENT_FRAMES` table and a
  `check_frames()` that is a HARD ERROR if a parent slot or argument
  arrives without a width and a named witness PC.
- **A correction the session made mid-build**: the generated native
  struct cannot preserve frame offsets (the parent's arguments sit at
  negative displacements, its locals at positive), so the header now
  says the native view *names* the slots and does not lay them out; the
  numbers live in `declarations.json`.
- **The translator**: `link_reg()` loads the link as an ordinary R41
  base and caches it like any other address, so a second uplevel
  reference in the same block reuses it and R8 flushes it at a block
  boundary — which is exactly why the book re-loads it per block.
  Uplevel reads, writes and subscripts share one emitter shape (R43).
  The `UPLINK` parameter does not count toward argc (R42).

## Rules

| # | rule | witnesses | conf |
|---|---|---|---|
| R42 | A nested procedure receives the enclosing frame pointer in ac1; WSAVS saves it at `wp(fp,-6)`. Independent of the callee's own argc (arguments travel on the stack at `wfp-10-2N`), so the two mechanisms do not compete | KILL_PLAYER.4, LIST_PLAYERS.3 (argc 1 + link), FIRE.1/.2 (argc 0 + link) | A |
| R43 | An uplevel reference is link-load-then-displace, re-loaded at every block that needs it (R8), with no read/write distinction; it reaches the parent's parameters as `R[link-10-2N]` | 288 base loads program-wide; FIRE.2 ×3 for the parameter form | A |
| R44 | The link is an R41 base when used as a base (ac2, then ac3) and an ordinary value when passed to a call | the 276/12/21 destination census | B |
| R45 | A nested call is `XCALL [pc+disp],N` preceded by a load of the link into ac1: the caller's frame (`ac1=ac3`) from a parent, or the caller's own link (`ac1=M32[wp(ac3,-6)]`) from a sibling | 63/63 sites | A |
| R21d | A by-reference parameter as the DO control variable (the indirect `XNDO` form) | LIST_PLAYERS.3 7016F55E | C |
| R21e | A DO limit that is an expression is evaluated once into a temp | UPDATE_SCREENS and LIST_PLAYERS.3, one on each side | B |
| — | Nesting is exactly one level deep program-wide — a **finding**, not a rule (no mechanism; a fact about this program). The five apparent double-nestings are addrbook mis-attribution of `nocall` ON-units | form-2 sibling calls | A |

**The register question, answered on evidence.** Destination census of
`M32[wp(·,-6)]`: ac2 276 (base), ac3 12 (base, only when ac2 holds a
live address), ac1 18 (value — every one immediately precedes an XCALL
to a sibling), ac0 3 (value — every one precedes `LJSR I.GOTO`). Two
roles cleanly split. This is evidence rather than consistency (§16):
ac0/ac1 are free at cost 0 at many of the 288 base sites and are never
taken.

**R21e was settled by a pre-registered test** — the session wrote the
prediction first (the constant takes ac0, the limit is reloaded into
ac1, the head matches), ran it, and all three came out. The loop head
then matched statement-for-statement. This is the methodological high
point of the compiler line so far: a hypothesis that could have failed,
tested rather than fitted.

## What did NOT land

- **LIST_PLAYERS.3 abandoned** — an R9/R10 divergence (temp created at
  the second reference, slot 6 not 4) unrelated to the static link. See
  `LIST_PLAYERS3_ABANDONED.md`.
- **FIRE.2 incomplete** — refused at a `cvwn`/divide width question,
  also not a link question. No match number quoted for either.
- **THE MATERIAL GAP: the FIRE.1/FIRE.2 mutual check was not
  performed.** FIRE's parent-frame layout in `declarations.json` rests
  on single witnesses per slot. Until the siblings agree independently
  it is **fitting, not derivation** — this was the guard the gate ruling
  attached to spelling (c), and it is outstanding. P40 starts there.

## Corrections to METHOD §16 (made by the integrator at merge)

Four, three from the session's §5 and one from its experience:
`crossings.py` cannot see intra-family mis-sizing (DROP.1 is the live
instance); **every `.N@` statement count is suspect** because ON-unit
bodies interleave into the parent's range; the addrbook's nested flag
conflates called procedures with `nocall` ON-units (the source of the
double-nesting false positive); and **marker-based sweeps are blind to
expression-level gaps** — two of P39's five validators failed on that
class.

## The revised bottleneck

The session's parting judgement, which the integrator accepted and
which set P40's shape: **the bottleneck is no longer the static link
(it works), nor twins, nor floats — it is expression-level modelling**
(R9/R10 temp placement, the width model for divide), blocking nested
routines for reasons that have nothing to do with nesting.

"16 of 23 nested entries are unblocked by the link" is an upper bound on
a **weaker** basis than it appeared at the gate: the sweep behind it was
statement-level and structurally blind to the R21d and divide-width
gaps. It promises nothing about translation.
