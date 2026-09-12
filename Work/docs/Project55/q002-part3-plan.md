# Project 55 — q002 PART 3 PLAN GATE

Worker session, Sep 12 2026. Ruling `a004` (boundary reversed for two
lowering changes). **Nothing has been modified.** This is a plan only.

**`a004` did not ask for a gate.** I am filing one anyway, for one reason: the
plan contains two scope judgements — a warrant I generated myself, and an
exclusion — that the integrator should see *before* the work rather than in
the report after it. `a004`'s warning is that this work is dangerous precisely
because it is sanctioned, and a scope judgement made silently inside sanctioned
work is the failure mode it names. If the integrator is content, no answer is
needed and I will proceed on the user's go-ahead; the file exists so the
judgement is on the record either way.

---

## Baseline, captured before any change

`emulation/tests/run_lowerc_difftest.sh 3` → **`LOWERC DIFFTEST: GREEN`**,
8/8 mutations caught, UPDATE_SCREENS 594/594, 594/594, 198/198, trap leg
fires, `quest.assumptions` 3 legs GREEN.

Census baseline (target `ir2.book`, merge-reduced ΔR):

| routine | ours | book | Δ | **ΔR** |
|---|---:|---:|---:|---:|
| GET_INPUT | 5 | 5 | +0 | **+0** |
| HIT_ANY_CHAR | 4 | 4 | +0 | **+0** |
| PICK_X_Y | 20 | 18 | −2 | **−1** |
| UPDATE_SCREENS | 15 | 18 | +3 | **−1** |
| INIT_SCREEN | 27 | 32 | +5 | **−3** |
| FAKE_OCEAN | 47 | 60 | +13 | **−2** |
| FAKE_LAND_MASS | 33 | 64 | +31 | **+16** |

---

## Change 1 — the `for` shape (`for_stmt`)

```
init
<entry test>:  cond        -> [after, preheader]
preheader:     goto body                       (unconditional, OVER the step block)
step:          next; cond  -> [body, after]    (the DO block: increment AND test)
body:          ... ; goto step                 (back edge; `continue` -> step)
after:
```

`continue` → step and `break` → after keep their current meanings, which is
what the book does (its `continue` edges target the DO block). `for(;;)` gets
no entry test and an unconditional step.

**Warrant: 206/206, no exceptions** (BlockCensus §2.2).

**Soundness:** ordinary loop rotation, semantics-preserving, and checkable by
the existing gcc oracle — which was the deciding argument for (a) over the
`DO()` macro in `a001`, and only holds if the corpus is actually run.

**A cost that must be reported, not hidden:** the condition is now emitted
**twice** — once as the entry test, once in the step block — so every loop's
condition expression and its `v`s are duplicated. This is faithful (the book
loads the limit in the header for the entry test and reloads it in the DO
block) but it raises the `v` count, and `v` count is a headline metric under
DESIGN §5.1. It will be reported as a before/after number.

## Change 2 — the one-armed builtin diamond (`builtin_abs`, `builtin_minmax`)

Default the result into the out-`v` in the test block, branch to a single arm,
fall through to the join. Drops the `ABS/pos`, `MIN/a` and `MAX/a` blocks.

**Warrants — and change 2 needed one `a004` did not cite:**

| idiom | witnesses |
|---|---|
| compare-against-**zero** (`WSGE r,r`) — covers ABS (14 `WNEG` + 8 `NNEG`) and MAX-against-0 (8 `WSUB`) | **36 / 36** one-instruction arms |
| register-**register** compare-skip — covers MIN/MAX generally | **403 / 416** one-instruction arms, of which **150** are the lone-`MOV` MIN/MAX diamond |

`a004` cited 36/36 and the general 807:199. **Neither is specifically about
MIN/MAX**, whose diamond is register-register, not compare-against-zero. Rather
than lean on the aggregate, I measured the register-register case on its own:
150 lone-`MOV` sites, 403/416 one-armed. That clears §6's bar independently.
**If the integrator reads the 807:199 as already covering MIN/MAX, the measured
150 changes nothing; if not, the 150 is the warrant.** Either way it is not
resting on a pattern-match from ABS.

---

## Deliberately NOT in scope — three exclusions

1. **Comparison materialisation.** A rewrite, not a lowering change, because
   the source does not determine it (P51 §2.2: the same PL/I `|` emitted both
   ways). `a004` agrees. Untouched.
2. **`while_stmt`.** The 206 sites are PL/I `DO` loops and lower from `for`.
   The seven contain no `while`. **No census covers a `while`**, so rotating it
   would be widening the ruling on a pattern-match — the exact move §6's clause
   forbids. Left as it is, which does mean `for` and `while` will emit
   different shapes; that asymmetry is deliberate and is reported.
3. **`if_stmt`.** It **already** emits one-armed when there is no `else`
   (`b_else` is only allocated when `n.iffalse is not None`). Nothing to fix,
   and the book has 199 genuinely two-armed conditionals, so both shapes are
   real. Reported so it is clear this was checked, not skipped.

---

## Acceptance

1. **`run_lowerc_difftest.sh` stays GREEN.** If it goes red, that is the
   finding and the change stops (`a004`).
2. All seven still compile and load in their P53 units.
3. `blockcensus.py` / `blockcmp.py` re-run: **before/after per routine, per
   class**, plus the `v`-count delta from change 1.
4. Plain statements: did the loop gap close on all seven? Did the conditional
   gap close at the 36 compare-against-zero sites?

## Pre-registered prediction — filed BEFORE running, so it cannot become an excuse afterwards

**I expect the loop gap NOT to close cleanly on all seven.**

REPORT §7.1: the book omits the entry guard on ~115 of 206 loops, and the
correlation with a statically-decidable limit is suggestive but is not a rule
I closed. Our lowering cannot know whether `lo ≤ hi`, so it must emit the guard
unconditionally — costing roughly **+1 block per constant-limit loop** against
the book.

If that appears, it is a finding about constant folding and belongs in the
report. **It is not a reason to add a condition to the lowering.** Any such
condition would need a book-wide census of its own, which is the standard these
two changes had to meet, and which the entry-test question explicitly did not
(§2.2: two detectors, two answers, the tight one returning zero).

DESIGN §7.2b's instruction to pre-register a measurement rather than assume it
is the precedent for writing this down first.

## Temptation register — carried forward per `a004`

`a004`: *"Now you hold a knob that moves the numbers you report."* Part 2's
three declines are in REPORT §6. Additions from Part 3 go in REPORT-2. Open at
the time of writing:

1. **Rotating `while_stmt` to match.** It would look consistent, cost four
   lines, and no reviewer would query it. Declined above: no census.
2. **Making the entry test conditional on a constant-bounds check**, which is
   the obvious way to make the prediction above come out right. Pre-registered
   against, for that exact reason.

## Deliver

`docs/Project55/REPORT-2.md`, pushed to `p55-blockcensus`. This gate goes to
**main**, per SOP §10 — a question on a branch is invisible.
