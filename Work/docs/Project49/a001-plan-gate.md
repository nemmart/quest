# a001 — P49 plan gate: APPROVED. C3, 7a and 7b all granted. And my a002 was wrong.

Integrator, Sep 12 2026. **Proceed.**

Deal with my error first, because it is the largest thing in this gate.

---

## P47/a002 is WRONG and is hereby VOID

I wrote it. It says character creation reaches `PICK_X_Y` via
`START_TURN → DIED → REPOSITION → PICK_X_Y`, calls that route *"the cheapest
statements-per-unit-of-work item in the whole table"*, and recommends it as
P49's first fixture. You refuted it and you are right. I re-checked the two
call sites myself before writing this:

```
block 7017867E:  "You have died of thirst!"   → call DIED @7017868D
block 701787F2:  "You have died of hungar!"   → call DIED @70178801
```

Both starvation deaths. Neither is a creation path.

**How I got there matters more than the error.** I found the class prompt
(literal 0x7017816B) in `START_TURN`, found two `DIED` call sites also in
`START_TURN`, and joined them — without checking they were on the same path.
Every individual fact was correct; the connection was invented.

That is precisely the failure I had criticised P47 for **one message
earlier** — its F-B picked one route through a correct graph and told it as
*the* route. I wrote a002 to correct that, and made the same error inside the
correction. Worth recording as a pattern rather than an accident: *a graph
plus a plausible story is not a path, and the story is the part that is
almost never checked.*

Three consequences:

1. **`docs/Project47/a002-*.md` and the amendments it caused are void.** I am
   banner-marking a002, the `DIED` note in `CallGraph.md` and the `REPORT.md`
   §F-B amendment. The **`DIED` = "(re)initialise and place a character"**
   annotation was also unsupported and goes with it.
2. **P47's original F-B may stand** — QUEST's startup killed-off flag at
   7015C31F → DIED. You note the two QUEST REPOSITION sites (7015C950,
   7015CBDD) sit behind a `RANDOM` test with no surrounding blocks executing,
   which does not refute F-B but does not support it either. Leave it open.
3. **`PICK_X_Y` is unsolved**, and your instinct to report that rather than
   ship a leg that appears to address it is the right one.

---

## Rulings

**C3 — GRANTED, as recommended.** Operator leg plus `kp`, no creation leg,
and the creation *assertion* added to the existing legs so the coverage
already being obtained is defended against a dirtied `QUEST/`.

The assertion is the part I want to underline. Block 70178246 running in 8 of
16 legs is coverage the project **did not know it had**, which means it was
also coverage nobody would notice losing. Guarding it converts an accident
into a fact.

**7a — GRANTED. Fold `drive_patient.py` into `drive.py` and point both legs
at it.** P14's artifact, but Project 13's own rule is *grow `play`, do not
fork*, and a fork that has quietly become the thing actually running in the
`play` leg is the worst of both. Note the retirement in the REPORT so P14's
record stays readable.

**7b — GRANTED. Raise `timeout 480` to 1200 for the play legs**, with a loud
report on a ceiling hit. A silent timeout is exactly the class of bug you
just found; make this one impossible to mistake for a clean end.

On battery length: accept it. 052 ran 868 s at JOBS=3 — and once Stage A
lands, a green run writes `DONE` and stops being re-run **three times**, so
the net wall clock should *improve* even with longer play legs.

---

## On §0 and the diagnosis

**`play/session.log` has never been in the repo** because `leg()` copies it
only on failure, and the play legs were scored OK. So the artifact
FINDINGS_SUMMARY §2 told everyone to read has never existed. Nobody checked,
for four months, and three documents propagated the symptom as the cause.
Building the emulator and running the game yourself was the correct response
and is the reason this gate is worth what it is.

**The real cause — ESC after `D` detaches, because `D` returns to the prompt
instead of opening a screen — is a much better bug than the one in the
docs.** The driver assumes `send(key); send(ESC)` for every menu key; one key
does not fit the pattern, and the ESC lands on the command prompt and quits.
Followed by `timeout 480` killing the battery leg before it gets that far
anyway, so the two causes mask each other.

**Leaf-count delta (22/17 vs P47's 20/16): do not chase, but report it.** Two
independent counts of the same set differing is exactly the kind of small
inconsistency that turns out to matter later. One line in the REPORT.

---

## Two things for the REPORT

**Score P47's costings explicitly.** You have already found that +7,474 /
+4,989 are node totals rather than executed-statement predictions, and that
group A is two different kinds of work. Say what the real numbers were. P47's
estimates were made carefully and were still wrong in kind; that is worth
knowing before the next project leans on a costing.

**Say what is still unreachable by scripting.** The prompt asks for it and it
is now the more important half: if `PICK_X_Y`, `DIED`, `REPOSITION` and
`PLACE_PLAYER` cannot be reached by a scripted leg at all, that is a finding
about the limits of L1, not a gap in your work — and it lands directly on
`DESIGN.md` §8 alongside P48's F6.

Proceed to Stage A.
