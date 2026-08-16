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

1. A session (working or planning) pushes `tasks/NNN-name.sh`.
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
