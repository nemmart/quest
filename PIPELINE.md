# The Quest pipeline — repo, task queue, runner

*Established Aug 16 2026. Replaces the tarball ferry.*

## Layout

```
Work/          emulator (c_src), docs (designs of record, project prompts/reports)
Tools/         the Java analysis tools (build: javac *.java */*.java)
Disassembled/  generated artifacts (quest.dis, addrbook inputs, argmap, callsites, ...)
QUEST/         the game files (pristine; runs always scratch-COPY, never symlink)
bin/           runner.sh (the poller — never changes), battery.sh (the standing battery)
tasks/         the queue: NNN-name.sh, self-contained, run from repo root, oldest-first
tasks/hold/    parked tasks the runner ignores (manual gate)
results/       runner output: results/NNN-name/{run.log, DONE|FAILED, artifacts...}
```

## Protocol

1. A session (working or planning) **claims its number in `NUMBERS.md`** (append, commit, push; on a rejected push,
   pull and take the next), then pushes `tasks/NNN-name.sh`. Never pick a number without claiming it there.
2. The runner box (10+ core Linux server, dedicated non-sudo user,
   systemd unit in runner.sh's header) polls every 60 s, runs the oldest
   unresulted task under a timeout, commits `results/NNN-name/`, pushes.
3. Sessions poll for their result dir (git pull) and read
   run.log / battery_summary.txt. DONE/FAILED marker is the verdict.

## Session conventions (replaces tarball instructions in prompts)

- Work sessions: clone with the project PAT, branch `pNN-topic`, work,
  push the branch AND any task scripts; planning session reviews via the
  repo. Designs of record are still edited only by the planning session.
- The battery is `bin/battery.sh <outdir> [legs]` — parallel by default
  (one port per leg via QUEST_PORT), SERIAL=1 to debug. Pass criteria
  per the banded matrix (Work/docs/Project14/REPORT.md §6).
- In-container smoke tests stay small (build + book-load or one m leg);
  full batteries go to the runner via a task.

## Security posture

- The PAT is fine-grained: nemmart/quest only, Contents read/write.
  Rotate on expiry; revoke from GitHub settings instantly if needed.
- The runner user has no sudo and owns only the queue checkout; the
  systemd unit is hardened (ProtectSystem=strict, NoNewPrivileges,
  memory/task caps, per-task timeout).
- Every task script is in the commit log before it runs — auditable;
  park anything you want to eyeball in tasks/hold/ and move it out to
  release it.
- main can be branch-protected later if session pushes should gate
  through review; current posture is trust-with-audit.


## Queue order: letter suffixes sort first (Nov 6 2026)

The runner orders `tasks/` with a locale-aware `sort`, so a task whose number has a letter suffix runs **before** the plain
number: `113b` and `113c` run ahead of `113`, `110b` ahead of `110`. Found by WORKER28, whose 113 waited behind its 113b-113d.
**When order matters, claim plain numbers** (a fresh number for a re-run, not a suffix), or expect suffixed tasks to go first.
(The runner could sort with `LC_ALL=C sort` to fix it; not changed.)

## Runner retry behaviour (Aug 23 2026)

A task is considered handled only when `results/<name>/DONE` exists. A
task that left only a `FAILED` result (e.g. a bug in the task script,
since fixed and pushed) is RETRIED — up to `MAX_ATTEMPTS` (default 3,
override via env) — instead of being blocked forever by the stale result
dir. Each attempt count is tracked in `results/<name>/ATTEMPTS`
(committed, so it survives the self-heal reset). To force a full re-run
of a task that hit the attempt cap, delete its `results/<name>` dir and
push. NOTE: deploying this requires copying the updated bin/runner.sh to
the runner box and restarting the systemd unit.

## Persistent data on the runner (Nov 4 2026)

`/tmp` is wiped when the box reboots (a power bump on Nov 4 lost `/tmp/w9-deliveries`, the selected-windows tarball and
the hand-copied `/tmp/w9-delivery.tgz`). **Large inputs that later tasks read go to persistent data, never `/tmp`:**

- **The location (owner, Nov 4): `~questrunner/data`** (`/home/questrunner/data`). Under the hardened unit in
  `bin/runner.sh`'s header (`ProtectHome=read-only`) it is writable only once the unit's `ReadWritePaths` lists
  `/home/questrunner/data`. Until then tasks fall back to **`~/queue/data`**, git-ignored inside the checkout (inside
  `ReadWritePaths`; the runner's resets and `git clean results/` leave it alone). Every task resolves `$DATA` with the
  same write check (meshsim `WORKER20/tasks/data_dir.inc`), and the task that writes the data records which directory
  was used in its `results/.../data_path.txt`.
- **Current contents** (meshsim WORKER20, tasks 103 and 104): `w9-deliveries/` (the 2^29 replay deliveries of evm-zone-29
  and cyclone-29, about 90 GB, with `<program>.COMPLETE` holding each per-program manifest's SHA-256),
  `w9-deliveries-selected/` and `w9-selected-windows.tgz`. The SHA-256s are in those tasks' `manifest.sha256`.
- **The runner is started by hand (owner, Nov 4):** it is not set to autostart, so after a power bump it stays down until
  the owner restarts it, and whatever task was running was cut off mid-way. **Every task must be safe to re-run from
  the start after an interruption** (the runner retries an unfinished task, up to `MAX_ATTEMPTS`):
  - persistent data is written into a staging directory beside the target and moved into place only when complete, with
    a completion marker; a re-run removes stale staging, and starts from scratch or skips what is already complete;
  - readers check the marker or the SHA-256 and fail at once if the data is missing;
  - scratch stays in `/tmp` and is `rm -rf`ed at the start of every task; results go only to `results/<task>/`.
