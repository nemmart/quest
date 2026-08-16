# Project 5 — The DEF?ON Lift + Fault Injector (SPEC)

Hi Claude! Single-session project. You implement; a reviewer session
tests and integrates. Read IN ORDER first: docs/METHOD.md,
docs/Layering.md (esp. "Why this exists" + ruling 6),
docs/TerminalDetach.md, docs/Project4/REPORT.md §5 (the lift
checklist this spec expands), docs/Project4/DERIVATION.md,
docs/SharedProtocol.md (merge rules — but like Project 4 you are a
solo session and MAY edit RTStubs/Machine/Lockstep/Makefile; list
every such edit in your REPORT for audit).

## Goal

DEF?ON is fully translated but STAGED (commented translation_table
row). Turn it on properly: DEF?ON becomes ordinary verified L2 —
the pair SYNCS on it instead of detaching at it — and the detach
frontier moves to the truly terminal points. A fault injector
provides the three validation shapes, two of which have never
executed in the program's history.

## Part A — the lift (ordered, must land together)

1. Uncomment `{ "DEF?ON", emu_rt::defq_on }` in RTStubs.cpp.
2. Remove "DEF?ON" from `terminal_table` (RTStubs.cpp). New terminal
   set: I.STOP, I.STOPM, ?FATAL (unchanged rows). Do NOT add new
   entries in this project; terminal syscalls (0310) are a separate
   task.
3. Audit every consumer of the changed classification, both roles:
   Machine.cpp run_steps (the terminal check precedes the translated
   check in the rt_sync entry block, AND the run-to-return escape) —
   a formerly-terminal now-translated entry changes the master's path
   (arrives at DEF?ON entry → now arms rt_pending_return instead of
   terminal-breaking) and the clone's (dispatches native instead of
   never arriving). Confirm the pairing story for each DEF?ON exit:
   - resume exits (type-2 return, resume-flag return): clone native
     ends via native_return at the caller; master run-to-return ends
     at pc==pending. Pair = native_span at the return point.
   - resignal exits: TRANSFER through native O?SIGNAL — handled →
     both engines reconverge at the game handler pc (range-exit
     rule); unhandled → the cascade re-enters DEF?ON (recursion!
     verify against the bytes what the original does — if it loops,
     both engines loop identically into the runaway guard; document
     what you find) or reaches ?FATAL → terminal pair at ?FATAL
     entry (the NEW frontier), detach there.
   - ?FATAL exit (flag clear): fall back? NO — DEF?ON's native path
     may native_transfer... CAREFUL: ?FATAL is RT-range terminal;
     per SharedProtocol's corrected composition rule, do NOT
     native_transfer to an RT-range terminal — the existing
     def_on.cpp fallback gate "(no-resume flag — ?FATAL terminal
     path, emulating)" already handles this correctly by falling
     back whole. Verify that gate survives the lift unchanged.
4. Build warning-free; run BOTH standard triggers (M+dir+abc;
   QUEST_FAIL_OPEN + L→P + continue) BEFORE the injector exists.
   Expected: signal 1 unchanged; signal 2's cascade now runs
   ?LIB_ERROR(native-fallback) → ... → DEF?ON — decide from your
   step-3 analysis whether the fallback span now ends at ?FATAL
   (deeper detach) and confirm empirically: 0 divergences, detach
   line reads `DETACHED at 7017F036 (?FATAL)`, clean master death.

## Part B — the fault injector (~50-80 lines, emulator-level)

Mechanism (agreed design): synthesize a raise SYMMETRICALLY in the
shared instruction path, so both engines do it identically at the
identical pc. No master-only or clone-only code anywhere.

- Env knob: `QUEST_INJECT=<site>:<type>:<code>[:RESUME]`
  - `<site>` = a game symbol name (resolved via SymbolTable at
    startup, e.g. FIRE_BOW — pick the real F-command handler symbol
    from Disassembled/quest.symbols and document it) OR a hex pc.
  - `<type>`,`<code>` = decimal/hex condition type and code.
  - `:RESUME` = also set bit15 of narrow [area+0x16] at injection
    time (both engines, identically), arming DEF?ON's resume branch.
- Implementation point: Machine::run_steps, guarded by rt_sync-style
  role check? NO — it must fire on BOTH client roles (master AND
  clone) and NOT on the server. Fire once per keypress-visit: when
  pc == site and an armed flag is set; disarm after firing; re-arm
  is out of scope (one shot per run is fine, document if you do
  more).
- On fire: synthesize the O?SIGNAL call exactly as a real site does.
  Template = DEF?ON's own resignal staging at 0x7017EF37-EF3D
  (Project4/DERIVATION §3.6): push three ARG CELLS holding type,
  key2(=0... verify what the template stages — derive, don't guess),
  code; per the LCALL convention set ac3 = return address = the
  injection pc (so the game resumes exactly where it was on a
  handled signal), stage ac0-2 as the convention requires (derive
  from EagleStack LCALL + the P2/P4 derivations), set pc = O?SIGNAL
  entry (0x7017EDED). The synthesized state must be IDENTICAL on
  both engines by construction — same code path, same inputs.
- The injection must coexist with the clone's native dispatch: the
  clone arrives at O?SIGNAL entry via pc-set, NOT via LCALL
  execution — check how the clone's native dispatch triggers (LCALL
  execute vs run_steps entry arrival) and make the injected entry
  dispatch native on the clone / emulate on the master exactly like
  a real call. If dispatch only happens at LCALL execute, you must
  route the synthesis THROUGH the dispatch path or replicate it —
  derive, decide, document.

## Part C — the three validation shapes (all under lockstep -silent)

1. **Handled**: inject at the F-command handler, type=positive
   (pick one from ON_ERROR_CATALOG's live categories), during a
   normal turn. Expect: full native chain fires, handler catches,
   unwind, game continues (F "does nothing"), 0 divergences.
2. **Unhandled cascade**: inject in the between-turns window (site =
   the REFRESH-region pc where signal-2's close failure historically
   raised — find it in the p4/f1 trace ret values or derive; document
   your choice), no RESUME. Expect: native DEF?ON resignal path →
   whatever step-3 analysis predicted (loop-to-guard or ?FATAL) →
   terminal pair AT ?FATAL, detach, master dies authentically.
3. **The resume branch** (never executed in history): same site,
   `:RESUME`. Expect: native DEF?ON returns, execution resumes at
   the injection pc, game continues, 0 divergences. Document what
   the resume flag's state is afterward (consumed or sticky —
   READ the bytes AND observe).
   If reality diverges from expectation on 2 or 3, that is a
   FINDING, not a failure — capture it precisely.

## Deliverables

- Code: the lift edits + `QUEST_INJECT` (files of your choosing;
  keep the injector in one place, tripwire-commented).
- docs/Project5/REPORT.md — SharedProtocol format §1-6, PLUS: the
  step-3 pairing analysis per exit, the shape-2 recursion finding,
  the resume-flag semantics finding, exact reproduction commands
  for all three shapes (METHOD §10: commands + expected values from
  the ACTUAL runs).
- docs/Project5/DERIVATION.md only if you derive new byte-level
  facts (the injection staging derivation goes here).
- Do NOT update Layering.md/NextSession.md/TerminalDetach.md — the
  reviewer integrates those.

## Gotchas (believe them)

- Turn cadence ~49s on slow containers; M+dir+abc is the cheap
  same-turn trigger; ESC quits; login CL/Claude/quest/Y/any/F;
  stdbuf -o0 -e0; scratch-copy QUEST/; QUEST_CAPTURE needs
  QUEST_CAPTURE_DEST or you lose a run.
- The ?FATAL death now runs master-only post-detach: its stdout
  traceback is the evidence shape 2 worked.
- grep -c "warning" on full builds false-positives in this tree;
  recheck with a targeted touch+make.

## The decision this project settles (tell the user in your REPORT)

The parked M3 provocation criterion is hereby answered by
construction: M3's error-path validation = the two natural triggers
+ the three injected shapes, all under lockstep, each at least once.
If you believe that set is insufficient, say so and why.
