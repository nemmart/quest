# P33-A worklog (Sep 6 2026)

1. Tree verified (main 8738975; Provenance prefixes; uploaded == repo). P31's
   regenerated census read from origin/p31-located-strings (3a230e5) and
   vendored under docs/Project33/inputs/ with its sha.
2. Plan gate. Checks that shaped it: (a) every group's claims sit in one
   block, 19 distinct blocks; (b) 18/19 groups have listed entries and 11 the
   ?WRITE_SCREEN crossing between claims and STASP → 6(b) false → symmetric
   Δ (S1); (c) all 12 claim routines are book-redirected → S2; (d) 19/19
   `LDASP r; WADI 2,r` before every WMSP and the last one is the pushed /
   copy-out address; (e) the master's ON-pop is the emulated I.GOTO
   (STAFP 2; WRTN; landing STASP), R?SIGNAL's STAFP+WRTN found (DEF?ON is L2).
3. tools/strhooks.py → quest.strhooks. First table said onpop 7017EC9E;
   the decode check would have caught it (XWLDA is two words) — EC9F.
4. StrHooks.{hpp,cpp}, hook lines in EagleStack/frames, Mapper assert, Lockstep
   (wsp term, F2-b, readout, dump), RTStubs (POKE role, loader call, report),
   Trace types, ClaimDelta range exit. Built clean.
5. strhooks_selftest: rigs must be heap-allocated (Memory's 2 MB inline
   permissions array — stack overflow otherwise, the P30 note); CallStack's
   "Empty call stack" throw after the WRTN hook swallowed by the rig; the
   broken build must print RED, not abort → main catches. 64 GREEN, teeth RED.
6. First live k1fo (flag on): abort "WRTN at 7017E312 discards frame 74003808
   with delta 22" — the ≥ rule compared area (0x74…) against real-stack wfps
   numerically. Fixed with Mapper::frame_precedes; hooks moved before
   area_wrtn_fixup/area_unwind_to (records must live); ClaimDelta got a
   predicate exit. Also: Δ==0 at frame exit is for ordinary WRTNs only
   (unwind cuts discard claims legitimately) — `unwind` rows added.
7. k1fo on/off, derr, derr-emu, forced all as wanted locally (REPORT §5).
8. Task 045 written from 040; docs; committed; branch pushed; 045 on main.
