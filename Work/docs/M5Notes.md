# M5 Notes — the signal-edge context problem (and one candidate direction)

*Planning discussion, Aug 15 2026 (user + Claude). Status: NOTES ONLY.
Nothing here is ruled or decided. Raised now so M5 planning starts from
the argument, not from scratch. M4 is unaffected; one dependency note
at the end.*

## The problem: spurious edges from the condition system

A flat code graph that models the condition system naively — one
O?SIGNAL node with in-edges from every raise site and out-edges to all
26 handlers' I.GOTO targets — creates execution paths that cannot occur
in the real program. Example: ?WRITE_SCREEN raises → ?LIB_ERROR →
O?SIGNAL → STORE's handler → STORE's re-prompt, yielding a graph path
MOVE → … → ?WRITE_SCREEN → … → STORE, though no real execution goes
from MOVE to STORE. Any analysis run over that graph (liveness,
reaching defs, read-before-write) inherits the garbage paths.

## Why the "correct" fix explodes

Dispatch is nearest-live-handler, and handlers have stack discipline
(I.PROLOG/O.ON at establishment, O.REVERT/I.EPILOG at exit). So the
edges CAN be pruned exactly — but only with context:

1. Split O?SIGNAL into one node per handler-stack configuration
   (nearest-wins means: per possible TOPMOST establisher).
2. That context must flow through every transitive callee of every
   scoped region, because any of them might raise: ?WRITE_SCREEN needs
   a STORE-flavor and a MOVE-flavor, and so does everything IT calls,
   and so do shared game utilities (READ_IN, DISPLAY_*, ...).
3. Bound is ~(#establishers that can be topmost) ≈ up to 27 clones of
   exactly the most-shared layer of the graph. 100% correct,
   impractical. (The running program does this with ONE word of dynamic
   state — the chain head. The blowup is the price of compiling that
   variable into graph structure.)

## The observation that deflates it

The explosion assumes every ? routine is a raise source. The evidence
says otherwise:

- **Organically observed signals across the entire project: two.**
  Boot (I.INIT's deliberate overflow probe) and CONVERSION from bad
  numeric input (M-trigger `abc`, store `ABC`). Nothing else, ever.
  FAIL_OPEN and QUEST_INJECT signals were induced by us. (Caveat:
  single-user runs, shallow play coverage; organic file errors under
  multi-user contention are plausible but unobserved.)
- **Recovery from most ? failures is incoherent by construction.**
  What would recovery from a failed ?WRITE_SCREEN look like? Every
  handler's recovery WRITES TO THE SCREEN — it re-prompts. The game
  codes no fancy recovery that inspects what failed. So for most
  wrappers: either the failure can't happen, or it is
  unrecoverable-in-principle and the honest behavior is fatal.
- All 26 handlers are catch-all (ac0 = -1) prompt-re-askers; they are
  plainly FOR conversions (and possibly file errors), not for the
  general library.

So the true raise set is probably ~2 classes: conversion routines
(?CHAR_TO_UNSIGNED etc.) and file operations (?OPEN_FILE proven
raise-capable via FAIL_OPEN). Everything else: can't-fail, or
fail-is-fatal.

## Candidate direction (NOT decided)

1. **Raise census tool** (mechanical, over the RT disassembly): for
   each ? wrapper, follow the error branch; classify (a) reaches
   O?SIGNAL, (b) reaches ?RETURN / fatal, (c) local retry/ignore. The
   graph gets exceptional edges ONLY from class (a) sites, each
   annotated observed/unobserved from gcalls/rtcalls evidence.
2. **Fatalize translation** (a ruling M5 would have to make
   explicitly): rewrite error checks outside the census raise set to
   jump to a fatal exit instead of raising. Observationally identical
   on every execution where the check doesn't fire; converts
   never-exercised, unvalidatable recovery paths into a loud stop.
   Fatal exits are terminal nodes (METHOD §13) — they add NO paths, so
   MOVE → STORE dies without any context machinery.
3. **Probe guard**: every fatalized branch carries a probe; if one
   ever fires in any run, the census was wrong at that site and we
   learn it before analysis is built on it. (FAIL_OPEN inverted:
   assert failures don't happen, get told if they do.)
4. Tiers: conversions stay handled (real, observed, recovery real);
   file ops stay handled OR fatalize pending contention evidence;
   everything else fatal.
5. If adopted, this REORDERS M5's opening: raise census BEFORE
   Close-the-Graph (which gets much cheaper), then the analyses. It
   also quietly is the eventual port's error-handling design (asserts,
   not a reimplemented condition system for paths that never ran).

## The same disease in NORMAL returns — and the standard cure (added later, Aug 15)

The signal edges were only the acute case. Ordinary calls have the
chronic form: every game path enters ?WRITE_SCREEN, so a naive graph's
return node fans out to every caller — MOVE → ?WRITE_SCREEN → (return)
→ STORE exists as a path. Context cloning fixes it and explodes,
exactly as with O?SIGNAL.

For normal control flow the standard, non-exploding cures exist and
likely define M5's graph design:

1. **Runtime routines are NOT graph nodes — they are INTRINSICS with
   summaries.** ?WRITE_SCREEN, from the game's view, is one opaque
   operation: reads its args and the named buffer, writes screen
   state, clobbers residue registers, returns to ITS caller. That is a
   transfer function, computed once from the RT disassembly (which we
   have read and validated under lockstep) and applied at each call
   site like a single fat instruction. No node, no return fan-out, no
   contexts — the printf/libc treatment.
2. **Game→game calls keep matched call/return discipline** (tag call
   edges by site; walk only balanced paths — the textbook "realizable
   paths" / summary formulation). MOVE→STORE requires an unbalanced
   path and simply is not walked. Cost: bottom-up summaries over the
   call graph; no cloning.
3. **Signals per the census/fatalize direction above** — and note the
   raise census is just one COLUMN of the intrinsic summary table
   ("can this raise, and to what"). The two ideas are one artifact:
   a summary catalog of the runtime.

4. **Raise-capable call sites get TWO successors** (user, later
   discussion): normal = next instruction; exceptional = the on-goto
   target of the handler lexically live at that site (the region
   between O.ON and O.REVERT/I.EPILOG, statically known; nested
   procedures' ON-units are regions of the PARENT's body — the
   catalog has the registration points). This is exactly LLVM's
   `invoke` (normal dest + unwind dest) — textbook, not invention.
   A raise in a routine with NO local handler composes with matched
   call/return: the unwind successor chains through the BALANCED call
   edge only, so context-sensitivity is encoded in edge discipline
   instead of node cloning.
5. **De-stackification makes handler dispatch STATIC** (user, later
   discussion). The runtime condition apparatus (chain walk,
   nearest-handler search, I.GOTO's wsp cut, frame teardown) exists
   because frames were stacked. With routines in fixed areas (M4) and
   the zero-stack call protocol (args written to areas, no pushes):
   the establisher's context is CONSTANTS (area base in the book,
   on-goto target in the catalog); intervening frames need no
   teardown (their areas just go stale, as after a real unwind);
   non-reentrancy — already asserted at every redirect — is the
   soundness condition. I.GOTO translates to goto (mechanics of
   cross-function jumps deferred). The census gains a column per
   raise site: nearest handler LOCAL / UNIQUE-CHAIN / AMBIGUOUS —
   expected AMBIGUOUS count is ZERO (all 26 handlers are prompt
   re-askers; conversions happen at prompts, so raise sites likely
   sit inside routines with their own ON-units). If that holds, the
   whole condition system translates to local branches.

So the M5 graph stratifies: game nodes with matched call/return
(invoke-style two-successor calls where the census says raise is
real); runtime as per-routine summaries; handler dispatch static via
M4's areas. Each layer's context problem dissolved by a different
standard mechanism — and M4 turns out to be the enabling half of
M5's exception design, not just a storage change. Status: same as
the rest of this file — NOTES, not a decision.

## What would change our minds

- The raise census finding class-(a) reachability in many wrappers.
- Any organic signal in play that is not a conversion (watch
  multi-user/store contention when that testing starts).
- An analysis need that genuinely requires the unpruned exceptional
  edges.

## One M4 dependency note

M4's remaining waves should not invest in machinery that assumes the
full condition system survives into M5 unchanged — e.g. exotic unwind
support for dyn routines' recovery paths that this direction would
fatalize. Cheap compliance now; decision stays open.
