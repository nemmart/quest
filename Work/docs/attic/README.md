# The attic — Projects 35–43 and the fitted codegen model

**Status: VOID. Nothing in this directory is current. Do not read any of it
unless a task explicitly sends you here.**

Written Sep 12 2026, at the session that ended the compiler line and opened
P44.

## What is in here

| path | what |
|---|---|
| `Project35/` … `Project43/` | the nine projects of the compiler line, Sep 8–9 2026 |
| `compiler/CODEGEN_RULES.md` | the 45-rule model of the DG PL/I code generator |
| `compiler/translate.py` | the C-subset → IR translator (3,148 lines) |
| `REWRITE_PLAN.md` | **SUPERSEDED BY ABSORPTION, not void** (see below) |

`Project41`, `Project42` and `Project43` are PROMPTS THAT WERE NEVER RUN.
They describe a direction (production tables, choice files, per-production
censuses) that P44 supersedes.

## Why it is void

P35 proved something real: a C subset could be translated to our IR and
compared against the book **register-exact**, and three routines matched
197/197. The line then ran nine projects on that basis and finished
**four routines out of ~130**. P38, P39 and P40 each closed **zero**.

The acceptance criterion had become "our translator emits what the DG
compiler emitted." Meeting it required a working model of a 1988 compiler's
register allocator, scheduler and peephole passes, inferred from its output
alone. Rules were derived one routine at a time, so most had a single
witness, and the sessions said so themselves — P40's own summary: *the model
closes the parts that had two witnesses and fails the parts that had one.*
P37 voided three beliefs as inadmissible evidence; P38 voided R3b outright
because the observation behind it could not have come out otherwise.

The structural diagnosis (user, Sep 9): the real compiler was a parser
driving a code-generation table, plus multiple passes and peepholes. Ours
was a recursive walk with ~45 rules as scattered conditionals. Collapsing
AST→binary into one rule set means **a divergence has no address** — you
cannot tell whether the wrong bytes came from the parse shape, the
lowering, the allocator or a peephole, so every diff gets patched wherever
it was observed. That does not converge, and it did not.

**The fault was never in wanting an exact match.** An exact match through a
sound translator is a genuine proof of recovery, and P44 keeps it as a goal.
The fault was making it the ONLY acceptance criterion, and building it on a
translator that was edited to close diffs — so the artifact under test and
the artifact doing the testing were the same thing.

## What replaced it

`docs/Project44/DESIGN.md`. In one paragraph: a naive, choice-free compiler
lowers C to IR with every value in its own private variable; separately, a
transformer applies precondition-checked IR→IR rewrites under an **oracle
file** that supplies the decisions (register binding, storage merging,
reordering, block merge/split) rather than predicting them. Soundness of
the compiler is established against gcc, not against the book. And a routine
can be *accepted behaviourally* (substituted into the running game under
lockstep) long before it is accepted by exact match.

## What a review session should salvage

A future session may be tasked with mining this directory. The material
divides sharply and the division is the whole job:

**Facts about the PROGRAM — likely still true, wanted in `docs/Salvage.md`:**

- **R40 — PL/I multiple ENTRY.** Some routines have two entry pairs, and
  per-routine addrbook statement counts MIS-SIZE them. This invalidates
  figures in `docs/Project34/Census.md`. Live data depends on it.
- **R39 — hand assembly is inadmissible evidence.** LOCK_FILE and
  UNLOCK_FILE are hand-written; observations about them contaminate their
  own metadata and their callees'. `compiler/crossings.py` is the detector
  and stays live.
- **The static link** (UPLINK / UP / UPARG, 23 nested entries across 13
  parents) — a real construct any lowering must handle.
- **FIRE's parent-frame layout in `game/declarations.json`** — every slot
  rests on a SINGLE WITNESS, and P41 established that the remedy named for
  it **does not exist**: the FIRE.1/FIRE.2 mutual check is **VACUOUS**, the
  siblings reaching DISJOINT slot sets, so they cannot check each other and
  never could (`attic/Project41/FIRE_MUTUAL_CHECK.md`). The taint is real;
  the named cure is impossible. A different confirmation route is needed, or
  the rows stay marked. **Do not re-run the mutual check.**
- **Observed phenomena**, e.g. the self-move at a join (RETURN_MESSAGE
  70176FF5, `WMOV 0,0`). Dead as a rule; alive as a fact P44 must be able
  to reproduce with a `keep` entry.

**Claims about the COMPILER — presumed void:** every numbered rule in
`CODEGEN_RULES.md`. They may be re-derived later from book-wide censuses,
but not by being carried forward.

The distinction to apply: *does this statement describe Quest, or does it
describe the machine that compiled Quest?* The first kind survives. The
second kind does not, however well-argued, because the argument was made
against one or two witnesses.


## A note on `REWRITE_PLAN.md`

Unlike everything else here, this one was **right**. Written Sep 9 2026, it
diagnosed the same failure P44 does and proposed the same remedy: a naive
generator plus oracle-driven semantics-preserving rewrites, with rewrite
count per routine as the metric. P44 was derived independently and arrived
at the same place.

It is in the attic because P44 supersedes it, not because it was wrong. What
P44 adds: the `v`/`t`/`s@` storage classes, the 0x74–0x77 address spaces,
compiler soundness established against gcc rather than against the book, and
the L1 behavioural acceptance level with bottom-up translation order — which
is what makes a playable C++ Quest reachable with no codegen model at all.

**One thing in it was deliberately NOT carried forward**: its rule that a
rewrite justified only by the book's shape is REFUSED. DESIGN §7.2a explains
why — soundness, not plausibility, is the real guard, and the refusal rule is
the same instinct that produced 45 single-witness rules. Its legitimate
concern survives as the rewrite-set tripwire.
