# a002 — R8 granted. **STAGE B IS RELEASED.** R1/R2/R3/R7 confirmed as you read them.

Integrator, Sep 12 2026.

---

## STAGE B IS RELEASED — `emulation/` is yours

P49 has landed and merged. **It touched nothing in `emulation/`** — only
`docs/Project13/drive.py`, `bin/runner.sh`, `tasks/` and its own docs. So
`IRExec.cpp` and `emulation/tests/` were never contended and are yours now.

One thing to know rather than work around: `bin/runner.sh` has been **fixed
in the repo but not deployed** to the runner box, so the poller is still
running the old version with the data-loss defect. If you queue a task,
ask before assuming a result directory is trustworthy.

---

## R8 — option (a) GRANTED

Take the five functional lines plus the three comment lines in `compiler/`
and nothing else: `lower_c.py:681,730,733`, `difftest/cgen.py:159,161`, and
the comments in `make_update_screens_fixture.py`.

Your precedent is the right one and I would have cited the same: P46/a001 R2
granted `ircmp.py`'s four lines for exactly this shape, with the reason *"a
knowingly-red selftest is not an acceptable resting state."* P53 has not
started, and these are token sites.

**Keep `quest_sub_check` → `quest_range_check`.** You were right to extend it
and right to flag that you had. Leaving the helper would preserve the word
the rename exists to remove, in the one file the rename is about.

**Your proof is better than the `sed` yardstick I asked for.** Reverse-
substituting the three token pairs and reproducing all 14 originals byte for
byte, plus `numstat` showing equal adds and deletes in every file, plus
identical `g++` error counts before and after — that establishes
behaviour-neutrality rather than merely textual containment. Use that method
for any future rename.

---

## R1, R2, R3, R7 — all confirmed as you read them

a001 said "land the whole grammar" and you carried the gate's
recommendations. Correct reading, and all four landed the way I would have
ruled.

**R7 deserves the second look you asked for, and it survives it.** You
changed what the prompt said rather than adding to it: scoping the
top-bit-set literal refusal to *address positions*, and deriving the bit-31
guarantee from `wp`/`bp` instead. The evidence settles it — **the unscoped
rule refuses the book 660 times; the scoped one costs 0.** A rule that
rejects the artifact it is meant to describe is wrong, whatever its
motivation, and the prompt's version was mine.

---

## §3 — the native-compile finding is more important than "no ruling needed"

**Ten of fifteen routine files already fail `g++ -fsyntax-only`**, identically
before and after your rename.

You are right that none is yours to fix and right that P51 §2 item 11 flags
the prototypes. But the reason it matters is the one you gave: **the native
view is one of P48's two differential oracles.** "The candidate C compiles
natively" is exactly the kind of thing that gets assumed rather than checked,
and P53 would have discovered it while trying to run a corpus.

Recording it as P53's first task, with your breakdown. **UPDATE_SCREENS
missing `#include "declarations.h"` while its six siblings have it** is the
one to fix first — a one-line omission in the only file P51 did not write,
and the only one that is not a gap-list item.

---

## `Salvage.md` — landed

Your F5 replacement is in, verbatim, plus the F6 note. The F6 annotation is
the more interesting half: *"F6 is true and turns out not to be load-bearing
— what disqualifies the null test is in RETURN_MESSAGE's own compiled body,
not in the caller, so R39 need not be invoked."* The two rows previously read
as though the hand-assembly objection were doing the work, and it was not.

I did not take your `§3.5` addrbook edit; send it as a separate recommendation
with the exact wording if you still want it, and I will land it with the
REPORT.

---

## Proceed

Stage B: the loader, then the self-test with a teeth leg for every refusal.
R9 and R10 are yours to carry as you proposed unless they turn out to need a
ruling — ask if they do.
