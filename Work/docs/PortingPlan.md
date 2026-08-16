# Quest Reconstruction — Porting Plan

*Updated: August 2026 — Phase 1 complete; Phase 2 harness complete and validated (steps 1-3, input verification, MirrorPage compare-on-read + pair-boundary page audit, fault injection across all three checkers); ready for real playtesting and then Phase 2 step 3 translations.*

## Background

Quest is a game written in PL/1 for the Data General MV/8000 (Eagle Eclipse)
running AOS/VS, circa 1988. The source is lost; the binaries (`QUEST.PR`,
`QUEST_SERVER.PR`) and their symbol tables (`.ST`) survive. A Java emulator
(~10K lines) emulates the hardware and enough of AOS/VS to run the game,
playable over telnet with a DG D200 compatible terminal.

**Long-term goal:** recreate the game source in modern C++17, keeping the
running code faithful to the original.

**Key difficulty:** the binaries are tightly bound to the AOS/VS + PL/1
runtime, and the Eagle CISC instruction set means the compiler emitted dense,
complicated code. Reverse engineering cannot be a purely mechanical lift; it
must be validated continuously. A prior translation effort (the Opus-era
attempt) ran aground precisely here: hand-translated functions were patched
into the emulator without mechanical checking, and errors compounded
silently.

Development environment: Windows + Cygwin, g++ 11.4, C++17.

## Directory Layout

```
work/
  brief.txt      project brief
  DG_Quest/      Java emulator (reference implementation) + tools
  QUEST/         game binaries, symbol tables, and data files
  c_src/         C++17 emulator (Phase 1 result) + dormant hook machinery
                 + prior-generation translation sources (reference only,
                 not built): rt/, emu_rt/, quest/, emu_quest/, types/,
                 emu_types/, EagleIntegration
  docs/          PortingPlan.md (this plan), SyscallHandling.md
                 (per-call matrix + traced findings), LockstepHarness.md
                 (master/clone scheduler + mediation design + build
                 history), EmulationVerification.md (as-built
                 verification reference: checkers, corner cases, per-call
                 semantics, known windows), Run.md (command-line
                 reference)
```

## Strategy Overview

1. **Phase 1 — Port the emulator from Java to C++17.** ✅ **Complete.**
   Everything that runs in Java runs in C++. Mechanical, faithful port.

2. **Phase 2 — Incremental translation with lockstep validation.** Build a
   harness hosting three environments in a single process, and migrate the
   game from emulated to native code piece by piece, validating by playing
   the game.

## Phase 1 — Status and Findings

The C++ emulator in `c_src/` builds warning-free (`-Wall -Wextra`) and runs
Quest end-to-end. Validation performed:

- **Byte-identical scripted sessions** against the Java emulator through
  login, bad-password handling, and character creation, including D200
  control sequences.
- **Extended interactive gameplay** confirmed working.
- **Opcode decode tables verified exactly identical** to the Java (404
  definitions).
- **Arithmetic core reviewed line-by-line** against the Java (add/sub/mul
  carry & overflow flags, shifts, narrow ops, Eclipse float conversions),
  including deliberate preservation of the Java's quirks.

Decisions taken during Phase 1 cleanup:

- **Prior translations removed from the build.** The old hand-translated
  functions (`rt/`, `emu_rt/`, `quest/`, `emu_quest/`, `types/`,
  `emu_types/`) are considered unreliable and are excluded from compilation
  (`Makefile` builds the pure emulator; `Makefile.full` preserves the old
  everything-build). The sources remain on disk as reference — they encode
  useful knowledge about PL/1 runtime conventions even where the code is
  suspect. Concretely observed: with the old translations enabled, the
  native `?READ` broke the login flow (task death in LOGON → READ_IN on a
  READ EXTENSION packet) where pure emulation matches Java exactly.
- **Native hook mechanism kept, dormant.** `NativeRegistry` (symbol-address
  → native function, consulted at LCALL/XCALL in `EagleStack`) remains in
  place with an intentionally empty registry. Phase 2 translations will be
  re-registered one at a time under lockstep validation. The XOP2/4/6
  opcode-table additions from the prior effort were removed; the calling-
  convention bridge (`EagleIntegration`) is out of the build until
  reinvented carefully.
- **No memory-mapped files.** The prior C++ mmap'd the shared data files;
  this was removed to match the Java: shared data files live in memory
  (ArrayPages with modified-tracking) and are written back at shutdown.
  Ctrl-C behavior matches Java: SIGINT → processes shut down → modified
  data files written (`Writing: :USER_DATA_FILE` etc.). When a QUEST
  session ends, QUEST_SERVER keeps running; Ctrl-C on the launcher performs
  the write-back.

Known quirk (present in **both** Java and C++, deferred): after a client
disconnect, QUEST_SERVER relaunches QUEST.PR, but a second telnet connection
is accepted without being wired to the new process. One session per launch
is the operating contract (per the original README). Proper multi-session
support would be a deliberate emulator feature added to both sides.

## Phase 2 — The Three-Environment Harness

A single process hosts:

1. **QUEST_SERVER (emulated)** — the original multiplayer backend, and the
   sole source of randomness in the system.
2. **QUEST (emulated) — the master**: the original game binary, running
   as today. Its effects reach the real server, the real filesystem, and
   the real terminal.
3. **QUEST (translated) — the clone**: initially a second copy of the
   emulation (trivially equivalent), progressively replaced
   procedure-by-procedure with native C++ translations via the
   NativeRegistry hooks. Its outbound effects are verified against the
   master's; its inbound data is copied from the master's.

The environments run in parallel. We telnet in with a D200 and play; every
session is a live regression test.

### Equivalence Definition

The translated game **matches** the original iff, given identical inputs:

- it makes the **same OS-layer calls**, in the same order, and
- it makes the **same changes to the shared data file**.

Register-level and instruction-level state is deliberately *not* compared.
This frees translated code to be idiomatic C++ rather than assembly
transliterated into C++.

### Inputs Must Be Identical

Both game instances — **master** (executes for real) and **clone** (gets
checked copies) — must observe exactly the same external world. Design
detail lives in `LockstepHarness.md`; summary:

- **Terminal input** — one real read on the master; bytes copied to the
  clone (?READ mediation).
- **Shared data** — master maps the canonical file pages; clone maps a
  private copy; the server maps MirrorPages that write both and compare
  both on read. The scheduler guarantees the server only executes while
  both clients are parked at identical points, so both always see
  identical shared state.
- **File system** — one real filesystem; file syscalls execute on the
  master only, results replayed into the clone.
- **Randomness** — lives entirely in QUEST_SERVER (confirmed by trace:
  QUEST makes zero ?GTOD calls), reaching both games identically via
  shared data. If any direct nondeterminism is ever discovered in the
  game, its syscall is mediated: one real read, master's value replayed.

### Scheduling and Sync Points

All instruction execution funnels through the single MachineThread worker
in ~1000-step batches. The harness gates batch admission: master and clone
advance in verified pairs of batches (same ending pc, syscall status,
instruction count); the server gets one batch between client pairs, or
free-runs while both clients are blocked; the server is always parked
before clients are resumed. Client syscalls are rendezvous points: both
clients must arrive at the same call with the same arguments.

Syscalls split into **LOCAL** (deterministic process-structure calls —
memory, tasks, mappings — execute in both) and **MEDIATED** (world-facing
calls — execute once on the master; result ACs, error, and caller-memory
side effects replayed into the clone). Per-call matrix:
`SyscallHandling.md`.

**Divergence detection:** slice-outcome mismatch, call/arg mismatch at
rendezvous, write-payload mismatch, or MirrorPage read mismatch ⇒ halt
both engines, report both positions with backtraces. Asymmetric progress
(one engine at rendezvous X, the other reaching Y ≠ X) is caught by the
pairing gate rather than deadlocking silently.

### Phase 2 Steps

1. Build the harness per `LockstepHarness.md`: scheduler gate → syscall
   rendezvous + LOCAL/MEDIATED mediation → private clone copies +
   MirrorPages → divergence reporter; validate with fault injection.
2. Run with **two emulated copies** of QUEST (clone slot = second
   emulation). Must trivially match; this validates the harness itself.
3. Reintroduce translations one function at a time via NativeRegistry,
   starting with small leaf routines, each validated by gameplay under the
   checker. Reinvent the calling-convention bridge (EagleIntegration's role)
   carefully as part of the first translation. Translated stretches have no
   instruction count, so their slice boundaries degrade to syscall-only
   sync points.

### Phase 2 Progress

**The harness is complete and validated.** All detail in
`LockstepHarness.md`; summary:

- **Trace facility** (done): `-trace FILE -types TYPE,TYPE,...`. Types:
  `scalls`, `shared` (per-write attribution incl. "(handler)" tagging for
  task-thread writes), `lockstep` (one line per verified pair — heavy;
  debugging only). Instance labels `QUEST1`/`QUEST2` stable from launch.
- **Step 1 — scheduler gate** (done): MachineThread runs master/clone
  batches as adjacent verified pairs (nothing between the halves), server
  batches between pairs or free-running while both clients are parked.
  Pair comparison covers result pc, real syscall-trap pc, AC0-3, carry,
  instruction counts; divergence reports carry wsp/wfp/psr and both
  backtraces.
- **Step 2 — syscall rendezvous + mediation** (done): every client call
  rendezvouses (call + ACs verified); LOCAL calls execute in both,
  MEDIATED calls execute once on the master with result ACs, error, and
  captured caller-memory writes replayed into the clone. One input stream
  (the master's terminal); the server sees exactly one client. Fixes that
  fell out: per-process deterministic tids, pair-span shared-write lock,
  replay skipping physically shared pages, honest trace attribution, the
  `run_steps` double-decrement bug.
- **Step 3 — clone private copies + server MirrorPages** (done): removes
  the shared-page read-modify-write boundary. Clone snapshots private
  copies of writable shared data pages under a world-pause; the server's
  mappings are rewrapped in MirrorPages that write real + copy as a unit.
- **Mediated-call input verification** (done): every caller-memory read
  during a mediated call is compared byte-for-byte against the clone
  (all ?WRITE payload varieties, ?ISR content, filenames, packet fields).
- **Spectator terminal** (done): the second telnet window mirrors the
  master's terminal output (host-side echo; no lockstep effect).
- **Fault injection** (demonstrated): a flipped replayed keystroke trips
  the pair checker within ~50 instructions; a flipped clone ?WRITE
  payload byte trips input verification at exactly that byte. Boot-time
  perturbations of server-hot shared bytes are self-healed by the mirror
  path — perturbation tests must target consumed data.
- **Validation runs**: scripted sessions through login, character
  creation, class selection, and gameplay (movement, map rendering):
  ~1.4M verified pairs, zero divergences. A real user playtest also ran
  green (short session).
- **Performance**: client batches 500 true instructions (constant in
  `Machine::run`; shrink when hunting a divergence), server 1000. Run
  without the `lockstep` trace type for normal play. Playtest line:
  `./emulator -lockstep QUEST QUEST_SERVER @QUEST @QUEST > log`
- **Build robustness**: Makefile tracks header dependencies (`-MMD -MP`).
  Practice note: verify the binary timestamp after every build — two
  debugging detours this phase came from silently-failed builds running
  stale binaries.

### Next Steps

1. **Extended real playtesting** (user, over telnet): combat, castles,
   inventory, saving, logout/reconnect. Expect rough edges around session
   lifecycle (?RETURN pairing, respawn) and possibly ?TASK ordinal-1
   pairing the first time the game spawns its ctrl-A/ctrl-C listener
   (which is otherwise inert: terminal interrupts are not wired in this
   port, so both listeners just park).
2. **Systematic fault injection** (step 4 proper): broaden beyond the
   demonstrated cases using the read-triggered flip technique
   (`c_src/tools/fault_injection_on_read.md`).
3. **Deferred harness items**: ?WDELAY joint-release refinement,
   shutdown/respawn pairing. (MirrorPage compare-on-read and the
   pair-boundary page audit are DONE and fault-injection proven —
   see EmulationVerification.md §7.3/§7.5/§11.)
4. **Begin translations** (the point of it all): reintroduce translated
   functions one at a time via NativeRegistry into the clone slot,
   starting with small leaf routines, each validated by gameplay under
   the checker; reinvent the calling-convention bridge as part of the
   first translation. Translated stretches have no instruction count, so
   their pair boundaries degrade to syscall-only sync points — the
   rendezvous + input verification carry the checking there.

## Open Questions

- Exact mechanism for handing control between still-emulated and
  already-translated procedures inside environment 3 (call-boundary shims /
  the reinvented bridge).
- Whether the game itself has any direct nondeterminism (trace evidence
  says no — zero ?GTOD from QUEST; keep confirming over long sessions).
- RESOLVED: `Machine::run_steps` double-decrement was a bug; fixed.
  Batch counts are now true instruction counts.
- Threading-model note: the C++ emulator executes all instructions on a
  single MachineThread worker (a deliberate departure from Java's
  thread-per-task, avoiding per-page locking). Any harness design must
  respect this — blocking inside the worker starves all tasks (this bit the
  prior effort's native `?AWAIT_CONSOLE_INTERRUPT`).
