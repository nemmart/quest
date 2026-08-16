# Project 6.5 — Harness Prep: Crossings-Only Rendezvous (+ recalibration gate)

> **SUPERSEDED (Aug 13 2026).** This prompt was never run. Its goal —
> crossings-only rendezvous + recalibration gate — was implemented in a
> direct session with the user, WITHOUT the QUEST_SYNC flag (user
> ruling: one sync model, no modes; the flag/microscope design below is
> void). See docs/CrossingsChecker.md (design + evidence) and
> docs/CheckerHistory.md (generation record). Kept for the record.


Hi Claude! Solo session, code allowed (harness only — hw/Machine.cpp,
hw/Lockstep.cpp, hw/RTStubs.*, hw/MachineThread if needed). NO changes
to runtime/ wrappers, no new L2. This is the Phase-2 prerequisite done
early, against the current bit-faithful L2, so the checker is
recalibrated while ground truth is still in place to expose checker
bugs.

Read IN ORDER: docs/METHOD.md; docs/Project6/L2Contract.md §0 (crossing
definition), §5 (crossing inventory), §7; docs/Project6/NativeDesign.md
§7 (the worklist this implements); docs/Project6/REPORT.md H1/H6;
docs/Layering.md; docs/SharedProtocol.md (pairing rules history);
hw/Machine.cpp run_steps IN FULL — it is the object of study.

## The sync-surface ruling (user-ratified)

Checking happens at: (1) the L1 fabric as today — L0/L1 runtime-service
calls, syscalls, the batch-exhaustion heartbeat — UNCHANGED; (2) L1↔L2
crossings per Contract §5 — interior L2 is invisible; (3) L3
rendezvous — the terminal-pair machinery, already built, UNCHANGED.
"Check the game's fabric continuously, check the handler machinery at
its skin, check death once at the door."

## Step 0 — CHARACTERIZE BEFORE CHANGING (mandatory, half the value)

The current pairing is ALREADY nearly crossing-shaped for the native
chain (composite native spans end at range-exit; interior C++ is
invisible; fallback spans absorb inner entries via rt_pending_return +
the central four-site guard). Do not assume H1's framing; MEASURE.
Instrument or trace (rtcalls + a temporary pair log, precedent in
SessionPlan Aug-11 record) the COMPLETE pair sequence for: login, one
M-trigger signal, one FAIL_OPEN double-signal, one QUEST_INJECT resume
shape. Produce a table: every pair's pc, classified CROSSING /
INTERIOR / HEARTBEAT / SYSCALL / TERMINAL against Contract §5. The
DELTA between that table and the ruling above is your implementation
scope — expected to be small (candidates: per-call native-leaf pairs
from emulated L1 that are genuine crossings and STAY; any interior
entry that still breaks; span/count accounting nits). If the delta is
EMPTY, say so — "no change needed, here is the proof" is a triumphant
outcome, not a failure.

## Step 1 — implement the delta, behind a flag

Env QUEST_SYNC=entries (default, today's behavior) | crossings. Design
sketch from NativeDesign §7 (l2_depth) is a STARTING POINT — if Step 0
shows the existing pending/range-exit machinery already provides the
depth semantics, prefer extending it over adding a parallel mechanism.
The T?AREA dual-role case (crossing from emulated ?LIB_ERROR, interior
within native composites) is the acceptance test. DISPATCH_RET (H6):
in the CURRENT bit-faithful world the tail is symmetric emulated code —
verify Step 0 shows that, leave its re-entry machinery to Phase 2, but
DOCUMENT the finding for it.

## Step 2 — the recalibration gate (the point of the project)

With QUEST_SYNC=crossings and the UNCHANGED bit-faithful L2: full
regression — login+move+ESC, M-trigger, FAIL_OPEN both signals,
QUEST_INJECT all three shapes, the :ABORT test terminal. Required:
zero divergences, identical verified outcomes, all detach/abort/retire
lines identical, write-back intact, AND the pair log shows exactly the
predicted structure (fewer, coarser, at Contract-§5 addresses only,
plus heartbeat/syscall pairs). Any deviation = a checker bug found
while ground truth can expose it — fix before proceeding.

## Deliverables

- The code (flag-gated; default behavior byte-identical — prove with
  one default-mode regression).
- docs/Project6.5/REPORT.md: the Step-0 characterization table (this
  is a permanent reference — Phase 2 inherits it), the delta
  implemented, gate evidence per METHOD §10, and the DISPATCH_RET
  finding for Phase 2.
- The flag is PERMANENT (user-ratified): crossings mode for the
  native-L2 future; entries mode + bit-faithful L2 = the project's
  debugging microscope forever. Say so in the code comment.

## Gotchas

The usual set (NextSession.md): ~49s turns, scratch-copy QUEST/,
stdbuf, login CL/Claude/quest/Y/any/F, M+dir+abc cheap trigger,
QUEST_CAPTURE needs DEST, grep-warning false positives. Plus: the
pair-log instrumentation precedent (and its removal discipline) is in
SessionPlan's Aug-11 record — temporary logs come OUT before delivery,
gated env-var logs may stay.
