# P33-B worklog (Sep 6 2026)

1. Tree verified (main fa5b180; P32 table shas; uploads identical). Read
   StringsDesign §2–§6, P33-A REPORT, P32 Census/ledger, string_sites.py's
   evaluator, IRExec's string executor, lower.py's string folds.
2. tools/p33_census.py on the evaluator: 19 groups, 128 WCMVs = 96 + 32,
   37 blocks, registers at every block end. INIT_OBJ_TBL's intermediate
   pointer survives to 7016DFB7 → plan gate → per-claim twins ruled.
3. quest.arena (arena.py) — first stride 0x1000 overlapped closed ends
   (loader refuses) → 0x2000. strhooks regenerated without the layout.
4. string_sites.py --p33: pair → twin constant at the WADI (DISPLAY_MAGIC
   stores the base before its WMSP); `arena` class ahead of the code-range
   test (the arena is above it); shl of a twin constant keeps the bp form;
   twins are not literals; register-count branch names twins. 9 groups
   refused on "STASP register is ac0" → `release` names its register → 19/19.
5. lower.py slice 7 / ir 6 / arena provenance; IRExec t@/claim/release;
   Arena module; StrHooks per-claim; Mapper claim ordinal; self-tests.
6. First live run: the IR loads before RTStubs → Arena loads first (idempotent).
7. Divergence inside ?WRITE_SCREEN: wsp off by the caller's claims →
   Δ total over live frames (finding 1). Then ac2/ac3/wfp off by the same:
   the callee's frame is shifted → the Mapper's claim-insertion layer
   (finding 2); its ToMaster condition must compare no-claim coordinates
   (worked example in the code); mediated READ verification of the ?WRITE
   buffer → a master temp address maps to its bound twin; the I4 round trip
   → a twin word maps back through its row; clone_location's empty-records
   shortcut lifted for stock mode. k1fo book/stock K=1 green.
8. derr-emu (all-emulated clone claims itself) → cancel the master's
   insertion by pc. SIGTERM graceful for the verdict lines.
9. derr, forced, wrong-capacity refusals; play (book K=50) 0 div but the
   O/D/L/H keys never reach the prompt — HELP demoted (ruling).
10. Task 047 on main; IR.md §5.9; Provenance; CensusB; this report.
