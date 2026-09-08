# P35 — partial delivery (session crashed)

Sep 8 2026. The P35 session finished its work (REPORT: three routines
197/197 MATCH, register-exact, C99+C++17 clean; DIED refused at the
first bit-field test) but CRASHED before sending a Work.tgz — it sent a
5-file zip instead. Landed here from that zip:

- compiler/translate.py, compiler/ircmp.py, compiler/CODEGEN_RULES.md
- docs/Project35/REPORT.md, REPORT_worklog.md

**Verified on landing**: both tools parse; `ircmp.py --selftest` PASSES
against the real emulation/quest.ir2.book (identity 100% MATCH on all
four routines, slot-permutation → RENAME, one regswap → one DIFF,
folded-identity clean; 1.7 s). So the comparator and the book agree.

**OWED (not in the zip — the session must resend, or a short P35-b
regenerates from the report + CODEGEN_RULES.md):**
- compiler/gen_declarations.py
- game/quest_rt.h (filled in), game/declarations.h (generated)
- game/routines/{PICK_X_Y,UPDATE_SCREENS,REFRESH_SCREEN,DIED}.c
- docs/Project35/PLAN.md, docs/Project35/results/*
Until the routines + header are back, translate.py cannot be run here
(no C source to translate). ircmp.py is fully usable now.

The result stands on the report and the self-test; this note only marks
what to recover before P36 builds on it.
