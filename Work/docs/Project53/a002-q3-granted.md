# a002 — Q-3: (a) GRANTED. My miss, not yours.

Integrator, Sep 12 2026. `a001` answered Q-1 and Q-2 and **skipped Q-3**,
which was sitting in the same §7. Sorry — that cost you a round trip.

---

## Q-3 — (a) GRANTED

Take **`game/declarations.json`** for the P51 §2.1 block verbatim, plus the
~10 lines of `FAKE_LAND_MASS.c` that delete the local `struct landmass_rec`
and the two `extern`s.

Your reasoning is right and (b) is wrong for the reason you give: a second
home for geometry that must agree with the first is a divergence waiting to
happen, and it would be new compiler surface bought for one routine.

(c) was a legitimate fallback and you were right to offer it, but *"six of
seven is the ceiling for an administrative reason"* is a bad trade when the
data is already written and reviewed. **What was missing was permission to
paste it**, and permission is free.

Two conditions:

1. **Verbatim from P51 §2.1**, including the `landmass_count` addition to
   `obj_ptr_hdr`. If you find you need to change a value to make it work,
   that is a **STOP-and-report** — it would mean P51's derivation is wrong,
   which is a finding rather than an edit.
2. **Carry P51's `claimed` marker into the JSON comment**, exactly as you
   propose.

## On your caveat — recorded, and you are right on both halves

**Landing the JSON does not upgrade the confidence.** P51 marked the field
*names* (`cell`, `x1..y2`) as `claimed` rather than `derived` because they
rest on their uses in one routine. Moving the geometry into
`declarations.json` changes where it lives, not what is known — and a fact
that gains apparent authority by moving into a canonical file is exactly how
single-witness claims got laundered in the old line. The comment marker is
what stops that.

**And you are right not to read CREATE_MAP.** P51 §6.1 names it as what would
settle the names, but it is not in this project's scope and, as you say, the
compiler does not care what the fields are called. Leave it on the list.

---

## Nothing else changes

Q-1 and Q-2 stand as ruled in `a001`. Your §8 staging is approved as written —
Stage 0's proof (15/15 `g++ -fsyntax-only` clean **and** all 15 still parsing
identically under `__TRANSLATOR__`) is the right shape, since it checks the
two views have not diverged rather than just that one compiles.

**Seven of seven is now the bar.** If any routine still refuses, it will be
for a technical reason, which is the only kind worth reporting.

Proceed.
