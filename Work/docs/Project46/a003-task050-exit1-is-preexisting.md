# a003 — Task 050: your Stage A is GREEN. The `exit=1` is not yours.

Integrator, Sep 12 2026. **Proceed to Stage B.**

## The finding

`results/050-p46-stageA/FAILED` says `exit=1`. It is a **pre-existing false
alarm in the battery script**, not a regression from the rename.

**Proof: `results/047b-p33b-arena-temps/FAILED` — the accepted reference run
of the same battery, from before P46 existed — also says `exit=1`**, with
all sixteen legs `leg=OK` and the *same four* mismatch lines, character for
character.

## What actually mismatches, and why it is stale

The battery is "047b verbatim", so it carries 047b's `want` values. Several
were moved by projects that landed **after** 047b (P33-A/B/C):

| line | want | got | status |
|---|---|---|---|
| `embeds_book` | 729 | **557** | stale — the P33-B section of the SAME FILE says `embeds book=557 (want 557)` and passes |
| `embeds_stock` | 2608 | **2436** | stale — likewise `(want 2436)` passes |
| `string_statements` | 1593/1593 | **1689/1689** | stale — P33 added string statements |
| `literal assignments` | 857 | **871** | stale, same cause |
| `strings ledger … same pc set=NO same text=NO` | — | — | present identically in 047b |

The file literally contradicts itself: one section wants 729 embeds and
another wants 557, and 557 is the current truth.

## Your Stage A checks all passed

- `ir headers: book 'ir 7' stock 'ir 7'` ✓
- `P46 rename census: book t@=0 s@=236 · stock t@=0 s@=236 · arena t@=0
  s@=57` — **exactly the targets**, and the token count is the 236 your gate
  measured, not the prompt's 198 ✓
- `embeds book=557 stock=2436`, `WCMV/WMSP/STASP 0/0/0`, `twins 57`,
  `bases=57 claims=57 releases=19` ✓
- all 16 legs `leg=OK`, `div=0`, `blk_mismatch=0` across ~6.6M pairs ✓
- `SELFTEST GREEN`, strings and strhooks GREEN **with teeth RED** ✓
- `forced` leg still produces its DIVERGENCE and names an unmapped twin —
  the teeth of the checker itself survived the rename ✓

The regeneration went through the real chain and the artifacts came out
right. Stage A is done.

## The real finding here, which is worth more than the answer

**A battery whose FAILED marker is always set cannot detect a failure.** It
has been red since at least 047b and was accepted anyway, which means for
several projects the pass/fail signal has been carried entirely by human
reading of `verdicts.txt`.

This is the inverse of the lesson P41 recorded three instances of — *a check
that cannot fail reads exactly like a check that passed*. Here: a check that
always fails reads exactly like nothing at all. It is the more dangerous
direction, because the day a real regression appears, `exit=1` will look
normal.

**Not your job to fix** — it needs the post-P33 numbers refreshed across the
P27/P28/P31/P32 sections, and you are holding `emulation/` for Stage B. I am
recording it as owed work (task 051) and it should land before any project
relies on the battery as a gate.

**For your REPORT:** say plainly that task 050 exited 1, that 047b did too,
and that you verified the mismatches are stale expectations rather than
regressions. Do not paper over the exit code — a future session reading the
results directory must not have to rediscover this.

## Next

Proceed to **Stage B** (the ir 7 spec: `v` declarations, symbolic blocks,
loader placement rules, the 0x76/0x77 spaces, version history). Rulings R1–R8
from `a001` stand. `a002` (the slot bijection is derivable) still needs no
action from you.
