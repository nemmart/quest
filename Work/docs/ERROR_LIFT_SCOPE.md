# Scoping: What "Lift the Error Handler" Actually Means

The medium-term goal has been stated as "lift the whole error handling
system." That is vague enough to hide both the real cost and the real
blockers. This document makes it concrete: what is in scope, what is
explicitly out, what the completion criterion is, and what stands in the
way.

Status: **ANSWERED (Aug 2026) — superseded by M3Plan.md**, which carries
the agreed scope. Resolution of the open questions at the end:
Q0 → Tier 1 + Tier 2 (both in the 616-word minimal lift; the heap came
first and is done/closed). Q1 → yes, Tier 3 is out — stronger than out
of scope: ?FATAL/DEF?ON are TERMINAL entries now (TerminalDetach.md,
implemented). Q2 → definition-of-done broadly stands, except I.GOTO's
rt_pending_return-clearing plus the provocation criterion, which remains
the ONE OPEN QUESTION (see M3Plan.md Validation). Q3/Q4 → folded into
that open question; terminal detach removes the ?FATAL-crash obstacle to
fault-injected testing. The tier tables and blocker analysis below
remain accurate and useful, with one correction: the I.GOTO row's
"FPU fault decoder + XCT" belongs to I.FFALT, not I.GOTO.

## Why it is worth doing at all

Not because the cluster is next in some list. Two independent reasons:

1. **It is the fix for a harness gap.** `Machine::run_steps` arms
   `rt_pending_return = ac[3]` when the master enters a translated
   routine and runs until pc matches it. A routine that exits
   **non-locally** never reaches that pc — the master spins to the 10M
   runaway guard, or silently "recovers" later with a mangled batch
   structure. Two things exit non-locally, and both route through this
   cluster: an explicit signal, and the `DERR` trap (which does not
   throw — `EagleSpecial::DERR` pushes the fault address and jumps
   through the trap vector at word 39). With `I.GOTO` native, the
   unwind stops being something the master must infer and becomes an
   event we raise: `rt_pending_return` is cleared explicitly by the
   code doing the unwinding.
2. **It gates most of the remaining runtime.** Failing syscalls signal
   (ERROR_PROCESSING.md §2): `?READ`, `?WRITE`, `?WRITE_SCREEN`,
   `?READ_SCREEN`, `?OPEN_FILE`, `?CLOSE_FILE`, `?OPEN_SHARED_IO_FILE`,
   `?GET_SHARED_PAGE`, `?LOOKUP_PORT`, `?CONNECT` all call
   `?LIB_ERROR` on non-EOF failure. Every one of those has a
   non-returning path. Translating them before the unwind is handled
   means translating routines whose error path breaks the harness.

## The heap cluster is a prerequisite, and it is not small

An earlier draft of this document mentioned `I.ALLOC`/`I.FREEW` only as
a one-line blocker. That badly understated it. The heap and task-support
cluster is **1,084 words** — larger than the Tier 1 dispatch core it is
supposed to be a prerequisite for.

| Addr | Routine | Words | Reached? | Status |
|---|---|---|---|---|
| 0x7017E7D0 | `I.LOCK` | 29 | yes | **DONE** |
| 0x7017E7ED | `I.UNLOCK` | 24 | yes | **DONE** |
| 0x7017E866 | `I.ALLOC` | 223 | yes | **needs `QSEARCH`** for the free-list path |
| 0x7017E94C | `I.FREE` (+`FREEB`/`FREEW`) | 217 | yes (`FREEW`) | **needs `QSEARCH`**; includes the coalescer |
| 0x7017EA7A | `I.GINIT` | 237 | yes | heap init |
| 0x7017EB67 | `I.NOTICE` | 65 | cold | |
| 0x7017E786 | `T.INIT` (+`T?INITN`/`T?KILL`) | 76 | cold | task-area setup |
| 0x7017EA2E | `I.INIT` | 48 | cold | |
| 0x7017E838 | `I?INHPWS` | 27 | cold | |
| 0x7017EA5E | `?UKIL` | 26 | cold | |
| 0x7017EBA8 | `I.FPU` | 24 | yes | |
| 0x7017E820 | `I?INHPW` | 20 | yes | heap-range check, used by `I.FREE` |
| 0x7017E853 | `I?SALLOC` | 19 | cold | |
| 0x7017EA1E | `I.TOFREE` | 16 | cold | |
| 0x7017E805 | `I?HPOWNER` | 12 | yes | |
| 0x7017E811 | `I?ASIZE` | 11 | cold | |
| 0x7017E81C/834/EA78 | `I?INHPB`, `I?INHPBS`, `?STACK_OVERHEAD` | 10 | cold | trivial |

Better validation profile than Tier 1: **7 of these are reached in a
clean session**, via `?CREATE_TASK` at startup. No fault injection
needed.

### ...and the dependency is CIRCULAR

The heap cluster is not simply "below" the condition system. It signals
into it:

```
7017e86d LCALL [0x7017EE33],0; # O.SERROR   <- I.ALLOC, allocation failure
7017e95c LCALL [0x7017EE33],0; # O.SERROR   <- I.FREE, heap corruption
7017e965 LCALL [0x7017EE33],0; # O.SERROR   <- I.FREE, bad free
7017e96c LCALL [0x7017EE33],0; # O.SERROR   <- I.FREE, not owner
```

while `?LIB_ERROR` (Tier 2) calls `I.ALLOC`/`I.FREEW` for its message
buffer. So:

- `I.ALLOC` / `I.FREE`  --signal-->  `O.SERROR` (Tier 1)
- `?LIB_ERROR` (Tier 2) --allocate-->  `I.ALLOC`

**Neither cluster can be translated fully before the other.** The
subtree rule ("no re-entering emulation mid-routine") cannot be
satisfied by ordering alone. Options at the boundary:

  a. Translate the heap's error paths as **entry-guarded fallbacks**
     (the `I.LOCK` pattern) — native on the success path, fall back to
     emulation when about to signal. Cheap, and those paths are cold.
  b. Translate both clusters in one pass — ~1,560 words, no partial
     validation.
  c. Accept a documented one-way re-entry at the `O.SERROR` boundary —
     violates the subtree rule; not recommended.

(a) is the natural extension of what already works, and the heap's four
signal sites are all "the heap is corrupt" paths that have never run.

## The inventory

Total ~810 words for Tiers 1+2, ~2,700 including `?FATAL`, and a
further **1,084 words of heap cluster** if Tier 2 goes native (above).
For scale: `O.ON` + `O.REVERT`, already done, were 66 words, and
`I.LOCK` + `I.UNLOCK` were 53.

### Tier 1 — dispatch core (the actual goal, ~480 words)

Registration, search, signal raise, and unwind. Everything else is
either an entry point into this or a consumer of it.

| Addr | Routine | Words | Reached? | Notes |
|---|---|---|---|---|
| 0x7017ED9B | `O.ON` | 48 | yes | **DONE** (native) |
| 0x7017EDCB | `O.REVERT` | 18 | yes | **DONE** (native) |
| 0x7017ED93 | `T?AREA` | 8 | cold | task-area accessor; called by nearly everything |
| 0x7017EDDD | `O.SEARCH` | 10 | cold | `XPSHJ` 0x7017EE62 (select loop) |
| 0x7017EDE7 | `O.SIGNAL` | 6 | cold | leaf |
| 0x7017EDED | `O?SIGNAL` | 21 | cold | leaf; the raise entry |
| 0x7017EE07–2D | `O.S*` shorthands | 44 | cold | 6 routines, 6–8 words each, fixed condition codes |
| 0x7017EE33 | `O.SERROR` | 35 | cold | calls `O.SET`, select loop, **indirect `XCALL [ac2+0]`**, `I.STOP` |
| 0x7017EE56 | `O.SET` | 175 | cold | select loop 0x7017EE7A, deep walker 0x7017EE9D, `I?LINEID` |
| 0x7017EC7B | `R.GOTO` | 1 | cold | |
| 0x7017EC7C | `I.GOTO` | 80 | cold | **the unwind** (CORRECTED Aug 2026: the FPU fault decoder and `XCT` are in I.FFALT at 0x7017ECCC+, not here — I.GOTO has no XCT hazard) |
| 0x7017E733 | `I.PROLOG` | 29 | **yes** | frame entry |
| 0x7017E750 | `I.WPROLO` | 22 | cold | |
| 0x7017E766 | `I.DISPLA` | 23 | cold | |
| 0x7017E77D | `I.EPILOG` | 7 | **yes** | frame exit |

### Tier 2 — signal sources (~330 words)

Entry points that raise into Tier 1. Each can be left emulated during
the transition (the O.ON precedent: emulated caller, native callee).

| Addr | Routine | Words | Reached? | Blocker |
|---|---|---|---|---|
| 0x7017E33A | `?LIB_ERROR` | 152 | cold | **calls `I.ALLOC`/`I.FREEW`** |
| 0x7017DE25 | `?LIB_ERROR_CODE` | 14 | cold | |
| 0x7017E3D2 | `?DEFAULT_ERROR_HANDLER` | 30 | cold | |
| 0x7017ED1C | `DERR.TRP` | 119 | cold | trap-vector entry, not called |
| 0x7017EBC0 | `I.SFALT` | 121 | cold | stack fault vector (0x700001C0) |
| 0x7017ECCC | `I.FFALT` | 80 | cold | FP fault vector (0x700001CA) |
| 0x7017EC39 | `I.SFCON` | 66 | cold | |
| 0x7017EF05 | `DEF?ON` | 76 | cold | calls `O?AREA`, `P?DEFON`, `?FATAL` |

### Tier 3 — error reporting (~1,890 words) — RECOMMEND OUT OF SCOPE

| Addr | Routine | Words | Reached? |
|---|---|---|---|
| 0x7017EF54 | `R?SIGNAL` / `?ERROR` | 220 | cold |
| 0x7017F030 | `?SNAP` | 6 | cold |
| 0x7017F036 | `?FATAL` | **1662** | cold |

`?FATAL` alone is **2.5x the entire dispatch core**. It formats a
diagnostic report — line numbers, symbol names, register dumps — and it
runs only when a condition reaches no handler at all, i.e. when the
process is dying anyway. It is also where the observed
`Floating point underflow` crash lands (`?FATAL` -> `C?TRIM`).

Translating a 1,662-word formatter that only runs on the way to process
death is poor value against every other candidate. **Recommend
explicitly excluding Tier 3** and saying so, rather than leaving it
implied by "the whole error handler."

## Proposed definition of done

> **The error-handler lift is complete when every routine in Tier 1 is
> native, `I.GOTO` clears `rt_pending_return` explicitly at the unwind,
> and a fault-injected session drives at least one signal end-to-end —
> raise, search, handler dispatch, unwind, resume — with a zero-word
> footprint diff and no lockstep divergence.**

Tier 2 stays emulated (raising into a native core, the inverse of
today's O.ON arrangement, which is already proven). Tier 3 is out of
scope; revisit only if `?FATAL` becomes reachable in normal play.

That is a concrete, testable finish line. "Lift the whole error handler"
is not.

## Blockers, in dependency order

1. **`?LIB_ERROR` needs the heap cluster** (0x7017E37E `I.FREEW`,
   0x7017E399 `I.ALLOC`). The agreed short-term task, and ~1,084 words
   in its own right — see above. It blocks **Tier 2 only**: the Tier 1
   core is allocator-free (verified — the allocator's only call sites
   in the whole runtime are 0x7017DBCE/0x7017DBFC/0x7017DC3A in
   `?CREATE_TASK`, plus these two).
1b. **`I.ALLOC` / `I.FREE` need `QSEARCH`**, still unimplemented and
   still not to be guessed at. Both free-list paths are provably cold,
   so a matched abort is acceptable — but it means the heap cluster
   can only ever be *partially* translated until the reference pages
   turn up.
2. **Everything in Tier 1 except `O.ON`/`O.REVERT`/`I.PROLOG`/
   `I.EPILOG` is COLD.** This is the biggest practical problem — see
   below.
3. **Indirect dispatch in `O.SERROR`** (`XCALL 0,0,[ac2+0x0]`) and in
   `?LIB_ERROR` (0x7017E3CD, same form). The target is a handler
   address from the chain, i.e. runtime data. A native translation must
   transfer to an arbitrary emulated address — the same "native code
   hands control to emulated code and does not return" pattern as the
   unwind, so it needs the same mechanism.
4. **`XCT` in `I.GOTO`** (0x7017ECF4) — no longer a blocker, `XCT` was
   implemented. It builds `FSS n,n` from the FPU status word to zero
   the faulting accumulator; a native version can zero `fpac[n]`
   directly.

## The real problem: validation

This is what makes the cluster different from every previous target,
and it deserves to be the deciding factor in scoping.

Of the 15 Tier-1 routines, **4 are reached in normal play** — `O.ON`,
`O.REVERT`, `I.PROLOG`, `I.EPILOG`. The other 11, including the entire
raise/search/unwind path, are cold in a clean session. METHOD.md §9
says to choose targets by validation path; on that criterion most of
this cluster fails outright.

Everything therefore depends on **fault injection**. We have one working
trigger: `QUEST_FAIL_OPEN=USER_DATA_FILE` plus `L` -> `P` reaches
`LIST_PLAYERS`' handler (ON_ERROR_CATALOG #13) and drives a real signal
through the chain. That is one path, and it currently **crashes** in
`?FATAL` on a nested second signal.

So before committing to this project, the honest prerequisite is:

> **Build a fault-injection suite that reaches a decent fraction of
> Tier 1 by distinct routes, and confirm what each one exercises.**

ON_ERROR_CATALOG documents 26 handler sites. If most can be provoked,
the cluster is validatable and the project is sound. If only two or
three can, then translating 11 cold routines means shipping code we
cannot exercise — and lockstep cannot help, because it only compares
paths that actually run.

Candidate triggers to assess, beyond the working one:
- other `?OPEN_FILE` failures (ALLY_PLAYER, KILL_PLAYER, DISPLAY_MAP)
- `?READ` / `?WRITE` failures via the same injection hook
- a `DERR` bounds-check failure (the ASSERT patterns the block
  disassembler absorbs inline) — reaches `DERR.TRP`, an entirely
  different entry
- `I.FFALT` via a genuine FP underflow — relevant to the open Step 3
  question
- deliberate stack overflow -> `I.SFALT`

## Open questions

0. **INCONSISTENCY TO RESOLVE FIRST.** The definition of done above
   leaves Tier 2 emulated — under which the heap cluster is **not** on
   the critical path for the error lift at all, since emulated
   `?LIB_ERROR` calls emulated `I.ALLOC` quite happily. But the agreed
   short-term task is "`I.ALLOC`, `I.FREEW`, then `?LIB_ERROR`", which
   implies `?LIB_ERROR` **is** meant to go native, i.e. Tier 2 is in
   scope. Both are defensible; they are not the same project:
     - **Tier 1 only** (~480w): fixes the harness gap, needs no heap
       work, but leaves every signal *source* emulated.
     - **Tier 1 + Tier 2** (~810w + 1,084w heap = ~1,900w): a genuinely
       lifted error system, and the heap comes with it whether or not
       it is thought of as part of "the error handler".
   The second is roughly four times the first. Which did you mean?

1. **Is Tier 3 (`?FATAL`, 1,662 words) agreed out of scope?** I
   recommend yes, explicitly.
2. **Does the definition of done above match what you meant?** In
   particular: Tier 2 staying emulated, and one end-to-end signal being
   the acceptance test rather than coverage of all 26 handler sites.
3. **Should the fault-injection suite be a prerequisite project of its
   own**, before any Tier 1 translation? It is a few days of work that
   produces no translated routines — but without it, most of this
   cluster is untestable.
4. **What is the fallback if a Tier 1 routine turns out unreachable by
   any trigger?** Options: translate it anyway and mark it
   unvalidated (against METHOD.md §6); leave it emulated and accept a
   partial lift; or defer the whole cluster until a trigger is found.
