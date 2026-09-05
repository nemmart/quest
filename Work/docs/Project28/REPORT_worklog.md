# Project 28 — worklog

## Sep 5 2026 — Phase A (plan gate)

- Tree verified: uploaded Work/Disassembled == main 895fc34 byte-for-byte;
  Provenance prefixes match (quest.dis 5c1db5fb…, blocks.split 1d3baaf6…).
  Branch p28-rt-call; rebased onto main 9972b85 after the P27 merge
  (bd3369c) — no conflicts (Phase A files only). P27 baseline confirmed on
  main: ir2.book 6,258 embeds, ir2.stock 8,137.
- tools/rt_sites.py written (text-only, imports lower.py). Runtime 1.1 s
  (well under the 10 s flag). Output identical before/after the rebase
  (blocks.split/dis unchanged by P27).
- Census reproduces the Sep 5 numbers exactly (987; 887/72/15/11/2).
  Deltas from the prompt's estimates: 18 callees not ~15; 3 RT WPSH not 8;
  0 byte-pointer pushes; 68 Nova loads in blocks.split of which 1 is in
  the excluded block (→ 67).
- Findings F1–F7 in Census.md §2; user rulings taken in-session: F1
  terminator (mirror `call`, site=), F2 argc-set, F6 machine.pc = site
  (recorded as a text difference vs the master's push pc, never fired),
  LNDO/Nova as proposed, LDSP A1 by default (B acceptable), prediction
  2,322 / 4,201 is the landing-bar basis.
- Correction recorded (METHOD §11): the Sep 5 flag said setting
  machine.pc = site would make the stack-fault text match the master's —
  it does not (master names the push pc); Census.md F6.
- STOPPED at the plan gate. Phase B not started (lower.py / IRExec / IR.md
  untouched).
