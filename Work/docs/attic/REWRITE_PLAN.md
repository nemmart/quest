# PLAN — dumb generation, then oracle-driven rewrites

*Design of record, Sep 9 2026 (user + integrator). This supersedes the
approach P35–P42 have been taking, and re-scopes P42 and P43. It does
not discard their work: the production table stays, the rules become
rewrite specifications, and the four matched routines remain the
regression bed.*

## The problem it solves

Every rule in `CODEGEN_RULES.md` is currently **predictive**: the
generator must emit the book's exact form on the first try. That forces
a bottom-up code generator to know things only a later pass can know —
R36 must decide, while reducing `T(i).f`, that it is inside a loop and
which part of the reference is invariant. It was read off one routine,
was wrong for the next, and needed an awkward amendment. P42's ledger
then found the real shape of it: **the hoist fires no production at
all**, and fires in exactly the one routine R36 was read off.

Four projects (P37–P40) each closed zero or one routine while
discovering that a handful of rules had been fitted to a single witness.
The pattern is structural, not a run of bad luck: a predictive rule
inferred from one routine is usually that routine's coincidence.

## The approach

**Generate dumb, then rewrite.**

1. **A naive generator**: deliberately uniform, deliberately stupid. One
   template per production, no cleverness — no hoisting, no temp
   pooling, no register cost model, no lookahead. Its output is
   *correct* (it computes what the C says) and *approximately shaped*
   like the book.
2. **A rewrite set**: small, fixed, each rule independently justified as
   something a compiler does — hoist a loop invariant, materialise a
   constant, coalesce a copy, reuse a dead temp, fold a spelling. Each
   is semantics-preserving, stated as precondition → postcondition on
   the IR.
3. **An oracle**: at each site where more than one rewrite could fire,
   or one rewrite could fire in more than one place, the oracle input
   says which. That is a far better-defined question than "which
   register", because **the legal set is enumerable by construction** —
   you can run the applicable rewrites and see.

A routine closes when: naive IR + a recorded rewrite sequence = the
book, exactly.

## Why this is better, concretely

- **A diff becomes a search, not a puzzle.** Given naive IR and the
  book's IR, ask "what sequence of rewrites takes A to B?" That is
  mechanical to explore, and the answer *is* the oracle input. You no
  longer have to guess a rule in order to record a choice.
- **Rewrites are checkable in a way generation rules are not.** A
  rewrite has a precondition and a postcondition and must preserve
  semantics — testable. A generation rule has nothing to check it
  against but the diff it was invented to explain.
- **The rewrite set is shared, not per-routine.** Hoisting is one rule
  used by every loop in the game, rather than a parameter of a reference
  production re-derived per routine.
- **It matches the hypothesis about the compiler**: flat code plus
  passes, which a 1986 compiler plausibly was.

## The line that must not move

A sufficiently rich rewrite set can take *anything* to *anything*, at
which point a match means nothing and "proven source" degrades into "we
found a path". Three guards, all binding:

1. **Every rewrite is independently justified** as a transformation a
   compiler performs, stated as precondition → postcondition, and
   semantics-preserving on its own terms. A rewrite whose justification
   is "the book has this shape here" is REFUSED — that is a diff patcher,
   not a compiler model.
2. **The set is small and fixed**, extended only by a recorded finding
   with its evidence. Growth is a reportable event.
3. **The rewrite count per routine is reported, and it is the metric.**
   Six rewrites means the generator's shape is right and the rewrites
   are doing compiler-like work. Sixty means the naive generator is
   wrong and the rewrites are laundering that fact. Drive it down.

Also unchanged from P43's original framing: **a rewrite selects, it does
not create.** It may not change which values are computed or the
observable effect order. Anything that alters the computation means the
C is wrong, and that is the finding.

## Consequence: the generator gets *simpler*

Everything clever in `translate.py` becomes something to **remove**:
`hoist_invariant_subscripts`, the temp pool with its liveness reuse, the
register cost model with its protections, the deferred stores. A rewrite
can do each of those better and be checked. The rules that governed them
are not lost — they become **rewrite specifications**, which is what
they were always trying to be.

## Re-scoping

- **P42 (running)** — finish, but its purpose is now narrower: the
  production table is the *naive generator*'s structure, not the place
  rules live. Ports continue; the ledger and the path check stay (they
  are how a rewrite site gets an address). Do not port cleverness that
  is about to be deleted — record it instead.
- **P43 (prompt to be rewritten)** — the **rewrite engine**: the rule
  set, the oracle format keyed to rewrite sites, the A-to-B search, and
  the four matched routines re-derived as naive-plus-rewrites. The
  phase split it was going to need is now unnecessary — there is one
  oracle, keyed to rewrite sites, and "generation vs transformation" is
  just "before or after the rewrite stage".
- **P44** — censuses per rewrite rule rather than per production: how
  often does this rewrite fire across the whole book, and where does it
  not fire when it could? That is where a rewrite's *trigger* is
  derived, which is the last thing the oracle would otherwise have to
  supply.

## What this does not change

The four matched routines stay the regression bed. `ircmp.py`'s seven
equivalences are unchanged. The C sources are unchanged — this is
entirely about how the IR is produced from them. The discipline stands:
derive before asserting, one witness is confidence C, a correlation with
no mechanism is not a rule, no match number for a partial translation.
