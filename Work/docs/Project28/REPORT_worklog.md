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

## Sep 5 2026 — Phase B

- Rulings received (F6 accept, F7 confirmed, LDSP A1, slice order, 041 =
  040 + verdict lines, bar 2,322/4,201).
- PROVENANCE UNCERTAIN, REVIEWED AND ADOPTED (METHOD §10): on resuming,
  lower.py carried ~170 uncommitted lines of rt_call scaffolding
  (RT_* regexes, RT_ARGC, rt_push_of, RTSite, rt_window, rt_slice_ok,
  parse_ldsp_tables, `ir 4` header) modified 15:39, after the 15:25 plan-
  gate commit. Not the user's or the integrator's (confirmed); most
  likely this session's own budget-truncated work with the context
  compacted away. Reviewed line by line against tools/rt_sites.py
  (identical window logic; deviation: an argument needing a t-place
  REFUSES rather than emitting the untested form — kept) and adopted.
- Slice mechanics found and fixed: parse_blocks ignored the `c <callee>
  n <ret>` edge line (call blocks had no successors — the first slice-1
  run refused all 987 with "successor list is not [site+4]"); Nova `SKP`
  has one successor (6 blocks refused under --leftovers until handled as
  `goto [pc+2] 0`); `ADD 2,0` has no `.CC SS` regex group (KeyError).
- Regression: --rt-slice 0 without --leftovers == P27 artifacts byte-
  for-byte (modulo header), both modes.
- Local gates (leg times ~110 s each on this 1-core box; runs detached
  because of the tool's per-call limit): k1fo s1 book/stock, s2 book/
  stock, s3 book/stock, s4(leftovers) book/stock, k1play s4 book — all
  0 div; K=50 play book for RANDOM_NUMBER liveness (§5a of REPORT).
- IR.md → ir 4; HWFindings §7; Provenance post-P28; Run.md note;
  task 041 written from 040; REPORT/CURRENT_STATE/NextSession.
- Tool runtimes: rt_sites.py 1.1 s; lower.py full emission ~6 s per
  mode; nothing > 10 s.

## Sep 5 2026 — battery

- 041: 15/15 legs GREEN, 0 div, 903 s; marker FAILED (2 attempts) from two
  verdict lines I had not run against the artifacts (METHOD §10): ledger
  header counted as a row (988), and `WPSH 0,0` matching the RETURN_MESSAGE
  temp at 70169B77. Reported; user ruled: run 042.
- 042: lines corrected and pre-verified (987/0, 0, 2273); 15/15 GREEN,
  DONE attempt 1, 0 div, 902 s. REPORT §5c, CURRENT_STATE, NextSession.
