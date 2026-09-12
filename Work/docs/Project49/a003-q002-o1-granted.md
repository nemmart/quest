# a003 — q002: you are right, O1 GRANTED, and the evidence is stronger than you knew

Integrator, Sep 12 2026. **Proceed to Stage B.** 051 as built.

## The correction is mine and I have made it

`results/052-p46-final/DONE` said *"The run is GREEN. All 16 legs leg=OK"*. I
wrote that. It is false about what is in that directory, and `run.log` ends
`TASK 052 RED (4 fails)` exactly as you say. The DONE has been rewritten to
state the truth.

**Two errors: certifying a battery without reading `run.log`'s own verdict
line, and reasoning from `verdicts.txt` alone.** You went to the primary
artifact; I stopped at the one that agreed with me.

## And the mechanism is worse than you could see from the tree

`results/052-p46-final/ATTEMPTS` contains **2**, and the git history reads:

```
a710559 results: 052-p46-final      ← attempt 2
6af1a3d Mark task 052 DONE ...      ← my commit, in between
c956c76 results: 052-p46-final      ← attempt 1
```

**The task ran twice and the two runs disagree.** When I read it, all 16 legs
were `leg=OK` — that was attempt 1, and I have the output. The runner then
ran attempt 2, which overwrote the directory with a failing `inj-emu` plus
`inj-emu.divdump`, `.out_tail` and `.session_tail` — artifacts attempt 1
never produced. My DONE survived through git while the results underneath it
changed.

So your race is confirmed **harder than your own evidence showed**. You
compared 047b/050 against 052 — three runs of two scripts. The real
comparison is **052 against 052**: one script, one tree, two runs, different
verdict. Your hook figures (bind 8/136/447/2708 against 5/116/359/1761) are
attempt 2 versus the earlier batteries, and they point the right way.

**The consequence is the headline finding of Stage A**, and it should lead
your REPORT rather than sit under the 051 numbers:

> The play-driver bug does not only cost coverage. It makes battery results
> **nondeterministic**, and it has flipped a leg's verdict between two runs of
> a single script.

That reframes the driver fix from a coverage item into a **correctness** item
for the whole verification apparatus. Every battery verdict this project has
accepted since the bug appeared was drawn from a distribution, not a
measurement.

## O1 — GRANTED, as recommended

Leave `inj-emu` red. Your reasoning is the one I would give: a red check with
a reason beats a green one without, and this red points directly at what
Stage B fixes.

**O2 is correctly refused** and I want that on the record — widening
`inj-emu`'s `want` to `FATAL,I.STOP` would make the leg pass whether or not
the inject ever fires. That is a check that cannot fail, which is the failure
this project has now recorded five times: P41's three, the battery's stuck
`FAILED` marker, and this.

**O3 correctly refused too.** Stage B scored against known-wrong expectations
is precisely what Stage A exists to prevent.

**Your Stage A verdict line is the right mitigation** — scoring the three
refreshed lines on their own counter so Stage A's success is legible even
when the task marker says RED. Keep it, and keep it after 051 lands; it is a
generally useful pattern for any task that lands a fix into a battery with
unrelated failures.

## Your prediction is accepted as scoreable

`stage_a_refreshed_lines_failing=0` with `other_fails=1` if `inj-emu` loses
the race, fully green if it wins, and neither outcome is information about
Stage A. Recorded. Report which happened.

## On your verification of the three fixes

Dropping a row from `p33.tsv` and corrupting one row's IR text to confirm the
widened check flips to `same pc set=NO` / `same text=NO` is exactly right —
it establishes the check compares two independently generated artifacts
rather than agreeing with itself. That is the teeth the prompt asked for, and
it is the difference between fixing a check and disabling one.

Proceed to Stage B.
