# Project 17 — M4b: stack_offset (mid-window checkpoints) + QUEST base record

Hi Claude! We're recovering the source of a 1986 Data General MV/8000
game (only the binaries survive) by running it under a lockstep
emulator: a stock "master" and a modified "clone" run in parallel and a
checker verifies they agree instruction-by-instruction, so we can
transform the clone safely. M4a (done) relocated every callable routine's
stack FRAME into a fixed area. M4b redirects caller-side ARGUMENT PUSHES
into the callee's area. Project 16 built the M4b mechanism on ONE call
site (DIST,4 @ 70166E1C) and PROVED it works — then hit an anticipated
checker condition and stopped clean at its boundary. This project
implements the ruled fix and re-validates. Prior sessions built M4a and
the P16 mechanism; do NOT rebuild them.

## Read first (in the tarball)

1. Work/docs/CURRENT_STATE.md
2. Work/docs/M4bNotes.md — THE design of record. Read the two rulings at
   the end especially:
   - "P16 first-slice — mid-window pairs → stack_offset in LiveRecord"
   - the QUEST-base-record section (empty-records handling)
3. Work/docs/Project16/REPORT.md — what P16 built and proved, and the
   mid-window finding (verified: shadow off by exactly 2k mid-window).
4. Work/c_src/hw/Mapper.{hpp,cpp} (LiveRecord, push_record, wrtn_fixup,
   unwind_to, map_word), EagleStack.cpp (the P16 caller_write hooks at
   XPEF/LPEF/LCALL, the WSAVS consume-and-clear).
5. Work/c_src/quest.addrbook, quest.pushmap.

## The finding P16 stopped on (context)

The checker compares master vs clone every 500 instructions. That
quantum boundary can land INSIDE a redirected arg window, where the
master has pushed k args (wsp +2k) but the clone WROTE them to the area
(wsp flat) — so shadow_wsp is off by exactly 2k and the pair diverges.
It is source-agnostic (a mid-window signal does the same). This is a
CHECKPOINT accounting gap, not a mechanism bug — the 84 completed DIST
calls before it were all correct.

## Task 1 — stack_offset in LiveRecord (the ruled fix)

Implement exactly the M4bNotes ruling:
- Add `int32_t stack_offset` to `LiveRecord`, initialized 0 when
  push_record pushes the record.
- **Decorated push** (the caller_write path at XPEF/LPEF/XPEFB/LPEFB/
  WPSH): `records_.back().stack_offset += 2 * words_written` (2 per
  word; a WPSH writing k words adds 2k). If `records_.empty()` here →
  mapper_abort (fail-loud; must not happen once QUEST is decorated —
  Task 2).
- **Decorated LCALL** (and, later, decorated WPOP): `records_.back().
  stack_offset -= 2 * words` (LCALL subtracts its own 2*argc).
- **Checkpoint comparison**: where the checker compares clone wsp to
  master wsp via shadow_wsp, use `delta = records_.empty() ? 0 :
  records_.back().stack_offset;` and compare `shadow_wsp + delta` (find
  the exact compare site from the P16 divergence — the wsp_differs
  check). Empty → 0 → identical to today's closed form.

Why in the record (do not change this design): it is frame-scoped for
free (the arg window runs while the CALLER's record is on top — pushes +
LCALL fire before the callee's WSAVS pushes its record), and
`unwind_to`'s existing suffix-pop carries/discards it, so O.ON/I.GOTO
unwinds and mid-window signal abandonment need NO new code. Only
decorated ops move it (MSP, tombstone push, non-decorated pushes move
both wsps equally and must NOT touch it).

## Task 2 — decorate QUEST as the base record

So `records_` is never empty (kills the empty case at the root). This is
NOT special base-record handling — it's ordinary M4a copy-mode
migration. QUEST is entered by the LOADER, not a decorated LCALL, so the
"args written" flag is CLEAR at its WSAVS, and the existing
(flag-clear + book) rule = M4a copy mode. So "decorate QUEST" = simply
**add QUEST to the address book** (currently `#7015C005 ... push,nocall`);
it then migrates its frame to area 0x74000000 in copy mode exactly like
the other 100 routines, and because it is the outermost frame its record
is the base (records_ never empty). No new WSAVS/WRTN code. Validate:
- Boot is lockstep-clean with QUEST's frame in area 0x74000000 (it is
  the first frame — any error diverges at ~instruction 1, so this is
  quick to confirm or refute).
- Confirm the WSAVS redirect's ac3→area-wfp assumption holds from
  boot-time loader state (not caller-set-up state) — a slotpatch-style
  check as in the P16 report.
If boot-frame migration hits a real wrinkle, STOP AND REPORT (do not
force it) — the three-layer guard (M4bNotes fallback) is the backup, but
try QUEST-as-base-record first.

## Boundaries — BINDING

1. **Scope: the DIST site + QUEST base record only.** Do not convert a
   second arg site, do not touch WPSH arg redirection, do not widen.
2. **Design-vs-reality: STOP AND REPORT.** If stack_offset or the QUEST
   base record contradicts what the code actually does — the checkpoint
   compare isn't where expected, the first-record path doesn't take a
   copy-mode argc-0 QUEST cleanly, unwind doesn't carry the offset —
   write it up with evidence + candidates and stop. You MAY edit the
   Mapper for this (the ruling authorizes stack_offset + QUEST base
   record); do NOT invent a different accounting model without a gate.
3. **Implementation bugs: fix and record.**
4. **Runner is the test bench.** In-container: build smoke only. Full
   battery via the runner (bin/battery.sh; inj at normal driver speed,
   m/fo/play at 10x).

## Stages

**Stage 0 — plan gate.** Confirm: the exact checkpoint compare site
(file:line) where `+delta` goes; the three hook points (push +=, LCALL
−=, empty→abort); the LiveRecord field + init; the QUEST book entry
change + how it takes the existing first-record/wrtn_fixup paths; the
boot-validation plan. WAIT for go-ahead.

**Stage 1 — build.** stack_offset + QUEST decoration, clone-only, master
untouched.

**Stage 2 — verify (runner).** On the 101-live book + QUEST + the DIST
push_map:
- Boot clean with QUEST in area (or STOP+REPORT if not).
- Full battery GREEN (m/fo/inj/abort/play): **div=0**, 0 i2, 0 probes,
  0 m4b_aborts. The mid-window pairs that diverged in P16 (task 018/019)
  now pass — re-run that exact m-leg and show a mid-window compare pair
  passing with `shadow + stack_offset == master.wsp` (the 70166E19
  quantum-boundary case from the P16 report is the target).
- Copy mode still byte-identical for the 100 other routines; DIST args
  still land in the area; marker still tombstoned + written.
- Show the DIST window trace: offset climbing +2 per push, −8 at LCALL,
  back to 0.

**Landing.** REPORT.md: what was built, the mid-window pair now passing,
QUEST-base-record boot result, battery table, any frictions. CheckerHistory
append (stack_offset is a comparison-term change, not a mapper-identity
change — note it, no Gen-5 restructuring). Update CURRENT_STATE + fold
QUEST from the M5 nocall set into M4b in the roll-call. STOP — widening
to N sites and the WPSH arg case are later projects. Hand back Work.tgz
(or push branch p17-stackoffset).

## Environment

Repo github.com/nemmart/quest (details in project instructions) OR
tarball flow. Runner polls tasks/, pushes results/. login
CL/Claude/quest/Y/space/F; scratch-COPY QUEST per run; inj at NORMAL
driver speed, m/fo/play at 10x.
