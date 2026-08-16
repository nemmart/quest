# Milestone 3 — Minimal Lift Plan (agreed Aug 2026)

Supersedes the *scoping* in ERROR_LIFT_SCOPE.md (whose open questions this
session answered) and the *ordering* in HeapSignalPlan.md (historical).
Method and per-routine checklist: METHOD.md. Terminal machinery:
TerminalDetach.md (implemented).

## M3 redefined

**M3 is done when the condition system is native** — the whole
I.PROLOG / O.ON / O?SIGNAL / O.SET / I.GOTO / I.EPILOG machinery off the
MV/8000 stack — because that is what gates de-stackification (M4).
Leaf runtime routines (`?RANDOM_NUMBER`, `SQR31?3`, `?WRITE_SCREEN`, the
I/O wrappers, ...) may stay emulated into M4 and be lifted later; under
the M4 model (below) emulated leaves keep working unchanged, since the
stack survives as a transient call/argument channel.

## Decisions locked in (this session)

- **DERR** (SUPERSEDED by Layering ruling 7, Aug 13 — final: DERR.TRP is
  a terminal-with-ABORT entry; the DERR VECTORS faithfully, both engines
  converge at DERR.TRP, one verified pair, then abort_world(save=false)
  naming the DERR number + pc. IMPLEMENTED. Original decision kept for
  the record:) hard abort with instruction location and DERR number.
  `EagleSpecial::DERR` was to be changed
  from vectoring (word 39) to throwing — symmetric on both engines. Drops
  `DERR.TRP` (119w) from scope and reduces the non-local-exit harness gap
  to signals only, which native `I.GOTO` closes by clearing
  `rt_pending_return`. If a DERR ever fires in play, that is the signal to
  revisit (it names its pc, so we will know exactly where).
- **QSEARCH**: lumped with DERR — hard terminal, never guessed at, fixed
  only if it ever fires. It has left the critical path entirely (see heap
  note below).
- **Terminal set**: `I.STOP`, `I.STOPM`, `?FATAL`, `DEF?ON`, and O.SET's
  `I?LINEID` branch → clone detaches, master completes
  (TerminalDetach.md). `?FATAL` + Tier 3 are permanently out of
  translation scope.
- **O.SET's `I?LINEID` branch = terminal — and STATICALLY DEAD**
  (Project 1 correction, verified against the .PR bytes): the walker's
  first instructions are `WLDAI 0` + `WSGT 0,0` — an immediate-zero
  gate making the branch to the empty-result path unconditional, so
  the I?LINEID call and the whole EEA4–EEE9 region are unreachable in
  this binary (explains the empirical I?LINEID ×0; an earlier
  "mid-walk comparison" description here was wrong). The native
  walker re-reads the gate wide each execution and falls back to
  emulation if it is ever positive. Given the names (`I?LINEID` → `?FIND_SCOPE` →
  `?FIND_LINEID_INDEX` → `?GET_LINEID_ENTRY` is the line-number machinery
  `?FATAL` uses for its traceback), the branch is *suspected* to be the
  no-handler → source-location → default-handling path. **Suspected, not
  proven.** Treated exactly like ?FATAL: native O.SET reaching it detaches
  the clone and lets the master emulate onward into the authentic
  no-handler death. If the master then *resumes* instead of dying, the
  detach makes that visible (master plays on, clone gone) rather than
  silently wrong — and the 908-word subtree becomes a real, motivated
  translation target (it is a pure pc→static-table lookup, no stack
  reads, so it survives de-stackification).
- **Heap: closed.** Quest barely allocates — the observed allocator use is
  ?CREATE_TASK building the C_A_LISTENER task's stack at startup (two
  allocs, one free), plus ?LIB_ERROR's message buffer. The ?LIB_ERROR
  pattern is free-then-alloc — verified in the disassembly at
  0x7017E371–E39C: load the saved buffer pointer `[area+0x3]`, skip
  `I.FREEW` when zero (first signal), `I.ALLOC` a new buffer, store it
  back. Freeing the newest block and immediately reallocating keeps the
  free list empty for the process lifetime, so the QSEARCH/free-list paths
  are structurally dead, not just unobserved. The existing translation
  (fast paths + fallback) is final; no further heap work.

## The minimal lift: 616 words — **TRANSLATED AND INTEGRATED (Aug 2026)**

All rows below are native, merged from the three parallel projects
(docs/Project1-3), chain-validated end-to-end on both triggers with 0
divergences. M3's remaining open item is the exit criterion below, not
translation work. Per-routine derivations: docs/ProjectN/DERIVATION.md.

Sizes are symbol-extent word counts from `Disassembled/quest.symbols`
(include any interior data holes). Addresses verified against the
disassembly this session.

| Routine | Addr | Words | Live evidence |
|---|---|---|---|
| `T?AREA` | 0x7017ED93 | 8 | ~9–12 calls per signal |
| `?LIB_ERROR` | 0x7017E33A | 152 | every provoked signal |
| `?LIB_ERROR_CODE` | 0x7017DE25 | 14 | 1 game caller (LOGON) |
| `?DEFAULT_ERROR_HANDLER` | 0x7017E3D2 | 30 | every provoked signal (installed at `[area+0x1E]`, called indirectly) |
| `O.SEARCH`→`O.SET` cluster | 0x7017EDDD–EF05 | 296 | O?SIGNAL/O.SET live per signal; contiguous: O.SEARCH, O.SIGNAL, O?SIGNAL, six `O.S*` shorthands, O.SERROR, O.SET + EE62/EE7A/EE9D interior walkers |
| `I.GOTO` | 0x7017EC7C | 80 | live per handled signal (26 game call sites — handlers GOTO out) |
| `I.PROLOG` | 0x7017E733 | 29 | every READ_IN bracket, 10+/session |
| `I.EPILOG` | 0x7017E77D | 7 | ditto |
| **Total (all native since Aug 2026)** | | **616** | |

Already native: `O.ON` (48), `O.REVERT` (18) — validated including a live
signal-path session (O.REVERT + re-O.ON under lockstep, 0 divergences).

Excluded, with reasons:

| What | Words | Why out |
|---|---|---|
| `DEF?ON`, `P?DEFON`, `R?SIGNAL`, `O?AREA` | 348 | SUPERSEDED: translated (Project 4) and lifted (Project 5) — verified L2, frontier moved to ?FATAL |
| `?FATAL` + traceback subtree (`I?PCS`, `I?LINE`, `?SNAP`, `P?SNAP`, ...) | ~1,900 | terminal (TerminalDetach.md) |
| `I?LINEID` subtree | 908 | contingent — only if the O.SET terminal branch ever proves resumable |
| `DERR.TRP` | 119 | terminal-with-ABORT entry (ruling 7): both engines converge, verified pair, hard stop — body never translated |
| `I.SFALT`, `I.FFALT`, `I.SFCON` | 267 | fault vectors — ruling SPLIT and revised (Layering.md ruling 6): FP faults throw → I.FFALT frozen; stack faults VECTOR FAITHFULLY (load-bearing startup overflow protocol) but the installed handlers are loader/game-side — I.SFALT never observed installed, frozen until it is |
| `I.WPROLO`, `I.DISPLA`, `O.SSUBSC` | 51 | zero callers anywhere (static scan, game + RT) |

## Corrections to earlier docs baked in above

- `?LIB_ERROR` entry is **0x7017E33A**, not 0x7017E2E0 (NextSession.md had
  a mid-routine pc).
- The FPU fault decoder and the `XCT` at 0x7017ECF4 are in **`I.FFALT`**
  (0x7017ECCC–ED1C), not I.GOTO — I.GOTO ends at 0x7017ECCC and contains
  **no XCT hazard**. (UNIMPLEMENTED.md §1, ERROR_LIFT_SCOPE.md Tier 1
  table, and NextSession.md all carried the wrong attribution.)
- `X.CB` (live, 153/session) calls `O.SCONVE`, so X.CB's own translation
  depends on the shorthand cluster being native first.

## Known design obstacles (from ERROR_LIFT_SCOPE.md, still real)

- **Indirect handler dispatch**: `?LIB_ERROR` (0x7017E3CD) and `O.SERROR`
  end in `XCALL 0,0,[ac2+0x0]` — a call through a handler address held in
  runtime data. A native translation must transfer control to an arbitrary
  emulated address and not return — same mechanism as the unwind. Design
  this once, use it for both.
- **Non-standard conventions**: I.PROLOG returns pc+7, O.ON pc+3,
  I.EPILOG/I.STOP never return. Derive each individually with captures;
  do not force the uniform bridge model (the Opus-era failure mode).
- **`I.GOTO` last**: it clears `rt_pending_return` at the unwind — the
  harness-gap fix — but everything upstream should be native first so the
  whole chain switches at once.

## Validation

Working triggers: `M`+direction+`abc` at the turns prompt (CONVERSION,
handled, same-turn, no injection — the cheapest; validated through
native I.GOTO, second distinct unwind shape), store "ABC" (CONVERSION,
handled, resumes) and
`QUEST_FAIL_OPEN=USER_DATA_FILE` + `L`→`P` (failed open, handled,
resumes; a second unhandled signal path exists behind it — see
UNIMPLEMENTED.md §9 status). With terminal-detach live, the unhandled
path now ends in a clean detach instead of a pair-gate mess, so both
trigger families are usable end-to-end.

**OPEN QUESTION (deliberately unanswered this session): the M3 exit
criterion for error-path provocation.** Options on the table: every
ON_ERROR_CATALOG handler (26 sites) fired once under lockstep; or a
representative set (the two working triggers + one per category); or
whatever play surfaces. ERROR_LIFT_SCOPE.md's fault-injection-suite
question (its Q3/Q4) folds into this. Decide before declaring M3 done —
not before starting the lift.

## Order of work (translation order itself: no strong preference)

1. `EagleSpecial::DERR` → throw (small, unblocks nothing but closes a
   decision).
2. `T?AREA` (8 words, trivial, fires constantly on signal paths) — warmup
   and the area-layout decoder everything else needs.
3. `I.PROLOG` / `I.EPILOG` (every-turn liveness via READ_IN).
4. `?LIB_ERROR` + `?LIB_ERROR_CODE` + `?DEFAULT_ERROR_HANDLER` (needs the
   indirect-dispatch mechanism; heap calls already native).
5. The `O.SEARCH`→`O.SET` cluster (one chunk; O.SET carries the terminal
   branch at the I?LINEID call).
6. `I.GOTO` (clears `rt_pending_return`; harness gap closed).
7. End-to-end validation per the (to-be-decided) provocation criterion.
