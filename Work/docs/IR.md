# quest.ir — THE IR SPECIFICATION (consolidated, standalone)

Version: ir 2. This document is self-contained and normative; it
consolidates IRPhase1.md (as amended) and Project23/IR2.md. History
and rationale live in Project23/REPORT.md. When this spec and older
documents disagree, this spec wins.

## 1. Role and dispatch

quest.ir is an acceleration/representation overlay for the CLONE in
lockstep runs. A block PRESENT in the file is executed by the clone's
IR interpreter (hw/IRExec); a block ABSENT is emulated; the MASTER
always emulates. Omission is therefore always safe, and the emitter
(tools/lower.py) is TOTAL by policy: any block it cannot express it
omits (with a censused reason), never approximates.

Everything the IR does is verified against the master at every K=1
pair on the strict surface (registers, c AND ovr, wsp/shadow, fp
state, pc, structure). The IR carries no semantics of its own beyond
this spec: wherever an operation corresponds to machine behavior, the
executor calls the SAME emulator code paths the instruction would
(shared helpers for #-ops; the real decode/execute path for
instructions, calls, and rets).

## 2. File structure

    ir 2
    mode <stock|book>
    source  <path> sha256=<hex>
    blocks  <path> sha256=<hex>
    pushmap <path> sha256=<hex>
    argmap  <path> sha256=<hex>

    block <hex8> seg <hex8>
    <block lines...>
                                  <- blocks separated by BLANK lines
    block <hex8> seg <hex8>
    <block lines...>

    blocks <count>                <- trailer (count of block sections)

- `;` begins a comment anywhere on any line (the grammar has no other
  use of `;`). Comments are audit trail: emitters SHOULD echo source
  disassembly on derived lines.
- Provenance: the loader recomputes sha256 of the file named by
  QUEST_BLOCKS and refuses on mismatch with the `blocks` line; other
  recorded inputs are verified when present on the host. quest.ir is
  therefore bound to the exact CFG the run uses.
- `mode`: `book` declares that decorated-site lowering (§6) is
  present; the loader REFUSES book-mode IR unless QUEST_ADDRESS_BOOK
  and QUEST_PUSH_MAP are set (in a stock run the area pages are not
  mapped and decorated semantics are wrong). `stock` IR is valid in
  both configurations.
- Trailer: `blocks <count>` must equal the number of block sections
  (truncation net; the terminator rule of §4 is the mid-block net).

## 3. Block lines

Distinguished by first token. ONLY literal machine instructions carry
addresses; blocks are single-entry, so statements need no identities.

    @<hex8> <text>                 INSTRUCTION. The machine instruction
                                   at that address, executed via the
                                   normal fetch/decode/execute path
                                   with all hooks. <text> is audit
                                   trail only — never parsed.
    <lvalue> = <expr>              STATEMENT (addressless). §5.
    call <tgt> args=<n> marker=<hex8> site=<hex8> ret=<hex8>
                                   Decorated call. TERMINATOR. §6.
    ret                            WRTN. TERMINATOR. §6.
    goto <hex8>                    Unconditional exit. TERMINATOR.
                                   Target must be a listed block
                                   start. Covers fall-through into
                                   the next block and lowered
                                   unconditional WBR.
    save <hex>                     RESERVED (not implemented; loader
                                   refuses). WSAVS reads its frame
                                   word from memory, so it needs an
                                   address story first.
    assert(e)                      STATEMENT (P25). Evaluates e; 0 is
    assert(e, "message")           failure. Never a terminator. On the
                                   CLONE, failure prints the statement
                                   ("IR ASSERT FAILED [block, stmt]:
                                   <source text>") and DETACHES — the
                                   clone halts, the master (ground
                                   truth) continues unverified, and
                                   compare_pair's detached early-out
                                   keeps the truncated batch from
                                   reading as a divergence (user
                                   ruling, Aug 29). Outside lockstep
                                   it throws (loud, METHOD §8). The
                                   message may not contain '"'
                                   (grammar) and cannot contain ';'
                                   by construction (comments strip
                                   first); malformed forms REFUSE at
                                   load.

One machine instruction MAY lower to several statements (e.g. WPSH,
future): no bookkeeping is required or possible — statements are
sequence, not identities.

## 4. Block rules (loader-enforced; violations REFUSE at load)

- `block <pc> seg <s>`: pc must be a listed quest.blocks start;
  s == pc & 0xF0000000; no duplicates; excluded blocks (7015BD6B)
  refused.
- Instruction addresses within a block strictly increase.
- TERMINATOR RULE: the last line of every block is an instruction,
  `call`, `ret`, or `goto`. (A final instruction's control transfer —
  branch, skip, return, call, fault — IS the exit.)
- Anything unrecognized refuses. No silent skips, ever — including in
  the emitter's own input parsers.

## 5. Statements and expressions

    lvalue  := ac0..ac3 | M8[e] | M16[e] | M32[e]
    expr    := unary/primary chained with binary ops (left-assoc,
               single level — emitters parenthesize anything else)
    binops  := + - * & | ^ #+ #-         (#* #/ reserved, refused)
    primary := acN | constant (0x… or signed decimal) |
               byte-pointer literal 0xW:b (b in {0,1}) | M8[e] | M16[e] |
               M32[e] | R[e] | wp(e, e) | bp(e, e) | sx16(e) | zx16(e) |
               zx8(e) | trunc16(e) | ( e )
    M1 and t-places are reserved (IQ3 / P26) and refused.
    (`<<` was listed as a binop by earlier revisions of this spec but
    was never implemented by IRExec; P25 removed it in favor of `*` —
    see ByteEA.md. Spec-vs-executor gaps are findings, not features.)

Semantics (32-bit unsigned host arithmetic, wrap):

- M16 reads return the raw 16-bit cell zero-extended; extension is
  ALWAYS explicit in the text (`sx16(M16[…])` etc.). M16 stores write
  value & 0xFFFF; emitters write `trunc16(…)` for the audit trail.
- M8 (P25, byte addressing): reads return the byte zero-extended
  (Memory::read_byte); stores write value & 0xFF (write_byte); emitters
  write `zx8(…)` for the audit trail. The M8 INDEX IS RAW — no segment
  wrap. Byte pointers carry their own segment in bits 31:29
  (set_byte_segment packing) and the hardware applies no masking at the
  point of use (WLDB/WSTB deref ac[II] unmasked; see
  Project25/ByteEA.md). A garbage byte pointer faults in read_byte
  exactly as the emulated instruction would — loud and identical, per
  METHOD §8.
- wp(b, d) / bp(b, d) (P25 pointer builders — masking lives in the
  executor, never in emitted text; user ruling, Aug 29): wp is the word
  segment wrap of b+d — ((b+d) & 0x0FFFFFFF) | seg, seg from the block
  header (Machine::copy_segment semantics). bp is
  Machine::set_byte_segment(seg, b*2 + d) — the base is a word address
  scaled to bytes, the displacement is already in bytes (that asymmetry
  is the hardware's; eagle_x_byte_indexed ii=2/3). L-form byte EAs
  apply NO masking and therefore never render as bp: they emit raw
  arithmetic (acN*2 + disp) or constants (ByteEA.md has the per-mode
  table read out of the emulator source).
- `*` is host 32-bit multiply, wrap, no flags (distinct from the
  reserved #-op `#*`, which owns flag semantics if ever needed).
- Byte-pointer literal `0xW:b` (P25, user ruling): value = W*2 + b.
  Pure notation for a 32-bit byte pointer in the disassembler's fold
  form — W is the WORD address (what memory dumps use), b the byte
  select. `:b` means BYTE SELECT exclusively and permanently: b is 0
  or 1, anything else refuses. Emitters use it for every constant
  byte EA and for L-form byte-table bases (`acN*2 + 0xW:b`), so the
  IR text is greppable against word-addressed dumps and matches the
  dis rendering. Word-pointer constants stay plain hex (already
  word-addressed). wp/bp remain the REGISTER-RELATIVE forms only —
  no wp(0,d)/bp(0,d) is ever emitted. Bit-pointer literals (M1,
  future) must NOT overload `:` — see §8.
- SEGMENT WRAP (executor rule): every M/R INDEX is evaluated as
  (e & 0x0FFFFFFF) | seg, seg from the block header. The emitter
  refuses any absolute or pc-folded EA outside the block's segment,
  which makes the uniform wrap provably identity-or-hardware-exact.
  The wrap applies ONLY to memory/resolve indices — a computed
  address stored as a VALUE (e.g. an arg-slot EA, a byte pointer)
  must carry any needed masking explicitly in its expression.
- R[e]: hardware indirect resolution — deref e, then follow bit 31
  until clear (executor: eagle_resolve_indirect(wrap(e)|0x80000000),
  inheriting the depth limit and its throw). An R result used as a
  memory index is NOT re-wrapped (chain pointers are full addresses).
  Emitters produce R only where the instruction's indirect bit is set.
- #+ / #-: `l #+ r` == EagleInstruction::add(machine, src=r, dst=l);
  `l #- r` == sub(machine, src=r, dst=l) — the SAME helpers the
  emulated instructions call, including their c/ovr writes (sticky
  ovr |=). No formulas live in the IR: when the helpers change (see
  Project23/WideCarry.md), the IR changes with them. Every statement
  whose expression contains a #-op ends with the emulator's
  `ovk && ovr` check; the throw attributes to the BLOCK (statements
  have no pc — accepted downgrade, the path has never fired).
- Class cap (what lower.py currently emits): loads/stores (X/L ×
  N/W LDA/STA, modes 0–3, direct/indirect), XLEF/LLEF, NLDAI/WLDAI,
  WMOV, WADD/WSUB/WADDI/WSBI, byte addressing (XLEFB/LLEFB values,
  XLDB/XSTB/LLDB/LSTB/WLDB/WSTB via M8 — P25), plus §6's argpush
  stores (word wp/constant/R, byte value, and WPSH group stores —
  P25). Word base-indexed EAs render as wp(acN, d) in BOTH value and
  index positions (index wrap on a wp result is identity). Everything
  else stays an instruction. The cap widens by extraction, never by
  assumption.

## 6. Operations

Common principle: operands are DECLARED BELIEFS. The emitter computes
them from the artifacts; the loader cross-validates them against the
artifacts it can reach; the executor anchors to real emulator code so
a false belief diverges loudly rather than being trusted.

- `call <tgt> args=<n> marker=<m> site=<s> ret=<r>` — a
  pushmap-decorated LCALL or XCALL site. The executor sets pc=s,
  performs the batched copied-args accounting
  (mapper.note_arg_write(machine, n) — n is the ELIDED WIDES of the
  site's redirected pushes, whose stores appear earlier in the block
  as plain `M32[slot] = <ea>` statements), then executes the actual
  instruction at `site` via the normal path: byte-exact decorated
  protocol (marker value (psr<<16)|argc or argc&0x7FFF, marker PUSH,
  marker-slot write, args_written, callee-verification abort, ovr
  clear, ac3 = real return pc). `ret=` is validated at load (pushmap)
  and never used at runtime except as a belief — ac3 comes from the
  instruction. Loader checks: (site, marker) is a pushmap call entry;
  n equals the pushmap's wides sum for the site. NOTE deliberately
  absent: any notion of instruction LENGTH — that is why `site` is
  explicit.
- `ret` — executes WRTN's fixed opcode (0x87A9) through the normal
  decode path (WRTN is address-independent; the synthetic address is
  the block start, reaching only abort messages). Frame pop, psr/ovk
  restore, carry from bit 31 of the return slot — all shared code.
- `goto <t>` — pure exit: materialize registers, return t.

Scope by decoration ("no mixed metaphors", user ruling): a decorated
site's pushes lower ONLY if every decorated push of the site is
expressible and in the site's block; otherwise the whole push
sequence AND its call stay instructions (uniform accounting per
site). As of P25 all 566 decorated sites lower (566/566;
Project25/ByteEA.md is the ledger): B-form pushes emit byte-pointer
VALUES, one WPSH x,a emits its wides as ascending group stores
M32[slot+2k] = ac((x+k)&3) (AC[XX] at the base slot — the emulated
hook's verified ordering, EagleStack.cpp P18 tranche B), and borrow
brackets inside decorated blocks stay @addr INSTRUCTION pairs (user
ruling, Aug 29: push/pop as instructions, args as stores — the P20
slot-redirect hooks fire on the normal execute path and the
bracket's note_arg_write/pop nets zero; the site's args= never
counted the bracket).

## 7. Execution model (normative behavior of hw/IRExec)

- Registers: the interpreter holds ac0–3 in locals; instructions and
  every terminator materialize to machine.ac (and instructions
  re-read after). EVERY exit path materializes.
- Instructions: full barrier — materialize, Capture hook, fetch,
  decode, execute, ovk/ovr check, instruction_count++, re-read.
  Syscall sentinel (0x30000000) propagates to the batch machinery.
- CONTINUATION TRIPWIRE: a NON-final instruction must continue to
  addr + Disassembler::word_length(decoded format). A target outside
  [game_start, game_stop) is a legitimate fault/OS edge and exits the
  block; an in-range mismatch THROWS. (This cross-validates execute
  against the decode table at runtime; it is what surfaced the
  LNADI/LNSBI listing defect. Its trustworthiness depends on the
  tinyImmediateWideIndirect length fix — landed in both toolchains.)
  This rule presumes the post-split CFG: ALL conditional-length
  instructions are block terminators (tools/split_skips.py; user
  ruling).
- Statement memory faults are rethrown with [block, statement index,
  store address] context.
- Debug: QUEST_IR_DEBUG_BLOCK=<hex> prints per-statement ac state for
  that block. First execution of each block logs once to stderr
  (coverage evidence).
- QUEST_IR requires -lockstep (refused otherwise: only the clone
  dispatches IR; a non-lockstep run would silently ignore it).

## 8. Reserved / roadmap

`save`; `#*` `#/`; M1 (bit addressing, IQ3 — when it lands, bit
pointers get the function-style literal `bitp(w, n)` (n = 0..31),
matching the wp/bp precedent; the colon form `0xW:b` is byte-select
FOREVER and is not to be overloaded — user ruling, Aug 29 2026). Byte addressing (M8,
wp/bp, B-form pushes) LANDED in P25 — and the formula this section
used to park, ((base<<1)+disp)&0x1FFFFFFF | (seg<<29), was found to
describe only the X-form ii=0/2/3 cases: the L-form byte EA applies
no masking in any mode and the X pc-relative mode none either
(Project25/ByteEA.md §2, semantics read from
Machine::eagle_{x,l}_byte_indexed per METHOD §5; recorded as a
correction, METHOD §11). t-places and conditional exits
(`end if`-class) are P26; borrows convert to t-places there (no
borrow ops — standing ruling; the two decorated borrow brackets ride
as @addr instructions until then).

## 9. Version history

ir 1 (Project 23 phase 1): @pc-prefixed statements, `embed` keyword,
`end` / `end fall`, per-statement pcs. Superseded; loaders refuse it.
ir 2 (this spec): addressless statements, @addr instructions only,
blank-line blocks + trailer, call/ret/goto, mode discipline,
decoder-length continuation. Amended once in-session: call gained
site= (length knowledge removed).
ir 2, P25 amendment (byte addressing + pointer builders, Aug 29
2026): M8[e] lvalue/primary (raw index; read_byte/write_byte
pass-through), wp(b,d)/bp(b,d) pointer builders (masking in the
executor, not the text — retires pef_value's spelled-out word mask
and the old unwrapped XLEF/LLEF value emission, a latent
inconsistency recorded in ByteEA.md), `*` host-multiply binop, `<<`
removed (was specified, never implemented), WPSH group stores,
@addr borrow brackets inside lowered decorated blocks, and the
byte-pointer literal 0xW:b (word-addressed fold notation; `:` is
byte-select only, bitp(w,n) reserved for M1). Second P25 amendment:
`assert(e[, "msg"])` statement — clone prints + detaches on failure;
compare_pair gained the detached early-out (which also closes the
documented straddling-batch latent race after process-wide detach). Grammar is a
superset except `<<`; pre-P25 loaders refuse the new forms (regenerate
artifacts and binaries together, as always).

> Forward design of record for the NEXT grammar revision (terminator,
> booleans, s/u comparisons, add()/sub() family superseding #+/#-):
> docs/Project26/MathDesign.md (Aug 29 2026). This spec remains the law
> for the shipped rev-2 artifacts until that revision lands.
