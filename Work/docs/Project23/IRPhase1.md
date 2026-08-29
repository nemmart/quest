# IR Phase 1 — specification (the P23 artifact and machinery)

*Spec draft, Aug 28 2026 planning session (user + Claude). Status:
DRAFT for user review. Binding subset of docs/Project22/IRDesign.md
for the FIRST lowering project (P23): register-faithful, 1:1,
class-capped, no temps, no folding. Where this spec and IRDesign.md
differ in ambition, this spec wins for P23; IRDesign.md remains the
end-state sketch (temps, folding, intrinsics arrive P24+).*

## 1. Pipeline shape (user-agreed)

    quest.dis + quest.blocks (+ quest.tags, quest.argmap,
    quest.pushmap.M4, quest.addrbook)
            |
       c_src/tools/lower.py        offline, static, per-class rules
            v
       c_src/quest.ir              textual, diffable, provenance-
            |                      stamped (sha256 of every input)
            v
       hw/IRExec.{hpp,cpp}         loader + block interpreter in the
                                   emulator; QUEST_IR env (matching
                                   QUEST_SYNC_LIST); CRLF-tolerant
                                   per P22 ruling (c); refuses on
                                   provenance mismatch or any
                                   validation failure

- **Dispatch rule:** a block PRESENT in quest.ir is executed as IR by
  the CLONE; absent = emulated. Presence is the translation decision.
  The master always emulates (it is the oracle). quest.ir and
  quest.synclist are sibling artifacts; in Phase 1 the synclist stays
  IDENTITY (no block merging — every quest.blocks start listed).
- The executor is an INTERPRETER. Slow is fine; the oracle bounds
  correctness, not speed. Native codegen is a distant later stage
  reusing this artifact.
- fold.py (P24) will be IR-to-IR: reads quest.ir, writes a folded
  quest.ir in the SAME grammar. The executor never knows which it is
  running. This spec's grammar must therefore not preclude temps —
  the statement forms below leave room (t-places are reserved, unused
  in Phase 1).

## 2. The two statement formats (user ruling, Aug 28)

A block body is a sequence of statements. A statement line either
CONTAINS `=` or it does not — two disjoint sub-languages:

1. **Expression statement** — always has `=`. Left side: an
   assignable place (ac0..ac3, c, ovr, or a memory cell M1/M8/M16/
   M32[expr]). Right side: an expression. The `#`-operator family
   lives ONLY here.
2. **Embedded instruction** — no `=`. The original Eagle instruction,
   executed verbatim by the emulator against real machine state.
   Written as `embed <pc> ; <disassembly text>` — the pc is
   authoritative (the executor runs the instruction at that address);
   the text is a human-side comment lower.py copies from quest.dis.

The Nova/Eagle `.#` (no-load) suffix therefore appears only in
embedded lines; the IR `#+` (flag-writing) operators appear only
right of an `=`. The two `#` meanings can never occur in the same
context.

The IR is TOTAL: a block of nothing but embedded statements is valid.
"Not yet lowered" is an embedded statement, never an error.

## 3. Expressions

    place  := ac0 | ac1 | ac2 | ac3
              (t0.. reserved for P24; invalid in Phase 1. There is
               deliberately NO c/ovr place: flags are not IR-visible
               state in Phase 1 — see the # family below)
    mem    := M1[e] | M8[e] | M16[e] | M32[e]
              (native encodings per width: word addresses for M16/
               M32, byte pointers for M8, bit pointers for M1 —
               encoding confirmed from emulator source before
               lower.py emits any M1/M8: IQ3 stands)
    expr   := place | constant | mem | R[e]
            | e + e | e - e | e * e | e / e        (PURE: no flag
                                                    effects, full
                                                    32-bit wrap)
            | e #+ e | e #- e | e #* e | e #/ e    (FLAG-WRITING:
                                                    the executor
                                                    updates MACHINE
                                                    c/ovr eagerly as
                                                    a side effect,
                                                    per the hardware
                                                    op's semantics)
            | sx8(e) | sx16(e) | zx8(e) | zx16(e) | trunc16(e)
            | shl(e,n) | shr(e,n) | ...            (as the class cap
                                                    needs them)
            | bitwise and/or/xor/not
    R[e]   := the hardware resolve chain (follow bit 31 until clear).
              Rewrite R[e] -> M32[e] only under the producer-audit
              certificate (BlockSyncDesign); unproven chains keep R.

**Flag semantics of the `#` family (user ruling, Aug 28).** Flags
are NOT IR places and nothing in a Phase 1 expression can read them.
An operator is written `#+`/`#-`/`#*`/`#/` iff the hardware op writes
c and/or ovr; the executor performs that write to MACHINE state
eagerly, at the statement, per the extracted formulas. `+` is emitted
only where the hardware op truly leaves flags alone. This is sound in
Phase 1 because the only computational carry reader in the entire ISA
is the Nova ALC datapath (verified exhaustively from emulator source,
Aug 28: every other machine.c touch is frame save/restore capture or
a pure write) and NO NOVA ALC INSTRUCTION LOWERS IN PHASE 1 — every
flag consumer executes as an embedded/emulated instruction against
real machine state, which the eager `#`-writes keep current. The
rendezvous surface is UNCHANGED (c and ovr still compared strictly at
every pair — a wrong or missing `#` cannot survive K=1).

**Sticky ovr (extraction finding, Aug 28):** the wide add/sub helpers
do `ovr |= overflow_bit`, not `=` — overflow ACCUMULATES until
something clears it. The per-op tables must record OR-into vs clear
semantics exactly; a `#`-op that ORs must OR.

**Exact per-op c/ovr formulas are a lower.py deliverable EXTRACTED
from the emulator source** (METHOD §5), not hand-derived: the 16-bit
ALC 17-bit-datapath rules are already nailed by P22 REPORT §8; the
wide W/X-form carry/overflow behavior gets the same extraction
treatment. Every formula is then verified by 6.1's K=1 lockstep —
a wrong formula cannot survive a battery.

**OVK extraction item (open, blocks nothing in Phase 1 planning but
gates lower.py's `#/` and `#+` emission):** if the machine can run
with overflow faulting enabled (OVK), a `#`-op is not a pure
assignment — it has a fault edge. EXTRACT from the emulator's fault
path + the game's PSR handling whether Quest ever enables OVK. If
provably disabled: ovr is a pure flag; spec so states with evidence.
If enabled anywhere: `#`-ops in affected regions stay EMBEDDED in
Phase 1 (the total-IR escape hatch), and P24 designs the trap edge.

## 4. Phase 1 class cap (user ruling: cap by instruction class)

Lowered classes — load/store/move/add-sub, wide and narrow, with
explicit extension:

- loads: XNLDA/LNLDA -> `acX = sx16(M16[ea])`; XWLDA/LWLDA ->
  `acX = M32[ea]`; byte loads if the M8 encoding is confirmed;
  WLDAI/NLDAI immediates -> `acX = const`.
- stores: XNSTA/LNSTA -> `M16[ea] = trunc16(acX)`; XWSTA/LWSTA ->
  `M32[ea] = acX`.
- moves: WMOV and register-to-register forms without skip suffixes.
- add/sub: WADD/WSUB/WADDI/WSBI — emitted with `#+`/`#-` (they
  write flags on the real machine; the IR says so). **NO NOVA/16-bit
  ALC INSTRUCTION LOWERS IN PHASE 1** (user ruling, Aug 28) — the ALC
  carry datapath stays entirely in embedded/emulated territory, which
  is what licenses flag-free expressions. The 31 ALC carry-consumer
  blocks from P22 §8 are thereby moot for Phase 1.
- effective-address forms feeding the above (XLEF etc.) as plain
  address arithmetic.

Everything else stays EMBEDDED in Phase 1, explicitly including:
skip-suffixed anything (terminators and guards — ALL block
terminators are embedded in Phase 1; the original branch/skip/call
instruction performs successor selection), WSAVS/WSSVS/WRTN, all
calls, SYSCALLs, DERR, multiplies/divides (in or out per session
health — if in, `#*`/`#/` with extracted semantics), all FP, WCMV
and other microcoded ops, ENQT/DEQUE, anything the extraction finds
surprising.

**Block exclusion lists (from P22 REPORT §4):** 7015BD6B (interior
unresolved LJSR) and every block containing ENQT/DEQUE (unmodeled
skip edges — the drawn CFG lies) are excluded from lowering entirely
— lower.py refuses to emit them even fully-embedded, so the
dispatch table cannot route them to IRExec.

**Pilot set:** small driver-reachable routines chosen by the session
(suggested starting pool: RANDOM, DIST, OWNS, TERRAIN bodies — high
fan-in, small, battery-exercised); at least one pilot block MUST
contain an embedded statement mid-block, so the barrier machinery is
exercised in P23, not discovered in P24.

## 5. argstore annotation (user-agreed) and the Q5 answer

Arg-slot stores at decorated call sites are ordinary expression
statements carrying a metadata prefix:

    argstore <TARGET>@<site-pc>.slot<N> : M32[<book-addr>] = <expr>
    argstore.ea <TARGET>@<site-pc>.slot<N> : M32[<book-addr>] = <ea-expr>

- Semantically IDENTICAL to the unannotated store (same address, same
  value); the annotation is metadata over identical semantics.
- **Loader cross-validation:** every argstore must agree with
  quest.pushmap.M4 on (site, slot, book address); every pushmap-known
  store inside a lowered block must carry the annotation; any
  disagreement refuses to load. The annotation layer is un-driftable
  by construction.
- **Q5 RESOLVED:** when IRExec executes an argstore statement it
  fires note_arg_write — the SAME mapper hook the instruction-level
  redirect fires today. Gen-4/5 shadow accounting stays live,
  identically denominated, on both paths; mixed emulated/IR operation
  cross-validates the counters for free. Borrow-bracket (former
  WPSH/WPOP group) stores get the same treatment with their book
  tag when those blocks are ever lowered (Phase 1: they may simply
  stay embedded).
- P23 must remove the TEMPORARY instruction-count delta compare
  (P22 ruling (a)) before the first IR block executes.

## 6. quest.ir grammar (concrete)

    ir 1
    source  quest.dis     sha256=<hex>
    blocks  quest.blocks  sha256=<hex>
    pushmap quest.pushmap.M4 sha256=<hex>
    argmap  quest.argmap  sha256=<hex>

    block 7016B549
      @7016B549  ac0 = sx16(M16[0x70000216])
      @7016B54B  embed 7016B54B ; WSGTI 0,10
      @7016B54D  embed 7016B54D ; WSGT 0,0
      @7016B54F  embed 7016B54F ; DERR 17
      ...
      @7016B560  embed 7016B560 ; MOV.# 2,2,SNR   <- terminator,
    end                                              embedded

- One statement per original instruction (register-faithful 1:1);
  every statement carries its source pc (`@pc`) for divergence dumps
  and coverage accounting.
- Comments `;` to end of line. CRLF tolerated. Unknown/future
  directives REFUSE (no silent skip), version-gated by the `ir 1`
  header.
- Loader validation beyond argstore: every `block` pc a quest.blocks
  start; statement pcs monotonically within the block's range; no pc
  gaps (every instruction of a lowered block accounted for, as
  expression or embed); excluded blocks absent; widths/encodings
  well-formed.

## 7. Executor contract (hw/IRExec)

1. On clone dispatch at a lowered block's entry: read machine ACs
   into block-local variables (flags stay machine-resident; `#`-ops
   write them eagerly in place); execute statements in order.
2. Expression statements evaluate against block-locals and memory.
   ALL memory operations go through the SAME Machine memory path as
   emulated instructions — mirror compare-on-read, mapper redirects,
   page audit all see identical traffic. One memory op per statement
   (canonical form) in Phase 1.
3. **Embedded statement = full barrier:** materialize all block-
   locals to machine state; emulator executes the single instruction
   at the statement's pc (with all its hooks, exactly as the master
   does); re-read block-locals. A skip-taking embedded terminator
   sets pc; the executor's job at block end is only to hand control
   back to normal dispatch.
4. Block exit: materialize block-locals; block_ordinal ticks via the
   normal listed-entry mechanism at the next block (BlockSync
   machinery unchanged).
5. Any executor-detected impossibility (unvalidated statement shape
   at runtime, pc mismatch after embed, R chain deeper than the
   certificate allows) THROWS loudly — METHOD §8, never continue.

## 8. Validation story (how P23 proves it)

- Stage gates as BlockSyncDesign 6.1: pilot blocks lowered ->
  K=1 battery leg (every rendezvous compares every AC + machine
  c/ovr + FP surface at every listed entry — the flag compare is the
  `#`-correctness check) -> full battery at K=50 under
  the strict 030-shaped gate (pairs floor, endpoint pin) -> land.
- Coverage accounting in the report: which blocks lowered, statement
  census (expression vs embed per class), how many embedded
  statements executed live (the barrier path must show nonzero).
- M4-style batch/land/bisect per pilot batch; red bisects by
  removing blocks from quest.ir (the dispatch rule makes bisection
  a file edit, no rebuild).

## 9. Open items carried

- IQ3 (M1/M8 encodings) — extract before emitting those widths.
- OVK (§3) — extract; gates `#`-op emission per region.
- Wide-form c/ovr extraction tables (incl. OR-into vs clear per op
  — sticky ovr) — lower.py deliverable.
- trap_if / exit forms / temps / folding — P24+, grammar reserved.
