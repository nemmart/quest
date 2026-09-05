# Docs Index

## Origin (for the record — Aug 2026)

Quest came into this family from Westborough: the user's stepfather
worked at Data General (shared-library support on the MV line) and
gave him the game, which he carried to his university to play. The
original tapes were later lost. The bits survived through a DG
users-group archive that served the raw files over HTTP; the user
pulled them down, wrote Java code to GUESS AT AND UNPACK the DG .DMP
dump format — derivation from bytes, no spec, verified by whether
the files came out sane — then wrote the MV instruction emulation
and a minimal OS emulation around them. That Java became M1's C++
port, and the derive-from-bytes/verify-by-behavior method became,
scaled up and given the lockstep oracle as referee, this entire
project. The runtime the reconstruction audited byte-by-byte — and
found disciplined, no slop at any exit — is the work of that same
DG engineering culture. The audit came home.

Status key: **CURRENT** = binding/live · **REFERENCE** = stable findings
· **HISTORICAL** = superseded, kept for the record · **SESSION** =
one-time change/review notes.

## Start here
| Doc | Status | What |
|---|---|---|
| NextSession.md | CURRENT | Re-entry prompt: state, next task, gotchas |
| METHOD.md | CURRENT | How to work on this project (binding) |
| IR.md | CURRENT | THE quest.ir specification (ir 3, P26) — normative, spec-wins; grammar, block rules, executor model, version history |
| Plan.md | CURRENT | Milestones + Steps 1–4 big picture |
| M3Plan.md | CURRENT | Milestone 3 minimal lift (616w), decisions, order |
| TerminalDetach.md | CURRENT | Terminal-detach design + validation (implemented) |
| CheckerHistory.md | CURRENT | The checker, generation by generation (append-only) |
| CrossingsChecker.md | CURRENT | The live (M3b) checker: design, characterization, gate evidence |
| Run.md | CURRENT | Running the emulator, flags, debugging aids (lockstep section updated for the crossings checker) |
| CODE.md | CURRENT | C++ style guide + architecture |
| UNIMPLEMENTED.md | CURRENT | Gap catalogue + failure modes + benign noise |
| Layering.md | CURRENT | Four-strata entry-point model + census + scans (pre-contract) |
| SharedProtocol.md | CURRENT | Parallel-project merge rules + frozen interfaces (transfer pairing, rt::t_area) |
| Project1/ .. Project5/ | REFERENCE | The five translation sessions: prompts, DERIVATIONs (the per-routine ground truth), REPORTs. P4 = DEF?ON cluster, P5 = the lift + QUEST_INJECT |
| Project6/ | CURRENT | The L2 contract project: PROMPT.md (governing principle + rulings), SESSION_REPORT_AUG13.md (scans 1–5, BINDING evidence base), L2Contract.md + NativeDesign.md + REPORT.md (Phase-1 deliverables, hazards H2–H7), tools/ |
| Project6.5/, Project7/ | HISTORICAL | Never-run harness-prep prompt drafts, superseded by the Aug 13 crossings-checker session (banners in each); Project6.5/REPORT.md is the session's development report, written for prior-session review |
| M4aDesign.md | CURRENT | M4a design of record (Aug 15 2026): 0x78000000 address book, WSAVS/WRTN hijack, Checker Generation 4 (shadow wsp + translator) |
| Project12/ | REFERENCE | M4a first migration: READ_IN off the stack, 0 div; REPORT.md = as-built hijack + Gen-4 checker tables, audits, evidence (APPROVED Aug 15) |
| Project13/ | REFERENCE | M4a widening: batches 1+2 LANDED (45 live, base 0x74000000, end-inclusive T + stride, @-flag inverse); REPORT.md §6 |
| Project8/ | CURRENT | Phase 2 prompt: the stack-free L2 implementation (rulings to settle, landing stages, hazards H2–H7, A/B matrix) |
| Project22/ | REFERENCE | Gen-6.0 block-sync checker LANDED (Aug 28): BlockSyncDesign.md (design of record), PROMPT.md (rulings), REPORT.md (as-built + task-030 gate + Q2 carry census 163/13495 + the 029 invalid-green record); IRDesign.md = P23 context |
| Project26/ | CURRENT | The math & control grammar (ir 3) LANDED on branch (Sep 5): MathDesign.md (design input + rulings), Census.md (27,600-embed census + per-mnemonic semantics with emulator citations), REPORT.md (as-built + battery 037 13/13), PROMPT.md, p26cov.py (coverage tool) |
| Project4/ | REFERENCE | The DEF?ON cluster (O?AREA, P?DEFON, R?SIGNAL, DEF?ON-staged): derivations, reports, lift checklist |

## Working docs
| Doc | Status | What |
|---|---|---|
| SessionPlan.md | CURRENT | Step-1 running log; session records at bottom |
| RTWorklist.md | CURRENT | Play-session data: reached routines, call sites |
| NextSession.prev.md | HISTORICAL | Previous re-entry prompt |

## Findings (stable reference)
| Doc | Status | What |
|---|---|---|
| I_ALLOC.md | REFERENCE | Heap derivation — COMPLETE, heap closed |
| O_ON.md | REFERENCE | O.ON/O.REVERT derivation (native, validated) |
| SQR31.md | REFERENCE | SQR31?3 derivation complete; translation deferred |
| UNSIGNED_TO_CHAR.md | REFERENCE | Derivation + RESOLUTION |
| QUEUE_INSTRUCTIONS.md | REFERENCE | ENQH/ENQT/DEQUE from the DG manual; QSEARCH gap |
| ERROR_PROCESSING.md | REFERENCE | Condition-surface map; failing syscalls signal |
| ON_ERROR_CATALOG.md | REFERENCE | All 26 game handler sites |
| CONSOLE_INTERRUPT.md | REFERENCE | ctrl-A/ctrl-C contracts (unwired) |
| GAME_REFERENCE.md | REFERENCE | Game data layouts, commands |
| GAME_CONCEPTS | REFERENCE | Game notes |
| LockstepHarness.md | REFERENCE | Harness architecture (steps 1–3) |
| EmulationVerification.md | REFERENCE | Original dual-emulation design |
| SyscallHandling.md | REFERENCE | Syscall dispatch architecture |
| PortingPlan.md | HISTORICAL | Phase 1–2 plan (complete) |
| HeapSignalPlan.md | HISTORICAL | Superseded by M3Plan.md; banner explains |
| ERROR_LIFT_SCOPE.md | HISTORICAL | Scoping doc; open questions answered, see banner; tier tables still accurate |

## Session notes
| Doc | Status |
|---|---|
| CHANGE_FLOAT_SHADOW.md | SESSION — float-shadow fix (resolved the LIST_PLAYERS underflow) |
| CHANGE_FATAL_SYSCALLS.md | SESSION — ?RNGPR/?SCLOSE/?ERMSG/?DFRSCH |
| REVIEW_UNSIGNED_TO_CHAR.md | SESSION — cross-review notes |
| I_ALLOC.md captures | docs/captures/ |
| HeapSignalPlan.md, SessionPlan.md revision histories | in-file |
