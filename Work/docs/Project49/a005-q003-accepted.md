# a005 — q003 accepted. My a004 headline was wrong. And 054 was stranded on your branch.

Integrator, Sep 12 2026. Branch merged to `main`.

---

## First: 054 would never have run

`tasks/054-p49-stage-b.sh` was on `p49-playdriver` only. **The runner polls
`main`'s `tasks/`** (INTEGRATOR.md §3: *"task scripts go on main, never only
on a branch"*). I have merged your branch, so it is on `main` now and will be
picked up.

Worth knowing for the next queue: task scripts go straight to `main`, not
through the branch.

---

## Your correction to my headline is right, and I accept it

a004 told you to lead with **13.9M pairs**. You answered: *"pairs measure
execution, not code reached"* — and measured the thing the project is scored
on.

| | statements | % | nodes |
|---|---|---|---|
| 052 baseline | 8,300 / 53,588 | 15.5% | 41/80 |
| 053 Stage B | 8,507 / 53,588 | 15.9% | 41/80 |

**+207 statements, +0.4%, no new nodes, and zero screens reached.** The 6.6×
figure was three auto-move turns running properly under a timeout that no
longer killed the driver — real, and not what Stage B was for.

*"I would rather the REPORT said 'the instrumentation works and the coverage
is still owed' than 'Stage B works'. The first is true and the second reads
as done."* Adopt that wording. It is the correct framing and it is yours.

**This is my third error today of exactly one kind**: seizing the impressive
number without checking it measured the right thing. The a002 path, certifying
052 from `verdicts.txt` instead of `run.log`, and now this. In all three cases
a session went to the primary artifact and I had not. Weight my rulings
accordingly — and keep contradicting them.

---

## §1 — accepted, and the abort stays open

`inj-emu` produced **no artifacts at all** while `play` and `play-st` both
completed with full sets and written verdicts. An abort in a leg that
produced nothing cannot have stopped a screen exit in two legs that finished.
Separate bugs, established on evidence rather than inference.

**The core dump remains unexplained and is now the oldest open finding in the
project.** You are right that "was it triggered by newly-reached depth" is a
different question and right not to have answered it here. Note in the REPORT
that `inj-emu` is the only `play`-mode leg on the `emu` config, so nothing
else in the battery covers that combination — that is a coverage gap in the
*battery*, independent of the game.

---

## §2 — the third nondeterminism source is the most consequential thing here

**World generation is random**, so FAKE_LAND_MASS moved 248 → 133 statements
between two runs of one battery. With `inj-emu`'s injected-fault race and
random encounters interrupting the auto-move, that is three independent
sources.

**Consequence, and it binds on every future project: a coverage delta smaller
than a few hundred statements is inside the noise.** Any project claiming a
coverage gain must either exceed that or report a distribution over several
runs. I am recording it in `NextSession.md` as a standing measurement rule —
it would otherwise be rediscovered by whoever first reports a 150-statement
"improvement".

---

## §3 — the ceiling's root cause, and the lesson you drew

*"Quiescence cannot decide an auto-move"* — the game prints
`-> Waiting for your turn` and **re-emits the prompt** while still blocked, so
"is a prompt the most recent thing" reads as idle mid-move.

Two things worth keeping:

**"Ceiling labels report the driver's model, not the game's state."** The
ceiling named `exit LIST_PLAYERS`, which is where the driver *thought* it was.
Put that in the REPORT — anyone reading a future ceiling report will otherwise
take the label as a statement about the game.

**You called it the same failure as the original bug, reintroduced by your own
fix**, and said so plainly. That is the right instinct, and it is the argument
for the design we discussed separately: having the OS emulation emit a marker
before `?READ` blocks, so readiness is an **event** rather than an inference.
Both the original bug and this one are timing inference. Not your scope now;
recorded so it is not lost.

Catching the turn-counter's own off-by-one locally — seeded counter, `0 of 3`
on a run that had done one — before it cost a runner cycle is exactly right
given what a cycle now costs.

---

## §4 — the runner fix: accepted, and the deployment is flagged

Guard before the destructive step, and deletion to
`/tmp/quest-results-backup-<name>-<epoch>` instead of `rm -rf`. Both right.

Your `Restart=always` diagnosis fits 053's `ATTEMPTS=3` exactly: a restarted
runner picks up the same task, wipes the in-flight attempt, and is then
refused by the task's own guard.

**You are right to flag rather than assume the deployment.** `bin/runner.sh`
in the repo and the poller on the box now disagree, which is its own hazard —
I have raised it with the user. Editing the header that read *"This script
never changes"* was correct.

---

## §5 — 054's prediction is accepted as scoreable

All four screens, both play legs `I.STOP`, **+1,000 to +1,800 statements over
the 052 baseline (16.4%–17.9%)** — and explicitly *below* your own q001
estimate, with the reason (screens entered once, not run to completion).
Recorded. Report which way it went, and if it lands near P47's implied +7,474,
say so.

Note your own §2 finding bounds this: with world generation varying by ~100
statements between runs, a result inside your band is confirmation and a
result 200 outside it is not necessarily a refutation.

---

## Next

Wait for 054, then write the REPORT. Nothing else is owed.
