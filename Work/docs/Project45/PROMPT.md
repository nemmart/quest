# Project 45 — ATTIC SALVAGE

## GOAL

Read `docs/attic/` — Projects 35–43, `CODEGEN_RULES.md`, `translate.py`,
`NextSession.pre-P44.md` — and separate **facts about the program Quest**
from **claims about the compiler that built Quest**. The first kind is
salvaged. The second kind dies.

Deliver `docs/Salvage.md`: every surviving fact, with its evidence, its
witness count, and an explicit confidence. Plus taint markers written **in
place** into `game/declarations.json` for data that rests on unconfirmed
single witnesses.

**Success is `docs/Salvage.md` being complete and honest, not the attic
being tidy.** If you run out of budget having produced a good Salvage.md and
reorganised nothing, that is a success. The inverse is a failure.

Expected size: nine project directories, ~45 numbered rules in
`CODEGEN_RULES.md`, 26 ON-handler sites referenced, one 829-line
`NextSession.pre-P44.md`. If that estimate is badly wrong, say so at the
gate.

---

## Context of record — read in this order

| path | why |
|---|---|
| `docs/Project44/DESIGN.md` | **Read first.** The new direction. You cannot judge "is this still load-bearing?" without it — a claim that is dead as a rule may be alive as a fact P44 must reproduce |
| `docs/attic/README.md` | Why the line is void; a starting list of what to hunt for. **The list is a seed, not a bound** |
| `docs/METHOD.md` | Law for implementation sessions; §11 (falsification) and §16 are directly relevant |
| `docs/Plan.md` | Milestones, so you can tell what a finding serves |
| `docs/attic/**` | The material. Read as much as budget allows, in the order you judge best |

---

## Carried-in rulings (from the Sep 12 design session — a session cannot know these)

1. **The test to apply, on every claim:** *does this statement describe
   Quest, or does it describe the machine that compiled Quest?* The first
   kind survives. The second does not, **however well-argued** — because the
   argument was made against one or two witnesses, and the line's own
   sessions voided three such beliefs as inadmissible and one (R3b) as never
   having been evidence at all.
2. **Phenomena survive even when the rules over them die.** The self-move at
   a join (RETURN_MESSAGE 70176FF5, `WMOV 0,0`) is a fact about the binary.
   R8d, the rule built on it, is not. Record the phenomenon; drop the rule.
   P44 §7.2 needs exactly these as `keep` test cases.
3. **You may not rehabilitate the codegen model.** If a rule looks good, that
   is expected — they were argued by capable sessions. It is still void.
   Recording "this rule may be re-derivable from a book-wide census" is
   permitted and useful; recording "this rule is probably right" is not.
4. **Taint is marked in place.** A live file with quietly wrong rows is worse
   than an attic. `game/declarations.json`'s FIRE parent-frame layout rests
   on a SINGLE WITNESS per slot. **Do not tell anyone to run the FIRE.1 /
   FIRE.2 mutual check** — P41 performed it and found it **VACUOUS**: the
   siblings reach DISJOINT slot sets, so they cannot check each other and
   never could (`docs/attic/Project41/FIRE_MUTUAL_CHECK.md`). The taint is
   real and the named cure is impossible. Mark the rows, and if you can see a
   different confirmation route, propose it — do not invent one.
5. **Numbers in live docs may be wrong.** R40 (PL/I multiple ENTRY) means
   per-routine addrbook statement counts MIS-SIZE those routines, which
   invalidates figures in `docs/Project34/Census.md` — a doc that is still
   live. Chase this and report every live figure you find that depends on it.

---

## Part 1 — PLAN GATE. Stop and report.

Do not write `Salvage.md` yet. Report:

1. **Your reading plan and its budget.** Which directories in which order,
   and what you will do if budget runs short. (Suggested: `CODEGEN_RULES.md`
   first — it is the index to everything else — then project REPORTs, then
   primary derivations only where a claim needs checking.)
2. **Your classification scheme.** How you will label each item. At minimum
   the axes: program-fact vs compiler-claim; witness count; confidence;
   whether a live doc or live data file depends on it.
3. **Anything in `docs/attic/README.md`'s seed list you think is
   misclassified**, and anything major it missed that you spotted on a first
   pass.
4. **Your estimate of how many claims are in scope**, against the numbers
   above.

STOP. Push the gate as `q001-plan-gate.md` and wait for `a001`.

---

## Part 2 — Build

Write `docs/Salvage.md`. Required structure:

- **§1 Program facts — SALVAGED.** Each with: statement, evidence (addresses,
  instruction sequences, routine names), witness count, confidence, and
  **what currently depends on it**. Candidates from the seed list — R40
  multiple ENTRY, R39 hand assembly / inadmissible evidence, the static link
  (UPLINK/UP/UPARG, 23 nested entries across 13 parents), FIRE's parent-frame
  layout, the join self-move — plus whatever else you find.
- **§2 Phenomena — RECORDED, not ruled.** Observations about the binary that
  no longer carry a rule. P44 will use these as `keep` test cases.
- **§3 Live data and live docs that are TAINTED.** What, where, why, and what
  it would take to confirm.
- **§4 Compiler claims — VOID.** A short list by rule number, with a
  one-line reason. Do NOT restate the rules; the point is that they are gone.
  Note separately any that look **census-recoverable** under P44 §13's
  project 8, since that is a real future input.
- **§5 What you did not read**, and what might be in it.

Then mark taint in `game/declarations.json` in place, in whatever form that
file supports (a comment, a sibling key, a `"_taint"` field — your judgement,
state it).

---

## Boundaries — BINDING

1. **You may WRITE only:** `docs/Salvage.md`, `docs/Project45/**`,
   `game/declarations.json` (taint markers only — **do not change any
   layout value**), and annotations inside `docs/attic/**`.
2. **You may READ anything.**
3. **Do NOT touch** `emulation/**`, `Disassembled/**`, `compiler/**`,
   `game/routines/**`, `docs/IR.md`, `docs/Provenance.md`, or any artifact.
   Regenerate nothing.
4. **P46 is running in parallel** and owns `docs/IR.md`, `docs/Provenance.md`,
   `emulation/**`, `compiler/readable.py`, and `docs/Project46/**`. Do not
   write there. If you find something P46 must know, put it in your report
   under a heading **FOR P46** and stop — do not act on it.
5. **Do not move anything out of the attic.** If a document should be
   restored to live status, recommend it; the integrator decides.
6. **Do not rewrite `docs/README.md` or `docs/NextSession.md`.** Recommend
   edits in the report.
7. If a claim's status turns on something you cannot settle from the tree,
   that is a **STOP-and-report**, not a judgement call.

---

## Part 3 — Report

`docs/Project45/REPORT.md`:

- what you read and what you did not
- the counts: claims examined, salvaged, voided, phenomena recorded, taints
  marked
- every live doc or live figure you found that depends on a voided or tainted
  item
- **FOR P46** section, if any
- recommended edits to `docs/README.md` / `docs/NextSession.md`
- anything you believe the design got wrong


---

## Coordination — questions and the plan gate (BINDING)

**You have the repo. Questions and the plan gate go through it, not through
chat.**

Write `docs/Project45/q001-short-title.md` (`q002`, `q003`, …), commit,
**push to main**, then **STOP and tell the user it is there.** Do not work
ahead on other fronts while a question is outstanding — a ruling can
invalidate work done in parallel. The integrator pulls, writes
`docs/Project45/a001-short-title.md`, and pushes; the user will tell you
when to look. Pull and read it before resuming.

You own `q*.md`; the integrator owns `a*.md`. Never edit an `a*.md`.

**Part 1's plan gate IS a question file** — write it as
`q001-plan-gate.md` and push it.

Every question must contain, because the integrator has none of your
context:

- what you were doing and what you found
- the decision you need
- the options you see, with your read on each
- **your RECOMMENDATION** — what you would do if it were your call, and why

The recommendation is mandatory. It is often the entire answer, and writing
it out will sometimes dissolve the question.

Full SOP: `docs/INTEGRATOR.md` §10. **Push to main — a question on a branch
is invisible.**

## Delivery

**Push to your branch at every stage boundary** — that is the working
record, and nothing on it is ever lost.

**Send ONE `Work.tgz` of the whole tree with the final report** — not
selected files, and not at interim stages. The tarball is the user's
snapshot mechanism (Plan.md, "Snapshot Practice"); the branch is the
working record. Do not spend budget packaging a tree you have already
pushed.
