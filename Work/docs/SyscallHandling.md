# Phase 2 — System Call Handling Plan

How the lockstep harness (QUEST_SERVER + master QUEST + clone QUEST — see
`LockstepHarness.md` for the scheduler/mediation design) treats every
system call implemented by the emulator. Based on traced sessions
(`-trace FILE -types scalls,shared`); per the project owner, every
implemented call is used somewhere, so this is the complete universe
(~31 calls).

**Status:** IMPLEMENTED. Full verification semantics per call (including
mediation corner cases) are in `EmulationVerification.md` §9; the matrix
below is as-built — it matches
`LockstepMediator::is_local` exactly, and an unclassified call at
rendezvous is a hard error. Validated through login, character creation,
and gameplay (~1.4M verified pairs, zero divergences). Still never
observed: ?UIDSTAT, ?REC. QUEST made zero ?GTOD calls all session —
nondeterminism confirmed server-side.

**tid note:** tids are now allocated per-process (base 100, so every
main task is tid=100), not from a global counter. This makes master and
clone tid sequences identical, so identity writebacks (?UIDSTAT ?UTID,
?TASK ?DID) are deterministic and those calls can stay LOCAL. Older
trace excerpts below showing tid=101/102 predate this change.

**Communication model (long-trace finding):** the ?ISR request/reply
handshake occurs ONLY at login (both ?ISRs in the first ~100 events).
During gameplay there is NO IPC at all: the server free-runs, updating
shared pages on its own schedule, and the game sits blocked in terminal
?READ — each keystroke reads current shared state, acts, redraws, and
blocks again (?READ count ≈ keystrokes). ?WDELAY appeared only 4 times,
clustered in one stretch (a single timed screen/mode, ac0=0x7D0=2000ms),
not a game loop.

**Harness consequence — scheduler-enforced isolation** *(supersedes the
earlier "snapshot stream keyed by call index" design)*: instead of frozen
copies refreshed by write-set replay, the LockstepHarness scheduler
guarantees the server never executes instructions while a client slice
runs. Server shared-page writes go through MirrorPages (write both the
master's canonical pages and the clone's private copy) and can only land
while both clients are parked at identical points — so both clients
always observe identical shared state, with no capture/replay stream
needed. A future `sharedrd` trace type (shared-page reads) can identify
exactly which offsets the game consults, useful for checker diagnostics.

**Trace lines** carry the instance label as caller: `QUEST` in single-game
runs, `QUEST1`/`QUEST2` in dual-emulation runs, `QUEST_SERVER` for the
server, e.g.:

```
scalls seq=013330 caller=QUEST tid=101 call=?ISR ac0=7000021C ac1=00000000 ac2=700010F8
scalls seq=013334 caller=QUEST tid=101 call=?ISR ret err=0000 ac0=7000021C ac1=00000000
```

## Observed Architecture

Traced pattern (login → character creation → gameplay):

```
QUEST:        writes request/state into shared pages
QUEST:        ?ISR            -- send to server port, block for reply
QUEST_SERVER: ?IREC returns   -- receives the request
QUEST_SERVER: bulk writes     -- ~950 words SHARED_DATA_FILE,
                                 ~3000 words WORLD_DATA_FILE per turn
QUEST_SERVER: ?ISEND reply    -- "update done" handshake
QUEST:        ?ISR returns    -- game proceeds
```

Key facts:
- **?ISR return is the game's natural sync point** — the handshake the
  original design relies on. The server sits in a ?IREC loop.
- **Shared-page traffic is bidirectional.** In one session QUEST wrote
  2,785 words to SHARED_DATA_FILE and 3,534 to WORLD_DATA_FILE; the server
  wrote 10,131 and 3,381 plus 334 to CASTLE_DATA_FILE. Game writes are part
  of the equivalence check, not just server output to be replayed.
- **?GTOD is called only by QUEST_SERVER** (once, at startup — RNG seed).
  The game read no clock in traced sessions, supporting the belief that all
  nondeterminism lives server-side. To be re-verified in longer sessions.
- QUEST runs a second task (tid=102) blocked in ?INTWT
  (console-interrupt wait), used for ctrl-A attention handling
  (C_A_LISTENER in the server plays the same role).

## Handling Categories

Per `LockstepHarness.md`: **master executes, clone gets checked copies** —
clone-outbound data (payloads, args) is verified against the master's;
clone-inbound data (read results, ACs, errors) is copied from the master's.
Client syscalls rendezvous: both clients must arrive at the same call with
the same args before anything executes.

- **MEDIATED** — world-facing: executes once on the master; result ACs,
  error code, and caller-memory side effects are captured and replayed
  into the clone.
- **LOCAL** — executes independently in both clients: deterministic calls
  whose effects are internal process structure (memory, tasks, mappings).
  Still rendezvous'd (same call, same args) so divergence is caught at
  the boundary.
- **SERVER** — made only by QUEST_SERVER, which runs single-copy; no
  harness treatment needed.

(The earlier RV/DUP/REPLAY categories collapse into this model: RV is the
rendezvous every client call performs; DUP and REPLAY are both "clone gets
copies" — of external input and of syscall results respectively.)

## The Matrix

### Terminal and file I/O
| Call | Who | Handling |
|---|---|---|
| ?READ (terminal) | QUEST | **MEDIATED**: one real read on the master; same bytes copied into the clone's buffer. The hot path (≈ one per keystroke). |
| ?WRITE (terminal) | QUEST | **MEDIATED**: clone payload verified against master's; sent to telnet once. Primary gameplay-visible check. |
| ?READ ?WRITE (file) | both | QUEST: **MEDIATED**. Server: **SERVER**. |
| ?OPEN ?CLOSE | both | QUEST: **MEDIATED** — master allocates the real channel; the clone's view of channel numbers arrives via replayed ACs/packet results (clone executes no FS ops, so it needs no channel table of its own). Server: **SERVER**. |
| ?UPDATE | server (observed) | **SERVER**. If QUEST ever calls it: **MEDIATED**. |

### Shared memory
| Call | Who | Handling |
|---|---|---|
| ?SOPEN ?SPAGE | both | QUEST: **LOCAL** — master maps the canonical FSFile pages; the clone's ?SPAGE clones private copies and maps those. Server's ?SPAGE maps MirrorPages over (canonical, clone-copy). |
| ?GSHPT ?SSHPT | both | Shared-partition bookkeeping; **LOCAL**. |
| (page writes) | both | Not a syscall. Server writes go to both copies via MirrorPage; server *reads* compare both copies (divergence check at the moment of consumption). Client writes land only in that client's own mapping. |

### IPC — the handshake
| Call | Who | Handling |
|---|---|---|
| ?ISR | QUEST | **MEDIATED**: only the master's message reaches the server (the server must see exactly ONE client); request payloads verified at rendezvous; reply replayed into the clone. Both clients pend ⇒ server free-runs to handle it (no special scheduling). |
| ?ILKUP ?CON ?DCON | QUEST | Port lookup/connect: **MEDIATED**; clone observes the master's handles via replay. |
| ?ISEND ?IREC ?SERVE ?CREATE ?RECREATE | server | **SERVER** (its side of the RPC). If QUEST is ever observed using ?ISEND/?IREC directly, treat as ?ISR. |

### Process, task, memory, misc
| Call | Who | Handling |
|---|---|---|
| ?MEM ?MEMI | both | Memory sizing/allocation (page tables): **LOCAL** (deterministic). |
| ?PNAME ?DADID | both | Identity queries: **MEDIATED** — clone observes the master's pid etc. via replay; harness routes internally by real ids. |
| ?UIDSTAT | never observed | **LOCAL** (as built): its writebacks (?UUID task slot, ?UTID tid) are deterministic under per-process tid allocation, so both sides produce identical values without mediation. |
| ?GTOD | server only (observed) | **SERVER**. If QUEST ever calls it: **MEDIATED** — one real read, master's value replayed. Same rule for any future time/randomness source in the game. |
| ?TASK ?KILAD ?IXIT ?REC | QUEST | Task management: **LOCAL** (each instance needs its own real tasks). ?DID writeback deterministic via per-process tids. Purpose per project owner: ?TASK spawns the ctrl-A/ctrl-C move-interrupt listener. Ordinal-1 pairing untested (short sessions never spawned it). |
| ?INTWT | QUEST | Ctrl-A listener park: **LOCAL**. Note: terminal interrupt delivery is NOT wired in this port (INTWT_call just sleeps until process teardown; the terminal read path never detects control bytes) — so both listeners park inertly and no delivery mediation is needed unless/until interrupts become a real feature. |
| ?WDELAY | QUEST (observed, 2000ms) | **LOCAL** with joint release: both rendezvous, one real timed wait, both released together — with the server parked first, per the resume rule. |
| ?RETURN | QUEST | Process exit: rendezvous — both must exit together; a lone exit is a divergence. Server's respawn of QUEST.PR must spawn the next *pair*. |

## Sync Model

Defined in `LockstepHarness.md`. Summary:

1. **Client lockstep pairing** — master and clone advance in paired
   MachineThread batches; slice outcomes (pc, syscall status, instruction
   count) compared every pair.
2. **Server quantum** — one server batch between client pairs; server
   free-runs while both clients are blocked; server is parked before
   clients are resumed after input/delay.
3. **Rendezvous at every client syscall** — same call, same args, then
   LOCAL (both execute) or MEDIATED (master executes, clone gets replay).
4. **Shared pages** — clone maps private copies of writable shared data
   pages; server MirrorPages write real + copy as one gated unit and
   compare both on read (use real, check copy; the once-blocking
   master-write -> clone-replay window is closed by a funnel dual-write).
   A pair-boundary page audit catches drift in unconsumed bytes.
5. **Blocking is task-level, not thread-level** — all instruction
   execution shares one MachineThread worker, so all waiting parks OS
   tasks; the worker is never blocked.

The trace facility's `shared` type (per-write attribution with
file:page:offset:value) remains the diagnostic for shared-page traffic.

## Divergence Reporting

As built, three independent checkers, each halting both engines with a
two-sided report (labels, pcs incl. real trap sites, AC0-3, carry,
wsp/wfp/psr, backtraces):
1. **Pair comparison** — every master/clone batch pair's end state.
2. **Rendezvous verification** — call number + entry ACs at every client
   syscall.
3. **Mediated-call input verification** — every caller-memory read of a
   mediated call compared byte-for-byte against the clone (all ?WRITE
   payload varieties, ?ISR content, filenames, packet fields), excluding
   ranges the call itself wrote and the mirrored shared region.
Fault-injection validated: a flipped replayed keystroke trips checker 1
within ~50 instructions; a flipped clone ?WRITE payload byte trips
checker 3 at exactly that byte.

## Open Items

- ?UIDSTAT and ?REC remain unobserved; confirm they're QUEST-side dead
  code or classify them when first seen (unknown call at rendezvous is a
  hard error, so nothing can slip through unclassified).
- Confirm over long sessions that QUEST never reads time or any other
  ambient nondeterminism directly.
- Shutdown/respawn pairing: define ordering so the server's
  respawn-on-death (?RETURN → relaunch QUEST.PR) creates the next
  master/clone pair cleanly.
- Dynamic ?TASK creation: per-process tids guarantee identical ids;
  ordinal-1 batch pairing itself remains untested (no session has
  spawned the listener yet).
