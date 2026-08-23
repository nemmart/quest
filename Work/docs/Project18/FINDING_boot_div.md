# Project 18 tranche-A — OPEN divergence (needs diagnosis)

## What we know (tasks 021, 022)
- **The push_map is valid.** quest.pushmap.A loads clean: 515/516
  decorated calls registered, zero validation rejects. Generation is
  confirmed good (two generators agree; slots validated).
- **Boot is clean with all 515 decorated** (task 022: full map, booted
  8s with NO driver, div=0; also boot-only, no-boot, and INIT_OBJ_TBL-
  alone subsets all div=0).
- **Under DRIVEN gameplay, it diverges early** (task 021: every leg
  m/fo/inj/abort/play div=1). The m-leg had writeWSAVS=1, argwr=0 and
  exactly 1 redirect line before divergence — i.e. it diverged after the
  FIRST write-mode call, before any arg was actually written, amid
  ISR/message-passing.

## What we DON'T yet know
- The exact diverging instruction. Task 021 did NOT copy stdout ($R/out)
  into results, and task 022 (which did capture stdout) didn't reproduce
  because it removed the driver — so no divergence detail was captured.
- Which decorated site is the trigger. It fires under driver input, very
  early (~first write-mode call), not during idle boot.

## Task-design errors to avoid on the retry
- 021 load-check used `-types none` without `-trace` → "-trace and
  -types must be used together" abort (cosmetic; the real legs were fine).
- 022 bisected the MAP but dropped the DRIVER, so none of its configs
  exercised the sites that fire under input → false all-clean.

## The diagnostic the next session should run
A DRIVEN run (m leg, normal driver) with quest.pushmap.A, that:
1. CAPTURES stdout ($R/out) into results — the LOCKSTEP DIVERGENCE dump
   names the diverging pc + what differs (wsp / register / memory).
2. From the dump, identify the decorated call site active at divergence
   (cross-ref the redirect trace's last WSAVS mode=W line + the pc).
3. THEN bisect the map WITH the driver running: e.g. binary-split the
   515 sites, driven m leg each half, to localize the offending site(s).
4. Compare against DIST (the one proven site): what does the diverging
   site have that DIST doesn't? (argc, push opcodes, callee frame shape,
   whether the callee itself is book-live/migrated, recursion/re-entrancy,
   a callee reached via message dispatch, etc.)

## Hypotheses (UNCONFIRMED — do not assume)
- A decorated site whose callee is invoked via the ISR/message path
  (not a plain LCALL return), where the marker/flag lifecycle differs.
- A site that fires re-entrantly or from a signal/ISR context where the
  args_written flag or stack_offset interacts with an unexpected frame.
- A callee with a property the census marked CLEAN but that matters under
  redirection (e.g. it reads its args before its own WSAVS, or aliases).
Note: DIST proved the mechanism; this is about WHICH site breaks it, so
the fix is likely to EXCLUDE a small class of sites from tranche A (push
them to a later tranche), not to change the mechanism.

## State
- quest.pushmap.A committed and valid. Tasks 021 (finding), 022
  (boot-clean, non-repro) in results/. No mechanism/doc changes made.
