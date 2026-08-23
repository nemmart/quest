# Project 18 — HANDOFF (finish tranches A & B)

Hi Claude! You're picking up Project 18 of the QUEST work: recovering the
source of a 1986 Data General MV/8000 game (only the binaries survive) by
running it under a lockstep emulator — a stock "master" and a modified
"clone" execute in parallel and a checker verifies they agree every
instruction, so the clone can be transformed safely. M4a relocated every
routine's stack FRAME to a fixed area; M4b redirects caller ARGUMENT
PUSHES into the callee's area. The mechanism is proven on one site
(DIST); Project 18 widens it. A previous coordinator session did the
generation half and got a partial test result — this note hands you the
open item to finish.

## Orient first (read in this order)
1. Work/docs/CURRENT_STATE.md
2. Work/docs/M4bNotes.md — the design of record (the stack_offset and
   QUEST-base-record rulings + the write-mode-WSAVS subtraction-timing
   amendment are law).
3. Work/docs/Project16/REPORT.md, Project17/REPORT.md — the proven
   mechanism (args→area, marker tombstone+written, flag consumed at
   WSAVS, mid-window stack_offset accounting, QUEST as base record).
4. Work/docs/Project18/PROMPT.md — the widening plan (tranches A=515
   flat single-word, B=20 WPSH multi-slot; C&D are a later project).
5. **Work/docs/Project18/FINDING_boot_div.md — THE OPEN ITEM. Start here
   for what's unresolved.**

## What's DONE (don't redo)
- The push_map for tranches A (515 sites) and B (20 WPSH sites) is
  GENERATED, VALIDATED, and COMMITTED: Work/c_src/quest.pushmap.{A,B,AB},
  generator tools/gen_pushmap.py. Two independent generators agree; every
  slot validated inside its callee's arg region; marker == wfp−10. The
  map loads clean on the runner (515/516 decorated calls, 0 rejects).
- Boot with all 515 decorated is CLEAN (task 022: div=0).

## What's OPEN (your job)
Under DRIVEN gameplay (task 021, m/fo/inj/abort/play) the clone diverges
EARLY — after ~1 write-mode call, before any arg is written, amid
ISR/message-passing. It was NOT reproduced by task 022 only because that
task dropped the driver (a task-design slip — see FINDING doc). So the
divergence is real but its exact cause is uncaptured.

**Step 1 — capture it.** A driven m-leg run with quest.pushmap.A that
copies stdout ($R/out) into results; the LOCKSTEP DIVERGENCE dump names
the diverging pc and what differs. Cross-ref the redirect trace's last
`WSAVS mode=W` line to identify the decorated site active at divergence.
**Step 2 — localize.** Bisect the 515-site map WITH the driver running
(binary-split, driven m leg each half) to pin the offending site(s).
**Step 3 — diagnose.** Compare the offending site to DIST (the proven
one): what does it have that DIST doesn't? Likely candidates in the
FINDING doc (callee reached via message/ISR dispatch, re-entrancy, a
callee that reads args before its own WSAVS, etc.). The mechanism is
proven, so the fix is most likely to EXCLUDE a small class of sites from
tranche A into a later tranche — probably NOT a mechanism change. If you
find it IS a mechanism gap, STOP AND REPORT with evidence before changing
Mapper/EagleStack.

## Then finish the project
- Land tranche A (515 minus any excluded class): driven battery GREEN,
  div=0, report per-leg coverage.
- Tranche B (20 WPSH sites) needs NEW code not yet written: the WPSH
  multi-slot store hook in EagleStack.cpp (`case WPSH` has no
  caller_write hook today — verified) AND a loader grammar extension
  (the current loader parses 2-field `push <pc> <addr>`; tranche B's
  lines are 3-field `push <pc> <base_slot> <wides>`). CRITICAL ordering
  (verified, see REPORT §4 / FINDING): WPSH XX,AA pushes AC[XX] first to
  the LOWEST slot; AC[XX] = the HIGHEST arg number in the group. Write
  ascending from the base (lowest) slot. Getting it backwards corrupts
  values with div=0 — so verify VALUES, not just div=0.
- REPORT.md + CheckerHistory note + CURRENT_STATE update. STOP — tranches
  C (26 XCALL/nested) & D (5 RETURN_MESSAGE) are the next project.

## Working style / boundaries
- Design-vs-reality: STOP AND REPORT if reality contradicts the plan;
  gate mechanism changes. Implementation bugs: fix and record.
- The RUNNER is the test bench (bin/runner.sh polls tasks/, pushes
  results/; see PIPELINE.md). In-container: build smoke only. inj leg at
  NORMAL driver speed; m/fo/play at 10x. Write a task NNN-*.sh, commit,
  let the runner execute it. Capture stdout ($R/out) into results —
  that's where divergence detail lives (the prior session forgot to, and
  lost the detail).
- Repo: github.com/nemmart/quest (PAT in project instructions). Or work
  from the Work.tgz tarball and hand a new one back.

## One process note
The prior coordinator session ran long and had made a couple of
task-scripting slips by the end. Fresh eyes on the driven-divergence
diagnostic are exactly what's wanted — trust the captured stdout dump
over any prior hypothesis.
