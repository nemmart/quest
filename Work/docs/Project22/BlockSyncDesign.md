# Gen-6 Block-Sync Design — the translation-invariant checker

*Design draft, Aug 28 2026 planning session (user + Claude). Status:
DRAFT for user review. Nothing implemented. When approved, this is the
design of record for Checker Generation 6, and CheckerHistory.md gets
its Generation 6 section at the first landing.*

## Why the checker must change shape

Every generation so far assumed the clone executes ORIGINAL
INSTRUCTIONS — natively translated routines had to be bit-faithful,
and the sync fabric (500-insn heartbeat batches, instruction counts,
per-instruction hooks) is denominated in instructions. The M5+
direction is IR translation: blocks of Eagle instructions replaced by
folded expression evaluation. A translated clone executes a DIFFERENT
instruction stream — different instruction counts, different
intermediate register states, no per-instruction events. The
instruction-denominated fabric cannot pair such a clone with the
master at all.

What survives translation: BASIC BLOCK BOUNDARIES. A correct
translation of a block enters at the same address, exits to the same
successor, and produces the same memory effects and the same
block-exit register state as the original instructions — even though
it shares no instruction stream with them. Block entry/exit is the
natural translation-invariant sync event.

**The Gen-6 sync identity: the (block entry address, per-client block
ordinal) pair.** Master and clone both count block entries — the
master trivially (it runs original instructions; block boundaries are
static, from quest.blocks) — and rendezvous every K blocks for a full
state compare. Default K=50, configurable; K=1 is the debug mode.

**The sync list is a TRANSLATION ARTIFACT (user ruling, Aug 28).**
The set of counted addresses is an input file supplied by the
translation, not a fixed property of the program. Both sides count
entries only at LISTED addresses — the master runs original
instructions but does not count unlisted ones — so "entry" means the
same thing on both sides by construction. A translation may REMOVE a
block by delisting it: e.g. a DERR sink merged into its guard block
as a trap_if. Rules:

1. Delisting only COARSENS detection (divergence surfaces at the next
   listed entry or gate instead of the next block); it never disables
   it.
2. An address may be delisted only if the clone never dispatches at
   it — interior to a translated superblock, or a merged sink.
3. Gate addresses (syscall/crossing/call targets) are PERMANENTLY
   listed; no merge across a gate.
4. The list validates on load: every entry a known block start
   (cross-checked against quest.blocks); the loader refuses novelty.
5. Gen-6.0 ships the IDENTITY list (all of quest.blocks): the list
   machinery exists from day one, exercised removal starts with
   translation projects.

This preserves the invariant of every generation (CheckerHistory
header): the master runs the original bytes and is the oracle; the
clone runs the thing under test; anything the clone does differently
must be invisible to the comparison surface.

## What the clone is allowed to do differently (end state)

Execute, for any block on the TRANSLATED list, an IR transfer function
instead of the original instructions:

    block := { ordered memory operations } + { final AC assignments }
             + one exit decision (successor selection)

in the M-notation of docs/Project22/IRDesign.md (the companion doc: statement
kinds, temps, widths, ordering, folding): M1[a], M8[a], M16[a],
M32[a] for bit/byte/word/long reads and writes; R[a] for a hardware
resolve chain. `@` indirection
lowers to M32[R[a]] honestly, and rewrites to M32[M32[a]] ONLY where
the producer audit (see below) certifies every storer into `a` already
resolved. Untranslated blocks run emulated, forever if need be —
per-block coexistence is the point, and the game stays playable
through the entire migration.

**Intra-block folding (user ruling, Aug 28): only FINAL AC values are
visible.** Register traffic that is dead at block exit — the
single-def-single-use expression staging that is ~90% of all register
use — folds into expressions and is never materialized. The checker
compares answers, not pencil marks. Two disciplines make this sound:

1. **Alias discipline**: memory operations preserve program order
   within the block until non-aliasing is PROVEN (same rule as the
   census: conservatism is directional). Folding a load across a
   store is where silent wrongness lives.
2. **Flag discipline**: the carry/flag surface is settled by SCAN,
   not assumption — see Open Question Q2.

## The comparison surface

At every K-block rendezvous (and at every gate event, which forces a
rendezvous regardless of K):

- pc (= next block entry address);
- block ordinal;
- the four wide ACs;
- FP ACs and float status — see Q3;
- carry — see Q2;
- exception/abort text, as today.

Memory: UNCHANGED — mirrored shared pages with compare-on-read plus
the periodic page audit remain exactly as they are; they are already
instruction-agnostic.

Retained gates, all unchanged: the syscall gate (every trap a
rendezvous, mediated replay as today); L1↔L2 crossings pairing in both
directions; the L3 door; terminal machinery (DETACH/ABORT/RETIRE).
Blocks never span these events — a trap, LCALL into the runtime, or
crossing ends a block by construction, so gate events land on block
boundaries.

What Gen-6 gives up, stated honestly (CheckerHistory discipline):
intra-block register states and per-instruction counts are no longer
observable. A wrong folded expression that produces correct block-exit
state on every exercised input is invisible — same epistemic status as
any translation under any generation (METHOD §2: lockstep cannot
adjudicate what both engines share). Mitigations: K=1 mode; on any
divergence, both sides dump the last N block entries with entry/exit
state; the master can additionally log intra-block state on demand for
a named block (it still runs real instructions).

## The M4 accounting question

Gen-4/5's shadow accounting (wWSAVS/wWRTN counters, argwr deltas,
mapper hook bookkeeping) is denominated in instruction-level hook
firings. Translated blocks fire no instruction hooks; their arg-slot
and frame-area writes are ordinary M32 stores in the transfer
function. Ruling proposed: the Gen-4/5 counters remain live for
EMULATED blocks and are recomputed from IR effect lists for TRANSLATED
blocks (the pushmap says which stores at which sites are arg writes;
the transfer function knows its stores statically — the counts are
derivable, and the cross-check between derived and observed during the
mixed phase is itself a free validation of the IR effect lists).

## Staged landing plan

**Gen-6.0 — re-sync only.** Replace the 500-insn heartbeat with the
K-block rendezvous; ALL blocks still emulated on both sides. Pure
sync-model change, master == clone by construction. Recalibration gate
exactly as the crossings-checker landing: the FULL regression battery
(all legs incl. inject/abort/fail-open) at 0 divergences, plus a play
session. Nothing else lands until this is green.

**Gen-6.1 — the IR interpreter, register-faithful.** Clone executes
IR per-block for a small pilot set (proposed: SQR31?3's routine
DELIBERATELY EXCLUDED from the pilot — its FRDS 0,0 bug is the
canonical translation trap and deserves its own later landing with the
bug replicated; pilot = a handful of small, driver-reachable game
routines). The 6.1 interpreter computes EVERY register write
(register-faithful, no folding) so that K=1 comparison is exact at
every block — this stage validates the LOWERING, isolating it from
folding. Battery green per batch, M4-style bisection on red.

**Gen-6.2 — folded transfer functions.** Same blocks, folding ON
(final-AC-only, alias discipline enforced). K=1 still passes by
construction of the fold (dead defs don't reach block exits); battery
green. From here, translation proceeds routine-by-routine under the
usual batch/land/bisect rhythm, and the census (the shelved Project
21, rewritten) runs over IR whose semantics the oracle has been
checking in live play.

**The producer audit** (R→M32 certificate) is a Gen-6.1 prerequisite
deliverable: enumerate every arg store from the pushmaps; PEF-family
word pushes are clean by construction (verified in emulator source:
eagle_resolve_indirect returns only when bit 31 is clear, and
XPEF/LPEF push that resolved value); PEFB byte pointers must never be
word-@-dereferenced (argmap slot-type × callee @-read cross-check);
all other producers enumerated and proven or left as R[] in the IR.
Fixed-cell chains (LLEFB @[0x6xxxxxxx]): resolve through quest.mem
only for cells proven writer-free.

## Project split (user ruling, Aug 28)

Sized to not kill sessions: **P22** = Gen-6.0 exactly (the re-sync;
docs/Project22/PROMPT.md). **P23** = Gen-6.1 WITHOUT temps: 1:1
statement-per-instruction lowering, capped by INSTRUCTION CLASS, not
routine count — load/store/move/add-sub with explicit sx/zx on a
pilot set of small driver-reachable routines; multiplies and R-chains
in or out per session health; everything else embedded (the total-IR
property means stopping early still lands green). **P24** = temps +
folding (Gen-6.2). P23/P24 prompts get written after their
predecessor's REPORT exists.

## Open questions for user ruling

- **Q1 — K.** Default 50? And: is K counted per-client in game blocks
  only, or do RT blocks count? (Proposal: game blocks only; the RT is
  crossed at gate events anyway.)
- **Q2 — flags.** Claim: compiler-generated code consumes
  flags/carry within the defining block (blocks end at the
  conditional). Scan quest.blocks for any block whose instructions
  read carry/skip state before writing it. If ZERO: carry leaves the
  cross-block surface by proof. If nonzero: those blocks are listed
  and carry stays in the surface. Run the scan before ruling.
- **Q3 — FP ACs.** In the rendezvous surface always, or only for
  blocks touching FP? (Proposal: always — four extra compares is
  cheap and the float-shadow history says floats bite.)
- **Q4 — block identity under M4.** quest.blocks was cut on the
  pre-M4 program; M4 redirects change no pc flow, so boundaries
  should be unchanged — verify by regenerating and diffing before
  6.0.
- **Q5 — the M4 accounting ruling** as proposed above.

## Ordering consequence (supersedes the Aug 28 Project 21 spec)

Project 21 (access census) as written assumed a trusted static IR. It
is SHELVED pending Gen-6: banner added to docs/Project21/PROMPT.md.
New order: (1) Gen-6.0 re-sync; (2) Stage-A IR + 6.1/6.2 landings
with the producer audit; (3) the census, rewritten over verified IR;
(4) the raise census and the M5 analyses. Each step keeps the game
playable and the checker green — no analysis is ever built on
unverified semantics.


## Addendum (Aug 28, post-P22 ruling): the sync list frees the runtime for conversion

Because only game blocks are counted, ?-call interiors are unobserved
spans between crossings — the checker is already indifferent to how
the runtime computes, watching only the doorframe (L0/L1 entry and
leaf pairs), the syscall gate, and surface-visible effects (caller
memory, shared pages, exit ACs). Any ?-routine may therefore be
replaced by a native implementation honoring its summary — reads,
writes, error class — at any time after Gen-6.0, independent of game
translation progress. This generalizes M3b's contract-fidelity
concession from L2 to the entire runtime: the summary catalog is the
conversion contract, and RT conversion becomes per-routine
incremental work parallel to, not downstream of, M5.
