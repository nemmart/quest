# Project 49 — THE PLAY DRIVER, THE LOGIN FIXTURES, AND THE BATTERY'S EXPECTATIONS

## GOAL

Make the lockstep battery **exercise more of the game** and **report honestly
when it does**. Three things, in priority order:

1. **Fix the play driver.** Every play leg since task 034 has ended by
   SIGTERM; four screens are never visited by any scripted leg.
2. **Add the login legs.** Character creation, and an operator login — the
   two highest-value coverage additions the project has.
3. **Fix the battery's stale expectations (task 051).** They make the script
   exit 1 on a green run, which means no `DONE`, which means the runner
   re-runs it up to three times at ~15 minutes each.

**The number that defines success: statement coverage of the book, measured
before and after, by the same method.** P47's baseline is **8,305 / 53,588 =
15.5%**, 41 of 80 call-graph nodes, 16 of 20 leaves. P47 costed the driver fix
at **+7,474 statements** and the two login fixtures at **+4,989**. Those are
predictions made by someone else; report what you actually get, and if they
were wrong say so plainly.

This project writes **no game C, no IR, no compiler**. It is coverage and
instrumentation.

---

## Why this outranks translating routines

`DESIGN.md` §8 makes **L1 — behavioural equivalence under lockstep** the
primary acceptance path, and L1's entire oracle is gameplay. At 15.5%
statement coverage, L1 can validate a small fraction of the program no matter
how good the compiler is. P47's twelve proposed first routines total 1,768
statements — **3.3% of the book** — while the program's mass sits in subtrees
(ATTACK, OP_EDIT, MOVE_PLAYER, CAST) that **no scripted leg reaches at all**.

So: more coverage is worth more right now than more routines. That is the
whole reason this project exists and is scheduled where it is.

---

## Context of record — read in this order

| path | why |
|---|---|
| `docs/Project33/FINDINGS_SUMMARY.md` §2 | **The driver bug, with the fix already scoped.** Start here |
| `docs/Project13/drive.py` | the driver itself, mode `play` |
| `docs/Project47/Coverage.md` | the partition: what is reachable, what is L2-only, what the driver fix would buy |
| `docs/Project47/a002-character-creation-reaches-pick-x-y.md` | the character-creation route, and why it needs no data fixture |
| `docs/Project47/Order.md` | the ranking your work is about to move |
| `tasks/052-p46-final.sh` | the battery of record (16 legs); task 051's three stale checks are at lines 179, 212, 221 |
| `results/052-p46-final/` | a green run, with `DONE` and the diagnosis in it |
| `docs/Run.md` | launch recipes and flags |
| `docs/CheckerHistory.md` | Gen 6.2's owed items |
| `bin/runner.sh`, `PIPELINE.md` | the retry rule (`MAX_ATTEMPTS`, `DONE`) |
| `docs/METHOD.md` | law for implementation sessions |

**Do not read `docs/attic/`.**

---

## Carried-in rulings and facts

1. **The driver bug, as diagnosed** (FINDINGS_SUMMARY §2): after the auto-move
   the keys O / D / L / H / ESC are sent but never land on the command prompt,
   so HELP, OBSERVE, DISPLAY_MAGIC and LIST_PLAYERS are never visited and the
   game is never quit. The fix is to read `play/session.log` from any battery
   — what is actually on screen when each key is sent — and adjust waits and
   keys until the sequence reaches the prompt, ending with a real quit.
2. **A play leg must end `I.STOP`, not `clean`.** `end=clean` was a silent
   kill. Change the play/play-st verdicts to *require* `I.STOP`.
3. **Character creation needs NO data fixture** (P47 a002). Log in with unused
   initials; the game prompts *"What would you like your character to be — 'W'
   = wizard, 'C' = cleric, 'D' = druid, 'F' = fighter 'B' = barbarian"*
   (literal 0x7017816B, block 70178246 in START_TURN); answer with one letter.
   That reaches **PICK_X_Y, REPOSITION, PLACE_PLAYER, DIED and part of
   START_TURN**, including two of the four unreached leaves.
4. **`DIED` is not the death routine.** It is *"(re)initialise a character and
   place them"*, reached from death AND from first creation. Do not reason
   about it as death-only.
5. **Task 051's three stale checks**, with what is actually true:
   - line 179: `embeds_book` want 729 → **557**; `embeds_stock` 2608 →
     **2436**. Justified independently: the same file's P33-B section already
     wants 557 and gets it.
   - line 212: `string_statements` want 1593 → **1689**; literals 857 →
     **871**.
   - line 221: **structurally** stale, not mis-numbered — it diffs lower.py's
     ledger against `p31.tsv` + `p32.tsv` and never consults `p33.tsv`, so it
     cannot pass on any post-P33 tree. **Widen it; do not retune it.**
6. **DO NOT set a `want` to whatever the tree currently prints.** That
   converts a test into a tautology: it would pass by construction on any
   tree, including a broken one. Every number needs a reason, the way (5)'s
   first one does. If you cannot justify a number, **say so and leave the
   check red** — a red check with a reason is worth more than a green one
   without.
7. **Coverage must be measured the same way before and after**, or the
   comparison is worthless. P47 used the union of `first execution of block`
   lines over all legs, with `compiler/callgraph.py --coverage`. Use that.

---

## Part 1 — PLAN GATE

Write `docs/Project49/q001-plan-gate.md`, push to main, **STOP**. Report:

1. **What `play/session.log` actually shows** — where each key lands, and
   your diagnosis of why the sequence stalls. This is the thing nobody has
   looked at; everything else follows from it.
2. **Your driver design**: the full key sequence you intend, how you will
   make it robust to timing rather than tuned to one run, and how it ends in
   a real quit.
3. **The login legs**: how a leg gets a fresh set of initials that is unused
   (the user data file persists between runs — say how you handle that), and
   what the operator login needs.
4. **Task 051**: your justification for each replacement number, and your plan
   for the `p33.tsv` widening.
5. **Your predicted coverage after**, against P47's +7,474 / +4,989, so the
   estimate can be scored.
6. **Anything in the diagnosis that does not survive contact with the log.**

STOP. Wait for `a001`.

---

## Part 2 — Build, in this order

**Stage A — task 051.** Small, and it unblocks honest verdicts for everything
after it. Land it and confirm a green run now writes `DONE` and is not
re-run.

**Stage B — the driver fix**, then re-run the 16-leg battery.

**Stage C — the login legs**, then re-run.

**Stage D — the standing coverage verdict.** Add the "IR statements never
executed by any leg" line (CheckerHistory Gen 6.2) so coverage is visible on
every battery from now on, not just when someone runs a census.

---

## Boundaries — BINDING

1. **You may WRITE:** `emulation/` (driver, tests, verdicts), `tasks/`,
   `bin/`, `docs/Project13/drive.py`, `docs/Project49/**`,
   `docs/CheckerHistory.md`.
2. **Do NOT touch** `compiler/` (P48's, just landed), `game/`,
   `Disassembled/`, `docs/IR.md`, `docs/Provenance.md`,
   `docs/Project44/DESIGN.md`, `quest.ir2.*`, `quest.addrbook`,
   `quest.arena`. **The IR does not change in this project** —
   FINDINGS_SUMMARY §2 says so explicitly, and if you find yourself needing
   an IR change, that is a STOP-and-report.
3. **Nothing else is running.** If you need something outside the boundary,
   ask rather than assume.
4. If a new leg produces a **divergence**, that is a real finding and a
   STOP-and-report — it means the clone and master disagree on code that has
   never been exercised before, which is exactly what this project is for.
   Do not tune it away.

---

## Part 3 — Report

`docs/Project49/REPORT.md`:

- **coverage before and after**, same method, in statements and in nodes, and
  the per-leg breakdown
- **your prediction vs P47's** (+7,474 / +4,989) and vs your own gate estimate
- every divergence or new failure the wider coverage exposed — **these are
  the most valuable lines in the report**
- task 051: each number and its justification; anything you left red and why
- **whether `Order.md`'s ranking changes.** Half of it is coverage-based and
  you are about to move the inputs. Say which routines gain or lose a
  behavioural oracle.
- what is still unreached, and your judgement on how much of it is reachable
  by scripting at all versus needing a different approach

---

## Coordination — questions and the plan gate (BINDING)

Write `docs/Project49/q00N-short-title.md`, commit, **push to main**, then
**STOP and tell the user it is there.** Do not work ahead while a question is
outstanding. The integrator answers in `a00N-short-title.md`.

You own `q*.md`; the integrator owns `a*.md`. Never edit an `a*.md`.

Every question states: what you were doing and what you found, the decision
needed, the options with your read on each, and **your RECOMMENDATION**.
Full SOP: `docs/INTEGRATOR.md` §10.

---

## Delivery

**Push to `p49-playdriver` at every stage boundary.** **One `Work.tgz` with
the final report.**
