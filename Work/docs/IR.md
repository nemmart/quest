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

    lvalue  := ac0..ac3 | M16[e] | M32[e]
    expr    := unary/primary chained with binary ops (left-assoc,
               single level — emitters parenthesize anything else)
    binops  := + - & | ^ << #+ #-        (#* #/ reserved, refused)
    primary := acN | constant (0x… or signed decimal) | M16[e] |
               M32[e] | R[e] | sx16(e) | zx16(e) | zx8(e) |
               trunc16(e) | ( e )
    M8/M1 and t-places are reserved (IQ3 / P24) and refused.

Semantics (32-bit unsigned host arithmetic, wrap):

- M16 reads return the raw 16-bit cell zero-extended; extension is
  ALWAYS explicit in the text (`sx16(M16[…])` etc.). M16 stores write
  value & 0xFFFF; emitters write `trunc16(…)` for the audit trail.
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
  WMOV, WADD/WSUB/WADDI/WSBI, plus §6's argpush stores. Everything
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
site). Currently inexpressible: B-form byte-pointer pushes
(XPEFB/LPEFB), WPSH multi-wide pushes, borrow-adjacent blocks (all
listed with counts in Project23/REPORT.md §5/§8).

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

`save`; `#*` `#/`; M8/M1 (IQ3 byte addressing — also unlocks B-form
pushes; byte pointers are values: ((base<<1)+disp)&0x1FFFFFFF |
(seg<<29), no executor wrap); t-places and conditional exits
(`end if`-class) are P24; borrows are the designated P24 t-place
pilot (do not add borrow ops — see REPORT.md §8.6).

## 9. Version history

ir 1 (Project 23 phase 1): @pc-prefixed statements, `embed` keyword,
`end` / `end fall`, per-statement pcs. Superseded; loaders refuse it.
ir 2 (this spec): addressless statements, @addr instructions only,
blank-line blocks + trailer, call/ret/goto, mode discipline,
decoder-length continuation. Amended once in-session: call gained
site= (length knowledge removed).
