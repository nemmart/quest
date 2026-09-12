# q002 — 052 was not green: there is a fourth fail, and it is a real leg

Sep 12 2026, Stage A. **No action needed before 051 runs** — 051 is written,
verified and pushed on the assumption below. This is a correction to the
record plus one ruling I would like before Stage B.

## What I was doing

Building `tasks/051-p49-stage-a.sh`. Stage A's acceptance test is *"confirm a
green run now writes `DONE` and is not re-run"*, so before writing it I
checked what else stands between 052 and exit 0.

## What I found

**`results/052-p46-final/run.log` ends `TASK 052 RED (4 fails)`, not 3.**

The fourth is not a stale number. It is a leg:

```
inj-emu  cfg=emu  div=0  ...  end=I.STOP  want=FATAL  leg=FAIL
```

`results/052-p46-final/inj-emu.status` contains `FAIL`. The hand-written
`DONE` in that directory says *"The run is GREEN. All 16 legs leg=OK"* and
attributes the RED to exactly the three stale expectations. That is not what
the run produced. I am not treating 052 as a green reference run, and I do
not think `Order.md` or the REPORT should either.

**It is a regression, and it is new in 052:**

| battery | inj-emu end | verdict |
|---|---|---|
| 047b | FATAL | OK |
| 050 | FATAL | OK |
| 052 | **I.STOP** | **FAIL** |

**It is almost certainly the play-driver bug from q001 §1.2.** `inj-emu` is a
`play`-mode leg with `QUEST_INJECT=7016A896:-1:0x2006`, so it is a race
between the injected fault firing and the driver quitting the session. The
hooks show 052's run got substantially *further* through the game than the
two that passed:

```
047b / 050   bind=5  rebind=116  claim=359  frame_exit=1761
052          bind=8  rebind=136  claim=447  frame_exit=2708
```

Further through the game means it reached the stray `ESC` after `D` — which
detaches — before the injected fault was hit. `inj` (same inject, book
config) still ends FATAL, which is consistent: the two configs run at
different speeds and land on different sides of the race.

So the driver bug is not only costing coverage. **It is making battery
results nondeterministic**, and it has already produced one leg that silently
flipped verdict between two runs of the same script.

## The decision

Stage A's acceptance test is now unreachable by Stage A alone: if `inj-emu`
loses the race again, 051 exits 1, writes no `DONE`, and is re-run up to
three times at ~15 minutes — the exact waste Stage A exists to stop.

**Options:**

- **O1 — land 051 with the three fixes only; leave `inj-emu` red.** The
  refreshed lines are then provably correct and the remaining red is a real
  signal that Stage B is expected to clear. Costs up to two wasted retries
  if it loses the race.
- **O2 — widen `inj-emu`'s `want` to `FATAL,I.STOP` now.** Gets `DONE`
  immediately. But it is setting a `want` to what the tree happens to print,
  which PROMPT ruling 6 forbids, and it would hide the race permanently —
  the leg would pass whether or not the inject ever fires.
- **O3 — reorder: do Stage B before 051.** Fixes the race at its source
  first. But it inverts the prompt's ordering and means Stage B is scored
  against a battery whose expectations are still known-wrong, which is what
  Stage A exists to prevent.

**RECOMMENDATION: O1**, and it is what I have built. A red check with a
reason is worth more than a green one without, and this red is pointing at
the very thing Stage B is about to fix.

To stop O1's cost falling on a reader, 051 scores the three refreshed lines
on their own counter and prints:

```
-- P49 STAGE A verdict (the three refreshed lines, scored on their own) --
stage_a_refreshed_lines_failing=0 (want 0)  other_fails=N
STAGE A GREEN but the battery is RED for N unrelated fail(s) — ...
```

So whether Stage A succeeded is legible even if the task marker says RED.

## What I verified locally before pushing

All three corrected checks run green against the real artifacts, and all
three still have teeth. Dropping one row from `p33.tsv` and corrupting one
row's IR text flips the widened check to `p33=228/0`, `same pc set=NO`,
`same text=NO` — so it is comparing two independently generated artifacts,
not agreeing with itself.

051 sources `p46-ir7`, the same branch as 052, so the two are a clean A/B on
exactly the three lines. I confirmed the inputs the three checks read
(`quest.ir2.book/.stock`, `strings.ledger`, `p31/p32/p33.tsv`, `p33.ledger`,
`quest.arena`) are byte-identical between `p46-ir7` and `main`, so nothing
turns on the choice.

## One prediction, so this is scoreable

If `inj-emu` loses the race again in 051, `stage_a_refreshed_lines_failing=0`
and `other_fails=1`. If it wins, 051 goes fully green and writes `DONE`. I
expect the race to be roughly even and would not read either outcome as
information about Stage A.
