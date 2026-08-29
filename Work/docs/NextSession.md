# Next session

## P25 LANDED — pending battery verdict (Aug 29 2026)
- P25 (byte addressing + call ledger) is IMPLEMENTED on branch
  p25-byte-addressing: **566/566 decorated sites lowered** (the user
  reversed the borrow exclusion in-session — bracket as @addr
  instruction pairs, args as stores; the old 564 target and the
  "96 B-form" framing are superseded — 18 of those 96 were word-form
  parse gaps, see Project25/ByteEA.md §1). Grammar: wp/bp pointer
  builders (masking in the executor), M8 raw-index loads/stores, `*`;
  `<<` removed (was spec'd, never implemented). Local gates 3/3
  green, 0 div. Battery 035: 12/13 green, ALL IR legs clean; the inj-emu red
  (all-emulated leg, injection pc unreached) RULED FLAKE by the user
  — verdict recorded in REPORT §5. NEXT: (1) integrate the branch.
  (2) Disassembler byte-operand
  defect: fix DEFERRED by user ruling (the byteIndexed masking is
  buggy in multiple ways; too risky to touch) — the lower.py
  reconstruction shim is STANDING; docs/DISASSEMBLER_BYTE_OPERANDS.md
  is the record and the future-fix protocol. (3) P26 = t-places (the two @addr borrow bracket
  pairs in lowered blocks are the pilot's first customers) +
  conditional exits.

## Battery template (Aug 29 2026)
- tasks/034-parallel-battery.sh is the battery template of record
  (parallel, JOBS=6, 13 legs incl. the emu isolation pair); hold/031's
  serial shape is superseded. Copy 034, not 031/032.

## P24 handoff (Aug 29 2026)
- P24 (wide-carry) is LANDED on branch p24-wide-carry; battery = task
  032, verdict RECORDED in Project24/REPORT.md §7: 9/11 GREEN; 2 RED =
  finding F6 (inject/terminal arming vs IR at non-block-entry pcs — a
  pre-existing P23 gap, no carry involvement, evidence in the report).
  **F6 RESOLVED (Aug 29, user ruling: both):** loader drops armed-pc
  blocks + standing all-emulated inj/abort legs; task 033 4/4 GREEN —
  the terminal legs are back in the battery. (b) RESOLVED (Aug 29,
  user): census suffices; KNIGHT_ATTACK/DIVX observed opportunistically
  in ordinary play — nothing gated. (was: whether the sites
  need live-pair demonstration (a manual combat/store play session) or
  the census classification suffices.
- Coverage caveat of record: the four ADC.C consumer sites are in
  BEING_ATTACK/KNIGHT_ATTACK (combat); scripted legs may not reach
  them. Census proves them fix-invariant regardless; live combat pairs
  need a manual play session if the user wants them demonstrated.
- Integrator: main lacks the P23-integrated tree; p24-wide-carry
  carries it as its base. Merge order: P23 integration then P24 (or
  the branch wholesale).
- NEXT per the P23 queue: P25 = t-places (borrows pilot), then B-form
  byte-EA extraction (96 call sites), WPSH multi-wide (25), `save`,
  @/bit-15 fix+regen+diff-audit (25 blocks). Pre-P25 census owed:
  crossings inside borrow-bracket interiors.

> **[Aug 29 2026 — P23 banner]** This re-entry narrative is from Aug 13
> and predates P14–P23. Current state now lives in CURRENT_STATE.md
> (newest entry on top). **P23 LANDED** (Gen-6.1: the IR — reviewed and
> integrated Aug 29; docs/Project23/REPORT.md, spec docs/IR.md; all P22
> obligations discharged). Next work, per REPORT §8: the wide-carry
> re-verification (parked task, docs/Project23/WideCarry.md — redo the
> carry-live-in census BEFORE landing the parked patch), then B-form
> byte-EA extraction (96 call sites), WPSH multi-wide (25), `save`,
> and the @/bit-15 listing fix+regen. **P24 = t-places** (borrows are
> the pilot per user ruling; `end if` conditional exits are the
> boundary). The narrative below remains valid history for
> Milestones 1–3.

Hi Claude!

Quest reconstruction project — a game written for the Data General
MV/8000 (Eclipse Eagle) under AOS/VS, circa 1988, in PL/1. Source lost;
binaries survive; the user's emulator runs the game; we work backwards
to reconstructed C++17, keeping running code faithful. The user has
been away for weeks and will have forgotten details — THIS DOCUMENT IS
THE RE-ENTRY PATH. Trust the docs over anyone's memory, including his.

## State of the world (end of the Aug 13 2026 sessions)

1. **Milestones 1–2 done.** Dual-emulation lockstep harness: master
   and clone clients + one server, verified pairs, shared pages
   mirrored/compared, syscalls mediated.
2. **Milestone 3 formally done.** The full condition chain — raise,
   search/dispatch, unwind, frames, registration, the DEF?ON cluster —
   runs NATIVE (bit-faithful) on the clone; validated on the natural
   triggers plus three injected shapes (QUEST_INJECT) at 0
   divergences. Terminal machinery complete: DETACH (I.STOP/?FATAL),
   ABORT (DERR.TRP, abort_world, save suppressed), RETIRE (?RETURN
   0310 at dispatch).
3. **M3b Phase 1 delivered and approved**: docs/Project6/L2Contract.md
   + NativeDesign.md + REPORT.md + REVIEW.md — the contract that lets
   a stack-free L2 replace the bit-faithful one under
   contract-fidelity.
4. **THE CROSSINGS-ONLY CHECKER IS LIVE (Aug 13, second session).**
   The lockstep checker was REPLACED — no flag, no modes (user
   ruling: one sync model, permanently). Sync surface: L1 fabric
   unchanged (heartbeat, syscall gate, L0/L1 entry and leaf pairs);
   L1↔L2 crossings pair in BOTH directions (entry pc via deferred
   dispatch, exit at the post-call/transfer target); interior L2→L2
   invisible BY RULE (layer map, not translation coverage); L3 door
   unchanged. Full design + Step-0 characterization + regression
   evidence: docs/CrossingsChecker.md. Generation lineage (M2
   entry-keyed → M3b crossings-only → anticipated M4 shadow
   accounting): docs/CheckerHistory.md. Recalibration gate: all
   seven regressions at 0 divergences against the unchanged
   bit-faithful L2; the two anomalies found reproduced bit-for-bit
   on the pre-change baseline binary (environmental, recorded in
   CrossingsChecker.md side findings).
5. **Superseded**: Project6.5/PROMPT.md and Project7/PROMPT.md (both
   drafts of the harness-prep project — done differently and without
   the flag in the Aug 13 session; banners added). H1 of
   Project6/REPORT.md is resolved; H6's rendezvous is built and
   waiting for its Phase-2 native tail handling.

## Read order

1. docs/METHOD.md — how to work here. Binding, unchanged.
2. docs/Layering.md — the strategic frame (start at "Why this
   exists").
3. docs/CheckerHistory.md + docs/CrossingsChecker.md — what
   "verified" means now, and how it got that way.
4. docs/Project6/L2Contract.md + NativeDesign.md + REPORT.md — the
   contract and the Phase-2 design + hazards.
5. docs/TerminalDetach.md, docs/SharedProtocol.md — the machinery.
6. docs/README.md — index of everything else.

## UPDATE — Aug 15 2026 planning session (supersedes "Next work" below)

M3 is COMPLETE (SessionPlan.md final entry). The flat-graph analyses
are DEFERRED; M4a comes first. Design of record: docs/M4aDesign.md.
Project 12 DONE and approved (READ_IN off the stack, 0 div; its
rulings folded into M4aDesign.md §8). Project 13 batches 1+2 LANDED (45 routines live at 0 div; M4aDesign §10). Project 14 Phase A LANDED and approved: hw/Mapper.{hpp,cpp} implements
docs/Mapper.md (now updated with the P14 rulings: three-call surface,
Q2 overlay, wsl latch, wave-scoped validity conditions); accreted
T/T_any/T_inv deleted; R-C book layout fix (wfp -2); QUEST_INJECT
fail-loud; regression matrix is BANDED for world-downstream counts
(?GTOD wall-clock — see Run.md). Next work: Project 14 PHASE B per
docs/Project14/PROMPT.md — batch 3 (33 remaining wave-one routines,
parents + callable children together), play-driver growth toward the
19 armed-but-unexercised routines, landing roll-call + CheckerHistory
Gen-4 append (include the stride-masking near-miss sentence).
Note: the Tools/ tarball is now named Disassembled/.

## Next work, in order (HISTORICAL — see UPDATE above)

1. **Phase 2: the stack-free L2 itself** — prompt WRITTEN:
   docs/Project8/PROMPT.md (Project 7 is a burned name; see its
   banner). Includes the rulings to settle with the user first
   (I.EPILOG mismatch, walker outputs, chain cap, Q1 veto check,
   landing stages) and the shadow→flip→stop-writing landing
   strategy. Spine:
   hazards H2–H7 in Project6/REPORT.md §6 (H1 is done). The
   rendezvous definition it must satisfy is implementation-
   independent (CrossingsChecker.md "What Phase 2 inherits"):
   dispatch at the same 20 entries pairs at the door before the new
   code runs; every traversal ends at the contractual L2→L1 exit
   state; the DISPATCH_RET re-entry pair already fires — Phase 2
   adds the native handling after it (H6). A/B validation regime:
   Contract §7 (register file never private; ABORT-INTENDED third
   result class; footprint captures retired for conforming
   implementations).
2. Then M4 de-stackification of L1 per Plan.md Step 2 (synthetic
   WSAVS copies to flat storage; the native condition system owns
   the wsp reset; checker premise = shadow stack accounting,
   CheckerHistory.md Generation 3 placeholder).

## Open questions (parked deliberately)

- Defensive-raise → abort: CLOSED, subsumed by DERR ruling 7; the
  contract marks those branches ABORT-INTENDED; wire to
  abort_world(save=false) when next touched.
- The L0? census rows resolve lazily on demand.
- The natural "Forced exit" stimulus remains unidentified (mid-session
  hard disconnect just waits for terminal reconnection).

## Environment gotchas (believe them; they cost hours once)

- Turn cadence on a slow 1-core container is ~49 s/turn. Waits under a
  turn look like hangs. Same-turn interactions (menus, login, the M
  prompt) respond instantly.
- Cheap signal trigger, no injection: `M` + direction (n/s/e/w) +
  `abc` at "For how many turns?" (CONVERSION, handled, full chain).
  Fault injection: `QUEST_FAIL_OPEN=USER_DATA_FILE` + `L`→`P`
  (handled signal 1; continue reaches the unhandled signal 2 →
  ?FATAL detach).
- ESC from the map quits cleanly (I.STOP detach + write-back). The
  historical plain L→P→ESC driver does NOT reach I.STOP on this
  container (first ESC leaves the list sub-menu / lands mid-turn;
  session ends at socket close) — reproduced on the pre-change
  baseline, so it is a driver artifact, not a checker bug. Use the
  M-trigger driver for ESC-detach regressions.
- Injected shape 1 dies at ?FATAL if the driver waits a full turn
  after P (an extra game turn runs before ESC; "invalid channel
  number" cascade) — also reproduced on baseline; Project 5's 15 s
  wait gets the documented clean continue.
- "Segment fault - block 0, page 1" at first login = server's
  IPC_TASK dying benignly (UNIMPLEMENTED.md §8). Every session.
  ~1,100+ "RESERVED ACCESS player[0]" stderr lines per session are
  standing noise. Ignore both.
- Backtraces → stdout, exceptions → stderr; use `stdbuf -o0 -e0`.
  Scratch-copy QUEST/ per run. Login: CL / Claude / quest / Y / any /
  F. `QUEST_TERMINAL=<hex-pc>[:ABORT]` = test terminal point.
- Capture tool: ALWAYS set QUEST_CAPTURE_DEST (Project3 REPORT sharp
  edge). QUEST_TERMINAL at the same pc as QUEST_CAPTURE suppresses the
  ENTRY snapshot (Project2 §4.4).

## Working agreements

Plan before code; explicit go-ahead; short replies over long
agreement. Opus-era rt/, emu_rt/, types/ are reference-only — the
disassembly wins, and for L2 the NATIVE SOURCE is the analysis
medium. Every translation validated under lockstep before the next.
Expect the docs to be wrong somewhere; that is the method — lockstep
turns conceptual mistakes into divergences.

## Setup

Extract tarballs: Work/ (c_src, docs, DG_Quest), QUEST/, Disassembled/,
Tools/. Build: `make` in Work/c_src (g++ ≥ 11, C++17, warning-free).
Run commands: docs/Run.md. Layering tripwires live in the emulator
source at EagleFloat.cpp (FP-throw ruling) and EagleStack.cpp
handle_overflow (wsp==wsl boot-gate — LOAD-BEARING: the game
deliberately overflows at startup; do not "simplify" it, we tried).
Previous prompt: NextSession.prev.md.
