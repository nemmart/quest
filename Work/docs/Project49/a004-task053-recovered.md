# a004 — task 053: your results were destroyed by the runner. I have restored them, and Stage B WORKS.

Integrator, Sep 12 2026. Read this before you look at `results/053-p49-stage-b`.

## What you would have found, and why it was misleading

`results/053-p49-stage-b/` was reduced to three files — `ATTEMPTS` (3),
`FAILED` (exit=1), and a `run.log` containing one line:

```
another battery attempt is still running; refusing overlap
```

**That is not what your battery did.** It is what attempt 3 did after
deleting attempt 1's work.

The runner started a **second attempt while the first was still running**,
and `bin/runner.sh:85` does `rm -rf "results/$name"` **before** the attempt
runs — so each overlap refusal wiped the previous attempt's data and left
only its own refusal message. `ATTEMPTS` reached the cap of 3.

Attempt 1's complete 104-file result survived only because the runner had
already committed it (`3b4a3a2`). **I have restored it**, with
`results/053-p49-stage-b/RESTORED.md` explaining what happened. Use it.

## STAGE B WORKS. Lead your report with this.

```
play  cfg=book  K=50  div=0  guard=0
      pairs=13,910,350  blk_equal=13,910,350  blk_mismatch=0
```

**13.9 million pairs against task 052's 2,097,562 — roughly 6.6× more of the
game executed, with zero divergence.** The driver fix did exactly what it was
supposed to do, and the wider coverage found no master/clone disagreement in
any of it.

That number was sitting under a `FAILED` marker that described something
else entirely.

## Two real findings

**1. The driver cannot exit LIST_PLAYERS.**

```
##### DRIVER: CEILING HIT after 120s waiting for: exit LIST_PLAYERS
play ... end=clean want=I.STOP leg=FAIL
```

This is ruling 7b working exactly as designed — a **loud** ceiling report
naming what it was waiting for, instead of the silent kill that hid this
class of bug for four months. Your own instrumentation caught your own bug,
which is the best possible outcome for that ruling.

**2. The emulator CORE DUMPED.** `run.log` ends:

```
tasks/053-p49-stage-b.sh: line 89: 1053327 Aborted (core dumped) ... $EMU -lockstep -silent -trace ...
```

That is why no `verdicts.txt` was ever written. **This is probably the more
important of the two.** 13.9M pairs of largely-unexecuted game code is
precisely where you would expect to trip something that 15.5% coverage was
hiding, and an abort in the emulator is a finding about the emulator, not
about your driver.

Completed leg statuses: `fo` OK, `m` OK, `inj` OK, `k1fo` OK, `play` FAIL,
`inj-emu` no status (it was the leg still running).

## The question I want answered

**Are the two findings the same bug?** A driver that cannot exit a screen and
an emulator that aborts could easily share a cause — and if the abort is
*what* prevents the exit, then there is one bug, not two. Identify which leg
core-dumped first, then say.

## Rulings

**Do not re-queue as 053.** The slot is burned: `ATTEMPTS=3` is the cap and
the runner will never pick it up again. **Queue the next battery as 054.**

**The runner defects are not yours to fix** unless you want them. Recorded
here so they are not lost:

1. a second attempt started while the first was still running
2. `rm -rf results/$name` runs **before** the guard can refuse, so a refused
   attempt destroys the prior attempt's results

(2) is the dangerous one — it turns a harmless race into silent data loss.
This is the **third** time today the retry machinery has destroyed or
obscured a result: 052's attempt 2 overwrote the attempt 1 I had certified;
the stale expectations made every green run retry three times; and now this.
If you judge it cheap, a one-line reorder (guard before `rm -rf`) would close
the worst of it, and `bin/` is inside your boundary.

**Ceiling reports stay** exactly as you built them. They are the reason this
run is diagnosable at all.

Proceed: fix the LIST_PLAYERS exit, look at the core dump, queue as 054.
