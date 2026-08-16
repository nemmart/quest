# Project 7 — Crossings-Only Rendezvous (harness prep for native L2)

> **SUPERSEDED (Aug 13 2026).** This prompt was never run — it is an
> earlier, more detailed draft of the same harness-prep project as
> Project6.5/PROMPT.md (the numbering went off the rails; Project 7
> was later intended to mean Phase 2, the native L2). The work was
> implemented in a direct session with the user, WITHOUT the
> QUEST_SYNC flag (one sync model, no modes). See
> docs/CrossingsChecker.md and docs/CheckerHistory.md. Phase 2 gets a
> fresh prompt. Kept for the record.


Hi Claude! Solo session; a reviewer session verifies. This is HARNESS
CORE work — the machinery that catches everyone else's mistakes — so
this project's discipline is: characterize first, change minimally,
prove against known-good code. NO new L2 code, NO contract changes,
NO shared-doc edits (report corrections in REPORT.md).

Read IN ORDER: docs/METHOD.md; docs/Project6/L2Contract.md §0 + §5 +
§7 + §11 (crossing definitions, inventory, validation regime);
docs/Project6/NativeDesign.md §7 (the worklist this implements);
docs/Project6/REPORT.md hazards H1/H6; docs/Layering.md (strata);
hw/Machine.cpp run_steps IN FULL; hw/Lockstep.cpp compare_pair;
docs/SharedProtocol.md (pairing rules learned the hard way);
docs/SessionPlan.md Aug-11/12/13 records (pairing bug history).

## The goal (user-ratified sync model)

Post-change, the sync surface is exactly:
1. **L1 fabric, unchanged**: L0/L1 runtime-service calls pair as
   today; syscalls pair at the gate; batch-exhaustion heartbeat
   (~500 insns) unchanged.
2. **L1↔L2 crossings** (Contract §5): every entry into the 20
   registered L2 entries FROM L1 + the indirect entry + the
   DISPATCH_RET/E3EF return-crossings; every L2→L1 exit (return,
   dispatch, unwind transfer, continuation). Interior L2→L2 is
   INVISIBLE — no break, no rendezvous, absorbed into the span.
3. **L3 rendezvous**: the existing terminal machinery, UNCHANGED
   (detach/abort/retire).

## Phase A — CHARACTERIZE (mandatory, before any edit)

Read run_steps + the dispatch sites and write down, in
docs/Project7/ANALYSIS.md, the CURRENT pair structure for each of:
(a) an M+dir+abc signal (all-native chain), (b) a FAIL_OPEN signal 1
(native chain + heap calls), (c) signal 2's fallback-whole cascade,
(d) a plain login (I.PROLOG/EPILOG + leaves). For each: the exact
sequence of batch breaks on master and clone with their trigger
(entry break / native_break / range-exit / terminal / exhaustion /
syscall). THEN derive the true delta to the goal model. Expected
findings (verify, don't assume): composite native spans already end
at range-exit (crossing-shaped); the deltas are (i) interior L2
entries still breaking batches when reached EMULATED (master
run-to-return already absorbs them — but the CLONE's emulated
fallback spans also absorb via the pending block — so enumerate who
actually still breaks where), (ii) native LEAF wrappers called from
emulated L1 pair per-call today and should CONTINUE to (they are
L0/L1 fabric — T?AREA is L2 but called from L1 = a crossing:
per-call pair is ALREADY crossing-correct), (iii) any true
granularity change may be SMALLER than H1 assumed. If Phase A
concludes the current structure ALREADY satisfies the goal for the
bit-faithful L2 (plausible!), say so — the deliverable becomes the
l2_depth mechanism for the FUTURE composite implementation, dormant
behind the flag, plus the proof harness.

## Phase B — IMPLEMENT (behind a flag)

Env flag `QUEST_SYNC=crossings` (default absent = today's behavior,
PERMANENTLY retained as the fine-grained debugging microscope —
user-ratified; the flag is a mode, not a rollout gate). Under the
flag, implement the goal model per your Phase-A delta. Sketch from
NativeDesign §7 (adapt to your findings): per-machine l2_depth;
crossing entries break/pair, interior entries absorb; DISPATCH_RET
re-entry as a rendezvous (H6); span/count accounting per-crossing;
terminal pairs keep equal-count requirements. Keep the diff SMALL
and heavily commented — every changed decision site cites the
Contract §5/§7 line it implements.

## Phase C — RECALIBRATION GATE (the acceptance bar)

Run the FULL suite against the UNCHANGED bit-faithful L2, flag on
AND flag off: login+move+ESC; M+dir+abc; FAIL_OPEN both signals
(incl. continue → DEF?ON-era cascade → ?FATAL detach);
QUEST_INJECT shapes 1–3; QUEST_TERMINAL=<pc>:ABORT. Required
outcomes: flag-off = byte-identical behavior to today (zero
regression); flag-on = same verified results, ZERO divergences, and
a rendezvous LOG (add temporary or flag-gated logging) showing the
predicted structure — coarser, at exactly the Contract §5 crossing
set. The T?AREA dual-role acceptance test: paired when called from
L1 (?LIB_ERROR emulated? — note ?LIB_ERROR is L1 but NATIVE, so its
T?AREA use is plain C++, invisible; construct the L1-emulated case
via the FALLBACK path and show T?AREA absorbed inside the span vs
paired when reached from emulated L1 outside any span). Any
deviation from prediction = STOP, diagnose, document — a checker bug
found now is the whole point of the gate.

## Deliverables

docs/Project7/ANALYSIS.md (Phase A), the flag-gated code,
docs/Project7/REPORT.md (SharedProtocol format: per-phase status,
the Phase-A delta verdict, gate evidence with EXACT commands +
outputs per METHOD §10, integration hazards for the native-L2
session).

## Gotchas

Turn cadence ~49s; login CL/Claude/quest/Y/any/F; M+dir+abc same-turn;
scratch-copy QUEST/; stdbuf -o0 -e0; grep-warning false positives on
full builds; port-8781 zombies (pkill stale emulators first);
kill-timing races on exit write-back checks (wait ≥120s after ESC).
The pairing bug history in SessionPlan (LJSR guard hole, terminal∘
transfer, count skews) is your checklist of ways this exact kind of
change has gone wrong before.
