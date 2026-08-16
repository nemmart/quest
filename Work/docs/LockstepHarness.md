# Lockstep Harness — Design and Implementation

> **Generation note (Aug 13 2026).** This document describes the
> Generation-1 (M2/entry-keyed era) checker as built. The batch
> BREAK RULES have since been rekeyed on layer transitions — see
> docs/CheckerHistory.md (lineage) and docs/CrossingsChecker.md
> (current). Scheduler, mediation, mirror pages, and pair-compare
> mechanics below remain accurate.

*Status: IMPLEMENTED and validated (steps 1-3, input verification,
spectator terminal; fault-injection proven). For the precise as-built
verification reference — every checker, mediation rule, corner case and
known window — see `EmulationVerification.md`. Sections below are kept in
design-document form with per-section status; "Step 2 Findings", "Step 3
Implementation", "Mediated-Call Input Verification", "Spectator
Terminal", and "Performance notes" describe what was actually built and
learned.*

## Goal

Run two QUEST instances — **the master (A)** and **the clone (B)** — in one
emulator process against one QUEST_SERVER, such that both observe an
*identical, deterministic* environment and therefore execute identically.
Any observable difference is a bug in the harness (now) or in a translation
(Phase 2 proper). The assumption under test, forever after: **if everything
matches, the translation is faithful.**

## Shared-Page Layout

Per shared data file (SHARED_DATA_FILE, WORLD_DATA_FILE, CASTLE_DATA_FILE):

- **A maps the canonical FSFile ArrayPages** (unchanged from today).
  Shutdown write-back (`FS::save_all`) is therefore unchanged.
- **B maps a private ArrayPage copy**, cloned at ?SPAGE time.
- **QUEST_SERVER maps a `MirrorPage`** wrapping (canonical, B-copy):
  - **read** → read both; USE the real value, COMPARE the copy; mismatch ⇒
    divergence: log page/offset/values + reader backtrace, halt.
    IMPLEMENTED (was deferred; see "Compare-on-read" below).
  - **write** → write both. Both clients thus receive every server write
    identically, and (by scheduling, below) only while parked at identical
    execution points.

Client writes land only in the client's own mapping. B's writes are checked
against A's at the moment the server *consumes* the data (MirrorPage read);
silent drift in unconsumed bytes is caught by the pair-boundary page audit
(every 16 pairs, byte-compare of all registered (real, copy) pages under
the still-held pair mutex — `Lockstep::maybe_audit_copies`).

## Scheduler

All instruction execution already funnels through the single MachineThread
worker in ~1000-step batches; task threads block in `run_steps` until their
batch completes. The scheduler is an admission gate on that queue. Roles:
SERVER / CLIENT_A / CLIENT_B, assigned at launch.

Rules:

1. **Client lockstep pairing.** Client batches are admitted in A/B pairs:
   run A's batch, run B's batch (same task-ordinal pairing within the
   processes), then compare slice outcomes: ending pc, syscall-trap status,
   instructions consumed. Any difference ⇒ divergence report. (Two
   identical emulations on identical inputs must match cycle-for-cycle.)
2. **Server quantum.** After each client pair-slice, the server (if it has
   a pending batch) gets one batch. Strict interleave while clients are
   runnable — the server is never starved, and never runs *during* a
   client slice.
3. **Client-blocked free-run.** When both clients are blocked (terminal
   ?READ, ?WDELAY, rendezvous wait), the server runs continuously. This is
   the gameplay steady state. Safe because both clients are parked at
   identical points for the whole free-run.
   **Resuming clients: park the server first.** When the blocking
   condition clears (terminal input arrives, ?WDELAY expires), the clients
   are NOT released immediately — the scheduler first lets the server's
   in-flight batch complete and holds its next batch. Only with the server
   parked at a batch boundary is input delivered and the client pair
   released. Rule 2's invariant — the server never runs during a client
   slice — must hold across this transition, or a server write could land
   between A's slice and B's.
4. **Never block the worker.** All waiting happens on task threads
   (condition variables in the gate / rendezvous), never inside the worker.

Note: translated (native) code in Phase 2 proper has no instruction count;
client slice boundaries then degrade to syscall-only sync points for
translated stretches. The quantum applies to emulated execution.

## Syscall Handling: LOCAL vs MEDIATED

**Principle: master executes, clone gets checked copies.** Everything
outbound from the clone (write payloads, message contents, call args) is
*verified* against the master's; everything inbound (read data, result
ACs, errors, packet results) is *copied* from what the master got. LOCAL
calls are the one exception: both execute, because the effects are
internal process structure and determinism guarantees they match.

Client syscalls rendezvous: when a client task traps, it parks until its
counterpart arrives at the same trap. First check: **same call, same ACs,
same packet contents.** Mismatch ⇒ divergence report (both positions +
backtraces). Then, by class:

- **LOCAL — execute in both.** Deterministic calls that mutate process
  structure and must exist in each instance: memory (?MEM, ?MEMI), task
  (?TASK, ?KILAD, ?IXIT), mapping (?GSHPT, ?SSHPT, ?SOPEN, ?SPAGE — B's
  ?SPAGE is where its private copy is cloned), ?WDELAY (one real timed
  wait, release both together; the wait itself is world-noise we
  synchronize, the call is otherwise local).
- **MEDIATED — execute once on A, replay into B.** World-facing calls:
  terminal ?READ/?WRITE, file ?OPEN/?CLOSE/?READ/?WRITE, IPC (?ISR),
  process identity (?PNAME, ?DADID, ?CON, ?DCON), ?GTOD (unobserved in
  QUEST, but mediated if it ever appears), ?RETURN/?UPDATE. Replay = same
  error code, same result ACs, same caller-memory side effects.

**Blocking mediated calls (?ISR):** no special scheduling — a client
pended inside a syscall (A in the real ?ISR, B at the rendezvous) counts
as blocked, so the server free-runs, replies, and parks at its next
boundary before the pair is released. The reason ?ISR is mediated at all:
the server must see exactly ONE client. If B's ?ISR actually sent, the
server would process each request twice and treat the mirror as a second
player.

**Memory side-effect capture:** during A's execution of a mediated call,
the OSContext's memory writes are recorded (address, width, value) and
replayed verbatim into B's memory. Addresses are identical by the lockstep
invariant. This uniformly covers ?READ buffers, packet result fields, etc.

**Identity example:** pids genuinely differ between A and B. B *observes*
A's pid via replay; the harness keeps routing by real pids internally. Any
call not in the matrix ⇒ hard error at rendezvous, so the classification
can never silently drift.

SyscallHandling.md's per-call table gets a new column: LOCAL / MEDIATED.

## Terminal Input

One real telnet session. Input is delivered to both clients' ?READ at the
rendezvous (A executes the real read once; B gets the replay). Output is
committed once from A; B's ?WRITE payload is compared against A's —
mismatch ⇒ divergence (this is the primary gameplay-visible check).

## Divergence Report

On any mismatch (slice outcome, call/args, write payload, MirrorPage read):
halt both clients, report both engines' positions (pc, call, tid),
backtraces via the existing debug/CallStack machinery, and the specific
mismatched data. Asymmetric progress (one client at rendezvous X, the
other reaches rendezvous Y ≠ X) is detected by the pairing gate itself.

## Implementation Order

1. **Scheduler gate** — DONE. MachineThread pairing + role tagging; pair
   comparison covers result pc, real trap pc (0x30000000 sentinel batches),
   AC0-3, carry, instruction counts; reports wsp/wfp/psr.
2. **Syscall rendezvous + LOCAL/MEDIATED + replay capture** — DONE.
   Validated through the full login sequence with input typed only into the
   master's terminal; the server sees exactly one client. See "Step 2
   findings" below.
3. **Private B copies + server MirrorPage** — DONE. See "Step 3
   Implementation" below. Compare-on-read — initially deferred over a
   theoretical master-handler-write/replay window — is now ENABLED: the
   OSContext write funnel dual-writes any mediated handler write to a
   mirrored shared page (real + copy under one WriteGate hold, replay
   skips it as delivered), closing the window at the source; off-worker
   MirrorPage reads take the WriteGate so the two-sided read cannot
   interleave a pair. A pair-boundary page audit additionally catches
   silent drift. Both fault-injection proven
   (EmulationVerification.md §11, tools/fault_injection_on_read.md).
4. **Milestone:** full play session, checker green, fault injection.
   Partially demonstrated: a flipped replayed terminal-input byte trips
   the checker within ~50 instructions of consumption. Remaining: longer
   play sessions (user playtest), multi-command coverage, and a
   systematic perturbation pass.

## Step 2 Findings

Fixes and hardening that came out of step-2 debugging:

- **Per-process tid allocation** (`OSProcess::next_tid`, base 100): master
  and clone generate identical tid sequences, so identity writebacks
  (?UIDSTAT ?UTID, ?TASK ?DID) are deterministic and those calls stay
  LOCAL. Previously tids came from a global OS counter and differed
  between the clients.
- **Pair-span shared-write lock** (`Lockstep::shared_write_mutex`): the
  worker holds it across each master+clone pair; any task-thread page
  write (syscall handler) takes it briefly per write via a gate in
  `ArrayPage::write_*`. Handlers therefore cannot mutate pages between the
  two halves of a pair. Blocking handlers never hold it across a wait, so
  ?ISR-style call/reply cycles cannot deadlock.
- **Replay skips physically shared pages**: mediated-call writes captured
  on the master are not re-applied to the clone when the target page
  object is identical in both memories (the write already landed;
  re-applying later would stomp newer server updates).
- **Honest shared-write trace attribution**: handler writes are tagged
  "(handler)" via a task-thread label (`Lockstep::task_thread_label`)
  instead of blaming whichever batch occupies the worker
  (`current_machine` is only meaningful on the worker thread).
- **Client batch size 100** under lockstep (server batches stay 1000), so
  a divergence is localized to within ~100 instructions.

### Step-2 boundary: shared-page read-modify-write

With pages physically shared, the clone observes the master's instruction
writes. Idempotent stores (the login path) are harmless — the clone
rewrites the same values. The first shared **read-modify-write** breaks
lockstep: during character creation, `INIT_OBJ_TBL` appends to a shared
string (load length at SHARED_DATA_FILE:123, add, store back). The master
half runs first (length 0x1C -> 0x20); the clone half then reads 0x20 and
stores 0x24, and registers diverge within the batch despite identical
code. This is inherent to physical sharing and is exactly what step 3
exists to fix. Reproduce: create a fresh character; divergence trips in
INIT_OBJ_TBL (~QUEST+0x5xx caller) a few batches after the creation ?ISR.

## Step 3 Implementation

Landed as designed, with these specifics:

- `OSProcess::lockstep_shared_page` resolves the page object for every
  shared mapping: MASTER maps the real file page; CLONE snapshots a
  `private_copy` per writable shared data page under a world-pause and
  registers it (`Lockstep::register_copy`); SERVER maps a
  `MirrorPage(real, copy)` when a copy already exists (late mapping), and
  `mirror_server_mappings` rewraps the server's *live* mappings when the
  clone registers (located via the `shared_pages` bookkeeping, no address
  scans). Shared code (exec) and read-only mappings stay physically
  shared. Copies inherit the trace label with a `~clone` suffix.
- `os/MirrorPage` writes both pages under one reentrant
  `Lockstep::WriteGate` hold so a pair can never observe the real page
  updated but the copy not; reads come from the real page.
- The worker now holds `shared_write_mutex` for **every** batch (not just
  pair spans, which it still holds across): this is what makes the clone's
  page-copy snapshot safe against server batches (no torn copies) and
  makes MirrorPage rewrapping safe against concurrent page-pointer use.
  The mutex is never held while the worker waits for work.
- `Lockstep::WriteGate` replaced the ad-hoc ArrayPage gate: engages only
  off the worker thread, reentrant via a thread-local depth so MirrorPage
  can hold it across its two nested ArrayPage writes.
- The mediator's replay page-identity check does the rest automatically:
  clone pages are now distinct objects, so mediated writes captured on the
  master (including handler writes to shared pages, e.g. ?ISR receive
  buffers) replay into the clone's copies.

Validation: scripted session through login, character creation (the
previous RMW boundary), class selection, and gameplay (movement, map
rendering) — ~1.4M verified pairs, zero divergences. Fault injection:
perturbing a boot-time copy byte in server-hot areas is self-healing (the
mirror path rewrites it) — perturbations must target data the clone
consumes; a flipped replayed input byte trips the checker immediately with
a clean two-sided report.

## Mediated-Call Input Verification

The mirror image of the write-log: every caller-memory read a mediated
call's handler performs on the master (packet fields, buffer bytes,
strings -- via the `mem_read_*` funnel in OSContext and the two direct
IPC loops) is simultaneously read from the clone's memory and compared.
This verifies all outbound data byte-for-byte with no per-call knowledge:
every ?WRITE variety's payload (plain, delimited/0xFFFF, extension-packet
cursor writes), ?ISR/?ISEND request content, ?OPEN filenames, and all
packet arguments. A mismatch aborts with the exact byte range, both
values, and both backtraces. The clone is parked at the rendezvous while
the master executes, so its memory is stable to read.

Two exclusions (see `OSContext::read_verifiable`): byte ranges the call
has already written (handlers re-read their own results, e.g. for
logging; the clone receives those via replay), and pages in the mirrored
shared region (server mirror writes race a two-sided read; clients'
shared-region agreement is enforced by the pair checker instead).

Validated: full session green with verification armed (no false
positives); a single corrupted clone ?WRITE payload byte is caught at
exactly that byte with the emitting call in the backtrace.

## Spectator Terminal

The clone's terminal ?WRITEs are mediated away, so its window would stay
blank. `OSContextFS::echo_to_clone_terminal` mirrors the master's terminal
output (payload and cursor-positioning writes) onto the clone's socket, so
the second telnet window shows the live session. Host-side only: no effect
on emulated state or lockstep semantics. Echo failures (spectator window
closed) permanently disable the echo without disturbing the master's
write. IMPLEMENTED: single-terminal launch via the `-silent` flag — the
clone gets a discarding null terminal (`FSNullStream` in Launch.cpp)
instead of the second telnet connection, and the echo discards into it.
Verification is unaffected: clone output is compared at mediation
(checker 3) before any terminal is involved. Without `-silent` the
two-terminal spectator behavior remains available.

## Step 3 Design (original notes)

- **Clone private copies:** when a CLONE-role process maps shared *data*
  pages (?SPAGE path in `OSProcess::map_file`, `shared && write_perm`),
  map `private_copy(page)` instead of the file's page. Shared *code*
  (read-only/exec) stays physically shared. Clone instruction RMW then
  hits its own copy; the existing replay page-identity check automatically
  starts applying mediated writes to the clone copy (pages no longer
  identical).
- **Server MirrorPages:** a `MirrorPage : Page` wrapping the real page
  plus the clone's copy; server writes land in both (its instruction
  writes already serialize against pairs via batch boundaries), reads
  come from the real page, optionally comparing against the copy to catch
  clone divergence at the point the server would consume it. Server
  mappings are wrapped when the clone maps its copies (registry keyed by
  (FSFile, page#)).
- **Master handler writes to shared pages** (e.g. ?ISR receive buffers in
  the shared region) land on the real page; the replay applies them to
  the clone copy — no extra work needed.

## Open Questions

- RESOLVED: `Machine::run_steps` decremented `count` twice per
  instruction (a bug; batches ran at half their nominal size). Fixed —
  batch counts are now true instruction counts. Client lockstep batches
  are 500 instructions (drop to 10-100 when hunting a divergence; the
  constant is in `Machine::run`), server batches 1000.

## Performance notes

Pair overhead is two scheduler round-trips per client batch, so batch
size dominates interactive speed: the step-2/3 debugging configuration
(batch 100, halved to 50 by the double-decrement bug, plus `-types
lockstep` writing one trace line per pair — ~93MB over a short session)
made gameplay crawl. For normal play run without the `lockstep` trace
type (and ideally redirect stdout to a file; the per-syscall packet dumps
are large). For divergence hunting, re-enable `lockstep` tracing and
shrink the client batch.
- Task pairing across A/B when tasks are created dynamically (?TASK order
  within a slice should be deterministic — verify).
- ?INTWT (console-interrupt task, tid 102): parked all session; LOCAL
  (each instance parks its own) with delivery, if ever, mediated.
- RESOLVED: B's non-consumed shared-page writes are audited by the
  pair-boundary page diff (every 16 pairs), complementing consumption-time
  compare-on-read.
