# What Is Not Implemented, and How It Fails

Master catalogue of emulator gaps, dead paths, and failure modes.
Written Aug 2026 after the queue-instruction work surfaced several
"never executed, so never noticed" cases. **Read this before planning a
translation**: a routine can look reachable and still contain
instructions or syscalls that would abort the machine on first use.

## Why this matters more than usual here

*Method and working principles: `docs/METHOD.md`.*


Lockstep verifies *clone == master*. Both engines share one
implementation, so a gap or a wrong implementation is **invisible to the
harness** — both sides are identically wrong. Only three things catch
these: an outright abort, external documentation, or reasoning about
paths the game has never taken. That is why unimplemented things are
recorded here rather than discovered later.

A useful property: an unimplemented opcode is a decoder entry with an
empty class and `oper = -1`, which builds a base `hw::Instruction`, and
`Instruction::execute` throws:

```
Instruction subclass must override execute <NAME>
```

So **"the game runs clean" is proof that no unimplemented instruction
has executed.** That is the strongest reachability evidence we have, and
several conclusions below rest on it.

---

## 1. Unimplemented instructions

195 of 404 decoder entries have no implementation. Most are 16-bit
Nova/Eclipse-mode instructions the program never uses (`JMP`, `LDA`,
`STA`, the `DIA`/`DOA` I/O family, ...). The ones that matter are the
ones sitting in code Quest actually contains:

| Instruction | Sites in Quest | Status |
|---|---|---|
| `QSEARCH*` | I.ALLOC 0x7017E8B7 (`0x001C`), free-insert 0x7017E9E7 (`0x001A`) | **hard terminal by decision (Aug 2026)** — the free-list paths are structurally dead (M3Plan.md heap note: Quest barely allocates; ?LIB_ERROR's free-then-alloc keeps the list empty), so QSEARCH left the critical path. Never guess at it; both engines throw the identical string if ever reached; fix only if it fires. |
| `XCT` | free-insert 0x7017E9F6, **I.FFALT** 0x7017ECF4 | **IMPLEMENTED Aug 2026** (`tools/xct_test.cpp`) |
| `ENQH` | free-insert (via `XCT`) | **implemented Aug 2026** — see QUEUE_INSTRUCTIONS.md |

### `XCT` — the one that is easy to miss

`XCT` executes an instruction held in a register. The free-list insert
builds an enqueue opcode as an immediate and executes it indirectly:

```
7017e9eb NLDAI 51193 (0xC7F9),3  ; = ENQT opcode
7017e9f0 NLDAI 51177 (0xC7E9),3  ; = ENQH opcode
7017e9f6 XCT                     ; execute whichever was selected
```

**Implementing `ENQH` did NOT unblock this path** — an earlier note in
HeapSignalPlan.md claimed the abort merely moved to the next
allocation's `QSEARCH`. That was wrong: `XCT` threw *first*, at
0x7017E9F6, before any enqueue happened. `XCT` has since been
implemented, so **`QSEARCH` is now the sole remaining blocker** on the
free-list path — and it sits before the enqueue/dequeue at both sites,
so nothing downstream can run until it exists.

`XCT` semantics (from the DG description, implemented in
`EagleGeneral`): execute the low 16 bits of the accumulator as an
instruction; multiword instructions fetch their extra words from the
words immediately FOLLOWING the XCT; continuation is address+1 for a
one-word instruction, address+2 for a two-word one, or the effective
address for a jump/skip. All three fall out of passing the XCT's own
`address` to the sub-instruction's `execute`.

`XCT` also appears at 0x7017ECF4 — **CORRECTION (Aug 2026): that address
is inside `I.FFALT` (0x7017ECCC–ED1C), not I.GOTO**, which ends at
0x7017ECCC and contains no XCT at all. Earlier notes (and NextSession.md)
attributed both the XCT and the FPU fault decoder to I.GOTO; both belong
to I.FFALT, the FP fault trap handler, which the emulator never vectors
into (§4) — so neither is a hazard for the signal-system lift.

Consequence for checklist step 0: grepping a routine for `ENQH`/`ENQT`/
`DEQUE`/`QSEARCH` is **not sufficient**. `XCT` can execute any
instruction at all, so its presence means the instruction set actually
used by a routine is not statically knowable from mnemonics. Also check
`WBR 2`-skipped holes for opcode-shaped constants — 0x7017E86B holds
`87A9` (`WRTN`), 0x7017E899 holds `C7C9` (`ISZTS`).

---

## 2. Unimplemented system calls

`OSContext::context_for_call` throws for any call it does not know:

```
Unimplemented system call <NNNN octal>
```

| Call | Meaning | Sites | Status |
|---|---|---|---|
| `0523` | wake a task waiting on a mailbox | I.UNLOCK 0x7017E800, MT?XMT 0x7017E19F | **not implemented at all** |
| `0251` `?RNGPR` | returns the .PR filename for a ring | `?FATAL` 0x7017F3F9 | **IMPLEMENTED** Aug 2026 |
| `0161` `?SCLOSE` | close a file opened for shared access | `?FATAL` 0x7017F66B | **IMPLEMENTED** Aug 2026 |
| `0311` `?ERMSG` | reads the error message file (ERMES) | `?FATAL` 0x7017F346 | **IMPLEMENTED** Aug 2026 |
| `0550` `?DFRSCH` | disable rescheduling, report prior state | `SWAT.REX` 0x7017E4BF / 0x7017E4E8 | **IMPLEMENTED** Aug 2026 |

**Inter-task messaging does not work.** `0523` is the wake half of the
mailbox pair; without it, nothing can ever satisfy a `REC` wait. That
makes the whole `MT?XMT`/`MT?REC` cluster non-functional, not merely
the heap-lock case. Consistent with CONSOLE_INTERRUPT.md's note that
the ctrl-A/ctrl-C task is deliberately unwired.

### `REC` (0525) — partially implemented, now fails loudly

Two outcomes work and are unchanged:
- a message already posted → returned in ac1
- a single-task process → falls through with ac1 = 0

The **blocking** outcome (mailbox empty, `count_tasks() > 1`) originally
polled in 3-second sleeps forever. Since it could never be satisfied
(see `0523` above), and since `REC` is classified **LOCAL** in
`LockstepMediator` — so master and clone each block while holding their
lockstep slot, stalling the pair gate — it now throws:

```
REC (0525) would block: inter-task wait is not supported - the wake path
SYSCALL 0523 is unimplemented. Reached from I.LOCK (heap-lock
contention) or MT?REC.
```

A hang gives no information; an abort names the situation and stops
both engines symmetrically.

### Blocking syscalls with NO guard (hazard for native callers)

`RTBridge::syscall` (Aug 2026) has **no blocking-call whitelist**, by
design: blocking is a property of a call *in a state*, not of a call —
`REC` blocks only on an empty mailbox with `count_tasks() > 1`, so a
static list would be wrong about `REC` in the common case and would give
false confidence about anything not yet examined. The guard belongs at
the point of blocking (§ `REC` above, METHOD.md §8).

But only `REC` has such a guard today. Three other calls block with
**no guard at all** — a native routine reaching one would sit in a sleep
loop while holding its lockstep slot, the exact failure mode the `REC`
abort removed:

| Call | Blocks how |
|---|---|
| `INTWT` (`OSContextSystem::INTWT_call`) | `while(!task->halt) sleep(100)` — never returns normally |
| `WDELAY` (`OSContextSystem::WDELAY_call`) | sleeps ac0 ms in 100 ms chunks — **live**: `?DELAY` fired 42× in a recent trace |
| `RETURN` (`OSContextSystem::RETURN_call`) | waits for sibling tasks to terminate |

Do not read "rely on the contexts" as "the contexts are all safe."
Before any native routine issues one of these calls, that call needs a
`REC`-style would-block guard first. None of this is in the way of
`MEMI`/`MEM` (the I.ALLOC extend path — non-blocking, plain register
arguments).

---

## 3. Implemented but not trusted

| Thing | Concern |
|---|---|
| `ENQT` / `DEQUE` | Rewritten Aug 2026 from the DG manual (link order was transposed; `DEQUE` never cleared the removed element's links). **Skip conventions are derived from `LOCK_FILE`, not documented.** See QUEUE_INSTRUCTIONS.md. |
| `ENQH` | Implemented from the manual; the insert-before-reference case has **no live caller**, so it is validated only against the manual's figures. |
| Queue skip semantics | Confirmed against one call site each. The reference pages would settle them. |

---

## 4. Emulator stricter than hardware (suspected)

The emulator turns several hardware *conditions* into C++ exceptions
that kill the machine. On real hardware these would trap through a
fault vector into the PL/1 condition system — the runtime has
`I.SFALT` (0x700001C0), `I.FFALT` (0x700001CA), `I.CFALT`
(0x700001CD) wired for that. IMPORTANT ASYMMETRY (Aug 2026, Layering
ruling 6): this strictness covers FP/arithmetic faults ONLY, and is
now a documented RULING with tripwires (I.FFALT frozen). **Stack
faults are NOT in this class**: `EagleStack::handle_overflow` vectors
faithfully, and the game's startup deliberately overflows its stack as
a sizing protocol — that path is load-bearing (we broke it once by
"simplifying"; don't).

| Throw | From |
|---|---|
| `Floating point underflow` / `overflow` | `EagleInstruction::validate_exponent`, `double_to_eclipse_wide_float` |
| `Floating point conversion overflow` | `WFFAD` |
| `Division by zero` | `FDS`/`FDD` |
| `Overflow occurred at %08X` | `Machine::run_steps` when `ovk && ovr` |
| `WMESS indirection` | `WMESS` with the indirect bit set |

**Observed live**: `Floating point underflow` kills the client on the
`LIST_PLAYERS` error path, inside `?FATAL` → `C?TRIM`. Whether the
emulator is too strict or the value is genuinely bad is **not yet
determined** — see HeapSignalPlan.md Step 3. Do not assume the former.

---

## 5. Deliberately unwired features

| Feature | Note |
|---|---|
| ctrl-A / ctrl-C move abort | `?CREATE_TASK` / `?AWAIT_CONSOLE_INTERRUPT` contracts preserved, not wired. Auto-move runs to completion. See CONSOLE_INTERRUPT.md. |
| Inter-task messaging | Non-functional; see §2. |
| Multi-client sessions | `QUEST_SERVER` supports it; not exercised. `LOCK_FILE`'s queue and the heap lock only become contended here. |

---

## 6. Paths proven never to have executed

Established by the "unimplemented instruction would have aborted"
argument plus coverage bitmaps:

| Path | Evidence |
|---|---|
| Heap free-list search (I.ALLOC) | Coverage stops at 0x7017E8B4, the free-list-empty branch |
| Heap free-list insert | 0x7017E9DD–0x7017E9F6 zero coverage; `XCT` and `QSEARCH` never threw |
| I.LOCK contended / I.UNLOCK wake | `REC` never called; `0523` never threw |
| `MT?REC` / `MT?XMT` | zero coverage |
| `I.GOTO`'s `XCT` (0x7017ECF4) | zero coverage (now implemented, so no longer a hazard) |
| Error-reporting cluster (`?SNAP`, `?FATAL`) | reached once, under deliberate fault injection — and crashed |

**Allocation never enqueues; only freeing does**, and only for a block
that does not abut the heap break (otherwise 0x7017E9C8 just retracts
it). Freeing the most recent allocation is the common case, which is
why the free list has never held an entry.

---

## 7. Harness limitations

| Limitation | Detail |
|---|---|
| Non-local exit vs run-to-return | `Machine::run_steps` arms `rt_pending_return = ac[3]` on entering a translated routine and runs until pc matches. A routine exiting via signal never reaches it → 10M runaway guard or mangled batch structure. CLOSED Aug 2026: run-to-return escapes at terminal entries AND on RT-range exit (transfer pairing, SharedProtocol.md); DERR vectors to DERR.TRP = terminal-ABORT (Layering ruling 7); the signal unwind pairs via the range-exit rule. No remaining gap. |
| `compare_pair` ignores FP state | Compares `ac[0..3]` and carry only. `fpac[0..3]`, `fplr`, `quads[]`, `fpr` are unchecked — a wrong `SQR31?3` would pass silently. Must be fixed before that translation (docs/SQR31.md). |
| `Capture` second window | Was entry-ac2 only; routines whose footprint is at a static address need `QUEST_CAPTURE_DEST=<hex>` (added for I.LOCK, whose footprint is the lock object at 0x70000200). |
| Shutdown after mid-read disconnect | Historically hung the pair gate ("Forced exit" path). Likely FIXED by terminal detach (Aug 2026): every exit now detaches at I.STOP and shutdown is never paired — clean-exit path validated; the mid-read disconnect case not yet specifically re-tested. |
| `StartStop` misclassifies live code | Several `mem` ranges are executable. All four holes in the heap region are code. Disassemble suspicious ranges explicitly — note the addrs file is read from the **host** cwd, not the emulated filesystem. |

---

## 8. Benign noise (do not chase)

| Message | Note |
|---|---|
| `Exception: Segment fault - block 0, page 1 not loaded` | **Re-attributed Aug 2026**: NOT startup — it is the server's `IPC_TASK` dying inside `UPDATE_USER_DATA_FILE`'s ?WRITE at **first login**, every session (backtrace: QUEST_SERVER → IPC_TASK+0xB9A → UPDATE_USER_DATA_FILE+0x77 → .UKIL). The multi-task server survives; still benign in effect, but a real server task dies writing the user record — root cause never investigated. |
| `Exception: INTWT interrupted` / `Exception: EXIT!` | normal shutdown |
| `call return address; X, stack return address: Y` | shadow call-stack notices during a non-local unwind |

---

## 9. ?FATAL now runs to completion (Aug 2026)

Five fixes — the float load shadow plus `?RNGPR`, `?SCLOSE`, `?ERMSG`,
`?DFRSCH` — take `?FATAL` from dying in `C?TRIM` to printing a full PL/1
call traceback. See **CHANGE_FLOAT_SHADOW.md** and
**CHANGE_FATAL_SYSCALLS.md**.

Two residual unknowns, both harmless today:
- `?SCLOSE`'s channel decodes to -2 in the observed run, so it takes its
  `ERFNO` return rather than closing anything. Either a chain terminator
  or an off-by-one in the DG bits-2-31 reading.
- `SWAT.REX` stores 7 at 0x700001B2 (the "rescheduling was enabled"
  branch). Correct per the `?DFRSCH` doc, but the meaning of that flag
  is not established.

**Error mnemonics live in `os/AOSVSSymbols.cpp`** — `ERRNL`, `ERIRB`,
`ERFNO`, `ERTXT`, `?DSCH` and the rest. Look them up with
`OSContext::aos_symbol()`; do not invent placeholder codes.

## Quick reference: how each failure looks

| Failure | Message |
|---|---|
| Unimplemented instruction | `Instruction subclass must override execute <NAME>` |
| Unimplemented syscall | `Unimplemented system call <octal>` |
| Blocking inter-task wait | `REC (0525) would block: ...` |
| Overflow trap | `Overflow occurred at <pc>` |
| Lockstep mismatch | `LOCKSTEP DIVERGENCE` on stdout, one-line notice on stderr |

All of these print a backtrace via `OSTask`'s handler (stdout) with the
exception line on stderr — run with `stdbuf -o0 -e0` to keep them
ordered.
