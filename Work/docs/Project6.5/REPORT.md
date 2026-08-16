# REPORT — The Crossings-Only Checker Session (Aug 13 2026)

**For review by the prior session** (author of Project6.5/PROMPT.md
and Project7/PROMPT.md). This session implemented your project's GOAL
— crossings-only rendezvous + recalibration gate against the
bit-faithful L2 — but NOT your design: the user redirected it live.
This report is written to make the deviations easy to audit and the
risky spots easy to probe. Format adapted from SharedProtocol's
required REPORT fields (this was harness work, not translation work,
so "routines" become "mechanisms"). Companion docs:
docs/CrossingsChecker.md (design + evidence, primary),
docs/CheckerHistory.md (generation record), SessionPlan.md final
addendum (session narrative).

## 1. Status, per mechanism

| Mechanism | Status |
|---|---|
| Step-0 characterization (pair census vs Layering census) | DONE — measured before any change, per both prompts' Phase A |
| Layer map (RTStubs::l2_bits, 30 entries) | Implemented, lockstep-validated |
| Deferred dispatch (Machine::pending_native; entry rendezvous for translated L2) | Implemented, lockstep-validated |
| Untranslated-L2 both-role pending arming (interior-invisibility law) | Implemented; DORMANT by construction (no live untranslated L2), validated only by not perturbing live paths |
| Return-crossing rendezvous (EE40 / E3EF arrival breaks) | Implemented; DORMANT on live paths (live handlers unwind via I.GOTO), same caveat |
| QUEST_SYNC flag / crossings "mode" | NOT BUILT — user ruling, see §4 |
| l2_depth counter | NOT BUILT — subsumed, see §4 |
| Recalibration gate (7-run regression, bit-faithful L2 unchanged) | PASSED, 0 divergences each; 2 anomalies A/B'd to baseline |

## 2. Code touched (the merge-relevant inventory)

- `hw/Machine.hpp/.cpp` — `pending_native` member (init'd, cleared at
  its single consumption site); run_steps: (a) top-of-loop deferred
  execution branch (native fn runs in place of fetch+decode at the
  entry pc; NOT counted as an instruction — the native body was zero
  instructions when it ran inside the dispatch instruction, and stays
  zero); (b) the entry block rekeyed on l2_bits/translated_bits (see
  §3 for the decision table); (c) the return-crossing break after the
  terminal check, gated `rt_pending_return==0 && !native_break`.
- `hw/RTStubs.hpp/.cpp` — l2_bits + the 30-symbol table;
  `is_l2_entry` / `defer_dispatch` / `is_return_crossing`;
  `inject_fire` now defers like a real call site.
- `hw/EagleStack.cpp` (LCALL/XCALL) and `hw/EagleGeneral.cpp`
  (LJSR/XJSR) — the deferral branch AFTER the existing
  nested-in-fallback guard, so pending_native can only be set when
  rt_pending_return==0.
- UNTOUCHED: runtime/ wrappers, Lockstep::compare_pair, terminal
  machinery, mediation, mirror pages, rtcalls/coverage/captures.

## 3. The as-built entry-block decision table (audit target #1)

At an entry_bits pc under rt_sync:

| Condition | Behavior |
|---|---|
| terminal_bits | unchanged (terminal_reached, break) |
| translated && pending_native set | CLONE at a deferred L1→L2 crossing: break at entry (the crossing pair); native runs on resume |
| translated && no pending_native | MASTER (or clone landing emulated — fallback semantics preserved): arm rt_pending_return=ac3; if L2, ALSO break at entry (mirror pair); if fabric leaf, continue silently (today's behavior) |
| untranslated && L2 | arm rt_pending_return=ac3 on BOTH roles, break at entry (crossing pair; subtree becomes one absorbed span) |
| untranslated && fabric | break at entry (unchanged) |

Entry pairs compare under the STRICT count rule (both engines
emulated identically to the door); exit pairs keep the native_span
exemption. The ENTRY pair is the new checking your prompts' designs
did not have: argument state verified at the door, uniformly, before
any implementation runs — the property Phase 2 inherits.

## 4. Deviations from your prompts (each user-ruled in-session)

1. **No flag, no modes.** Your central design (QUEST_SYNC=crossings,
   fine-grained mode retained as the microscope) was explicitly
   rejected: "I don't want to have switches and multiple modes of
   checking." One sync model, permanently; the microscope is a
   HISTORY build if ever needed. Consequence you should weigh: the
   recalibration gate could NOT A/B flag-on/flag-off in one build —
   the proof is "full suite under the new model against the unchanged
   bit-faithful L2, zero divergences," which is what was run.
2. **No l2_depth.** Your acceptance test (T?AREA dual-role) assumed a
   depth counter. As built, depth semantics come from EXISTING
   machinery: rt_pending_return absorption (now armed on both roles
   for untranslated L2) + composite subsumption. T?AREA dual-role
   resolves by construction: from native ?LIB_ERROR it is
   C++-interior (subsumed by the composite's boundary pairs — a
   RULING, see §5); from a hypothetical emulated L1 caller it pairs
   at entry via l2_bits.
3. **Entry rendezvous added** (not in either prompt): every L1→L2
   crossing pairs AT the entry pc via deferred dispatch, master
   mirroring. Grounds: Contract §7 lists the raise entry as a
   rendezvous; uniformity for Phase 2; cost measured at ~24 extra
   pairs per session.
4. **Numbering**: your Project 6.5 and Project 7 prompts turned out
   to be two drafts of the same project; both carry SUPERSEDED
   banners; Phase 2 is now Project 8 (docs/Project8/PROMPT.md).

## 5. Rulings made in-session (audit target #2 — challenge these)

- **Composite subsumption**: L1→L2 crossings INSIDE a native
  composite (native ?LIB_ERROR calling t_area/o_qsignal as C++) do
  NOT pair — subsumed by the composite's boundary pairs, per METHOD
  §7 whole-subtree. No shared architectural point exists mid-span
  without breaking up composites.
- **Escalation raise from the emulated dispatch tail** (EE4E test →
  LCALL O.SERROR): treated as a FRESH crossing chain, not suppressed
  interior. Symmetric on both engines; verified at its own boundary
  pairs.
- **Non-L2 translated leaves keep exit-only pairing** ("same as
  today" per the user's directive; entry pairs for fabric would add
  ~150 pairs/session for no model reason).
- **E3EF at depth 0 is claimed unreachable** while ?LIB_ERROR is
  native (its O?SIGNAL return is C++-internal). The break is
  included anyway (dormant-but-correct). If you can construct a
  reachable path, that is a finding.

## 6. Validation evidence (summary; full table in CrossingsChecker.md)

Seven runs, new checker, bit-faithful L2 unchanged, 0 divergences
each: M-trigger (I.STOP detach+retire+write-back; 24 new L2 entry
pairs exactly at I.PROLOG×7/O.ON×7/I.EPILOG×6/O.REVERT×3/I.GOTO×1;
rtcalls chain identical to an old-checker baseline run), FAIL_OPEN
double signal (?FATAL detach; log shows entry pairs at strict equal
counts — I.GOTO 14/14, O.ON 16/16 — each followed by its exit span),
inject shapes 1–3 (shape 2: entry pair 7017EDED 11/11 then
terminal-bound fallback to ?FATAL; shape 3 RESUME: entry pair 11/11
+ resume exit span 106/4 at the raiser's post-call pc, play
continued), :ABORT test terminal (verified pair, banner, save
suppressed, clean self-termination). Anomalies: shape-1 post-handler
death and plain-L→P-ESC non-detach — both REPRODUCED BIT-FOR-BIT on
a pristine-baseline build with identical drivers (the A/B was run,
not assumed); recorded as environment gotchas in NextSession.md.

## 7. Open questions / spots a reviewer should probe

- **Deferral one-shot semantics**: pending_native is consumed at
  run_steps' top on the resume batch. Convince yourself no path
  breaks the batch between the deferral and the resume in a way that
  leaves pending_native set across something that matters (terminal?
  inject? halt?). We found none; the state is per-Machine and the
  resume is the immediately next run_steps call for that machine.
- **Exception-while-deferred**: if the dispatch instruction's
  successor batch never runs (world abort between pair and resume),
  pending_native dies with the machine — believed benign; confirm.
- **The dormant paths** (untranslated-L2 arming, EE40/E3EF breaks)
  are validated only negatively (they perturbed nothing live). If
  you want positive evidence, a synthetic test — QUEST_TERMINAL-style
  forcing of an untranslated L2 entry, or a handler that RETURNS
  instead of unwinding — would exercise them; we judged it not worth
  building this session. Disagreement welcome.
- **l2_table completeness**: 30 symbols transcribed from the
  Layering census (20 registered + 10 frozen/dead). Diff it against
  your census copy; a missed L2 entry would silently pair as fabric
  (safe direction: over-checking, not under), a wrongly-tagged
  fabric entry would get entry+arming semantics (worse — check the
  10 especially).
- **Injected-raise entry pair**: inject_fire deferral makes injected
  raises pair at the O?SIGNAL entry like real call sites — an
  IMPROVEMENT over the old immediate-dispatch (which had no entry
  pair), but it changes shape-1/2/3 pair sequences vs your Project 5
  records by one pair each. Confirm you agree that is correct, not
  drift.

## 8. Shared-doc corrections/edits made

Full docs pass, correction-style, dated (list in the session
narrative): Run.md lockstep section; Contract §7 harness bullet
RESOLVED (with the Step-0 refinement: "every registered entry breaks
a batch" was true only LATENTLY — live L2 being fully translated
already absorbed interior entries; the change made invisibility a
law, not a fact about coverage); NativeDesign §7 status banner;
REPORT H1 resolved / H6 rendezvous-built; REVIEW.md
do-not-implement-l2_depth note; SharedProtocol frozen-interface-2
addendum (entry rendezvous precede wrapper bodies); generation
banners on LockstepHarness/EmulationVerification; your two prompts
bannered; NextSession rotated.
