# Project 18 — M4b widening, tranches A & B: all flat-LCALL arg sites

Hi Claude! We're recovering the source of a 1986 Data General MV/8000
game (only binaries survive) via a lockstep emulator: a stock "master"
and a modified "clone" run in parallel, a checker verifies they agree
each instruction, so we can transform the clone safely. M4a (done)
relocated every routine's FRAME to a fixed area. M4b redirects caller-side
ARGUMENT PUSHES into the callee's area. The mechanism AND the checkpoint
accounting are both proven on one site (Projects 16–17: DIST,4 — args to
area, marker tombstoned+written, flag consumed at WSAVS, mid-window
`stack_offset` accounting green, QUEST as base record). This project
WIDENS that proven mechanism to all the FLAT-CALL arg sites. Do NOT
rebuild the mechanism — apply it.

## Read first
1. Work/docs/CURRENT_STATE.md
2. Work/docs/M4bNotes.md — design of record. The two end rulings
   (stack_offset; QUEST base record) and the write-mode-WSAVS
   subtraction-timing amendment are LAW here.
3. Work/docs/Project17/REPORT.md — what's proven and how it's verified.
4. Work/c_src/hw/Mapper.{hpp,cpp}, EagleStack.cpp (caller_write hooks,
   note_arg_write, push_record write-mode consume, checkpoint_offset),
   Lockstep.cpp (the checkpoint compare).
5. Disassembled/quest.argmap, quest.callsites, quest.wpsh_wpop,
   quest.argpush. Work/c_src/quest.pushmap (the P16/17 one-site map).

## Scope — tranches A & B (flat LCALL sites only)

**Tranche A — 515 sites.** Game→game, flat LCALL, args are all
single-word pushes (XPEF/LPEF/XPEFB/LPEFB). This is exactly the DIST case
× 515 — the proven path, no new mechanism.

**Tranche B — 20 sites.** Flat LCALL to TERRAIN / TERRITORY, whose arg
windows include a WPSH that writes 2–3 args in ONE instruction. The
ONLY new code path: `note_arg_write(m, wides)` with wides>1, and the
push→store hook writing `wides` consecutive area slots from one WPSH pc.
The mechanism was specified for this in P16/17 but DIST never exercised
wides>1 — so B is where multi-slot store actually runs.

**NOT in this project (tranche C & D, next project):** XCALL/nested
sites (26 — static-link interaction), RETURN_MESSAGE (5 — pass-by-ref
pointer args + [[noreturn]]). Do NOT decorate any XCALL site or any
RETURN_MESSAGE site. Also skip the 188 zero-arg CLEAN-EMPTY sites
(nothing to redirect).

## The push_map is ALREADY GENERATED (coordinator, this session)

NOTE (the push_map is a direct transcription of the argmap): a push_map
entry is a pure function of (callee, arg-number) → area slot, keyed by
the push PC — `slot = callee_wfp − 10 − 2N`. It does NOT depend on which
LCALL the push belongs to (a given PC pushes a fixed arg of a fixed
callee, whose frame is at a fixed area address). So the argmap already IS
the push_map; no call-site association or window-walking is needed. Two
independent generators (direct transcription and call-window walking)
produce IDENTICAL maps — cross-validation that the slots are right.

The coordinator generated + validated the tranche A & B maps:
`Work/c_src/quest.pushmap.A` (515 sites), `.B` (20 WPSH sites), `.AB`
(combined). Generator: `Work/c_src/tools/gen_pushmap.py`. Every site's
arg window resolved to exactly its argc (0 skipped), every push slot
validated inside the callee's arg region, every marker == wfp−10, and
the WPSH register→arg ordering verified (above). You should still
sanity-check them, but the generation + slot arithmetic is done. Your
job is the emulator side: the WPSH multi-slot hook + loading these maps
+ the battery. Load-time validation (already in the
loader): each push slot inside the callee's arg region; each marker slot
== some book entry's wfp−10; refuse a site whose containing routine is
not book-live (the QUEST-boundary guard).

The emulator hooks do NOT change (they're pc-keyed and already handle
XPEF/LPEF and — per spec — WPSH). Confirm the WPSH multi-slot store hook
actually exists and fires for tranche B; if it was stubbed as
"needed-when-a-site-needs-it" in P16, implement it now (write `wides`
slots, `note_arg_write(m, wides)`).

## Boundaries — BINDING
1. **Tranches A & B only.** No XCALL, no RETURN_MESSAGE, no zero-arg.
2. **Widen in two steps, verify between:** land A (515) and get a green
   battery FIRST; then add B (20) and green again. If B's multi-slot
   path surfaces something, it's isolated from A's 515.
3. **Design-vs-reality: STOP AND REPORT.** If any site resists the
   proven mechanism — a window that isn't the clean shape the census
   promised, a push_map validation failure, a divergence that isn't a
   simple bug — write it up with the specific site(s) + evidence and
   stop. You MAY edit the emulator for the WPSH multi-slot hook; do NOT
   change the stack_offset / mode / marker rulings.
4. **Implementation bugs: fix and record.**
5. **Runner is the test bench.** In-container build smoke only; full
   battery via runner (inj at normal driver speed, m/fo/abort/play 10x).

## Stages
**Stage 0 — plan gate.** The push_map generator design; the A-site list
(515) + B-site list (20) with counts reconciled against quest.callsites;
confirmation the WPSH multi-slot hook exists or the plan to add it; the
verification plan. WAIT for go-ahead.
**Stage 1a — tranche A.** Generate A's push_map, build, runner battery.
Expect div=0, all 515 sites exercised where the legs reach them (report
coverage: which of the 515 actually fired in m/fo/play).
**Stage 1b — tranche B.** Add the 20 WPSH sites, build, battery again.
**Stage 2 — verify.** Full battery GREEN (m/fo/inj/abort/play): div=0,
0 i2, 0 probes, 0 m4b_aborts, 0 mapper_aborts. Show: a multi-slot WPSH
window trace (offset += 2·wides, correct consecutive slots); copy mode
still byte-identical for anything not decorated; the write-mode WRTN
count == write-mode WSAVS count. Report per-tranche coverage (fired vs
migrated — unexercised sites are a coverage note, not a blocker, same as
M4a).
**Landing.** REPORT.md (what widened, coverage, any frictions),
CheckerHistory note, CURRENT_STATE update. STOP — C & D are the next
project. Hand back Work.tgz (or push branch p18-m4b-AB).

## Environment
Repo github.com/nemmart/quest (details in project instructions) OR
tarball. Runner polls tasks/, pushes results/. login CL/Claude/quest/Y/
space/F; scratch-COPY QUEST per run; inj at NORMAL speed, m/fo/play 10x.
