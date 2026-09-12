# Project 38 — implement, then match: finish routines

GOAL: **finish routines.** P35 delivered three; P36 and P37 together
delivered one (OWNS), because both were spent on discovery — necessary
discovery, and it produced R39, R40, the frame verdict and a much better
rule ledger, but the project now stands at 4 matched routines out of
~130. P38 has no discovery work in front of it: the frame rule is
derived and only needs implementing, the twins are designed, DIED is
pre-registered. So this project is measured in **completed routines and
one implemented construct**, and its binding constraint is:

> **Stage nothing new.** A routine is either finished (100 % MATCH,
> primary and `--folded`, native-clean) or explicitly abandoned with a
> reason. "Staged, constructs derived, deliberately not fitted" is NOT a
> permitted outcome in this project — if a routine needs a construct you
> would have to fit, abandon that routine and move to the next one.

The discipline that produced P37's findings stays: derive before you
assert, record falsifications, never invent a number. What changes is
the target — those are the *method*, not the *deliverable*.

Hi Claude! Read docs/METHOD.md first, **including §16** (R39/R40, the
detector, "consistent is not evidence"). Foundation: **compiler/
CODEGEN_RULES.md** (the model: P35's 25 + P36's R26–R32 + P37's
additions, with confidences), **compiler/translate.py**,
**compiler/ircmp.py** (seven equivalences, FIXED unless a construct
forces an eighth — a ruling), **docs/Project37/REPORT.md** and its
companions: **FrameRelocation.md** (the derived rule you implement),
**DIED_PREREGISTERED.md** (read before touching DIED),
InadmissibleEvidence.md, GetInputFinding.md, ReturnMessageFinding.md,
plus docs/Project35/REPORT.md and docs/Project36/REPORT.md. Also
game/routines/*.c (five models), game/quest_rt.h, game/declarations.h,
compiler/gen_declarations.py, docs/IR.md (ir 6), docs/Project29/
StringsDesign.md §5.2/§6 (twins). TREE VINTAGE: main after the P37 merge
(9eb32d9 or later) — state it; verify docs/Provenance.md. Nothing here
touches emulation/ or Disassembled/; no battery.

**Run `compiler/crossings.py` first** (METHOD §16): it is sub-second and
it is how R40 pairs and hand-assembly are caught before they cost a day.

## The work, in order

### Stage 1 — implement the frame rule (R33/R34). No new routine.

P37 derived it and deliberately did not implement it: `Regs` still pins
ac3. The rule (docs/Project37/FrameRelocation.md): **there is no
relocation** — the frame pointer is an ordinary R7-managed value,
materialised by LDAFP into whatever register R7 leaves free; ac3 is its
default holder and is otherwise ordinarily allocatable; when both a live
ac3 base and the frame are needed in one statement, the base is
preserved across the LDAFP by `WPSH 3,3` / `WPOP 3,3` (R34).

Implementation: `Regs` grows ac3 plus a "where does the frame live"
state; every `pick()` site must say whether ac3 is eligible. This
touches every emit site — which is exactly why it is Stage 1, alone,
with nothing else in flight.

**Validation, in this order**: (a) the five existing routines must stay
at 100 % — 349/349 primary, 242/242 folded — throughout; (b) then
FIRE.1@7016A3BD (89 statements) is the small case the rule was derived
from: finish it to 100 %. If FIRE.1 will not close, that is the rule
being wrong and it is a STOP-and-report, not a routine to abandon.

### Stage 2 — twins, on INIT_OBJ_TBL (173 statements)

Build `LEN(v)` and the claim/release sizing exactly as P36/P37 derived
it: claim words = `(bytes + 3) >> 2` over a running byte length whose
terms are literal piece lengths and `LEN(v)` for a VARYING parameter,
`+2` more for a final varying's length word. Two claims, one release.
Finish INIT_OBJ_TBL to 100 %.

Note P37's census correction: INIT_OBJ_TBL also has 10 string
statements, 8 `ac3 = wfp`, a WPSH/WPOP pair and a DERR 16 fold — Stage 1
should have covered the ac3 work already.

### Stage 3 — DIED (495), as the pre-registered experiment

Read **DIED_PREREGISTERED.md** before translating a line. Four
one-witness beliefs ride on it — R29a (bit assignment), R36 (the
loop-invariant hoist), R7c′'s release point (flagged in advance as the
most likely refutation), and the join self-move — each with what it
predicts and what would refute it. A contradiction is a **finding**,
recorded as a falsification; it is not a cue to adjust the rule until
DIED matches. Report each of the four as CONFIRMED / REFUTED / NO
WITNESS, whatever the match number turns out to be.

Two live search targets while you are in there: a **BITS()-shaped
argument** among DIED's eight call sites (the discriminator
GetInputFinding.md needs), and whether either DIED loop supplies R36's
second witness. Also expect one of the three **predicted arm-path
defects** (R9's scaled-subscript temp, R10's element-address temp, R3's
temp-free-from-last-use) to trip — they were left unfixed so that
tripping one is a confirmation.

### Stage 4 — only if stages 1–3 are complete

TRANSPORT_TERRAK/TRANSPORT_SUNDAR as **one** function with two entry
prologues, compared against the union of both ranges (R40). This is the
drawn routine and the first R40 reconstruction; if the project is
running short, it is the right thing to cut.

## Part 1 — plan gate (keep it short)

The design work is done; this gate exists to catch a wrong assumption
before the Regs change, not to re-derive anything. Report: the
re-baseline (five routines, selftest, `crossings.py` output), the Regs
design in a paragraph, the FIRE.1 and INIT_OBJ_TBL C you expect to
write, and anything in the carried-over rules you believe is wrong. If
you have no ruling requests, say so and start — a gate with nothing to
rule on should not cost a turn.

## Part 3 — report

Per routine: the C, the match table, the rules exercised, pragma count.
The four DIED pre-registrations, each with its verdict. Any rule
promoted, amended or falsified, with the instruction pair. Then the
number that matters: **routines finished this project, and the running
total out of ~130** — plus your honest estimate of how many could be
attempted today with the current subset, and what the next blocking
construct is.

## Boundaries — BINDING

1. Files: compiler/, game/, docs/Project38/. Nothing in emulation/,
   Disassembled/, no artifact, no battery.
2. **Stage nothing.** Finish or abandon-with-a-reason. (This overrides
   the habit P36/P37 established; it is not a criticism of those
   projects, whose discovery work made this one possible.)
3. The five existing routines must never regress; `--selftest` green
   after every rule change.
4. Derive before asserting; a pragma is an admission, recorded with its
   evidence and counted — but P37 showed the frame case does not need
   one, so a pragma here would be surprising.
5. No match number for a partial translation; no verdict for an
   untested rule; "not measured" is a valid line.
6. Commit and push at every stage boundary. Deliver a Work.tgz AND push
   the branch (`p38-routines`).
