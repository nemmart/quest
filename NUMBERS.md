# Task numbers

**Claim a number here before writing a task.** Every task in `tasks/` is `NNN-name.sh`, and the runner runs them
oldest-first by number, so two sessions must never pick the same NNN. Git makes the claim atomic:

1. `git pull`, then **append one line** to the table below: the next number after the last line, your session, and a
   few words on the task.
2. Commit (`claim NNN`) and **push before you write the task**.
3. If the push is rejected, or the pull conflicts on that line, someone claimed it first: `git pull` again, take the
   next number after theirs, and push again.

A number in this table is never reused, even if its task is abandoned. Re-runs of a task use a letter suffix on the
same number (`044b-...`), claimed the same way.

## Already used before this registry (2026-11-01)

| numbers | project |
|---|---|
| 001-063 (with 044b, 045b, 047b, 049b) | Quest (the emulator project) |
| 064-068 | meshsorter (v1.1's sort, on the runner) |
| 069 | meshsim WORKER9: the standalone tracer's 2²⁹ `--validate` runs |
| 070-072 | meshsim WORKER8: the v1.2 sort at full size, its per-function watches, run_tests extras |
| 073-076 | meshsim WORKER4: the v1.2 simulator's phase 4 full runs, quick suite, phase 6 |
| 077 | meshsim WORKER4: the v1.2 simulator and sort combined |
| 078 | meshsim WORKER11: the v1.25 replayer's calibration |
| 079-080 | meshsim WORKER9: the v1.25 deliveries, EVM Zone and Cyclone at 2²⁹ |

## Claims

| NNN | session | task |
|---|---|---|
| 081 | meshsim WORKER9 | probe: what the runner sees of the w9 tarballs in /tmp |
| 069b | meshsim WORKER9 | re-run of 069 (the 2^29 --validate runs) once the runner sees /tmp/w9-tracer.tgz |
| 082 | meshsim WORKER9 | probe: /tmp/w9-tracer.tgz present and its SHA-256, before 069b |
