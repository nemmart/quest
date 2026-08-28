# IR Design — the block-local expression IR (Gen-6 companion)

*Design draft, Aug 28 2026 planning session (user + Claude). Status:
DRAFT for user review. Companion to docs/Project22/BlockSyncDesign.md: that doc
says how translated blocks are CHECKED; this one says what a
translated block IS. Nothing implemented.*

## Shape

The unit of translation is the basic block (quest.blocks). A block in
IR is a sequence of STATEMENTS followed by one EXIT. A statement is
one of:

1. **Expression statement** — `dst = expr` or a memory store.
2. **Embedded instruction** — an original Eagle instruction executed
   by the emulator, verbatim, inside the block.
3. **Intrinsic** *(later, not day one)* — a complex instruction
   (WCMV, XCT'd queue ops, ...) as one opaque operation with DECLARED
   effects, so analyses can reason where an embedded instruction is
   opaque. Until an instruction earns an intrinsic, it stays embedded.

**The IR is TOTAL from day one**: a block with every statement
embedded is a valid (if useless) translation. Lowering proceeds
instruction-class by instruction-class; "not yet lowered" is an
embedded statement, not an error. Progress is a shrinking census of
embedded statements. Dirty instructions — status-register play that
does not fit the expression forms, the float instructions
(float-shadow history: distrust earned), anything weird — simply stay
embedded indefinitely.

## Values and expressions

    place  := ac0 | ac1 | ac2 | ac3          (block-local variables)
            | t0, t1, ... tN                  (temps)
    mem    := M1[e] | M8[e] | M16[e] | M32[e] (bit/byte/word/long;
                                               native encodings per
                                               width: word addrs for
                                               M16/M32, byte ptrs for
                                               M8, bit ptrs for M1)
    expr   := place | constant | mem-read | R[e]
            | e + e | e - e | e * e | e / e | ...
            | sx8(e) | sx16(e) | zx8(e) | zx16(e) | trunc16(e) | ...
            | (cond) ? e : e                  (if it earns its keep)
    cond   := e == e | e != e | e < e | e <= e | ...  (signedness
                                               explicit where it
                                               matters: <s vs <u)

**Widths and sign are explicit.** `XNLDA 2,[x]` lowers to
`ac2 = sx16(M16[x])`, never `ac2 = M16[x]`. Zero-extension likewise.
An IR that hides extension behavior lies about high halves; every
narrow load/store makes its extension visible.

**R[e]** is the honest hardware resolve chain (follow bit 31 until
clear, emulator's eagle_resolve_indirect). `@` lowers to M32[R[e]] /
store through R[e]. The rewrite R[e] → M32[e] (one proven hop) is
licensed PER SLOT/CELL by the producer-audit certificate
(BlockSyncDesign; PEF-family pushes proven clean in emulator source).
An R surviving in shipped IR is self-documenting residue: an unproven
chain, visible in the census.

## Temps

- **Strictly block-local** (user ruling, Aug 28). A temp never
  crosses a block boundary and never enters any comparison surface.
- **Single-assignment.** Each tN assigned exactly once per block.
  Def-use is trivial; folding is pure substitution.
- ACs within a block are variables exactly like temps; they
  materialize to machine state at block exit and at embedded-
  instruction barriers. Only ACs and memory cross blocks.

## Statement forms

    tN  = expr
    acX = expr
    M32[e] = expr          (and M16/M8/M1 stores)
    trap_if cond           (the WSGTI/WSGT/DERR triple and kin: a
                            guard whose failure edge is the DERR
                            terminal — a graph sink, no successor
                            inside the block. When translation merges
                            a separate DERR sink block into the guard
                            as a trap_if, the sink's address comes
                            off the sync list per BlockSyncDesign's
                            delisting rules)
    <embedded instruction>
    <intrinsic>            (later)

Exits (one per block, matching quest.blocks terminators):

    exit next B             (fallthrough / unconditional)
    exit cond ? B1 : B2     (conditional — the skip idioms' home)
    exit call ...           (LCALL etc.; gate events per Gen-6 —
                             blocks end at calls/traps/crossings by
                             construction)
    exit switch/jump ...    (dispatch forms, per terminator grammar)
    exit terminal           (DERR reached unconditionally, SYSCALL
                             0310, ...)

## Ordering rules (the soundness core)

1. **Memory operations preserve program order within a block** —
   READS INCLUDED. Shared pages make reads observable events (the
   mirror is compare-on-read and the server writes concurrently), so
   read order is harness-visible behavior, not an invisible detail.
2. **Canonical (unfolded) form: at most one memory operation per
   statement.** `t1 = M32[R[a]] + M32[b]` is a FOLDED form, legal
   only when the constituent reads are proven order-free: no
   intervening store may alias, and no shared-page read may be
   reordered past anything. Non-shared, non-aliasing reads may fuse.
3. **Embedded instructions are barriers.** Conservative day-one rule:
   full barrier — all ACs materialized before, re-read after, no
   memory op folded across. Refine later with per-instruction
   read/write sets if the embedded census says it is worth it.
4. **trap_if is an ordering point for observability**: effects before
   the guard happened, effects after did not (the DERR terminal must
   see the same memory state either side of translation). Folding
   never moves a memory op across a trap_if.

## Folding

Lowering and folding are SEPARATE passes with separate validation:

- **Lowering** (instruction → canonical statements, 1:1-ish) is
  validated by the oracle: Gen-6.1 runs the register-faithful
  interpreter under K=1 lockstep.
- **Folding** (canonical IR → folded IR: substitute single-use temps,
  fuse proven-safe reads, drop dead defs) is IR-to-IR and validated
  WITHOUT the emulator: execute both forms on the same inputs,
  compare final ACs + the memory-op trace. Gen-6.2 then runs folded
  IR under the oracle as the second, independent check.

Fold rules: substitution only within maximal runs of expression
statements (embedded statements bound them); dead-at-exit AC/temp
defs drop (final-AC-only visibility, per Gen-6); ordering rules above
are inviolable.

## Worked example

GET_QUEST body head (0x7016B549, after WSAVS):

    Eagle                              IR (folded)
    -----                              ------------
    LNLDA 0,[0x70000216]               t0 = sx16(M16[0x70000216])   ; PLAYER_NUM
    WSGTI 0,10; WSGT 0,0; DERR 17      trap_if !(0 <s t0 && t0 <=s 10)
    NLDAI 686,1; WMUL 1,0              t1 = t0 * 686
    LWADD 0,[0x70000210]; WMOV 0,2     t2 = M32[0x70000210] + t1    ; SD_PTR + rec
    XNLDA 2,[ac2+0x2A]                 t3 = sx16(M16[t2 + 0x2A])
    MOV.# 2,2,SNR; WBR ...             exit (t3 == 0) ? B_7016B569
                                            : B_7016B55D
                                       ; block-exit ACs: ac0=t2? ac1=t1?
                                       ; ac2=t3? — final-AC assignments
                                       ; emitted per actual liveness at
                                       ; the boundary (6.1 materializes
                                       ; all; 6.2 folds)

The bounds-check triple collapses to trap_if; the skip idiom is the
exit condition — flags as exit decisions, not state (the Q2 bet; the
scan decides the residue).

## Open questions

- **IQ1** — trap_if surface: is the DERR site pc part of the trap's
  identity (for pairing a translated trap against the master's DERR)?
  Proposal: yes, carried as metadata on the statement.
- **IQ2** — division/checked converts (WDIV, CVWN): trapping
  instructions mid-block. trap_if-style guard + expression, or
  embedded until proven? Proposal: embedded day one; promote with
  evidence.
- **IQ3** — M1 bit-pointer encoding: confirm exact encoding from the
  emulator's bit-instruction implementations before freezing the
  grammar (METHOD §5).
- **IQ4** — carry-consuming arithmetic (ADD.O#, NEG.L# SNC multi-word
  idioms): embedded forever, intrinsic pairs, or expression forms
  with an explicit carry temp? Wait for the Q2 scan's list; decide on
  the actual population.
