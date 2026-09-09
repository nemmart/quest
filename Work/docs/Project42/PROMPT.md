> **RE-SCOPED, Sep 9 2026 — read docs/REWRITE_PLAN.md first.** The
> approach has changed: the generator becomes deliberately DUMB and the
> book's shape is reached by an oracle-driven REWRITE stage (P43). This
> prompt's production table survives as the naive generator's structure,
> and the ledger/path-check survive as how a rewrite site gets an
> address — but the table is no longer where rules live. **Do not port
> cleverness that is about to be deleted** (hoist_invariant_subscripts,
> the temp pool's liveness reuse, the register cost model's
> protections, deferred stores): record it as a rewrite specification
> instead. Everything else below stands.

# Project 42 — the reduction-ordered emitter: productions, not methods

GOAL: restructure `compiler/translate.py`'s emit layer so that code is
produced by **firing a template per grammar production, in reduction
order**, instead of by ~90 hand-written methods calling each other.
Behaviour-preserving: the four matched routines must still be
349/349 primary and 242/242 folded when it lands. **This project adds no
rule and closes no routine.** It builds the structure that P43's choice
files ride on and that per-production censuses need.

Hi Claude! Read docs/METHOD.md first (including §16). Foundation:
**compiler/translate.py** (the thing you are restructuring — read it all
before proposing anything), **compiler/CODEGEN_RULES.md** (the ~45 rules
whose new home is the production table), **compiler/ircmp.py** (the
comparator, unchanged by this project), game/routines/*.c and
game/quest_rt.h. Reports for context on why: docs/Project40/REPORT.md
(four rules amended, all fitted to one routine each — the failure this
restructuring targets), docs/Project41/REPORT.md, docs/Project39/
REPORT.md. TREE VINTAGE: main after the P41 merge — state it; verify
docs/Provenance.md. Nothing here touches emulation/, Disassembled/,
game/ or docs outside Project42; no battery.

**Re-baseline before and after every stage**: `crossings.py`,
`ircmp.py --selftest`, AND translate all four routines end to end. The
selftest does not invoke the translator (P40 shipped a header that did
not parse and the selftest passed).

## Why this, and why now (the reasoning, so you can disagree at the gate)

The DG PL/I compiler was almost certainly a bottom-up parser driving a
code-generation table: a template per production, plus a register
allocator behind the reduction. Our translator is a recursive AST walk
with the rules scattered through it as conditionals. That mismatch is
what has made the last three projects expensive:

- A rule has no home. R36 ("hoist the loop-invariant subscript") lives
  in `hoist_invariant_subscripts` and in `for_stmt` and in
  `element_address`, so it was read off ONE routine and was wrong for
  the next. Attached to the *reference production* instead, it is one
  template parameter and OWNS vs QUEST.1 is one question, asked once.
- A divergence has no address. "The book put 686 in ac2, we said ac1" is
  currently a hunt through emit methods. Under a production table it is
  a named reduction with a named choice.
- The corpus is unreachable. To settle a rule today you need a routine
  that exercises it. With productions you can scan every instance of one
  production across the whole book — 200 examples instead of 2. That is
  the thing that breaks the one-witness treadmill, and it is P44.

We are NOT reproducing the DG's actual grammar and must not pretend to.
We need *a* production set whose reductions match the shapes in the
book. PL/I expression grammar is standard enough that they will be
recognisable, and the existing method names are already close:
`element_address`, `indexed_field_load`, `binop`, `field`,
`bit_address`, `subscript_sym`, `for_stmt`.

**No lexer, no parser generator.** pycparser already gives the AST. What
is needed is the second half: a bottom-up walk that visits nodes in the
order a shift-reduce parser would reduce them, firing one template per
production.

## The work

### Stage 1 — the production set (design, at the plan gate)

Enumerate the productions the four matched routines actually exercise,
derived from the existing emit methods and the book, each with: its
name, the AST shape it reduces, the IR template it emits, the **choice
points** it contains (register, temp slot, ordering, spelling), and
which existing rules attach to it. Expect on the order of 25–40. Bring
this list to the gate — it is the design of record for everything after.

Name the choice points explicitly. They are what P43's choice files
address, so a production with an unnamed choice is a production that
cannot be overridden later.

### Stage 2 — the reduction-ordered walk

A driver that visits the AST bottom-up in reduction order and fires
templates, with the existing `Regs`/`Frame`/`Layout` state used
unchanged. Land it behind a flag or in parallel with the old path if
that makes the diffing easier, but the tree must never be left with the
four routines regressed.

### Stage 3 — move the templates

Port the emit methods into the table one production at a time,
re-translating the four routines after **each** one. A production whose
template cannot be written without an `if` on which routine it is in is
a FINDING: name it, and leave that production on the old path rather
than encoding a routine-specific hack.

### Stage 4 — the rule map

`docs/Project42/RULE_MAP.md`: every rule in CODEGEN_RULES mapped to the
production(s) it now lives in, and — the useful part — **the rules that
turn out to belong to no production, or to more than one.** Those are
the ones most likely to be wrong; say which and why.

## Part 1 — plan gate

The production set (Stage 1) in full, the walk's design in a paragraph,
the staging you propose, and the re-baseline. **This gate is
substantive** — the production set is the project's design of record and
I would rather rule on it than on the code.

## Part 3 — report

The production table as built. The four routines' match tables before
and after (they must be identical). The rule map, including the
orphans and the multi-homed rules. Anything you left on the old path
with the reason. What P43's choice files would key on, concretely.
**No new rules, no closed routines, no fitted anything** — if you find
yourself wanting to change behaviour to make something fit, that is
out of scope and gets recorded instead.

## Boundaries — BINDING

1. Files: `compiler/`, `docs/Project42/`. NOT game/ (the four .c files
   and the header are the fixed regression bed), not emulation/, not
   Disassembled/.
2. **Behaviour-preserving.** 349/349 primary and 242/242 folded before,
   during and after. Verify by translating, not by selftest.
3. **No new rules and no rule changes.** If the restructuring exposes a
   rule that is wrong, record it in the report for P43/P44; do not fix
   it here. (A behaviour-preserving refactor that also changes behaviour
   cannot be verified by its regression bed.)
4. Never leave the tree regressed and unpushed; commit and push at every
   production ported.
5. Deliver a Work.tgz AND push the branch (`p42-productions`).
