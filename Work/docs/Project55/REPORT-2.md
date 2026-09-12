# Project 55 — REPORT-2: THE TWO LOWERING CHANGES

Worker session, Sep 12 2026, branch `p55-blockcensus`. Rulings `a004`
(boundary reversed for two changes), `a005` (plan approved, MIN/MAX warrant
accepted). Gate `q002-part3-plan.md`.

**Modified: `compiler/lower_c.py` only**, and only the two functions ruled.
`game/**`, `emulation/**`, `docs/IR.md`, `docs/Project44/DESIGN.md` and every
artifact untouched. No `.c` changed. Comparison materialisation untouched.

---

## 1. Acceptance — all four criteria

| criterion | result |
|---|---|
| P48's differential corpus GREEN | **`LOWERC DIFFTEST: GREEN`** — 100/100 AGREE over classes A/B/C/D, 9/9 hand cases, **8/8 mutations caught**, t-places 0, effectful ops 0, `quest.assumptions` 3 legs GREEN |
| all seven compile and load | **7/7**, in their P53 units |
| before/after census, per routine per class | §3 |
| plain answers on the two gaps | §4 (loop: **yes**), §5 (conditional: **yes on two, partly on two**) |

UPDATE_SCREENS end-to-end is unchanged and still exact: 594/594, 594/594,
198/198, 0 failed, trap leg fires, `no_sign_extend` teeth still bite.

The two hand cases that bear directly on what changed — `control_edges` and
`abs_edges` — both AGREE. Loop rotation being checkable by the existing gcc
oracle was the deciding argument in `a001` for changing the lowering rather
than adding a `DO()` macro; it has now actually been run.

---

## 2. The changes as built

**Change 1, `for_stmt`.** The canonical DG PL/I `DO`, 206/206 witnesses:

```
<entry test>:  cond       -> [after, preheader]    (in the CURRENT block)
preheader:     goto body                           (unconditional, OVER step)
step:          next; cond -> [body, after]         (the DO block)
body:          ... ; goto step                     (back edge)
```

`continue` → step, `break` → after, both unchanged in meaning, which is what
the book does. `for(;;)` gets no entry test and an unconditional step. Blocks
are created in the book's address order (§6: order carries no meaning — this
is legibility).

**Change 2, `builtin_abs` and `builtin_minmax`.** The result is defaulted into
the out-`v` in the test block; only the acting arm gets a block; the other path
is the skip's fall-through. `ABS/pos`, `MIN/a` and `MAX/a` are gone.

**Not changed, deliberately:** `while_stmt` (no census covers a `while`);
`if_stmt` (already one-armed with no `else`); comparison materialisation (a
rewrite).

---

## 3. Before / after census

Target `ir2.book`. `ΔR` is the merge-reduced residual — book minus ours.

| routine | ours before | ours after | Δ blocks | **ΔR before** | **ΔR after** |
|---|---:|---:|---:|---:|---:|
| UPDATE_SCREENS | 15 | 13 | −2 | −1 | **+1** |
| PICK_X_Y | 20 | 20 | 0 | −1 | **−1** |
| GET_INPUT | 5 | 5 | 0 | +0 | **+0** |
| HIT_ANY_CHAR | 4 | 4 | 0 | +0 | **+0** |
| INIT_SCREEN | 27 | 25 | −2 | −3 | **−1** |
| FAKE_OCEAN | 47 | 43 | −4 | −2 | **+2** |
| FAKE_LAND_MASS | 33 | 29 | −4 | +16 | **+20** |

Every block removed is accounted for: 2 `ABS/pos` in UPDATE_SCREENS, 2 in
INIT_SCREEN, 4 MIN/MAX arms in FAKE_OCEAN, 4 in FAKE_LAND_MASS. **Change 1
removed no blocks at all** — it replaced `for/head` with `for/preheader` and
rewired the edges; the gain is in shape, not in count.

**`v` count, the cost flagged at the gate:**

| | before | after | Δ |
|---|---:|---:|---:|
| the seven, total | 650 | **683** | **+33 (+5.1%)** |

Concentrated exactly where predicted — FAKE_OCEAN +12 (4 loops), INIT_SCREEN +9
(3), FAKE_LAND_MASS +9 (3), UPDATE_SCREENS +3 (1), and **+0** for the two
loopless routines and for PICK_X_Y's `for(;;)`, which has no condition to
duplicate. Faithful: the book loads the limit in the header and reloads it in
the DO block.

---

## 4. Did the loop gap close? **Yes — and my prediction about the residual was wrong**

**All 12 loops across the seven have the book's shape**, checked mechanically:
in every one, the preheader jumps into the body and *not* into the step block,
so the increment-and-test block is entered only on back edges.

UPDATE_SCREENS, merge-reduced, ours against the book:

| ours | book | |
|---|---|---|
| b0 → [b4, b1] | D635 → [D643, D644] | entry + guard |
| b1 → b3 | D644 → D64A | preheader |
| b2 → [b4, b3] | D645 → [D64A, D6A8] | the DO block |
| b3 → [b5, b6] | D64A → [D65B, D65C] | body |
| b5 → b6 | D65B → D65C | ABS arm |
| b6 → [b8, b7] | D65C → [D65E, D65F] | |
| b7 → b2 | D65E → D645 | continue |
| b8 → [b9, b10] | D65F → [D674, D675] | |
| b9 → b10 | D674 → D675 | ABS arm |
| b10 → [b12, b11] | D675 → [D679, D67A] | |
| b11 → b2 | D679 → D645 | continue |
| b12 → b2 | D67A → D645 | back edge |
| b4 (ret) | D643 (ret) **and** D6A8 (ret) | **the only difference** |

**Isomorphic on 13 of 14 blocks.** The single residual is that the book emits
**two** exit blocks — one for the not-entered path, one for the exhausted path,
both `WRTN` — where we share one `for/after`.

### 4.1 The pre-registration was wrong, which is what it was for

I filed, before running anything:

> I expect the loop gap NOT to close cleanly on all seven … the book omits the
> entry guard on ~115 of 206 loops and our lowering cannot know `lo ≤ hi`, so
> it must emit the guard unconditionally — costing roughly **+1 block per
> constant-limit loop**.

**A +1 appeared and the mechanism I named is not its cause.** UPDATE_SCREENS's
limit is a runtime value, so the book emits a guard there too; our guard is not
surplus. The +1 is **the book duplicating its exit block**, which is a
different thing entirely and which I had not noticed in Part 2.

Three consequences, and the middle one is the point of having pre-registered:

1. **The entry-test question is still open** and this did not test it. REPORT
   §7.1 stands unchanged; settling it still needs a real dominator analysis.
2. **The fix I pre-registered against would not have worked.** Making the entry
   test conditional on constant bounds — the obvious way to make my prediction
   come out right — would have changed a thing that was not causing the
   residual, on a routine where it does not apply, and the number would have
   moved for an unrelated reason. Filing the prediction is what makes that
   visible instead of leaving a plausible story standing.
3. **Exit-block duplication is a new difference class**, and not merge or split
   — merge and split preserve block identity; this needs one block to become
   two with different predecessors. **No census, so no change.** Whether it is
   general or specific to loops that end a routine is unmeasured; recorded for
   P56.

---

## 5. Did the conditional gap close? **Exactly on two, partly on two**

One-armed / two-armed conditionals, merge-reduced:

| routine | before | **after** | book |
|---|---|---|---|
| UPDATE_SCREENS | 0 / 3 | **2 / 1** | **2 / 1** ✅ |
| INIT_SCREEN | 0 / 3 | **2 / 1** | **2 / 1** ✅ |
| FAKE_OCEAN | 2 / 6 | 6 / 2 | 4 / 2 — two-armed matches, one-armed +2 |
| FAKE_LAND_MASS | 0 / 5 | 4 / 1 | 12 / 1 — two-armed matches, one-armed −8 |
| PICK_X_Y | 3 / 0 | 3 / 0 | 0 / 0 — untouched by this change |
| GET_INPUT | 1 / 0 | 1 / 0 | 0 / 0 — untouched |
| HIT_ANY_CHAR | 0 / 0 | 0 / 0 | 0 / 0 |

**The two-armed count now matches the book on all four routines that have
conditionals** — 1, 1, 2, 1 against 1, 1, 2, 1. That is the number change 2
was aimed at, and it is exact.

**FAKE_LAND_MASS's one-armed deficit of 8 is the materialisation class**, out
of scope by ruling: the book's 8 `WADC`/compare-skip/`WSUB` sites are one-armed
conditionals we correctly do not emit. The number is the same 8 that Part 2
measured. Nothing here is a surprise; it is P56's rewrite.

**FAKE_OCEAN overshoots by 2 one-armed** and I have not explained it. It is not
MIN/MAX — all four of its MIN/MAX sites converted. P51 §2.2 records that
FAKE_OCEAN is the routine whose `||` is emitted as *jumps* rather than
materialised, so the likely home is there, but **that is a hypothesis and it is
not measured.** Recorded as open rather than asserted.

---

## 6. The changes moved the raw gap the wrong way on two routines

`a004`: *"If a change closes the gap on one routine and opens it on another,
that is a finding, not something to tune."* It did.

`ΔR` sign flipped on UPDATE_SCREENS (−1 → +1) and FAKE_OCEAN (−2 → +2), and
**FAKE_LAND_MASS got worse, +16 → +20.**

**Both changes are nonetheless right, and the raw gap is now the wrong metric.**
Removing our surplus MIN/MAX arm blocks was correct on a 150-witness census; it
widened the raw gap only because the book has 8 one-armed conditionals we
deliberately do not emit, so taking away 4 of ours moved us further from a
number that was never the target. The class-level counts tell the true story
and they improved on every routine: two-armed exact on four of four, loop shape
exact on twelve of twelve.

Stated plainly, because it would be easy to read the table the other way:
**|ΔR| is no longer a progress metric.** After these changes the residual is
dominated by classes P55 does not own — materialisation (P56's rewrite) and
exit-block duplication (uncensused). P56 should track the class counts.

---

## 7. Temptation register — carried forward per `a004`

Part 2's three are in REPORT §6. Both from the gate held, and two are new.

1. **Rotating `while_stmt` to match.** Held. No census covers a `while`; the
   for/while asymmetry is now real in the compiler and is intended (a005: "it
   should look odd, because the evidence covers one and not the other").
2. **Making the entry test conditional on constant bounds.** Held —
   pre-registered against, and §4.1 shows it would have been a fix to the wrong
   thing.
3. **NEW — reverting change 2 for MIN/MAX, because FAKE_LAND_MASS got worse.**
   The most dangerous one here, because a number moving the wrong way looks
   like evidence against the change. Declined: the change rests on 150
   witnesses, and the widening is the *absence* of a rewrite that is explicitly
   P56's, not a defect in the lowering. Reverting would have traded a
   census-backed lowering for a better-looking table.
4. **NEW — conditioning the MIN/MAX change to stop FAKE_OCEAN overshooting by
   2.** Declined: no census, one routine, and §5 says the likely cause is a
   class the change does not touch. This is `a004`'s exact prohibition — a
   condition invented to make all seven come out.

---

## 8. Files

Branch `p55-blockcensus`. Modified: `compiler/lower_c.py` (`for_stmt`,
`builtin_abs`, `builtin_minmax`; each change carries its census in a comment so
the warrant travels with the code). Written: `docs/Project55/REPORT-2.md`.

**To reproduce:** `emulation/tests/run_lowerc_difftest.sh 25`, then compile the
seven per P53's line and run `compiler/blockcmp.py --pair ENTRY=<ir>`.

## 9. For P56

1. **Comparison materialisation** — 36 applications book-wide, 8 in
   FAKE_LAND_MASS, and now the single largest term in its residual. One rule.
2. **Exit-block duplication** — new, uncensused, 1 block per routine-final
   loop on the evidence of one routine. Census it before acting.
3. **FAKE_OCEAN's 2 surplus one-armed conditionals** — open, hypothesis in §5.
4. **The loop entry test** — still open; REPORT §7.1 unchanged. Needs a
   dominator analysis, not a heuristic.
5. **Track class counts, not `|ΔR|`** (§6).
