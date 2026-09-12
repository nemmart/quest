# a001 — P45 plan gate: APPROVED, with four rulings

Integrator, Sep 12 2026. Read against `main` @ `7279d22`.

**The gate is approved. Proceed to Part 2** with the rulings below. Three
change the prompt; one changes a live doc, which I have done myself.

This is a better gate than the prompt deserved. Two things in particular:
you noticed the carried-in ruling was stale on `main` rather than executing
it, and you proposed verifying rather than relaying. Both are exactly right,
and §4 is the most valuable part of the document.

---

## Q1 — the ABI line: **(b), approved**

Calling-convention, frame-image and ABI facts are `PROGRAM-FACT`.
Intra-body allocation, scheduling, temp placement and register choice are
`COMPILER-CLAIM` and die.

Your reasoning is the right one and I will restate it so it is on the
record: these describe **what the binary does at every boundary**, and they
are observable at each of thousands of call sites rather than inferred from
one. That is the opposite of the epistemic situation that killed the
compiler line. `DESIGN.md` §9.2's rendezvous contract is unwritable without
them.

Two constraints on how you record them:

1. **Cite the live corroboration per fact** — `M4aDesign.md`,
   `EagleStack.cpp`'s WSAVS/WRTN, `ON_ERROR_CATALOG.md` §B, the addrbook's
   own layout header. A fact with live corroboration is not really being
   salvaged from the attic at all; it is being *confirmed* there, which is a
   stronger status and should be visible as one.
2. **Mark any that rest on the attic alone.** Those are the ones a future
   session needs to treat carefully, and they should not be hidden among the
   corroborated ones.

## Q2 — what taint means: **(b), approved**

Re-verify P41's per-slot counts against `quest.dis` yourself, then mark each
row with its **actual** status.

You have the principle exactly right: *a live file with rows marked tainted
that are not is the mirror image of the failure ruling 4 guards against.*
Carried-in ruling 4 was written against a tree two commits stale and it is
hereby superseded by this answer — **do not follow it where it conflicts
with what you find.**

Binding conditions:

- **The verification is yours, not P41's.** P41 is in the attic; its counts
  are `Reported` until you re-derive them. That is the whole point of
  choosing (b) over (a).
- **If your counts disagree with P41's, STOP and report** — do not
  reconcile. P41 found roughly half of spot-checked witness PCs wrong, which
  makes disagreement a live possibility rather than a formality.
- `w12` → `derived` with the note you propose. `LIST_PLAYERS.w13` → `single`
  unless LIST_PLAYERS' own body corroborates slot 13; check it.
- The confirmation route question in the prompt is **discharged by this
  ruling**. The route is the family check, re-performed by you. Nothing
  further is owed.

## Q3 — where the marker lives: **(c), boundary WIDENED**

You identified the real problem and then deferred to a boundary that was
drawn without knowing it. Marking a generated file is theatre — the next
`gen_declarations.py` run wipes it.

**Your write boundary is extended to `compiler/gen_declarations.py`, for the
`PARENT_FRAMES` table only.** P46 does not touch that file (it owns
`compiler/readable.py` and `compiler/ircmp.py` — see `a001` there), so there
is no collision.

Conditions:

- **Marker in `PARENT_FRAMES`, which is the source of truth.** Then
  regenerate `declarations.json` and verify it is byte-identical except for
  the markers. Your `_taint` key shape is approved as proposed.
- **No layout value changes.** Not one displacement, not one width. If your
  verification says a layout value is wrong, that is a STOP-and-report.
- Do not touch anything else in `gen_declarations.py` — not
  `check_frames()`, not the generator logic.

## Q4 — scope: no ruling needed, and the number is accepted

~105 claims against my "~45" is a fair correction: I counted base rule
numbers, you counted rule *tokens*, and yours is the number that predicts
work. Recorded.

One budget instruction, since 105 is over twice the estimate: **if the
verification pass (your item 6) starts to crowd out §1/§3, protect the
verification and shrink the void list.** A verified salvage of twenty facts
is worth more than an exhaustive catalogue of sixty dead rules. Your
shortfall order already says this; I am ratifying it.

## Q5 — Census.md: approved, and CURRENT_STATE is mine

Recommend the replacement caveat text in the REPORT; do not touch
`Census.md`. Approved as proposed — and your observation that its caveat is
**too narrow** (naming only the two R40 pairs when METHOD §16 says every
`.N@` count is suspect) is itself a salvage item. Put the exact figures and
the replacement text in the report and I will land it.

**`CURRENT_STATE.md` was my miss, not yours.** Its landing-history entries
for P37–P41 read as live results citing rule numbers with no VOID marker. I
have banner-marked that section on `main` in the same push as this answer,
so you are not working against a stale doc. `game/quest_rt.h` — report only,
as you propose; it is P44-owned.

---

## Corrections to your §3 that I accept, and one I do not

**Accepted:** R39 and R40 are `METHOD-RULING`s already live in METHOD §16,
and the program facts underneath them (LOCK_FILE/UNLOCK_FILE as one
hand-assembly unit at 70169B0F..70169D69, the only two WSAVR entries; the
`.N@` mis-sizing) are what belong in Salvage §1. The seed list was loose
here.

**Your "missed by the seed list" items are all in scope and several are
better than anything in the seed** — R38's runtime BIT rebuild via
`X.CB @7017E708`, R35/slotpatch, unsigned CHARACTER, the one-level nesting
result, the twin sizing arithmetic, and both P41 censuses as `MEASUREMENT`s.
The measurement/conclusion split is a distinction I did not make and should
have; adopt it.

**The one I push back on:** DIED's signature correction living in
`game/routines/DIED.c`. You are right that it should not live only there,
but do not treat the C files as a salvage source in general — they are
P35–P43 output and carry the same fitting risk. Record the signature fact
with its *derivation* if the attic has one, or mark it `Reported`.

---

## Two carried-in changes since your prompt was written

1. **Delivery SOP changed** (`main`, after your gate): push to your branch
   at every stage boundary; **one `Work.tgz` with the final report only**,
   not at interim stages. Your prompt's older wording is superseded.
2. Nothing else in the prompt changes.

---

## What I want in the REPORT that the prompt did not ask for

- **The verification scoreboard**: of the facts you salvaged, how many are
  `Verified` vs `Reported`, and the PCs that failed to check out. P41's
  "roughly half wrong" figure deserves a second data point, and if your rate
  is similar that is a finding about the whole attic.
- **Whether the measurement/conclusion split held up** — did any claim
  resist classification?

Proceed.
