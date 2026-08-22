# Project 16 — M4b first slice: redirect ONE call site's args into the callee area

Hi Claude! We've been recovering the source for a 1986 Data General
MV/8000 game (only the binaries survive) by running it under a lockstep
emulator: a "master" runs stock and a "clone" runs a modified version,
and a checker verifies they agree instruction-by-instruction, so we can
transform the clone safely. M4a (done) relocated every callable
routine's stack FRAME into a fixed memory area on the clone. M4b
redirects the CALLER-SIDE ARGUMENT PUSHES into the callee's area too.
This project builds the M4b mechanism on ONE call site, end to end, and
stops. Prior sessions proved M4a; do NOT rebuild it.

## Read first (in the tarball)

1. Work/docs/CURRENT_STATE.md — where things stand.
2. Work/docs/M4bNotes.md — the M4b design discussion + all rulings.
   THIS is your design of record. Read every ruling.
3. Work/docs/WPSH_WPOP.md, Work/docs/NORETURN.md — arg/stack behavior.
4. Work/docs/Mapper.md — the mapper (esp. §3b wave-scoped conditions;
   the call-marker ruling means M4b needs NO mapper change).
5. Work/c_src/hw/Mapper.{hpp,cpp}, EagleStack.cpp (WSAVS/WRTN redirect),
   the address book loader.
6. Disassembled/quest.argmap + quest.callsites (the census: per-site
   arg→slot map) and Disassembled/quest.wpsh_wpop.

## The mechanism (from M4bNotes — these are RULINGS)

For a converted call site, three coordinated pieces (clone-only; master
runs stock so the checker always has a faithful reference):
1. **push_map[pc] → area slot**: at a decorated call's arg-push
   instruction, WRITE the value into the callee's fixed area arg slot
   instead of pushing to the stack.
2. **The call marker STAYS pushed on the stack** (ruling): the LCALL's
   arg-count/linkage word is still pushed (kept until return — a
   live-call tombstone so the mapper's address-ordering stays valid)
   AND written to the callee area. This means NO mapper change.
3. **A per-machine "args written not pushed" flag**: the decorated call
   sets it; **every WSAVS/WSAVR consume-and-clears it** with the three
   unconditional rules (M4bNotes): set+book-redirect→write mode, clear;
   set+non-redirect WSAVS→abort_world; clear+redirect→M4a copy mode.

## Scope — ONE site, then STOP

Pick ONE simple, frequently-exercised game→game call site: a fixed
arg count, all args pushed by XPEF/LPEF (NOT the WPSH multi-slot case
yet), the callee already M4a-live and exercised in play, and NOT
RETURN_MESSAGE (noreturn) and NOT a nested/XCALL site. Nominate it at
the plan gate with rationale (GET_INPUT's caller, a DISTANCE_TO_PLAYER
or DIST call site, etc. — something that fires constantly in the m/play
battery so lockstep exercises it hard).

Both caller styles must COEXIST: this one site writes args; every other
site still pushes (M4a copy mode). The book/decoration marks only the
chosen site.

## Boundaries — BINDING

1. **One site only.** Do not convert a second site, do not touch WPSH
   arg sites, do not build the general push_map loader beyond what the
   one site needs. Widening is a later project.
2. **Design-vs-reality: STOP AND REPORT.** If the mechanism contradicts
   an M4bNotes ruling — the flag can't be cleanly consumed at WSAVS, the
   marker-on-stack doesn't keep the mapper valid, open-window shadow
   accounting is actually needed for even one site — write it up with
   evidence + candidate rulings and stop. Do NOT change the Mapper or a
   design doc.
3. **Implementation bugs: fix and record.**
4. **The runner is the test bench.** In-container: build smoke only.
   Full battery runs as a runner task (bin/battery.sh; per-leg speed —
   inj at normal speed, others 10x).

## Stages

**Stage 0 — plan gate.** Nominate the site (with disassembly of its arg
window). Specify: the push_map representation (even if a 1-entry table),
the call decoration mechanism, where the flag lives on Machine, the
exact WSAVS/WSAVR consume-and-clear insertion point (clear BEFORE the
overflow test — M4bNotes), the write-mode WRTN fixup (wsp = W). Show the
before/after for this site's frame image. WAIT for go-ahead.

**Stage 1 — build the one-site redirect.** Implement clone-only. Master
untouched. Keep it minimal and readable.

**Stage 2 — verify (runner).** On the current 101-live book with the one
site decorated:
- The decorated site's args land in the callee area; the callee reads
  them correctly; lockstep stays 0-divergence through that call.
- The flag is consumed at every WSAVS (grep the trace: no dangling-flag
  aborts).
- Full battery green (m/fo/inj/abort/play), 0 div, 0 probes — the one
  conversion must not disturb the 100 other routines still in copy mode.
- Show a trace excerpt of the decorated call: args written (not pushed),
  marker still on stack, flag set→consumed at the callee's WSAVS.

**Landing.** REPORT.md: the site, the mechanism as built, the trace
evidence, battery results, any ruling frictions. Then STOP — widening to
N sites, the WPSH multi-slot case, and open-window accounting are later
projects. Update CURRENT_STATE. Hand back Work.tgz (or push branch
p16-m4b-first-site if using the repo).

## Environment

Repo: github.com/nemmart/quest (clone details in project instructions) —
OR tarball flow, your choice. Runner box polls tasks/, pushes results/.
Battery: bin/battery.sh. login CL/Claude/quest/Y/space/F. Scratch-COPY
QUEST per run. inj at normal driver speed (timing-sensitive), m/fo/play
at 10x.
