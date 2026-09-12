# a001 — P53 gate: APPROVED. Q-1 (a) and Q-2 (a) both granted.

Integrator, Sep 12 2026. **Proceed.**

Both questions are boundary collisions I created by writing the prompt
without checking where the files actually live. Both go your way.

---

## Q-1 — (a) GRANTED: `emulation/quest.assumptions`

Put it beside `quest.addrbook`, as rule 1 said. Your reasoning is right and
it is the reasoning I should have applied when writing rule 2: **rule 2
exists to stop your edits colliding with P54's, and a new file P54 never
opens cannot collide.**

Your point against (b) is the one that decides it — separated from the
artifact family (`quest.addrbook`, `quest.arena`, `quest.pushmap`,
`quest.synclist`), it is *"how the next session will fail to find it"*. The
file's value is entirely in being found.

**Create that one file and nothing else in `emulation/`.**

---

## Q-2 — (a) GRANTED: both files, narrowly

Take the default/banner line in `run_lowerc_difftest.sh` and the `w8` render
class in `lowerc_rig.cpp`. Roughly ten lines, both in `tests/`.

Your distinction is correct and worth making a standing one: **`emulation/tests/`
is the compiler's test harness that happens to live next to the emulator
because it links its objects.** P54's work is `IRExec`, `RTBridge` and the
LCALL replica. Different neighbourhood.

**And option (b) is disqualified by the hole you named rather than by
convenience.** You wrote it plainly:

> a byte-ADDRESS error that is consistent between the store and the load is
> invisible to it — which is precisely the bug class §2.3 says byte support
> introduces

That is the whole reason the prompt says generator-first for bytes. A corpus
that cannot observe a byte is not testing byte support; it is testing that
byte support is self-consistent, which any wrong-but-uniform addressing
scheme also passes. P48's own bug was exactly this shape — a type error
invisible until something *else* consumed the value.

Saying so rather than taking the easier option and hoping is the right
instinct.

**If P54 needs either file, it comes through a question and I serialise.**

---

## On §1

**"The masks vanish from node cells and concentrate on the `local` rows"** is
the observation worth carrying into the REPORT. The Walker's invariant is
unchanged — promoted types are always 32-bit, so no node cell is narrow and
no node read is extended — and ir 8 therefore moves every sign/width decision
to **exactly where C says a conversion happens**.

That is a real simplification rather than a relabelling, and it means the
part of the compiler most likely to harbour a soundness bug (P48 §2.1) gets
smaller rather than larger under this migration. Worth measuring: if the
mask-emitting code shrinks, say by how much.

72 emit sites is a big mechanical change but a uniform one. The risk is not
difficulty, it is a missed site meaning something *else* rather than
refusing — so your note that some become silent rather than loud is the one
to design against. If there is a cheap way to make every unconverted site
loud (a grep-able assertion, a temporary emit-time check), it is worth ten
minutes.

---

## Noted from the header

The ~50-minute emulator build started in parallel, and `emulation/emulator`
being tracked so it must be `git checkout --`'d before any commit — both
recorded hazards from P48, both handled without being told. Good.

Proceed to Stage A.
