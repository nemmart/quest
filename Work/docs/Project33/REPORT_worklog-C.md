# Project 33-C — worklog (Sep 6 2026)

- Start: branch p33b-arena-temps @ 9d3e18d; main 06337ca. 047 divdumps
  from results commit e578e74 (b4d1c91 had emptied the directory, exit 143).
- Read PROMPT-C, REPORT-B, p33.ledger (the 8 copy-outs: INIT_OBJ_TBL
  7016DFD6, DISPLAY_CAVE 7016698B, DISPLAY_SCREEN 70166EE9/70166FBC/
  70167049, OBSERVE 70172E4E/70172F1F, HELP 7016DC29), Run.md Capture.
- Capture extended (per-machine arming, IR block-entry check, indirect
  dest, window length, multi-window regions); committed 094ad7a.
- cap1 (login, book K=50, checker ON): INIT_OBJ_TBL's object table
  identical master/clone after the call (96 words).
- cap2 (patient play, book K=50): divergence reproduced (3.70M pairs,
  FIND_OBJECT, ordinals one apart, master insns=5 / clone 0); SD region
  (128K words) + OBJ pages (32K words) identical at every SIGNAL_TURN
  entry/return incl. the turn of the divergence; stderr shows
  "Interrupt - shutting down..." BEFORE the divergence line.
- Code read: Launch.cpp:374 (P33-B SIGTERM graceful), OS.cpp:167
  halt_task, Machine.cpp:257 batch break on halt, MachineThread.cpp:209
  compare_pair unconditional; Lockstep::aborting only on abort.
- cap3 (same, -types lockstep,scalls): clean (killed mid-game, 3.81M pairs).
- kt1..kt4: P32 slice-6 artifact (0 twins) under the P33-B binary,
  SIGTERM mid auto-move: kt1 clean, kt2 clean with a halt-cut equal pair
  (insns=0/0), kt3 login glitch, kt4 DIVERGENCE (ordinals 37 apart,
  insns=147/0) right after the SIGTERM — 047's signature without twins.
- User correction taken: C_A_LISTENER is dead code (ctrl-C/ctrl-A
  listener); its INTWT poll only throws at the kill. Report written;
  no mediator/shutdown change implemented (design items §3).
