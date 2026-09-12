# Attempt 1's results, RESTORED by the integrator — Sep 12 2026

This directory was reduced to three files (ATTEMPTS, FAILED, a one-line
run.log) by attempts 2 and 3. Restored here from git commit 3b4a3a2, which
is attempt 1's complete 104-file result.

## What happened

The runner started a SECOND attempt while the first was still running.
Attempts 2 and 3 each hit the task script's guard —

    another battery attempt is still running; refusing overlap

— but `bin/runner.sh:85` does `rm -rf "results/$name"` BEFORE the attempt
runs, so each refusal wiped the previous attempt's data and left only the
refusal message. ATTEMPTS reached 3, the cap, so the runner will never
retry 053 again.

**Two runner defects, and the second is the dangerous one:**

1. a second attempt was started while the first was still running
2. `rm -rf results/$name` happens before the guard can refuse, so a refused
   attempt DESTROYS the prior attempt's results

(2) converts a harmless race into silent data loss. Attempt 1 survived only
because the runner had already committed it to git.

## What attempt 1 actually showed — it is NOT a plain failure

**Stage B's driver fix works.** The `play` leg ran **13,910,350 pairs**,
`div=0`, `blk_mismatch=0` — against 2,097,562 pairs in task 052. Roughly
6.6x more of the game executed, with zero divergence.

**Two real findings:**

- **The driver cannot exit LIST_PLAYERS.** `play.driver`:
  `##### DRIVER: CEILING HIT after 120s waiting for: exit LIST_PLAYERS`,
  and `play.verdict` ends `end=clean want=I.STOP leg=FAIL`. This is ruling
  7b working exactly as intended — a loud ceiling report instead of a silent
  kill.
- **The emulator CORE DUMPED.** `run.log` ends
  `Aborted (core dumped)` at tasks/053-p49-stage-b.sh line 89, which is why
  no `verdicts.txt` was ever written. New crash on newly-executed code.

Leg statuses that did complete: fo OK, m OK, inj OK, k1fo OK, play FAIL,
inj-emu never produced a status.

## For whoever re-runs this

**Do not re-queue as 053.** The slot is burned (ATTEMPTS=3 = the cap).
Queue the next battery as **054**.
