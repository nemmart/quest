> **SUPERSEDED, Sep 9 2026 — do not run this.** P43 is now the REWRITE
> ENGINE, not choice files over a predictive generator. See
> docs/REWRITE_PLAN.md; a new prompt will replace this file. The parts
> that carry over: the oracle is checked (a selection must be legal and
> the production/rewrite publishes the legal set), a choice selects and
> never creates, single-option choices are pruned, and the count is the
> metric. The parts that do not: choice kinds keyed on reductions
> (register/slot/order/spelling) — the oracle now keys on REWRITE SITES,
> and the phase split it anticipated is unnecessary.

# Project 43 — choice files: supplying the compiler's decisions, legally

GOAL: let a routine close when its **C is right**, by supplying the
decisions the C leaves open — which register, which temp slot, which
order, which spelling — from a **choice file** keyed on P42's
reductions, instead of predicting them. And make the file *checked*: a
choice may only select among options the production declares legal, so
the file records what the 1986 compiler did rather than becoming a place
to write whatever makes a diff disappear.

This is the project that should start closing routines. It is also the
project that could quietly destroy the value of everything before it, if
the checking is weak. Read §"The line that must not move" before
building.

Hi Claude! Read docs/METHOD.md first (including §16). Foundation:
**docs/Project42/REPORT.md and its production table** (the design of
record — every choice point in this project addresses a production and a
choice kind defined there), **compiler/translate.py** as P42 left it,
**compiler/CODEGEN_RULES.md**, **compiler/ircmp.py**. Context for why:
docs/Project40/REPORT.md (four rules fitted to one routine each; 93
expression-level diffs on a routine that needed no new construct),
docs/Project41/REPORT.md, docs/Project39/REPORT.md. TREE VINTAGE: main
after the P42 merge — state it; verify docs/Provenance.md. Nothing here
touches emulation/ or Disassembled/; no battery.

**Re-baseline before and after every stage**: `crossings.py`,
`--selftest`, AND translate all four routines end to end (the selftest
does not invoke the translator).

## The mechanism

A choice file is a text artifact per routine,
`game/routines/<NAME>.choices`, with a provenance header (the .c's
sha256, the book's sha256, the tool version). Each line is:

```
<reduction-address>  <choice-kind>  <value>   # witness pc, if known
```

The **reduction address** comes from P42: the production name plus its
index in reduction order (a stable address that survives edits to the C
which do not change the parse — NOT a source line number, NOT emission
order). The **choice kinds** are the ones P42's table names per
production: `reg`, `slot`, `order`, `spelling`. Nothing else.

The translator reads `<NAME>.c` plus `<NAME>.choices` and emits the IR.
Where the file supplies a choice, the production uses it; where it does
not, the production decides as it does today.

## The line that must not move

1. **A choice selects; it never creates.** It may not change which
   statements exist, which values are computed, or the effect order of
   observable operations. Only register / slot / order / spelling. A
   choice that would change the computation is a hard error — and it
   means the C is wrong, which is the finding.
2. **A choice must be legal, and legality is checked by the production,
   not by the file.** Each choice point publishes its legal set from the
   state the translator already tracks:
   - `reg` — registers not holding a live value, intersected with any
     class the production restricts to (R41's {ac2, ac3} for base
     loads). Choosing a register holding a live value, or outside the
     class, is a hard error.
   - `slot` — frame slots free at that point, per `Frame`'s live
     ranges. **Your example: a slot holding a live variable is not in
     the set.** Hard error.
   - `order` — orders that preserve data dependence and observable
     effect order. Independent subexpressions may be reordered; a load
     feeding the next operation may not float past it.
   - `spelling` — the production's enumerated alternatives (e.g.
     `add(x,1)` WINC vs `add(x,0x00000001)` WNADI).
3. **A rejected choice is a FINDING, not a bug to work around.** It
   means either the legality check is wrong or the C is wrong. Report
   which, with the instruction pair; do not widen a legal set to admit
   a choice you wanted.
4. **When the legal set has one element, the file must not mention it.**
   A single-option choice is not a choice. The tool prunes such lines
   and reports how many it pruned — that keeps the file an honest
   measure of freedom.

## The metric this project introduces

**Choice-file size is the measure of what the model does not explain.**
Report per routine: statements, choices supplied, and the breakdown by
kind and by production. A routine with an empty choice file is fully
modelled. Totals across routines replace "routines finished" as the
headline number, because they can only go down as rules improve — and
P44's censuses will be aimed at whichever production accumulates the
most choices.

## The work

### Stage 1 — the file format and the checker (no routines yet)

The format above; the reader; the legality check per choice kind, using
`Regs` and `Frame` as they stand; the pruner for single-option choices;
a self-test with teeth (a choice naming a live register, a choice naming
an occupied slot, a choice naming a value outside a production's class,
a choice that would reorder a dependent load — each must be REJECTED,
and the self-test must go red if any is accepted).

**Validate on the four matched routines first**: each already matches
with no choices, so each must still match with an empty choice file, and
must still match when given a choice file containing only choices equal
to what the translator already picks. Then the interesting one: give
one routine a choice file with a *wrong but legal* value and confirm the
output changes and the comparator reports the DIFF — that proves choices
are actually consumed rather than ignored.

### Stage 2 — generate choice files from diffs

The loop that makes this project pay: translate with an empty choice
file, run `ircmp`, and turn each register/slot/order/spelling DIFF into
a choice line. Mechanical where the diff names a choice point;
**refused** where it does not — a DIFF that is not a choice is a
modelling gap or a wrong C, and must be reported as such, never
converted into a choice.

State plainly what fraction of a routine's DIFFs are convertible. If it
is low, this project's premise is wrong and that is the finding.

### Stage 3 — close routines

In this order, each finished or abandoned before the next:
**QUEST.1@7015C5E1** (P40's diagnostic — 16/86 with four amended rules;
its remaining divergences were register and hoist-ordering choices),
then **LIST_PLAYERS.3@7016F556** (abandoned on an R9/R10 temp-placement
divergence — a `slot` choice if anything is), then
**MOVE_PLAYER.1@701729E2**, then **RANDOM** (17 statements).

For each: the match table, the choice file, and — the part that matters
— **which choices the model should have predicted**. A choice file is a
list of things we do not understand yet; say which look systematic.

## Part 1 — plan gate

The file format and the reduction-address scheme concretely (show a real
line for a real reduction in PICK_X_Y); the legality check per kind and
where the state comes from; the self-test's teeth cases; and your
estimate of what fraction of QUEST.1's known divergences are
convertible. **Ruling requests only if the format or the legality
boundary needs one.**

## Part 3 — report

Per routine: statements, MATCH, choices by kind and production, and the
"should have been predicted" list. The totals table (the new headline
metric). The rejected-choice findings, if any. Which production
accumulated the most choices — that is P44's first census target. And
an honest statement of whether the premise held: **did routines close
when the C was right?**

## Boundaries — BINDING

1. Files: `compiler/`, `game/routines/*.choices`, `docs/Project43/`. The
   four existing .c files and `game/quest_rt.h` may be edited ONLY if a
   choice file cannot express something and the C is genuinely wrong —
   and that is a reportable finding.
2. The four matched routines must never regress, with or without choice
   files. Verify by translating.
3. **No new rules.** This project supplies choices; it does not model
   them. A rule change belongs to P44 after the censuses.
4. Never leave the tree regressed and unpushed; commit and push at every
   stage and every routine.
5. Deliver a Work.tgz AND push the branch (`p43-choices`).
