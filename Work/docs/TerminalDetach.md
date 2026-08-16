# Terminal Detach

Status: **IMPLEMENTED AND VALIDATED** (Aug 2026). Design agreed in-session;
built, tested end-to-end same session.

## The idea

Once execution reaches a point we know is terminal — normal exit
(`I.STOP`), unhandled-condition death (`?FATAL`; DEF?ON was a frontier member until the Aug 12 lift made it verified L2) — verification
stops paying for itself: a terminal path observes nothing the game can act
on. So: form **one final verified pair at the terminal entry** (the
decision to die, and the state it dies with, is the last checked event),
then **detach** — halt the clone, unhook all clone support, and run the
master to completion unverified.

This replaces two older ideas at once:

- `?FATAL` and the traceback machinery (1,662+ words, Tier 3 of
  ERROR_LIFT_SCOPE.md) are now **permanently outside** the translation
  scope, not merely deferred.
- The matched-abort scheme for `QSEARCH` (throw the master's exact string)
  is dead. `QSEARCH`, `DERR`, and the FP-fault throws are **hard
  terminals**: both engines throw identical strings, `compare_pair`
  already treats matching exceptions as agreement, both die. No detach
  machinery needed for those — detach exists only where the master should
  live on.

## Terminal taxonomy

| Kind | Members | Behavior |
|---|---|---|
| Graceful terminal (DETACH kind) | `I.STOP`, `I.STOPM`, `?FATAL` (design-era list also had DEF?ON — lifted Aug 12 — and O.SET's I?LINEID branch — later proven statically dead) | final verified pair at entry → clone detaches → master runs terminal path to completion (exit, traceback, writeback) |
| Proven-corruption terminal (ABORT kind) | `DERR.TRP` (Layering ruling 7) | final verified pair at entry → abort_world(save=false): world hard-stops, NO write-back |
| Hard terminal | `QSEARCH`, unimplemented instruction/syscall throws, FP-fault throws | both engines throw the identical string; pair agrees; both die informatively |

## Mechanism (~130 lines)

- `RTStubs::terminal_bits` — third bitmap beside `entry_bits` /
  `translated_bits`, marked for **both roles** from `terminal_table`
  (`I.STOP`, `I.STOPM`, `?FATAL` — DEF?ON was removed by the Aug 12 lift: it is ordinary verified L2 now, and the frontier sits one level deeper at ?FATAL). Test hook:
  `QUEST_TERMINAL=<hex-pc>` adds one terminal address checked at ANY pc
  (game code allowed).
- `Machine::run_steps` — pc arriving at a terminal point sets
  `terminal_reached` and breaks the batch. Three detection sites: the
  test-pc check, the rt_sync entry check (terminal beats translated), and
  an **escape inside the master's run-to-return loop** — a translated
  routine whose emulated body dies would otherwise spin to the 10M runaway
  guard.
- `QueueEntry::terminal` — transferred from the machine per batch, like
  `native_span`.
- `Lockstep::compare_pair` — `terminal` mismatch between halves is
  structural divergence (one-sided arrival at *different* pcs is already
  an address mismatch; this covers a mis-routing native wrapper at the
  same pc). Both-terminal after a CLEAN comparison → `Lockstep::detach`.
- `Lockstep::detach(master, clone)` — idempotent per ordinal:
  1. set `detached[ordinal]` (atomic),
  2. halt every task of the clone process (`OSTask::halt_task`; its main
     task is parked at this very batch boundary and exits on release; the
     listener's `INTWT` loop polls `task->halt` and exits within ~100 ms;
     no file writes on this path — master/server pages are canonical),
  3. `OSProcess::unmirror_server_mappings()` — see below,
  4. clear the copy registry (which also retires the pair-boundary page
     audit),
  5. one loud stderr line:
     `Lockstep: ordinal N DETACHED at <pc> (<symbol>) — clone halted,
     master continues unverified`.
- **Server unmirror** — the inverse of `mirror_server_mappings`: re-map
  the plain real page over every SERVER `MirrorPage(real, copy)`. Without
  this, the clone's frozen copies drift from the real pages as the master
  plays, and the server's compare-on-read fires false divergences. Runs at
  the `compare_pair` call site, where the worker already holds
  `shared_write_mutex` — the same world-pause the forward direction
  required. (`Memory::map_page` just replaces the slot pointer; the old
  MirrorPage objects leak harmlessly until teardown.)
- Scheduler (`MachineThread`): a MASTER batch whose ordinal is detached is
  served like SERVER work — runnable alone, non-pair mutex path — and is
  **not** held as `master_half` awaiting a clone (that fix mattered: the
  first draft deadlocked exactly there).
- `LockstepMediator::applies()` — false for a detached ordinal, so the
  master's syscalls dispatch directly instead of waiting 30 s for a
  counterpart that will never arrive.

Native code needs no separate detach API: a native routine whose path dies
arranges pc to land on a terminal entry and breaks; both engines converge
on the same address by construction.

## Companion machinery — BUILT Aug 13 as `Lockstep::abort_world` (see Lockstep.hpp; design record below)

Detach's mirror image. Detach = clone stops, master lives. This =
"paired verification has become impossible; stop the world on
purpose, cleanly." Needed because stack pressure is ASYMMETRIC by
design: the master emulating a body inside a native span pushes frames
the clone never pushed (and the future stack-free clone barely uses
the MV stack at all), so a master-side stack fault mid-span vectors to
an arbitrary handler and no pairing rule can recover — that is not a
bug to fix but a state to exit deliberately. NOTE: symmetric paired
emulation is NOT the concern (identical streams fault identically,
including the load-bearing startup overflow protocol).

Sketch:
- `Machine::stack_faulted` flag set by `EagleStack::handle_overflow`
  alongside its faithful vectoring.
- Master-side, inside the run-to-return span: `stack_faulted` observed
  → `Lockstep::force_shutdown(reason)`.
- `force_shutdown`: print the reason + backtrace; release any parked
  counterpart batches (no thread left waiting); halt all client
  machines at their batch boundaries; behave like operator ctrl-C from
  there — FS::save_all writes master/server pages; exit. An orderly
  named death, not a divergence report.
- Also the right home for any future "master detects unpairable X"
  case; detach and force_shutdown between them cover both directions
  of "the pair must end."

## Multi-client caveat

With one client pair, retiring the whole copy registry is exact. A second
simultaneous pair would need ordinal ownership tags on registry entries so
only the detaching clone's copies retire. Noted in `Lockstep::detach`.

## Validation (Aug 2026, this container)

Test terminal at `LIST_PLAYERS` (0x7016EB87) via `QUEST_TERMINAL`,
lockstep `-silent`, scripted session:

- pre-detach verified play (move served, 0 divergences);
- `L` → `P` → `DETACHED at 7016EB87 (LIST_PLAYERS)` after one final
  verified pair; clone "Thread halt" clean;
- **two full post-detach turns** (2,353 + 225 bytes of screen traffic) —
  turn grants require complete server round-trips, so the unmirrored
  server is proven functional, not merely non-crashing;
- ESC exit post-detach: all four data files written back, both coverage
  bitmaps dumped, clean shutdown.

A separate run validated the exit path itself: quitting the game
post-detach ran `I.STOP → LANG?STOP → FS::save_all` on the lone master
without any pairing involvement.

## Consequences

- Every normal ESC exit now detaches at `I.STOP`: the clone drops and the
  master shuts down alone. The old "Forced exit" / mid-read-disconnect
  pair-gate shutdown hang class should be gone — shutdown is never paired
  again. (Not yet specifically re-tested against a mid-read disconnect.)
- Everything downstream of a terminal entry is forever verification-free,
  including `?FATAL`'s traceback. Accepted deliberately: fidelity is owed
  to what the game can observe.

## Findings from the test sessions (unrelated to detach, worth keeping)

- **Turn cadence in a slow/1-core container is ~49 s per turn** (the
  server's full player-record round per turn is the cost). Scripted-play
  waits must exceed a turn or every move looks like a hang. Two hours of
  phantom-stall chasing are buried here.
- The `L` (LIST) command enters a sub-menu (`Hit (P)...(C)...(A)...(I)`)
  that silently ignores map moves; a non-menu key leaves it. ESC there
  quits the whole game (ESC is the global quit key).
- The documented "benign QUEST_SERVER startup segfault" is misattributed —
  see UNIMPLEMENTED.md §8: it is the server's `IPC_TASK` dying inside
  `UPDATE_USER_DATA_FILE` at **first login**, every session; the
  multi-task server survives it.
