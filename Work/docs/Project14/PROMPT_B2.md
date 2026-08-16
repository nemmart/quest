# Project 14 — Phase B2: all callable game routines live (repo-native)

Hi Claude! Solo implementation session. This is the FIRST repo-native
project: you work in the git repo, not tarballs, and you validate by
pushing task scripts to a runner box. Read PIPELINE.md in the repo root
first — it defines the whole workflow.

## Bootstrap

- Clone: the PAT and URL are in your project instructions
  (github.com/nemmart/quest). `git clone`, then branch `p14-phase-b2`.
- Designs of record: Work/docs/Mapper.md, Work/docs/M4aDesign.md
  (through §11), Work/docs/M4bNotes.md, Work/docs/M5Notes.md. Do NOT
  edit these — findings go in your report; the planning session folds
  rulings in.
- Predecessor: Work/docs/Project14/REPORT.md (Phase A) and REPORT_B1.md
  (B1 partial — READ its §10 B2 continuation note; this prompt is that
  note made into a project).
- The pipeline: you push `tasks/NNN-*.sh`; the runner (10+ core box,
  always polling) builds, runs, and pushes `results/NNN-*/`. Poll for
  your results with `git pull`. Tasks are self-contained bash from repo
  root. Battery timing there: an m leg ~20 s at 10× driver speed.

## The goal

M4a's final step: **every CALLABLE game routine's frame lives in its
0x74000000 area.** The wave-one "pure only" filter drops to
**nocall-only exclusion**. Expected ~101 live (130 entries − 29 nocall).

Rationale (ruled by the planning session — this is settled, not yours to
re-litigate, but STOP-and-report if the machinery contradicts it):
dyn routines (WMSP/STASP/LDASP) and push routines (WPSH/WPOP) are IN.
The dynamic stack ops are SYMMETRIC — both engines' wsp move by the
same amount — so the closed-form shadow accounting still holds; the
Mapper's compression leg already maps addresses above redirected frames;
the WRTN fixup already discards dynamic residue on both sides. The 29
nocall entries (boot QUEST, C_A_LISTENER task entry, the 26 ON-unit
bodies) stay excluded — they are reached by dispatch, not LCALL, so the
redirect's frame-word read is meaningless for them; they are assigned to
M5 (static handler dispatch makes them branch targets, not frames).

## Boundaries — BINDING

1. **Scope = flip the filter to nocall-only, validate, hand off.** No
   Mapper changes, no new redirect mechanism, no M4b caller-side work.
2. **Design-vs-reality: STOP AND REPORT.** If a dyn or push routine
   diverges in a way that contradicts the symmetry argument above — i.e.
   the closed-form shadow accounting genuinely breaks, or an escape the
   Mapper can't map appears — write it up with the divergence dump and
   candidate rulings, and stop. Do NOT patch the Mapper to accommodate
   it without a ruling. (A per-routine divergence that's just a book or
   tooling bug is boundary 3 — fix, record, continue; comment the
   routine out and keep going.)
3. Implementation bugs: fix and record (METHOD §11).
4. **The runner is the test bench.** In-container, only a build smoke
   check. All batteries run as runner tasks.

## Stages

**Stage 0 — plan gate.** State the exact tool change (the wave-one
filter: `wave_one = pure and not nocall` → `wave_one = not nocall` in
Work/c_src/tools/build_address_book.py, or an explicit `--all-callable`
flag — your call, show it). Regenerate the book locally, report the
live count and the delta vs the 45-live batch-2 book (should add the
dyn+push routines, ~56 of them). List the newly-live routines grouped
by why they were previously excluded (dyn / push). Wait for go-ahead.

**Stage 1 — book + smoke.** Commit the regenerated book. Push a smoke
task (build + book-load parse, like tasks/003). Confirm DONE.

**Stage 2 — sanity battery (runner tasks).** On the ~101-live book:
- `m` at 10× speed: 0 div, anchors exact (READ_IN=4, LOGON=1,
  GET_INPUT=8, INIT_SCREEN=1, REFRESH_SCREEN=1, HIT_ANY_CHAR=1),
  probes 0, I.STOP detach.
- `inj` at NORMAL speed (timing-sensitive — task 009 established that
  10× races past the injection window): QUEST_INJECT=7016A896:-1:0x2006
  → ?FATAL 7017F036.
- `abort`: QUEST_TERMINAL=7016871D:ABORT → WORLD ABORT both engines.
- `fo`: QUEST_FAIL_OPEN → handler, 0 div.
Each a task (or one combined task with per-leg speed — see bin/battery.sh
and the task 008/009 split for the speed rule). Bisect any red by
commenting book entries. The newly-live dyn routines that the `m`/`fo`
drivers actually exercise (their redirect lines > 0) are the ones
validated here; the rest are LIVE-UNEXERCISED pending play.

**Stage 3 — hand off for live play.** This is the coverage run and it
is the USER'S, not scripted. Prepare the handoff per REPORT_B1 §10's
checklist: runner-built emulator, ~101-live book, lockstep, redirect +
gcalls traces on, QUEST_CAPTURE armed. The user plays for breadth —
combat, store, bargain, cave, castle, movement, menus. On any
divergence the checker names the routine: comment it, record it,
regenerate, the user continues. Write the handoff as a short doc the
user can follow (or a runner task that launches the traced session for
them to attach to).

**Landing.** Roll-call from Stage-2 + play traces:
LIVE-VALIDATED / LIVE-UNEXERCISED / EXCLUDED(nocall→M5). Final book.
CheckerHistory.md Gen-4 append (include the B1 stride-masking near-miss
sentence still owed, and the dyn/push-are-symmetric result). Then STOP.

## Also owed from B1 (fold in if a play session or combat task reaches them)

- (b) sibling XWLDA link reload: a SPELL-KILL (ATTACK.3 → ATTACK.1 at
  7015E9D6, preceded by XWLDA 1,[ac3+0x7FFA] at 7015E9D4). Capture
  ATTACK.1's link= line + the gcalls site line. Recipe in REPORT_B1 §3.
- (d1) child-body inject: QUEST_INJECT=7015E853:-1:0x2006, cast once
  with ATTACK+ATTACK.3 live → ?FATAL terminal abandonment crossing the
  pair. NORMAL speed. Both are combat-gated → natural fits for the
  user's play session; not blockers for B2 landing.

## Deliverables (all in the repo, branch p14-phase-b2)

- Work/c_src/tools/build_address_book.py (filter change) + regenerated
  Work/c_src/quest.addrbook
- tasks/NNN-*.sh for every validation run (they and their results/ are
  the evidence)
- Work/docs/Project14/REPORT_B2.md: the tool change, live-count delta,
  the sanity results (with run/result task numbers), any bisected
  stragglers, the roll-call, boundary-2 findings if any
- Work/docs/CheckerHistory.md Gen-4 append
- the play-handoff doc
- Push the branch; the planning session reviews via the repo and merges.

## Environment

Per-leg driver speed matters: 10× (QUEST_DRIVE_SPEED) for m/fo/play,
NORMAL for inj (task 009 result). Scratch-COPY QUEST per run (never
symlink). Each parallel task needs its own QUEST_PORT. login sequence
CL / Claude / quest / Y / space / F. Startup "Segment fault - block 0,
page 1" + RESERVED ACCESS = noise. The runner reaps nothing between
your turns — that constraint was the container; the runner box runs
tasks to completion.
