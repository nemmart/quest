# Plan — Heap + Signal System Lift (agreed Aug 2026)

> **STATUS: HISTORICAL (superseded Aug 2026).** Step 1 (heap) is DONE and
> closed — see I_ALLOC.md and M3Plan.md's heap note. Step 2 is superseded
> by **M3Plan.md** (the current plan). Step 3's underflow was RESOLVED in a
> parallel session: the C?TRIM-path code used FP registers as a 64-bit
> copy device and the emulator's float shadow broke that — fixed
> (CHANGE_FLOAT_SHADOW.md); neither hypothesis (a) nor (b) below was the
> cause as framed. The QSEARCH matched-abort design below is DEAD —
> QSEARCH is a hard terminal now (TerminalDetach.md). Kept for the
> derivation record and the syscall-bridge design rationale, which remain
> valid.

Supersedes the "SQR31?3 next" ordering in NextSession.md. SQR31?3 and
?RANDOM_NUMBER + D.MOD are **deferred**; the SQR31?3 derivation is
complete and preserved in `docs/SQR31.md`.

## Why the order changed

Chasing SQR31?3's negative-input path surfaced a harness gap that is
not SQR31-specific:

- `Machine::run_steps` arms `rt_pending_return = ac[3]` when the master
  enters any translated routine, then runs until the pc equals it. A
  routine that exits **non-locally** never reaches that pc, so the
  master spins to the 10M runaway guard (or silently "recovers" later
  with a mangled batch structure).
- Non-local exit has two entry points, both routing into the condition
  system: an explicit **signal** (`?LIB_ERROR` → `O?SIGNAL` → `I.GOTO`)
  and the **DERR** trap (`EagleSpecial::DERR` pushes the fault address
  and jumps through the trap vector at word 39 — it does *not* throw).
  `SYSCALL 0310` also never returns, but is benign: the sentinel exits
  `run_steps` before the pending-return check and the machine is gone.
- **Failing syscalls signal.** ERROR_PROCESSING.md §2: every library
  I/O wrapper calls `?LIB_ERROR` on non-EOF failure — ?READ, ?WRITE,
  ?WRITE_SCREEN, ?READ_SCREEN, ?OPEN_FILE, ?CLOSE_FILE,
  ?OPEN_SHARED_IO_FILE, ?GET_SHARED_PAGE, ?LOOKUP_PORT, ?CONNECT. That
  is clusters 2–4 of the inventory, i.e. most of the remaining work.

So per-routine entry guards ("can this call signal?") do not scale, and
for a syscall failure the answer is not knowable at entry anyway. With
`I.GOTO` native, the unwind stops being something the master must
*infer* and becomes an event we raise: `rt_pending_return` is cleared
explicitly by the code doing the unwinding.

## Sequence

### Step 1 — I.ALLOC / I.FREE (this step)

**Progress: I.LOCK and I.UNLOCK are DONE** (Aug 2026). Uncontended
paths translated in `runtime/i_lock.{hpp,cpp}`, registered, validated:
3 native pairs, footprint diff **0 differing words** over 110 words per
pair, lockstep session clean. Contended paths are entry-detected and
fall back to emulation, and the emulated contended path now aborts
loudly on both engines (see UNIMPLEMENTED.md §2). Chosen first because
their live path contains no syscall, so they land before
`RTBridge::syscall` exists.

Correction to an earlier note in this plan: `RTBridge::emulate_frame()`
needed **no** `ovk` parameter. The `WSAVS`/`WSAVR` image holds ac0, ac1,
ac2, wfp, ac3|carry and no psr; `ovk` is psr bit 15 and `native_return`
restores psr from the LCALL frame word, so the R/S distinction is
invisible after return. Confirmed by capture.

Required before ?LIB_ERROR, not merely convenient: a native wrapper must
not re-enter emulation mid-routine, and ?LIB_ERROR calls `I.FREEW`
(0x7017E37E) and `I.ALLOC` (0x7017E399) on its message-buffer path.

Translate as **one chunk** — they share the heap-break words
(0x700001F0/0x700001F2), the free-list descriptor (0x700001F4), and the
coalescing helper at 0x7017E97C.

Subtree inventory (nothing may re-enter emulation):

| Address | Symbol | Notes |
|---|---|---|
| 0x7017E866 | I.ALLOC | entry; size rounding, free-list search, OS extend |
| 0x7017E945/49/4C | I.FREEB / I.FREEW / I.FREE | shared body from 0x7017E950 |
| 0x7017E97C–9B3 | (unnamed) | **coalescing; classified `mem`, must be disassembled explicitly** |
| 0x7017E9B3 | (unnamed) | free-list insert; QSEARCH + XCT ENQH/ENQT |
| 0x7017E9F9 / 0x7017EA01 | (unnamed) | I.LOCK / I.UNLOCK wrappers |
| 0x7017E9B3←0x7017E92D | (unnamed) | heap-limit update |
| 0x7017E970 | (unnamed) | heap ownership check → I?INHPW |
| 0x7017E820 | I?INHPW | `WCLM` range check |
| 0x7017E7D0 | I.LOCK | **SYSCALL 0525** on contention |
| 0x7017E7ED | I.UNLOCK | `WMESS` + **SYSCALL 0523** on wake |
| 0x7017E805 | I?HPOWNER | trivial |
| 0x7017E9F9→0x7017E959 | O.SERROR paths | heap-corruption signals |

**Native routines issuing syscalls is a REQUIREMENT, not an edge case.**
`?OPEN_FILE`, `?READ`, `?WRITE`, `?WRITE_SCREEN`, `?GET_SHARED_PAGE`
etc. are thin wrappers around a syscall — Step 1 of Plan.md cannot
finish without this. I.LOCK/I.UNLOCK are therefore the **proving case**
for the bridge, chosen deliberately: two calls, arguments straight from
registers, no parameter packet (`?OPEN_FILE` marshals seven packet
fields; deriving the bridge against that would be two hard things at
once).

`I.ALLOC`/`I.FREE` wrap their work in I.LOCK/I.UNLOCK (0x7017E9F9 /
0x7017EA01):

```
7017e7d9 WSZBO 2,1        ; I.LOCK: try to take the lock bit
7017e7da WBR   -> 7017E7E2
7017e7e3 SYSCALL 0525     ;   contended: REC, block on a mailbox
...
7017e7f8 WMESS;           ; I.UNLOCK: atomic clear
7017e7f9 WBR   2
7017e800 SYSCALL 0523     ;   someone waiting: wake them
```

### Native syscall bridge — design

`OSTask::dispatch_system_call` does five things; only two belong to a
native caller:

| Step | Native? |
|---|---|
| 1. read call number from `[wsp-2]` | **no** — emulated-trap plumbing |
| 2. `OSContext::context_for_call(...)` + copy ac0-2 | **yes** |
| 3. `LockstepMediator::applies() ? dispatch() : context->dispatch_system_call()` | **yes** |
| 4. pop the psr wide the trap pushed | **no** — nothing pushed it |
| 5. write back ac0-2, return `ret+1` / `ret` (skip-return) | **no** — return the error instead |

Proposed:

```cpp
// error code; ac0-2 updated in place on success
int32_t RTBridge::syscall(int32_t call, int32_t& ac0, int32_t& ac1, int32_t& ac2);
```

**The mediator is pc-agnostic — verified.** `LockstepMediator::dispatch`
keys on `machine->lockstep_role`, `machine->lockstep_ordinal`, the call
number, and the context's ac0-2. It never reads pc, wsp, or any
emulated-trap state, so a native caller can route through it unchanged.
This matters more than the syscall itself: the mediator is how master
and clone rendezvous and how clone output is compared before reaching a
terminal. **A native syscall that bypasses it punches a hole in exactly
the verification this project depends on.** `applies()` only needs
`task->machine`, and `Machine` already holds `OSTask* task`, so the
wiring is available.

**Blocking calls need separate treatment.** `?OPEN_FILE`'s syscall
returns promptly; `REC` (0525) parks the task (its implementation polls
with sleep). Blocking inside a native routine while the lockstep slot
protocol expects an arrival is a distinct problem from the
non-blocking case. Plan: non-blocking first, blocking deferred.

**The contended path already aborts today — on both engines.**
`SYSCALL 0523` is **not implemented**: no constant, no case, absent
from `OSContext::context_for_call`, which throws
`"Unimplemented system call 0523"`. So the I.UNLOCK wake path has never
executed, confirming no heap-lock contention has ever occurred with a
single client. Aborting there natively preserves current behaviour
rather than degrading it — but the abort string must match (see
QSEARCH note below).

Recommendation: build `RTBridge::syscall` for the non-blocking case as
part of this step; keep I.LOCK's contended branch and I.UNLOCK's wake
branch as matched aborts until multi-client support forces the issue.

**QSEARCH fallthrough → hard abort.** Native I.ALLOC/I.FREE must abort
where the emulated body would execute `QSEARCH` (0x7017E8B7,
0x7017E9E7). To keep lockstep meaningful the native abort must throw
the **exact string** the master produces, since `compare_pair` compares
exceptions with `strcmp`:

```
Instruction subclass must override execute QSEARCH*
```

(Alternative: relax `compare_pair` to treat "both threw" as agreement.
Prefer string matching — it keeps the divergence report honest.)

**CORRECTION (Aug 2026): the abort point has NOT moved.** An earlier
version of this plan claimed that implementing `ENQH` let the first
non-top free succeed, moving the abort to the next allocation's
`QSEARCH`. That is wrong: **`XCT` (0x7017E9F6) is itself unimplemented**,
so the free-list insert aborts there, before any enqueue runs. The
insert path needs `XCT` *and* `QSEARCH`. `ENQH` being implemented
changes nothing about reachability — it only means the instruction is
correct if the path is ever unblocked. See UNIMPLEMENTED.md §1.

**Hazards specific to this chunk:**
- `XCT` at 0x7017E9F6 dispatches `ENQH`/`ENQT` from a constructed
  opcode; grep for mnemonics does not find it (see
  QUEUE_INSTRUCTIONS.md, checklist correction).
- 0x7017E97C–0x7017E9B3 is classified `mem` by StartStop — disassemble
  explicitly with a hand-written addrs file.
- Several `WBR 2`-skipped holes hold opcode-shaped constants
  (0x7017E86B `87A9` = WRTN, 0x7017E899 `C7C9` = ISZTS,
  0x7017E85C block). Verify each before assuming data.
- `LDSP` jump table at 0x7017E875 (valid range [1,4], targets
  0x7017E881/886/88A/88C) — allocation-class dispatch.

### Step 2 — signal system + ?LIB_ERROR

Dispatch core: `I.PROLOG`, `I.EPILOG`, `O?SIGNAL`, `O.SET`, the EE62
select loop, the EE9D deep walker, `I.GOTO`, `O.SEARCH`, the `O.S*`
shorthands, `DERR.TRP`, `T?AREA`, `DEF?ON`. `O.ON`/`O.REVERT` are
already native (docs/O_ON.md). ~750 words total.

With `I.GOTO` native, add explicit clearing of
`Machine::rt_pending_return` at the unwind, closing the harness gap.

`?LIB_ERROR` becomes translatable **only after Step 1**.

### Step 3 — underflow

`Exception: Floating point underflow` kills the client on the
`LIST_PLAYERS` error path. Backtrace:

```
frame  3 -- LIST_PLAYERS+0xFC        <- injected open failure
frame  4 -- ?OPEN_FILE+0x87
frame  5 -- ?LIB_ERROR+0x93          <- first signal, handled
frame  6 -- START_TURN+0x7DE         <- unwound to the handler
frame  7 -- REFRESH_SCREEN+0x10
frame  8 -- ?WRITE_SCREEN+0x88
frame  9 -- ?LIB_ERROR+0x93          <- SECOND signal
frame 10 -- ?DEFAULT_ERROR_HANDLER+0x19
frame 11 -- O.SERROR+0xA
frame 12 -- DEF?ON+0x46
frame 13 -- ?FATAL+0x16B
frame 14 -- C?TRIM+0x23              <- underflow thrown here
```

Do **not** assume the emulator is merely too strict. Two hypotheses:
  a. `validate_exponent` throws on exponent < -64 where the hardware
     would set a status bit or trap through `I.FFALT` (vector at
     0x700001CA, which the runtime has wired). Emulator too strict.
  b. The first unwind leaves state inconsistent, the second
     `?WRITE_SCREEN` fails *because of that*, and the underflow is a
     downstream symptom. Then the real bug is in the unwind.
Pin the cause before proposing a fix. Note this is the error-*reporting*
cluster (inventory group 9: ?SNAP/?FATAL/line-number machinery), never
reached before in this project.

### Step 4 — retest

`QUEST_FAIL_OPEN=USER_DATA_FILE`, login, `L` → `P` repeatedly. Success:
repeated failing opens, each handled and resumed, no crash, and either
no queue-search abort or a clean matched abort on both engines.

## Reproduction assets (working, this session)

- **Fault injection**: `os/OSContextFS.cpp`, `QUEST_FAIL_OPEN="<substr>"`.
  Client-only (`instance_label` starts with `QUEST`, excluding
  `QUEST_SERVER` — note "QUEST" is a prefix of "QUEST_SERVER").
- **Telnet driver**: connect 8781, `CL` / `Claude` / `quest` / `Y` /
  any key / `F`, then `L` `P` per iteration, `ESC` to exit. Background
  the emulator and run the driver in the *same* shell invocation.
- **Unbuffered output** (`stdbuf -o0 -e0`) is required to capture the
  backtrace, which goes to stdout while the exception line goes to
  stderr.
- Always scratch-copy `QUEST/` first — data files write back at
  shutdown.

## Deferred

- SQR31?3 (derivation complete, `docs/SQR31.md`) — also needs
  FP-register comparison added to `Lockstep::compare_pair` and
  `debug/Capture`, which currently cover only ac0-3 and carry.
- ?RANDOM_NUMBER + D.MOD.
- `QSEARCH` — blocked on the per-instruction reference pages.
- `SessionPlan.md` / `RTWorklist.md` updates: the `XCT` checklist
  correction, `LOCK_FILE` being live every turn, and the queue rewrite.
