# R39 — Inadmissible evidence: what hand-assembly contaminates

Project 37.  This is an **evidence-admissibility rule**, not a codegen rule.
It is numbered so that a future session can cite it in one token when
discarding an observation: *"void under R39."*

---

## R39 (ruling)

> **Not every entry in `quest.addrbook` is compiler output.  An observation
> taken from, or about, a hand-assembly routine is INADMISSIBLE as evidence
> about the PL/I compiler — and the contamination travels: it reaches the
> addrbook metadata of routines that are themselves perfectly ordinary
> compiled code.**
>
> Before a convention, arity, frame value or entry variant is promoted to a
> rule, check whether its only witness sits inside a hand-assembly range.  If
> it does, the observation is void and the rule has no evidence.

Known hand-assembly span: **70169B0F..70169D69** — `LOCK_FILE` and
`UNLOCK_FILE`, which are one unit, not two routines.

## Why those two are hand-assembly (P37 plan gate, finding (a))

Decisive: **they branch into each other's ranges.**  `UNLOCK_FILE` jumps to
70169B75 and 70169B77 (inside LOCK_FILE); `LOCK_FILE` jumps to 70169BB8
(inside UNLOCK_FILE), and both fall into a shared error tail at 70169B78.  No
two PL/I procedures can do that.  Corroborating: raw `SYSCALL 0245`/`0246`,
`ind()` indirect addressing, a bare `c = 1`, `ac1 = ac1 ^ ac1`, and a
variable-index bit test (`lsh(ac1,-4)`, `0 - (ac1 & 15)`) that is nothing like
the constant-bit R26 form the compiler emits.

## The sweep — what they contaminated

Every call and syscall site inside the span:

    70169B24  SYSCALL 0246
    70169B70  SYSCALL 0245
    70169B82  LCALL [0x70176FDD],3 ; # RETURN_MESSAGE
    70169BB3  SYSCALL 0245

**Exactly one** call leaves the span, so the callee-metadata contamination is
bounded to a single routine — but it is real:

### 1. `RETURN_MESSAGE` is flagged `mixed:3/6`.  VOID under R39.

Four of its five call sites pass six arguments.  The lone 3-argument site is
70169B82, inside the span, building its arguments on the stack by hand.  The
arity flag records an assembly calling sequence, not a PL/I optional-argument
feature.  A routine with nothing to do with file locking carries a false
description because of who called it.

### 2. `WSAVR` is not a compiler entry variant.  VOID under R39.

`WSAVR` occurs **exactly twice in the entire addrbook** — and both are
LOCK_FILE and UNLOCK_FILE.  Every one of the other 100 live entries is
`WSAVS`.  So WSAVR is the hand-assembly's own prologue, and any model that
treats it as one of two compiler-emitted entry conventions is modelling
something the compiler never emitted.  (This one had not been noticed before;
it fell out of the sweep.)

### 3. Their `frame` and `flags` describe nothing.  VOID under R39.

`LOCK_FILE frame 0x01 dyn,push` and `UNLOCK_FILE frame 0x00` are the assembly's
own stack discipline.  They are not evidence about frame layout, about R1/R2/R3
slot allocation, or about what a `frame 0x00` routine looks like.

## What is NOT contaminated — the direction matters

Contamination flows **outward from an assembly call site**, not inward to one.

`LOCK_FILE`'s `argc 2` and `UNLOCK_FILE`'s `argc 1` are admissible: they were
inferred from *compiled* call sites inside `SIGNAL_TURN` (70177E7D, 70177EFD,
70177F0B), which is ordinary PL/I.  What a compiled caller does when calling
assembly is still evidence about the compiler.  What assembly does when calling
anything is not.

So the test is not "does this observation involve LOCK_FILE?" but **"is the
hand-written side the one producing the behaviour I am about to generalise?"**

## Scope, honestly stated

This sweep covers the one hand-assembly span currently identified.  It does not
prove there are no others.  The cheap detector is the one that found this pair:
**control flow crossing an addrbook boundary.**  A routine that branches outside
its own [entry, next-entry) range, or that is branched into from outside, is a
candidate.  A future session that finds another such pair should re-run this
sweep for it before trusting any rule whose only witness lies inside.
