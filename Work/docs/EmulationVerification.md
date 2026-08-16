# Emulation Verification — Technical Reference

> **Generation note (Aug 13 2026).** This document describes the
> Generation-1 (M2/entry-keyed era) checker as built. The batch
> BREAK RULES have since been rekeyed on layer transitions — see
> docs/CheckerHistory.md (lineage) and docs/CrossingsChecker.md
> (current). Scheduler, mediation, mirror pages, and pair-compare
> mechanics below remain accurate.

*How the lockstep harness verifies that two emulations (and later, an
emulation and a part-translated build) are behaviorally identical. This
is the as-built reference: every checker, every mediation rule, every
corner case and known window. Companion to `LockstepHarness.md` (design
narrative and history) and `SyscallHandling.md` (per-call matrix with
trace evidence).*

## 1. What "verified" means

Two client processes — master (QUEST1, role MASTER) and clone (QUEST2,
role CLONE) — execute the same instruction stream against one
free-running server. Verification asserts, continuously:

1. Both clients execute the same instructions with the same register
   outcomes, in batches compared pairwise (checker 1).
2. Both clients make the same system calls with the same arguments
   (checker 2), and every byte of data a mediated call sends outward is
   identical on both sides (checker 3).
3. The clone receives exactly the master's world: inputs, syscall
   results, and server shared-memory updates are duplicated to it, never
   independently generated.

Any violation halts both engines with a two-sided report: instance
labels, result pc and real trap pc, AC0-3, carry, wsp/wfp/psr,
instruction counts, and both backtraces (via `debug/CallStack`), plus
the differing data for checkers 2 and 3. Reports go to stdout (where
backtraces print) with a one-line stderr notice.

What is deliberately NOT asserted: full memory equality of private
pages (no page hashing), wsp/wfp/psr equality (reported on divergence,
not compared — see §8), and FPAC equality. Shared-page real-vs-copy
equality IS asserted, two ways: at every server read (MirrorPage
compare-on-read, §7.3) and by a periodic pair-boundary page audit
(§7.5).

## 2. Roles, ordinals, identity

- Roles are assigned at launch from the command line: duplicated program
  names become MASTER (first) and CLONE (second); everything else is
  SERVER. Role and a per-process **task ordinal** (creation order) are
  stamped onto each task's `Machine` in `OSProcess::register_task`.
- Pairing is by ordinal: master task N pairs with clone task N, for both
  batch comparison and syscall rendezvous. A batch or call from a task
  with no counterpart waits (with a 30 s diagnostic), it is never run
  unpaired.
- **tids are per-process** (`OSProcess::next_tid`, base 100). This is a
  verification requirement, not a convenience: master and clone must
  generate identical tid sequences so that identity-bearing writebacks
  (?UIDSTAT ?UTID, ?TASK ?DID) are equal without mediation. A global
  counter here caused a real (latent) asymmetry during step-2 debugging.

## 3. The scheduler contract

All instruction execution funnels through the single MachineThread
worker in batches (clients 500 true instructions, server 1000; the
client constant lives in `Machine::run`). Under `-lockstep`:

- A master batch runs only when its ordinal's clone batch is also
  queued; the clone half then runs **immediately after, with nothing in
  between** (`clone_next` has absolute priority in the picker).
- Server batches run between pairs, and free-run while both clients are
  parked (blocked in a syscall or waiting at a rendezvous). "Parked"
  falls out of queue contents — a pended client has no queued batch.
- The worker holds `Lockstep::shared_write_mutex` around **every** batch
  and keeps it held across a master+clone pair span. Consequences:
  task-thread page writes (§6) cannot land between the two halves of a
  pair; clone page-copy snapshots and MirrorPage rewraps (§7) cannot be
  torn by server batches. The mutex is never held while the worker waits
  for work.
- The master half's QueueEntry completion is withheld until the pair is
  compared (the entry lives on the submitting task thread's stack;
  early completion would free it early).

## 4. Checker 1 — batch-pair comparison

At the end of every pair (`Lockstep::compare_pair`), divergence if any
of: result pc differs; instruction-count delta differs; AC0-3 or carry
differ; both ended in the syscall-trap sentinel (0x30000000) but the
**real trap pcs** (surviving in `machine->pc`; both task threads are
parked until the comparison completes) differ; exactly one half threw;
or both threw with different exception messages.

Corner cases:
- The sentinel rule exists because equal-AC syscalls made from different
  code paths would otherwise compare equal (this hole masked nothing in
  practice, but it was real).
- Blocking calls produce no batches, so a client inside ?ISR/?READ
  simply has no pairs; the checker resumes at the first post-call batch.
- `-types lockstep` emits one trace line per pair (heavy — debugging
  only).

## 5. Checker 2 — syscall rendezvous

Every client system call routes through `LockstepMediator::dispatch`
before its handler runs. Per-ordinal slot, cycle/phase protocol:
arrivals allowed while no mediated cycle is in flight; the **second
arriver verifies** both sides' call number and AC0-2 under one lock
hold (so stale data from a previous cycle can never be compared), then
either closes a LOCAL cycle (both proceed) or opens a MEDIATED one; the
clone's result consumption closes a mediated cycle. First arrivers wait
on the cycle counter with 30 s diagnostics. All waiting parks the task
thread, never the worker.

Corner cases:
- AC3 is not verified at rendezvous (it is the return address and is
  covered by checker 1 at the surrounding batch boundaries).
- An **unclassified call number is a hard abort** — the LOCAL/MEDIATED
  table cannot silently drift as new calls appear.
- A lone arrival (e.g. one side exits or takes a different call path)
  surfaces as the 30 s wait diagnostic, then typically a checker-1
  divergence or scheduler stall report; a clean "lone exit" report is a
  known gap (§10).

## 6. Mediation semantics (MEDIATED calls)

Master executes the real handler; clone never executes it and receives:
result AC0-2, the error code (which drives the normal error-return
path identically), and a **replay** of every caller-memory write the
handler made.

- Write capture: all handler writes go through the `OSContext`
  `mem_write_byte/word/wide` funnel (the packet/array helpers and the
  two direct IPC loops are converted; any new handler write site must
  use the funnel). Captured as (width, address, value), replayed in
  order into the clone's memory on its task thread before it resumes.
- **Replay skips physically shared pages**: if the target page object is
  identical in master and clone memories (`Memory::find_page` on both;
  byte page = addr>>11, word/wide page = (addr>>10)&0x1FFFFF, wide
  checks both straddled pages), the write already landed on the common
  page and re-applying it later would stomp newer server updates.
  OBSERVED FACT: no mediated handler has ever written a shared-region
  address (replay instrumentation showed skipped=0 across all sessions;
  ?ISR replies are three private packet fields). The skip rule is
  defensive against the theoretical case only.
- **Funnel dual-write to mirrored shared pages**: if a mediated handler
  ever writes an address whose page has a registered clone copy, the
  funnel writes real + copy under one WriteGate hold, marks the logged
  write `delivered`, and replay skips it. This closes the theoretical
  real-updated/copy-stale window at the source (making compare-on-read
  safe, §7.3); a one-time stderr note fires if it ever occurs.
- Handler writes on the master to *real* shared pages happen on the task
  thread under the WriteGate (§7), so they cannot interleave a pair; the
  clone sees the equivalent data via replay into its copy.

### Checker 3 — mediated-call input verification

The mirror image of the write capture: every caller-memory **read** the
master's handler performs (`mem_read_byte/word/wide` funnel: packet
fields via the read_packet helpers, buffers and strings via
`read_byte_array`/`read_string`, the two direct IPC content loops) is
simultaneously performed against the clone's memory and compared. The
clone is parked at the rendezvous for the whole execution, so its
private memory is stable to read from the master's task thread.

This covers, with zero per-call code: every ?WRITE variety's payload
(fixed byte count; delimited 0xFFFF count, where the master's NUL scan
sets the length and each scanned byte is compared; screen-extension
packets and their cursor-position writes), ?ISR and ?ISEND message
content word-by-word, ?OPEN/?ILKUP filenames and service names, and
every packet argument of every mediated call.

Exclusions (`OSContext::read_verifiable`), both required for
correctness, not optimizations:
1. **Byte ranges the call has already written** (checked against the
   live write_log with byte-granular overlap; widths normalized to byte
   ranges). Handlers re-read their own results — e.g. ?IREC's logging
   re-reads the packet it just filled — and the clone receives those
   values only later via replay.
2. **Pages in the mirrored shared region** (master's page has a
   registered clone copy in `Lockstep::copy_for`). Server MirrorPage
   writes are atomic with respect to *pairs*, not with respect to a
   two-sided read pair performed on a task thread, so real-vs-copy could
   legally differ mid-write. Client agreement about shared memory is
   checker 1's job.

Reads performed by `OSTask::dispatch_system_call` itself (call number,
packet address) are pre-context and unverified; they are covered by the
AC verification and checker 1.

## 7. Shared memory model

### 7.1 Page classes
- **Shared code** (exec, read-only): one physical page for everyone.
- **Client-private data** (.PR data, stacks): per-process `private_copy`
  at map time, identical initial content.
- **Shared data** (?SPAGE mappings of SHARED_DATA_FILE etc.): the master
  and server share the real file pages; the clone gets private copies.

### 7.2 Clone copies and MirrorPages
On the clone's ?SPAGE (`OSProcess::lockstep_shared_page`), each
writable shared data page is snapshotted (`private_copy`) and registered
**under the WriteGate world-pause**, which excludes both server batches
(worker holds the mutex per batch) and handler writes — no torn
snapshots. In the same hold, every SERVER-role process's live mapping of
that page is rewrapped as `MirrorPage(real, copy)` (located via the
`shared_pages` bookkeeping, not address scans); a server that maps late
gets the MirrorPage at map time. Copies inherit the shared-trace label
with a `~clone` suffix.

`MirrorPage` forwards reads to the real page and performs each write to
real **and** copy under one reentrant `WriteGate` hold (thread-local
depth counter, so the nested ArrayPage gates don't self-deadlock).
Server instruction writes are worker-side (gate disengaged, batch
atomicity suffices); server handler writes take the gate and thus land
only between pairs. Because ?SPAGE proceeds per page, server writes
interleaving between page snapshots are safe: pre-snapshot writes are
included in the copy, post-snapshot writes are mirrored.

Timing note on initial content: the clone's ?SPAGE is rendezvous-paired
with the master's, and neither client executes instructions between the
rendezvous and its next paired batch, so the master cannot observe any
shared state the clone's snapshot missed.

### 7.3 Compare-on-read: ENABLED
MirrorPage reads read both pages, USE the real value, and COMPARE the
clone's copy; a mismatch is reported at the moment the server would
consume the data (page label, op, offset, both values, reader backtrace)
and aborts. Off-worker reads (server handler threads) take the WriteGate
so the two-sided read cannot interleave a client pair, during which real
and copy legally differ; worker-side reads are covered by the per-batch
mutex hold.

The formerly blocking window — a master handler shared-region write
racing its replay into the copy — is closed at the source by the funnel
dual-write (§6): such a write (never observed; ?ISR replies are private
packet fields) now lands on real + copy atomically and is skipped by
replay. History of the deferral: an earlier owner review had already
established the window was theoretical (zero shared-region handler
writes in any session).

### 7.5 Pair-boundary page audit
Every PAGE_AUDIT_INTERVAL (16) completed pairs, while the worker still
holds shared_write_mutex, every registered (real, copy) page is
byte-compared (`Lockstep::maybe_audit_copies`). At a pair boundary the
two must be identical — identical client writes, mirrored server writes,
and replay completes before the clone's next batch can queue — so any
difference is silent drift in bytes nothing has consumed yet. Report:
page label, first differing bytes (up to 8), both pair positions.
Interval constant in `Lockstep.hpp`; 0 disables.

### 7.4 Why this model exists (the RMW lesson)
With physically shared pages, both clients executing the same
read-modify-write (e.g. `INIT_OBJ_TBL` appending to a shared string:
load length, add, store) double-apply it — the clone reads the master's
freshly stored value. Idempotent stores (the whole login path) mask
this; the first shared RMW breaks lockstep deterministically. Private
copies remove it by construction.

## 8. What is intentionally not compared, and why

- **wsp/wfp/psr**: reported on divergence, not compared per pair. Stack
  pointers are fully determined by the compared program flow; comparing
  them adds little and was left out. Cheap to add if wanted.
- **Memory contents**: no page hashing. Divergent memory is caught when
  it reaches registers (checker 1), a syscall argument (checker 2), or
  outbound data (checker 3) — in practice within one batch. A periodic
  page-hash audit remains a listed option in `LockstepHarness.md`.
- **FPACs / floating state**: the game has not exercised it under the
  harness; unverified.
- **Server behavior**: the server is a shared dependency, not a
  verification subject. It is wall-clock nondeterministic by design;
  determinism is only required of what the *clients* observe, which the
  scheduler contract provides.

## 9. Per-call reference

LOCAL — rendezvous-verified, then both execute their own handler:

| Call | Notes and corner cases |
|---|---|
| ?MEM ?MEMI | Deterministic sizing/mapping. |
| ?GSHPT ?SSHPT ?SOPEN | Shared-table setup; per-process channel numbers are allocated deterministically. |
| ?SPAGE | Triggers the clone copy + MirrorPage machinery (§7.2) inside `map_file`. |
| ?TASK | Both spawn real tasks; identical ?DID via per-process tids; new tasks pair at the next ordinal. Ordinal-1 pairing not yet exercised (the game's listener task never spawned in test sessions). |
| ?REC ?KILAD ?UIDSTAT | Deterministic under per-process tids (?UUID slot, ?UTID). ?UIDSTAT/?REC never observed in any trace. |
| ?INTWT | Both listeners park. Terminal interrupt delivery is NOT wired in this port (the handler sleeps until process teardown; the terminal read path never inspects control bytes), so no delivery mediation exists or is needed yet. If interrupts become real: deliver to both queues before releasing either listener. |
| ?WDELAY | Both sleep concurrently on their task threads (~2 s observed). Wake skew is harmless: neither client runs an instruction until both have queued batches and are admitted as a pair, and the server legitimately free-runs during the sleep. The designed "joint release with server parked first" is thereby satisfied by the pairing gate itself. |
| ?RETURN ?RECREATE ?IXIT | Rendezvous forces joint arrival; a lone exit currently manifests as a wait diagnostic/stall rather than a clean report (§10). |

MEDIATED — master executes; clone gets ACs + error + replay; inputs
verified byte-for-byte:

| Call | Notes and corner cases |
|---|---|
| ?READ (terminal) | The input-duplication mechanism: master blocks on its socket; the typed bytes and ?IRLR reach the clone via replay. The clone's terminal is never read — type only into the master's window. |
| ?READ (file) | Record reads replay buffer + ?IRLR; error paths (?IRLR=0 + code) replay identically through the normal error return. |
| ?WRITE (all varieties) | Payload verified: fixed count, delimited (0xFFFF; NUL-scan length is master-determined and every scanned byte compared), screen-extension packet (?ETSP/?ESFC/?ESEP/?ESCR reads verified; ?ESCP cursor bytes are handler-generated). Master's terminal output is additionally echoed host-side to the clone's socket (spectator view — display only, outside verification). |
| ?OPEN ?CLOSE ?UPDATE | Filenames verified; channel slot assignment happens only on the master, but per-process channel counters keep the clone's *subsequent packet contents* (written by the game from replayed results) consistent. |
| ?ISR | The one blocking RPC. Send content verified word-by-word; server free-runs during the master's wait; reply replays to the clone — observed replies are three private packet fields (?IUFL, ?IRLT, and empty-content ?IPTR carrying e.g. the player-slot pointer); no shared-region writes ever observed. Server sees exactly ONE client. |
| ?ISEND ?IREC ?SERVE ?CREATE | Server-side of the RPC normally; mediated if a client ever issues them. ?IREC's post-write logging re-reads are exempt from input verification via the write-log exclusion. |
| ?ILKUP ?CON ?DCON | Names verified; handles observed via replay. |
| ?GTOD ?PNAME ?DADID | Time/identity: clone observes the master's values (pid 101 everywhere). Any future ambient nondeterminism the game grows must be classified MEDIATED — the unknown-call abort guarantees it cannot slip in unclassified. |

## 10. Known gaps and windows

1. **Lone exit / session lifecycle**: ?RETURN with only one side
   arriving stalls with diagnostics instead of a labeled "lone exit"
   divergence; server respawn-on-death does not yet spawn a paired
   master/clone. Expected to surface in extended playtesting.
2. **Ordinal-1 (dynamic ?TASK) pairing**: designed, untested.
3. RESOLVED: MirrorPage compare-on-read is enabled (§7.3) and the
   pair-boundary page audit covers silent drift (§7.5); both
   fault-injection proven (§11).
4. **Master handler shared writes vs server instruction batches**:
   byte-level interleave on the real page during a mediated call is
   possible (pre-existing in single-play too); both clients still
   observe identical state at batch granularity, so lockstep is
   unaffected — noted for completeness.
5. **Translated-function future**: native stretches execute no emulated
   instructions, so checker 1 degrades to syscall-boundary sync for
   them; checkers 2 and 3 carry the verification across those stretches.
   This is the intended operating mode for Phase 2 step 3.

## 11. Fault-injection evidence

- A single flipped replayed keystroke byte (clone side) tripped checker
  1 within ~50 instructions, with both sides' divergent code paths in
  the report.
- A single flipped clone ?WRITE payload byte (0x20→0x21) tripped checker
  3 at exactly that byte range, with the emitting call
  (`DISPLAY_SCREEN`) in the backtrace.
- Perturbing a clone shared-page copy at snapshot time in server-hot
  areas is **self-healing** (the mirror path rewrites it before
  consumption) — a correct outcome that means perturbation tests must
  target data that is actually consumed.
- **Compare-on-read proven** (read-triggered flip of
  :SHARED_DATA_FILE:4 byte 0x647, the low byte of the word the server
  polls ~45k times during character creation): tripped on the very read
  that consumed it — `op=word off=0x323 real=3ECF clone_copy=3ECE`,
  reader backtrace FIND_OBJECT <- IPC_TASK <- QUEST_SERVER. Timing
  finding: the server reads shared pages only in processing bursts and
  reads nothing while the game idles at the prompt, so wall-clock-timed
  flips are unreliable — trigger from the read path
  (`tools/fault_injection_on_read.md`).
- **Page audit proven**: the same byte flipped while client pairs were
  flowing tripped the audit within the interval, exact byte reported
  with both pair positions.
- **Checker-1 cross-check**: with the audit disabled, the same flip was
  caught when the clone consumed the byte — ac1 differing by exactly
  the flipped bit, identical backtraces (DISPLAY_SCREEN <- START_TURN).
  One fault class, three independent detectors.

## 12. Where things live

| Concern | Location |
|---|---|
| Roles, scheduler policy, checker 1, WriteGate, copy registry | `hw/Lockstep.{hpp,cpp}`, `hw/MachineThread.cpp` |
| Rendezvous, mediation, replay + skip rule (checker 2) | `os/LockstepMediator.{hpp,cpp}` |
| Read/write funnels, input verification (checker 3) | `os/OSContext.{hpp,cpp}` (+ converted sites in `OSContextIPC/Task/FS`) |
| Clone copies, MirrorPage wiring, per-process tids | `os/OSProcess.{hpp,cpp}`, `os/MirrorPage.{hpp,cpp}` |
| Task-thread write gate + trace attribution | `os/ArrayPage.cpp` |
| Spectator echo | `os/OSContextFS.cpp` (`echo_to_clone_terminal`) |
| Trace types (`scalls`, `shared`, `lockstep`) | `os/Trace.cpp` |
