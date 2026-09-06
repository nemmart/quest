# P33 — what was found, what is fixed, what is open (plain language)

*Written Sep 6 2026 at the weekend close, for re-entry. Everything here
is mechanical; nothing reopens a design.*

## 1. Fixed and merged (nothing to do)

**P33-B found two checker gaps while lowering the 19 claim groups.**
Both were fixed on the branch, verified by 047b (16/16 legs, 0 div),
and are on main.

- *wsp term.* The checker compares "claim-free" stack pointers:
  `master_wsp − outstanding_claims == clone_wsp`. P33-A computed the
  outstanding claims per frame; but `?WRITE_SCREEN` runs *inside* the
  claim bracket and its blocks are listed, so at a rendezvous in the
  callee the master's wsp still carries the *caller's* claims. Fix: the
  term is the total outstanding claims, all frames. (Hidden in P33-A
  because back then both engines claimed and the terms cancelled.)
- *Addresses above a claim.* A master claim is a stack insertion the
  clone doesn't make, so everything the master pushes afterwards — the
  callee's frame, its wfp, its arguments — sits at a higher address
  than the clone's copy. Fix: the Mapper's stack leg adds the
  outstanding claims below an address when translating clone→master
  (and subtracts them for the one permitted master→clone use, reading a
  temp during mediated verification). Same mechanism as the book-mode
  frame compression, opposite sign.

**P33-C found why 047's play legs "diverged".** Not a string bug: the
object table and the whole shared-data region were byte-identical on
both engines at every turn, and the P32 artifacts with no twins
reproduced the same signature. The cause was the battery's own
shutdown:

- the play driver never reaches the game's command prompt after the
  auto-move, so it never sends the quit; every play leg since task 034
  has ended by the battery's SIGTERM while lockstep was still running
  (`end=clean` was really "killed mid-game");
- P33-B made SIGTERM *graceful* (a good change), which halts the main
  task mid-batch — and `compare_pair` then compared a half-batch
  against an empty one and called it a divergence.

Fix (merged): `Lockstep::halting` is set by the graceful shutdown
before the halt, and `compare_pair` returns without comparing when it
is set. One flag; the pairing rules are untouched.

## 2. Open — mechanical, first thing next weekend

**The play driver.** `docs/Project13/drive.py` (mode `play`): after the
auto-move the keys O / D / L / H / ESC are sent, but the session log
shows they don't land on the command prompt, so those screens are never
visited and the game is never quit. Fix = look at `play/session.log`
from any battery (what is on screen when each key is sent), adjust the
waits/keys until the sequence reaches the prompt, and end with the
real quit (the leg should then end `I.STOP`, not `clean`). Then:

- change the play/play-st verdicts to *require* `I.STOP`;
- add a standing verdict line "IR statements never executed by any
  leg" so coverage is visible every battery;
- re-run the 16-leg battery (047b's script is the template).

Expected effect: HELP, OBSERVE, DISPLAY_MAGIC, LIST_PLAYERS and the
attack-message blocks become live in the scripted legs for the first
time. Nothing in the IR changes.

## 3. Open — small checker design choice, not urgent

The deterministic form of the P33-C fix: instead of "don't compare a
halt-truncated pair", defer the halt to the next pair boundary so both
engines stop at the same rendezvous. Nicer, not necessary; write it up
as a CheckerHistory Gen 6.2 ruling when convenient.

## 4. Nothing else is open in the string family

WCMV/WCMP/WBLM/WMSP/STASP embeds are 0; 1,822 string statements; 57
arena twins; the checker is on in every leg with Δ_clone ≡ 0. The
"single `p@b`" of the design is the readable-layer rendering of the
per-claim twins (`p@b ≡ t@b.last`); the IR of record uses the twins
because one intermediate temp's pointer is live across a listed entry in
INIT_OBJ_TBL.
