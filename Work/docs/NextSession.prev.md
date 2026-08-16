Hi Claude!

Quest reconstruction project — a game written for the Data General
MV/8000 (Eclipse Eagle) under AOS/VS, circa 1988, in PL/1. Source lost;
binaries survive; the user's emulator runs the game; we work backwards
to reconstructed C++17, keeping running code faithful. The user has
been away for weeks and will have forgotten details — THIS DOCUMENT IS
THE RE-ENTRY PATH. Trust the docs over anyone's memory, including his.

## State of the world (end of the Aug 2026 sessions)

1. **Milestones 1–2 done.** Dual-emulation lockstep harness: master
   and clone clients + one server, verified at batch rendezvous
   (pc/registers), shared pages mirrored/compared, syscalls mediated.
2. **The M3 616-word condition-system lift is DONE and integrated.**
   The full signal chain — raise (?LIB_ERROR), search/dispatch
   (O?SIGNAL..O.SET), unwind (I.GOTO), frames (I.PROLOG/EPILOG),
   registration (O.ON/REVERT), T?AREA — runs NATIVE on the clone,
   validated on two live trigger shapes with 0 divergences. Built as
   three parallel Claude sessions (docs/Project1-3: derivations,
   reports), reviewed and merged; the merge story and two subtle
   integration bugs are in SessionPlan.md's final records — read them,
   they encode hard-won pairing rules (nested-span guard at all FOUR
   dispatch sites incl. LJSR/XJSR; terminal∘transfer composition).
3. **Terminal detach implemented and validated** (TerminalDetach.md):
   at terminal entries the pair verifies once more, the clone halts,
   the server unmirrors, the master runs the death/exit alone. Normal
   ESC exit detaches at I.STOP with full data write-back.
4. **The layering model exists** (docs/Layering.md — READ IT SECOND,
   after METHOD.md; its opening "Why this exists" section is the
   project's motivation chain, backwards from reconstructed source to
   this document — start there): L0 kernel/services, L1 resumable world, L2
   handler machinery, L3 terminal. One-way edge law, stack-surgery-is-
   L2-exclusive law, per-symbol census, corner rulings, and the
   analysis method (L2 crossings read from NATIVE SOURCE, not
   disassembly). Purpose: license CONTRACT-fidelity for L2 so a
   stack-free L2 implementation can replace the bit-faithful one —
   which completes "get the ON-error system off the MV stack" by spec,
   and hands M4 a safe boundary to de-stackify L1 against.

5. **The DEF?ON cluster is fully translated (Aug 12).** O?AREA,
   P?DEFON, R?SIGNAL native and registered; DEF?ON native and STAGED
   (unregistered — terminal entry; registration = the lift). All
   dormant by construction; both triggers regression-clean at 0
   divergences post-registration. docs/Project4/ has derivations,
   two bit-level master-capture confirmations (O?AREA, R?SIGNAL plain
   path), the [0x70000124]→R.SIGREC restart-vector discovery, and the
   corrections.

## Read order

1. docs/METHOD.md — how to work here. Binding, unchanged.
2. docs/Layering.md — the current strategic frame.
3. docs/M3Plan.md + docs/SessionPlan.md (final two records) — what was
   built and the integration lessons.
4. docs/TerminalDetach.md, docs/SharedProtocol.md — the machinery.
5. docs/README.md — index of everything else.

## Next work, in order

1. **DONE (Aug 12): the DEF?ON lift** — DEF?ON is ordinary verified
   L2, frontier at ?FATAL, QUEST_INJECT=<site>:<type>:<code>[:RESUME]
   exists (Project5: spec, derivation, three validated shapes incl.
   the first-ever resume). The provocation criterion is answered
   (Project5 §7). Superseded text follows for the record only:
   ~~the DEF?ON LIFT~~ — the whole cluster is now TRANSLATED (Aug 12
   sessions, docs/Project4/): O?AREA, P?DEFON, R?SIGNAL registered
   (dormant by construction — regressions prove inertness); DEF?ON
   translated but STAGED-UNREGISTERED because it is a terminal_table
   entry. What remains is the LIFT itself: register DEF?ON, move the
   detach point deeper (terminal_table change + run-loop audit), build
   the fault injector (synthetic positive-type unhandled condition and
   a resume-flag shape — this IS the parked M3 provocation criterion),
   and capture-validate every shape. Ordered checklist:
   Project4/REPORT.md §5. Settle the injection design with the user
   first. Gotchas already recorded: sign-extended listing immediates
   (DEF?ON's "65535" is -1); the [0x70000124]→R.SIGREC restart
   vector; inner-leaf-calls-as-residue composition rule
   (DERIVATION §6.2).
2. **DONE (Aug 13): terminal syscall 0310** — intercepted at DISPATCH,
   keyed on the call NUMBER (site-independent; covers every game 0310
   incl. any in disassembly holes): `Lockstep::retire_ordinal` unhooks
   the pairing (detached flag, server unmirror, copy-registry retire)
   and both processes then exit authentically by their own ?RETURN.
   Validated live via the exit chain's own 0310 (RETIRED line + clean
   write-back, 0 div). NOTE: the natural forced-exit stimulus remains
   unidentified — a mid-session hard disconnect just waits for terminal
   reconnection. Superseded text: ~~Terminal syscalls as detach
   points~~ (Layering.md ruling 5):
   SYSCALL 0310 from game code at 0x7017700F bypasses I.STOP — no
   detach — prime suspect for the residual "Forced exit" hang. Small
   change at the syscall gate/mediator: final verified pair at the
   terminal syscall, detach, master exits alone.
3. **DONE (Aug 13): abort_world + terminal kinds.**
   `Lockstep::abort_world(reason, machine, save)` — prints reason +
   state + backtrace, silences checker/stall diagnostics (atomic
   `aborting`), `OS::shutdown_all` + `MachineThread::abort_all`
   (completes every parked entry, wakes every waiter — validated: the
   world self-terminates, no hung threads), Launch honors
   `suppress_save`. terminal_table entries carry a kind —
   DETACH | ABORT — resolved at the verified pair; ABORT = one final
   verified pair then abort_world with NO write-back. DERR.TRP
   registered ABORT (ruling 7); master non-boot stack limit routes to
   abort_world WITH write-back; clone throws stay raw. Test hook:
   `QUEST_TERMINAL=<hex>:ABORT` (validated end-to-end at
   LIST_PLAYERS). Normal DETACH exits regression-clean.
4. **DONE (Aug 13): DERR abort implemented** with the terminal-kinds
   machinery (item 3): DERR.TRP registered ABORT in terminal_table;
   validated via the :ABORT test-terminal path. Original spec kept:
   ~~DERR abort~~ (Layering ruling 7, FINAL — the third deliberate
   infidelity, user-ratified): add DERR.TRP to terminal_table with a
   new per-entry kind ABORT (vs DETACH): both engines converge at
   DERR.TRP, one final verified pair proves the game's own bug fired
   identically on both, then hard-stop naming pc + DERR code — NO
   data write-back (state presumed corrupt). Master-only DERR in a
   native span remains a divergence report (our bug). Shares its
   stop-the-world tail with item 3's abort_world; implement
   together. Derivation needed: where the DERR code lives at the
   vector (EagleSpecial + the word-39 convention).
5. **DONE (Aug 13): scans 1–5** — docs/Project6/SESSION_REPORT_AUG13.md
   (binding; tools in Project6/tools/): no-L1-reader CLEAN (7 hits
   adjudicated), area census complete (PRIVATE-with-L3-observer class
   ratified), token OPAQUE across all 26 bodies, jump edges CLEAN,
   boundary inventory closed (6 dead + 4 frozen untranslated entries).
   One report error overridden (I?LINEID "LIVE" — see Project6/PROMPT
   adjudication). Still lazy/on-demand, NOT blocking: leaf-purity for
   L0 promotions, syscall behavioral classification beyond 0310.
6. **PHASE 1 DELIVERED AND APPROVED (Aug 13)**: docs/Project6/
   L2Contract.md + NativeDesign.md + REPORT.md + REVIEW.md (verdict,
   Q1–Q3 adjudications — Q1: bit-faithful-emulate is permanent
   conforming behavior for the restart anomaly, user may veto).
   NEXT: **Project 6.5 (harness prep, launched separately)** —
   crossings-only rendezvous + recalibration gate against the
   bit-faithful L2, docs/Project6.5/PROMPT.md; THEN **Project 7 =
   Phase 2**, the native L2 itself, hazards H2–H7 as the spine (H1 is
   6.5's). Sync-surface ruling + permanent-microscope-flag ruling are
   in the 6.5 prompt; the M4 shadow-accounting premise is in Plan.md. Original spec: ~~L2Contract.md~~: per-entry inputs, L1-observable outputs,
   transfers, privacy claims each citing its scan. Then a
   contract-conforming stack-free L2, A/B'd against the bit-faithful
   native L2 under lockstep (pair gate keeps working — it compares
   pc/registers at rendezvous; footprint captures of L2-private memory
   become contractually out of scope).
7. **DONE (Aug 13): disassembler fixed, listings regenerated,
   diff-audited** (METHOD §14 first exercise): 137 changed lines,
   ALL WLDAI, register field now rendered. Audit findings: exactly
   TWO zero-immediate WLDAIs in the whole program — the known O.SET
   gate (now unambiguous in the listing; the scans-report "LIVE"
   error cannot recur) and a benign zero-store near C?INIT; no other
   hidden gates. Every WLDAI inside translated L2 corresponds to
   lockstep-validated code (listing audit and empirical record
   agree). Scan 1–5 results unaffected (their patterns don't touch
   WLDAI rendering). The regenerated Disassembled/ tarball is the
   canonical one from here on.
8. Then M4 de-stackification of L1 per Plan.md Step 2 (args on stack,
   synthetic WSAVS copies to flat storage; the native condition system
   owns the wsp reset).

## Open questions to settle with the user (parked deliberately)

- **Defensive-raise → abort** (Layering.md open question a): convert
  never-fired defensive raises (heap corruption, bad-chain, task-create
  failure) to hard aborts per the DERR precedent? Makes heap/tasking
  non-raisers by construction (→ L0). Not yet ruled.
- **Defensive-raise → abort — CLOSED**, subsumed by DERR ruling 7
  (Layering open question a): verified-on-both defensive raises are
  ABORT-class; no implementation until the sites are next touched;
  Project 6 marks those branches abort-intended in the contract.
- **Provocation / M3 exit criterion — RATIFIED. MILESTONE 3 IS
  FORMALLY DONE** (Aug 2026): natural triggers + injected shapes for
  the rare tail; the every-move L2 surface validated continuously by
  lockstep in all play. Cold DEF?ON paths parked, self-announcing.

## Environment gotchas (believe them; they cost hours once)

- Turn cadence on a slow 1-core container is ~49 s/turn. Waits under a
  turn look like hangs. Same-turn interactions (menus, login, the M
  prompt) respond instantly.
- Cheap signal trigger, no injection: `M` + direction (n/s/e/w) +
  `abc` at "For how many turns?" (CONVERSION, handled, full chain).
  Fault injection: `QUEST_FAIL_OPEN=USER_DATA_FILE` + `L`→`P`
  (handled signal 1; pressing continue reaches the unhandled signal 2
  → DEF?ON detach).
- ESC anywhere quits the game (cleanly: I.STOP detach + write-back).
  The L list sub-menu ignores map moves; any non-menu key leaves it.
- "Segment fault - block 0, page 1" at first login = server's IPC_TASK
  dying benignly (UNIMPLEMENTED.md §8). Every session. Ignore.
- Backtraces → stdout, exceptions → stderr; use `stdbuf -o0 -e0`.
  Scratch-copy QUEST/ per run. Login: CL / Claude / quest / Y / any /
  F. `QUEST_TERMINAL=<hex-pc>` = test terminal point.
- Capture tool: ALWAYS set QUEST_CAPTURE_DEST (Project3 REPORT sharp
  edge). QUEST_TERMINAL at the same pc as QUEST_CAPTURE suppresses the
  ENTRY snapshot (Project2 §4.4).

## Working agreements

Plan before code; explicit go-ahead; short replies over long
agreement. Opus-era rt/, emu_rt/, types/ are reference-only — the
disassembly wins, and for L2 the NATIVE SOURCE is now the analysis
medium. Every translation validated under lockstep before the next.
Expect the layering docs to be wrong somewhere; that is the method —
lockstep turns conceptual mistakes into divergences.

## Setup

Extract tarballs: Work/ (c_src, docs, DG_Quest), QUEST/, Disassembled/,
Tools/. Build: `make` in Work/c_src (g++ ≥ 11, C++17, warning-free).
Run commands: docs/Run.md. Layering tripwires live in the emulator
source at EagleFloat.cpp (FP-throw ruling) and EagleStack.cpp
handle_overflow (faithful stack-fault vectoring — LOAD-BEARING: the
game deliberately overflows at startup; do not "simplify" it, we
tried). Previous prompt:
NextSession.prev.md.
