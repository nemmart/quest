# Running the Emulator

```
./emulator [flags] <dir> <program 1> [<program 2> ...]
./emulator [flags] <program>              (single program, current dir as FS root)
```

The emulator is built in `c_src/` with `make`. Programs boot in the order
given and run concurrently against one emulated AOS/VS.

## Positional arguments

**`<dir>`** — the filesystem root: a host directory containing the `.PR`
program binaries, `.ST` symbol tables, and game data files (e.g. the
`QUEST` directory). All emulated file paths resolve inside it. Data files
are held in memory while running and written back at shutdown.

**`<program ...>`** — one or more programs to boot, each the name of a
`.PR` file in `<dir>` (the `.PR` extension is optional; names are
case-insensitive). Example: `QUEST_SERVER @QUEST @QUEST` boots the game
server plus two game clients.

**`@` prefix** — a program name starting with `@` gets a **telnet
terminal**: the launcher blocks at startup until a client connects to
**port 8781**, and that connection becomes the program's console.
Connect with a DG D200-compatible terminal emulator. Programs without
`@` use the launching console (stdin/stdout). With several `@` programs,
connections are accepted in command-line order — so in lockstep mode
(below) the **first** connection is the master's terminal.

**Instance labels** — a program launched once is labeled by its name
(`QUEST`); the same program launched N times becomes `QUEST1`,
`QUEST2`, ... in command-line order. These labels appear in traces,
logs, and divergence reports.

## Flags

Flags must come **before** the positional arguments.

**`-lockstep`** — dual-emulation verification mode (see
`docs/EmulationVerification.md`). Requires exactly one program to be
listed twice: the first instance becomes the **master**, the second the
**clone**; all other programs (the server) run normally. Both instances
execute in verified lockstep; type only into the master's terminal (the
first telnet connection) — the second window is a read-only spectator
view of the same session. Any divergence halts both engines with a
report on stdout (one-line notice on stderr).

**`-silent`** — with `-lockstep`: single-terminal play. The clone gets a
discarding null terminal instead of the second telnet connection, so
only one connect to port 8781 is needed. All verification is unchanged
(clone output is compared at mediation before any terminal is involved);
only the spectator display is dropped. Ignored (with a warning) without
`-lockstep`.

**`-trace FILE -types TYPE[,TYPE...]`** — write a sequenced trace to
`FILE`. Both flags must be given together. Types:

| Type | Records | Cost |
|---|---|---|
| `scalls` | every system-call entry/exit: caller label, tid, call name, ACs, error | light |
| `shared` | every write to shared-mapped file pages: caller, page, offset, value; task-thread handler writes tagged `(handler)`; clone-copy pages labeled `...~clone` | moderate |
| `lockstep` | one line per verified master/clone batch pair | **heavy** — ~1 line per 500 client instructions; debugging only |
| `rtcalls` | every PL/1 runtime entry reached via native stub dispatch (clone only — the master and non-lockstep runs have no stubs). One line per call: instance, entry symbol, return address. Includes RT-internal calls. | light |
| `redirect` | M4a: one line per redirected WSAVS/WRTN/unwind-drop of an address-book routine (clone only): routine, area wfp, argc, frame, real/shadow wsp, master wfp, depth. | light |
| `gcalls` | one line per LCALL/XCALL into the GAME range [QUEST, ?CHAR_TO_UNSIGNED): target pc + symbol, argc, call site; both roles. The routine-coverage instrument (Project 13): for every live book routine, `gcalls` count == `redirect` WSAVS count. | moderate |

## RT coverage

For QUEST processes (all modes), every executed pc in the runtime range
(?CHAR_TO_UNSIGNED..?NTOP) is marked in a per-instance bitmap and written
at shutdown to `rtcov-<label>.txt` (address + owning-routine attribution).
Safety net for entry paths the stubs don't see; also drives the Step-1
worklist and completion check (see SessionPlan.md).

Under `-lockstep`, batches additionally break per the CROSSINGS-ONLY
checker (Aug 13 2026 — docs/CrossingsChecker.md; generation history in
docs/CheckerHistory.md): L0/L1 fabric entries pair as before
(untranslated at the entry, translated leaves at the post-call point),
and every L1↔L2 crossing pairs in BOTH directions — at the L2 entry pc
(dispatch is deferred so argument state is compared under the strict
count rule) and at the L2→L1 exit (post-call point or transfer target,
"native_span" in the lockstep trace, instruction-count compare
exempted). Interior L2→L2 is invisible by rule. rtcalls lines from
translations are tagged "(native)". See RTStubs.hpp / RTBridge.hpp for
the design.

## Typical command lines

Normal single game (server + one client over telnet):

```
./emulator QUEST QUEST_SERVER @QUEST > log
```

Lockstep play session (fast settings — no tracing, stdout to a file,
single terminal):

```
./emulator -lockstep -silent QUEST QUEST_SERVER @QUEST @QUEST > log
```

Then telnet once to port 8781 and play. Drop `-silent` to get the
two-terminal variant: telnet twice, play in the first window, watch the
second (read-only spectator view).

Lockstep divergence hunting:

```
./emulator -lockstep -trace quest.trace -types scalls,lockstep \
           QUEST QUEST_SERVER @QUEST @QUEST > log
```

For the finest divergence localization also lower the client batch-size
constant in `Machine::run` (500 → 10-100) and rebuild; divergence is
then pinned within a few dozen instructions.

## Notes

- stdout carries per-syscall packet dumps and all divergence reports —
  redirect it to a file; the console stays readable via stderr.
- Port 8781 is fixed. A leftover emulator process holding it makes the
  next launch appear to hang at "Waiting for terminal client".
- Shutdown: terminate the emulator process; modified data files are
  written back to `<dir>` (`FS::save_all`). Under lockstep, note that
  master and clone shared-page state exists in two copies; the real
  (master/server) pages are the canonical ones saved.

## Debugging aids

**Capture (empirical footprint diffing)** — `debug/Capture.{hpp,cpp}`:

```
QUEST_CAPTURE=<entry-pc-hex>        arm A/B snapshots at a routine entry
QUEST_CAPTURE_DEST=<word-addr-hex>  fixed second window (default: entry ac2)
```

`QUEST_CAPTURE_DEST` is needed for routines whose footprint sits at a
static address rather than a caller-supplied one — e.g. I.LOCK, whose
whole non-frame footprint is the lock object at 0x70000200. Diff the
master's `RETURN` blocks against the clone's `NATIVE` blocks.

**Fault injection** — `os/OSContextFS.cpp` (temporary):

```
QUEST_FAIL_OPEN=<filename-substr>   fail ?OPEN_FILE for the CLIENT only
```

Client-only: the label must start with `QUEST` and not be
`QUEST_SERVER` (note "QUEST" is a prefix of "QUEST_SERVER" — a naive
substring test fails the server's opens too, which breaks the shared
data the game depends on). Drives `?LIB_ERROR` on demand;
`QUEST_FAIL_OPEN=USER_DATA_FILE` plus the `L` -> `P` command reaches
LIST_PLAYERS' handler (ON_ERROR_CATALOG #13).

**Output ordering** — backtraces go to stdout, exception lines to
stderr. Run under `stdbuf -o0 -e0` when you need them interleaved
correctly; otherwise a crash can lose the buffered backtrace entirely.

**Scripted play** — background the emulator and run the telnet driver
in the *same* shell invocation (a backgrounded process may be reaped
when the invocation ends). Login sequence: `CL` / `Claude` / `quest` /
`Y` / any key / class `F`. Always scratch-copy `QUEST/` first — data
files are written back at shutdown.

**Run-to-run coverage is BANDED, not fixed** (Project 14 finding):
wall clock enters via `?GTOD` (mediated — master executes, clone gets
checked copies, so both engines agree while runs differ) and feeds
world evolution (the server writes WORLD_DATA during a run; e.g.
TOWER_ATTACK's guard cell). Any redirect/gcalls count downstream of
world state wobbles across runs even from identical starting files —
first seen as DIST 596–604 across four same-tree `m` runs. Regression
criteria therefore pin: 0 divergences, gcalls == redirect per live
routine (strict), named signatures, probe count 0, and per-run
endpoints; count totals are recorded as bands, with only demonstrated
login-phase anchors (READ_IN=4, LOGON=1, GET_INPUT=8, …) compared
exactly. Do not chase a ±handful count delta on a world-touching
routine; do chase a broken invariant. Details: Project14/REPORT.md §6.

Failure modes and what is not implemented: **docs/UNIMPLEMENTED.md**.
