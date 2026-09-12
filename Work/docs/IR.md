# quest.ir — THE IR SPECIFICATION (consolidated, standalone)

Version: **ir 8** (Project 52, Sep 12 2026 — docs/Project52/{PROMPT,
q001-plan-gate,a001-plan-gate}.md, REPORT.md: THE VARIABLE FORM — a `v`
name denotes the CELL'S CONTENTS as a C variable does, not its address;
POINTER VTYPES `*i16 *u16 *i32 *u32 *char *varying n *words n`, one level
only, word/byte kind enforced and pointee width advisory; ARGUMENT CELLS
`a <ENTRY>.a<N>`, `a <ENTRY>.arg_count` and `a <ENTRY>.ret`, which make a
routine's signature DECLARED; INITIALISED `v`s, so a literal the image does
not already contain can be spelled; `trunc8`; `call`/`rt_call` legal in a
symbolic block with a SYMBOLIC RETURN LABEL; `wp`/`bp` resolving by operand
kind; §5.10. The loader refuses `ir 6` and, except as §5.10.9 allows,
`ir 7`). ir 7 (Project 46, Sep 12 2026 — docs/Project44/DESIGN.md §4–§6,
docs/Project46/{q001,a001}-plan-gate.md, REPORT.md) was declared storage
`v` and SYMBOLIC BLOCKS, so that a naive, unplaced program loads and runs
— `v <ENTRY>.v<k> <type>` declarations placed by the loader at 0x76,
`block <ENTRY>.b<k>` headers and `goto` labels placed at 0x77, §5.10; plus
the arena twins respelled `t@<block>.<k>` → `s@<block>.<k>`, §5.9. In ir 7
a `v` name was an ADDRESS CONSTANT; ir 8 reverses that and §5.10.9 says
what the reversal costs. ir 6 (Project 33-B, Sep 6 2026) was the arena
twins, `claim` and `release`. ir 5 (Project 31, Sep 6 2026 — located strings: the
`[@a, n] = piece` / `[@a, n varying] = piece` assignments, `ac1 =
cmp(piece, piece)` and `words(@d, k) = words(@s, k)`, executed on the
P30 string library; docs/Project31/{Census,REPORT}.md, §5.8). ir 4
(Project 28, Sep 5 2026) was the `rt_call` terminator:
the 987 game→runtime LCALL sites with their argument pushes folded into
PL/I-order argument expressions, real stack in both modes; plus LNDO,
the LDSP pair and the Nova LOAD forms; docs/Project28/{Census,
RTConventions,REPORT}.md). ir 3 (Project 26) was the math & control
grammar: `goto [labels] e` terminator, strict booleans, s/u
comparisons, word layer, the effectful op family replacing `#+`/`#-`,
t-places, stack-register reads. This document is self-contained and normative; it
consolidates IRPhase1.md (as amended), Project23/IR2.md and
Project26/MathDesign.md (the P26 design input — rulings of record;
this spec is the law once landed). History and rationale live in
Project23/REPORT.md and Project26/REPORT.md; the P26 census and
per-mnemonic semantics table (emulator source citations) is
Project26/Census.md. When this spec and older documents disagree, this
spec wins.

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
(shared EagleInstruction helpers for the effectful ops; the real
decode/execute path for instructions, calls, and rets).

## 2. File structure

    ir 5
    mode <stock|book>
    source  <path> sha256=<hex>
    blocks  <path> sha256=<hex>
    pushmap <path> sha256=<hex>
    argmap  <path> sha256=<hex>
    strings <path> sha256=<hex>     <- ir 5, present when string statements were emitted
                                       (docs/Project31/p31.tsv, the per-site artifact)

    v <ENTRY>.v<digits> <vtype>   <- ir 7: a DECLARATION (file level, outside any block;
                                       before the first reference in file order). §5.10
    v <ENTRY>.v<digits> char <n> = "<text>"      <- ir 8: an INITIALISED v. §5.10.1b
    a <ENTRY>.a<digits> <vtype>   <- ir 8: an ARGUMENT CELL declaration. §5.10.1c
    a <ENTRY>.arg_count u16       <- ir 8: the supplied-argument count. §5.10.1c
    a <ENTRY>.ret <vtype>         <- ir 8: the return cell of a valued routine. §5.10.1d
    block <hex8> seg <hex8>
    <block lines...>
                                  <- blocks separated by BLANK lines
    block <ENTRY>.b<digits>       <- ir 7: a SYMBOLIC block (no seg: the 0x77 space fixes it). §5.10
    <block lines...>

    blocks <count>                <- trailer (count of block sections, numeric AND symbolic)

- `;` begins a comment anywhere on any line (the grammar has no other
  use of `;`). Comments are audit trail: emitters SHOULD echo source
  disassembly on derived lines.
- Provenance: the loader recomputes sha256 of the file named by
  QUEST_BLOCKS and refuses on mismatch with the `blocks` line; other
  recorded inputs are verified when present on the host. quest.ir is
  therefore bound to the exact CFG the run uses. Since ir 7 the `blocks`
  line is REQUIRED only when the file names a numeric (hex8) block start
  or goto label — a symbolic-only program has no CFG to bind to; a file
  with any numeric block or label still refuses without it (the strict
  surface is unchanged).
- Names (ir 7; ir 8 adds `a`): a file that declares a `v`, an `a` cell or a
  symbolic block needs the addrbook for its entry names —
  QUEST_ADDRESS_BOOK, read by the IR loader for names and file order only
  (§5.10.2); refuse if unset.
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
    <located> = <piece>            STRING STATEMENT (P31, ir 5). §5.8.
    ac1 = cmp(<piece>, <piece>)    STRING STATEMENT: WCMP. §5.8.
    words(@e, k) = words(@e, k)    STRING STATEMENT: WBLM. §5.8.
    call <tgt> args=<n> marker=<hex8> site=<hex8> ret=<hex8>
                                   Decorated call. TERMINATOR. §6.
    call <ENTRY> args=<n> ret=<ENTRY>.b<digits>
                                   ir 8: a NAIVE game->game call from a
                                   symbolic block. No site/marker (there is
                                   no LCALL word to validate against); the
                                   arguments were written into the callee's
                                   `a` cells by preceding statements.
                                   TERMINATOR. §5.10.6, §6.
    rt_call <callee>(<expr>, ...) ret=<ENTRY>.b<digits>
                                   ir 8: a naive runtime call from a
                                   symbolic block. Arguments still go on the
                                   REAL STACK. TERMINATOR. §5.10.6, §6.
    rt_call <callee>(<expr>, ...) site=<hex8>
                                   Runtime call (P28, ir 4). TERMINATOR.
                                   The game→runtime LCALL at `site` (a
                                   `?`-prefixed callee) with its argument
                                   pushes folded into pure argument
                                   EXPRESSIONS in PL/I order: the first
                                   expression is argument 1. Exit =
                                   whatever the LCALL instruction
                                   returns (callee entry / native return
                                   / syscall sentinel), as for a final
                                   instruction. §6.
    ret                            WRTN. TERMINATOR. §6.
    goto [<label>, ...] <expr>     Exit. TERMINATOR (P26). label := <hex8> |
                                   <ENTRY>.b<digits> (ir 7, §5.10; mixed lists
                                   are legal; a symbolic label may be a
                                   FORWARD reference). The expr is
                                   a STRICT index into the label list:
                                   false=0 / true=1 for the two-label
                                   if, k for a table. Every label must
                                   be a listed block start; an index
                                   outside [0, count) is a loud
                                   executor FAULT — no coercion, no
                                   clamping, no default arm. The
                                   canonical unconditional exit is
                                   `goto [L] 0` (fall-through, WBR,
                                   direct XJMP); a single-label goto
                                   with any other index REFUSES at
                                   load.
    goto <label>                   Parser SUGAR for `goto [<label>] 0`
                                   (accepted, never emitted — the dump
                                   form is the list form).
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

One machine instruction MAY lower to several statements (WPSH group
stores, WXCH, XNDO, Nova tests): no bookkeeping is required or
possible — statements are sequence, not identities.

## 4. Block rules (loader-enforced; violations REFUSE at load)

- `block <pc> seg <s>`: pc must be a listed quest.blocks start;
  s == pc & 0xF0000000; no duplicates; excluded blocks (7015BD6B)
  refused. `block <ENTRY>.b<k>` (ir 7): no `seg` (refuse one); the name
  is placed at its 0x77 address (§5.10.3); no duplicates (by name).
- Symbolic blocks may contain statements, string statements, `assert`,
  `goto`, `ret` and (ir 8) `call`/`rt_call` with a SYMBOLIC return label
  (§5.10.6). An `@addr` instruction inside one still REFUSES: an
  instruction is fetched from real memory at its own address and a
  symbolic block has none. ir 7 refused calls too; §5.10.6 says why that
  was a scope boundary and what lifting it required.
- Instruction addresses within a block strictly increase.
- TERMINATOR RULE: the last line of every block is an instruction,
  `call`, `rt_call`, `ret`, or `goto`. (A final instruction's control transfer —
  branch, skip, return, call, fault — IS the exit.) A lowered skip is
  a `goto [fall, skip] test`; nothing follows a terminator.
- t-places (P26) are BLOCK-LOCAL, SINGLE-ASSIGNMENT: the loader refuses
  a read before the write and a second write in the same block
  (straight-line blocks make definite assignment exact).
- Anything unrecognized refuses. No silent skips, ever — including in
  the emitter's own input parsers.
- SYNC LIST (P27, ir 3 note — no grammar change): the loader validates
  NUMERIC block starts AND numeric goto labels against the SHIPPED sync list
  (QUEST_SYNC_LIST), not against quest.blocks. A translation that
  removes blocks ships a list without them (BlockSyncDesign.md rules
  1–2), and any IR line naming a delisted pc refuses at load. Symbolic
  labels (ir 7) are not listed and not counted (§5.10.7).

### 4a. DERR clusters (Project 27, Sep 5 2026 — docs/Project27/Census.md)

A compiler-generated bounds check is a skip chain with exactly two
exits — a `DERR nn` sink (TERMINAL: DERR.TRP is an ABORT-kind terminal,
nothing downstream of it is ever observed) and one continuation K.
`tools/lower.py --assumed-foldable docs/Project27/assumed-foldable.txt
--tags …` folds each listed cluster INTO ITS GUARD BLOCK (user ruling
F1 = A): the guard skip becomes

    assert(<path condition to K>, "DERR nn @<derr pc>")
    goto [K] 0

where the condition is a transcription of the skips (cond(K)=true,
cond(DERR)=false, `t ? cond(skip) : cond(fall)` rendered `(t) && …` /
`!(t) && …`, no other algebra) re-derived through lower_one and
cross-checked against the artifact text. The cluster's INTERIOR blocks
(second skip, DERR) are not emitted and are delisted from the shipped
list `emulation/quest.synclist.p27` (identity minus interiors of the
clusters actually folded; lower.py writes it with tags/blocks/artifact
sha256 provenance). K stays a listed block — the clone ticks its
ordinal on ARRIVAL at a listed pc (Machine.cpp:306), so a merged K
would skew ordinals. Totality: a guard block that refuses keeps its
interiors emitted AND listed — never a half fold.

Checker consequence (ruling F2-a, honest statement): a folded DERR is
no longer a VERIFIED terminal pair. The clone's assert fires inside
the IR block → `Lockstep::assert_detach` (clone halts); the master
executes the real DERR → DERR.TRP → O.SERROR → DEF?ON → ?FATAL and dies
its own way (the compare_pair detached early-out means no
`TERMINAL-ABORT … verified on both engines` line). The `derr` battery
leg therefore matches TWO lines: the clone's `IR ASSERT FAILED …
"DERR nn @pc"` at the predicted pc AND a non-clean master end
(DERR.TRP on its backtrace, START_TURN never reached). Follow-up F2-b
(assert-detach paired with a kind-2 terminal → TERMINAL-ABORT) is
recorded in Project27/REPORT.md, not landed.

Test knob: `QUEST_POKE=<hexpc>:<ac>:<value>` (RTStubs.hpp) — one-shot,
both roles, on arrival at pc; the derr leg's mechanism. Harness, not
checker; zero effect unset.

## 5. Statements and expressions

### 5.1 Grammar

    lvalue  := ac0..ac3 | tN | c | ovr | M8[e] | M16[e] | M32[e]
             | <cell>                            (ir 8: THE VARIABLE FORM — a write of the
                                                  named cell's CONTENTS, at the declared
                                                  width. §5.10.4)
               (wfp wsp wsb wsl are grammatically registers but WRITES are
               RESERVED — refused. The RT-call decoration project (P28)
               chose NOT to spell stack writes out: `rt_call` moves wsp
               only inside Machine::wide_push, §6; P26/P28 emit
               stack-register READS only)
    stmt    := lvalue = expr                         pure
             | lvalue = effop(args)                  effectful, ROOT ONLY (§5.5)
             | assert(e[, "msg"])
    expr    := one flat left-associative chain of ONE operator class
               over primaries — emitters parenthesize everything else;
               the loader REFUSES a chain that mixes classes or has
               more than one comparison (no precedence table exists)
    classes := word   + - * & | ^ /s /u %s %u
               cmp    == != <s <=s >s >=s <u <=u >u >=u   (exactly one)
               bool   && ||
    prefix  := ~ e (32-bit complement, word)   |   ! e (boolean NOT, 0/1 operand)
    cell    := <ENTRY>.v<digits> | <ENTRY>.a<digits> | <ENTRY>.arg_count |
               <ENTRY>.ret       (ir 8: a declared v or argument cell — §5.10)
    primary := acN | tN (N = 1..255) | c | ovr | wfp | wsp | wsb | wsl |
               <cell>             (ir 8: the cell's CONTENTS, read at the DECLARED
                                   width and signedness — §5.10.4. In ir 7 this
                                   spelling was the cell's ADDRESS; §5.10.9) |
               s@<block>.<k>      (a twin's word address, a constant — §5.9) |
               constant (0x… or signed decimal) | byte-pointer literal
               0xW:b (b in {0,1}) | M8[e] | M16[e] | M32[e] | R[e] |
               ind(e) | wp(e, e) | bp(e, e) | lsh(e, amount) | tf(e) |
               sx16(e) | zx16(e) | zx8(e) | trunc16(e) | trunc8(e) | ( e )
    effop   := add(a, b) | sub(a, b) | mul(a, b) | div(a, b) | cvwn(a) |
               ash(a, amount) | nadd(a, b) | nsub(a, b) | nmul(a, b)

    REFUSED AT LOAD (rt_call, P28): a callee that is not a `?` symbol;
    a `site=` whose word is not an LCALL or whose argc field (site+3,
    & 0x7FFF) != the number of argument expressions (checked against
    memory at execution, loud); a callee symbol that does not resolve to
    the LCALL's target; site+4 not a listed block start; rt_call
    anywhere but the last line of a block; an argument that is not a
    pure expr (effectful ops are not exprs; t-places ARE allowed — a
    pure read, single-assignment checked as everywhere).

    REFUSED AT LOAD: bare `< <= > >=`; bare `/ %`; the whole `#` family
    (`#+ #- #* #/`, retired in ir 3); C `<<`/`>>` at any tier (ruling
    R9: ash/lsh are the only shift vocabulary); functional
    and()/or()/xor()/com(); an effectful op anywhere but statement
    root; mixed-class or chained-comparison chains; t read-before-write
    or double write; stack-register writes; `goto [L] k` with k != 0;
    M1 (reserved); a hex constant in [0x76000000, 0x78000000) anywhere
    (ir 7: those addresses are loader-assigned, never authorable — only
    a `v`/`b` NAME denotes one). Anything else unrecognized: refuse.

    REFUSED AT LOAD (ir 8, §5.10): `M8[<word pointer>]`;
    `M16[<byte pointer>]`; `M32[<byte pointer>]`; `M<n>[<cell that is not
    a pointer>]`; a bare AGGREGATE cell name (`char`/`varying`/`words`)
    as a value or an lvalue — it names a region, not a value, and is
    legal only as the first operand of `wp`/`bp` and as an address in
    the §5.8 string forms; `**` or a `*` applied to anything but the
    seven pointee forms of §5.10.1; a cell reference with no
    declaration, and a declaration after its first reference; an `a`
    numbering with a hole; a naive `call` whose declared arity disagrees
    with the callee's `a` cells, or whose calling block writes a pointer
    `a` cell with a value of manifestly the WRONG KIND (§6 — the
    argument-kind check, P54: as built, it sees `wp()`, `bp()` and pointer
    cells, and only in the calling block); a top-bit-set literal in an
    ADDRESS position (§5.10.8).

    EXECUTOR FAULTS (loud, never a silent value): goto index outside
    [0, count); zero divisor in `/s /u %s %u`; INT_MIN `/s`/`%s` -1;
    a non-0/1 operand to `&& || !`; a non-0/1 value assigned to c/ovr;
    plus every fault the underlying Machine/Memory helper throws.

All host arithmetic is 32-bit unsigned with wrap unless a rule below
says otherwise; signedness is never implicit — it is in the operator
(`<s`, `/u`), the extension (`sx16`), or the helper.

### 5.2 Memory, pointers, segments

- M16 reads return the raw 16-bit cell zero-extended; extension is
  ALWAYS explicit in the text (`sx16(M16[…])` etc.). M16 stores write
  value & 0xFFFF; emitters write `trunc16(…)` for the audit trail on
  pure stores — but NOT around an effectful op (`M16[e] = nadd(M16[e],
  k)`; ruling R6, §5.5): the store's truncation is the rule, the
  wrapper is only audit trail.
- M8 (P25, byte addressing): reads return the byte zero-extended
  (Memory::read_byte); stores write value & 0xFF (write_byte); emitters
  write `zx8(…)` for the audit trail. The M8 INDEX IS RAW — no segment
  wrap. Byte pointers carry their own segment in bits 31:29
  (set_byte_segment packing) and the hardware applies no masking at the
  point of use (WLDB/WSTB deref ac[II] unmasked; see
  Project25/ByteEA.md). A garbage byte pointer faults in read_byte
  exactly as the emulated instruction would — loud and identical, per
  METHOD §8.
- SEGMENT WRAP (executor rule): every M16/M32/R INDEX is evaluated as
  (e & 0x0FFFFFFF) | seg, seg from the block header. The emitter
  refuses any absolute or pc-folded EA outside the block's segment,
  which makes the uniform wrap provably identity-or-hardware-exact
  (it is Machine::copy_segment). The wrap applies ONLY to memory/
  resolve indices — a computed address stored as a VALUE (an arg-slot
  EA, a byte pointer, an LEF result) must carry any needed masking
  explicitly in its expression (wp/bp).
- wp(b, d) / bp(b, d) (P25 pointer builders — masking lives in the
  executor, never in emitted text; user ruling, Aug 29): wp is the word
  segment wrap of b+d — ((b+d) & 0x0FFFFFFF) | seg. bp is
  Machine::set_byte_segment(seg, b*2 + d) — the base is a word address
  scaled to bytes, the displacement is already in bytes (that asymmetry
  is the hardware's; eagle_x_byte_indexed ii=2/3). L-form byte EAs
  apply NO masking and therefore never render as bp: they emit raw
  arithmetic (acN*2 + disp) or constants (ByteEA.md has the per-mode
  table read out of the emulator source). wp/bp are the REGISTER-
  RELATIVE forms only — no wp(0,d)/bp(0,d) is ever emitted.
- Byte-pointer literal `0xW:b` (P25, user ruling): value = W*2 + b.
  Pure notation for a 32-bit byte pointer in the disassembler's fold
  form — W is the WORD address (what memory dumps use), b the byte
  select. `:b` means BYTE SELECT exclusively and permanently: b is 0
  or 1, anything else refuses. Emitters use it for every constant
  byte EA and for L-form byte-table bases (`acN*2 + 0xW:b`), so the
  IR text is greppable against word-addressed dumps and matches the
  dis rendering. Word-pointer constants stay plain hex. Bit-pointer
  literals (M1, future) must NOT overload `:` — see §8.
- **wp/bp RESOLVE BY OPERAND KIND (ir 8).** A register or any other
  expression as the base means ITS VALUE plus the displacement — the rule
  above, unchanged. A `<cell>` name as the base means THE ADDRESS OF THAT
  CELL plus the displacement. The parser knows which it just consumed, so
  the choice is STATIC and the executor sees two different nodes and does
  no kind test at run time. This is a deliberate overload and it is the
  whole reason ir 8 needs no `&` operator: `wp`/`bp` already carry the
  machine's word/byte pointer distinction, so `wp(PXY.v3, 0)` is the word
  address of `v3` and `bp(PXY.v3, 0)` its byte pointer. Note the
  composition, which reads oddly once and is right: if `v3` is `*i32`,
  then `PXY.v3` is the pointer VALUE, `M32[PXY.v3]` is the pointee, and
  `wp(PXY.v3, 0)` is the address of the pointer cell itself.
- R[e]: hardware indirect resolution of an EA operand — deref the
  wrapped index, then follow bit 31 until clear (executor:
  eagle_resolve_indirect(wrap(e) | 0x80000000), inheriting the depth
  limit and its throw). An R result used directly as a memory index is
  NOT re-wrapped (chain pointers are full addresses). Emitters produce
  R only where the instruction's indirect bit is set.
- ind(e) (P26, ruling R4): hardware indirect resolution of a VALUE —
  Machine::eagle_resolve_indirect(e): while bit 31 of e is set, e =
  M32[e & 0x7FFFFFFF]; the result is e with bit 31 clear. SAME helper
  as R, different entry: R[e] forces one dereference of the wrapped
  index first, so R[e] ≡ ind(the word the hardware reads at wrap(e)).
  `ind` is what WBTZ/WBTO/WSZB apply to ac[XX] (EagleCompute.cpp
  :261/:272/:283); `R` is what an indirect EA operand applies. An
  `ind()` result standing alone as an index is a full address; used
  inside a larger index expression (`M16[ind(acX) + lsh(acY, -4)]`) the
  sum is wrapped by the uniform index rule — which is exactly the
  instruction's copy_segment(address, resolved + …).

### 5.3 Pure operators, booleans, the word layer

- `+ - *` are host 32-bit arithmetic, wrap, no flags. `*` is distinct
  from the effectful `mul()`, which owns the ISA's ovr semantics.
- `/s %s` are truncating signed 32-bit divide and remainder, `/u %u`
  unsigned; both FAULT on a zero divisor and `/s %s` fault on INT_MIN
  ÷ −1 (host UB otherwise). No flags. (No emission site in the P26
  census uses them — WDIV is the effectful `div`; they exist so the
  s/u convention is complete.)
- Booleans (MathDesign §2): comparisons yield exactly 0/1; ordering
  comparisons carry a MANDATORY s/u suffix (`<s` compares as int32,
  `<u` as uint32); `==`/`!=` compare the 32-bit patterns. `&& ||` are
  EAGER (both operands evaluated; exprs are pure, so order is
  unobservable) and require 0/1 operands (fault otherwise); `!` is
  boolean-only. `tf(e)` is the word→boolean normalizer: 0 if e == 0
  else 1. A `goto` index expression is an ordinary expr — for the
  two-label form emitters use a comparison or `tf()` so it is 0/1 by
  construction.
- Word layer (MathDesign §3): `& | ^` infix, `~` prefix — 32-bit, no
  0/1 constraint. `lsh(x, amount)` is EagleInstruction::logical_shift
  (writes no flag): signed amount, positive = left, negative = right,
  |amount| >= 32 → 0, amount 0 = passthrough. There is no pure
  arithmetic-shift primary: `ash` is effectful (its ISA form
  accumulates ovr) and therefore statement-root only (§5.5).
- Extensions: `sx16` sign-extends bits 15:0; `zx16`/`trunc16` are both
  `& 0xFFFF` (the names record intent: load vs store); `zx8` is
  `& 0xFF`.

### 5.4 Registers, flags, t-places

- ac0..ac3: the interpreter's register locals (§7).
- `c` and `ovr` READ the machine flags (machine.c / machine.ovr) and are
  assignable at statement root with a 0/1 value (`c = 1` is CRYTO).
  They are the SAME cells the effectful helpers write, so a pure
  expression may consume a flag an earlier effectful statement set.
- `wfp wsp wsb wsl` read machine.wfp/wsp/wsb/wsl — the clone's own
  registers, the same values LDAFP/LDASP read (EagleStack.cpp:527/
  :512). Writes are reserved (refused): the RT-call decoration project
  (P28) kept it so — `rt_call` is the one construct that moves wsp and
  does it inside `Machine::wide_push`, which owns the wsp>wsl overflow
  fault (§6).
- t-places tN: 32-bit block-local scratch, SINGLE-ASSIGNMENT (loader:
  refuse a read before the write, refuse a second write in the block;
  straight-line blocks make definite assignment exact). A t may be the
  destination of an effectful op. Emitted where the instruction
  itself needs scratch: the 23 P20 borrow brackets (`t = acX` at the
  WPSH, `acX = t` at the WPOP — the bracket's memory write is dropped:
  in book mode the borrow slot is read only by its own WPOP; in stock
  mode the real-stack word below wsp is no longer written, wsp is
  restored inside the block, so every rendezvous agrees and only
  dead-stack residue differs — Project26/REPORT.md §3), WXCH,
  XNDO/XWDO (new value / test), and the Nova test decomposition (the
  17-bit ALU value).

### 5.5 Effectful operations (statement root only)

Each is DEFINED as the shared EagleInstruction helper it names, with
f(a, b) == helper(machine, src=b, dst=a) — so `sub(a, b)` is a − b and
the first argument is always the destination-like operand:

    add(a,b)  = EagleInstruction::add           c = ALU carry-out; ovr |=
    sub(a,b)  = EagleInstruction::sub           a − b; c = complement-add carry; ovr |=
    mul(a,b)  = EagleInstruction::mul           ovr |= 1 if the int64 product does not fit int32; no c
    div(a,b)  = EagleInstruction::div           a ÷ b truncating; divisor 0 or INT_MIN ÷ −1 → ovr = 1
                                                and the RESULT IS a UNCHANGED (hoisted from WDIV, P26)
    cvwn(a)   = EagleInstruction::cvwn          sx16(a & 0xFFFF); ovr |= 1 if a did not fit int16
                                                (hoisted from CVWN, P26)
    ash(a,n)  = EagleInstruction::arithmetic_shift  ISA amount semantics (as lsh); ovr |= sign change
    nadd(a,b) = EagleInstruction::narrow_add    16-bit add; result SIGN-EXTENDED to 32; c/ovr
    nsub(a,b) = EagleInstruction::narrow_sub    16-bit a − b; result sign-extended; c/ovr
    nmul(a,b) = EagleInstruction::narrow_mul    16-bit; result ZERO-extended (& 0xFFFF); ovr = 1 on overflow

Rules: exactly one effectful op per statement, at the root; its
arguments are pure exprs, evaluated first (they may read c/ovr), then
the helper runs, then the result is assigned. NO FORMULAS live in the
IR or in IRExec: when a helper changes, the IR's meaning changes with
it (WideCarry.md ruling). Every effectful statement ends with the
emulator's `ovk && ovr` check, identical throw string, attributed to
the BLOCK (statements have no pc). An effectful op storing to M16 is
NOT wrapped in trunc16 (ruling R6). The result conventions listed
(sign-extend vs zero-extend, `|=` vs `=`) are the emulator's, recorded
not judged — see Project26/Census.md §2d and REPORT §3 for the open
manual question.

### 5.8 Located strings (Project 31, ir 5 — docs/Project29/StringsDesign.md §2–§4, docs/Project31/Census.md)

    located := [@<addr>, <n>]              ; fixed CHAR(n): n bytes at the BYTE address <addr>
             | [@<addr>, <n> varying]      ; CHAR(n) VARYING: length word at the WORD address
                                           ;   <addr>, data at <addr>+1; capacity n
             | [@<addr>, varying]          ; a varying READ whose declared capacity the site
                                           ;   does not reveal (length from the length word);
                                           ;   sources only — never an lvalue
    piece   := [@0xW:b, "<text>"]          ; a literal: located, contents known (quest.strings);
                                           ;   0xW:b its byte address in the image; length =
                                           ;   the byte count of <text>
             | located
    <addr>  := any pure expr.  For a FIXED string a byte-pointer VALUE (bp(...), 0xW:b, a
               register holding one, or such a value + a byte count) used RAW, as M8 is.
               For a VARYING string a word address, wrapped into the block's segment
               exactly like an M16 index.  A register is a legal address ([@ac2, 13]:
               the setup was kept rather than folded — P31 ruling O4).
    <n>     := a constant 0..32767         ; P31
             | any pure expr (§5.1)         ; P32 (docs/Project32/Census.md §3): the count the
                                           ;   instruction uses — a register holding the master's own
                                           ;   count (ac0, ac1), a length-word read sx16(M16[a]), a
                                           ;   wide read M32[a] (the compiler's precomputed totals),
                                           ;   a sum; no 32 K check on a runtime value (F-B1)

    stmt   += <located> = <piece>          ; PL/I assignment
            | ac1 = cmp(<piece>, <piece>)  ; WCMP: cmp(string 1, string 2) = -1/0/+1 in ac1
            | words(@<addr>, <k>) = words(@<addr>, <k>)   ; WBLM: k words, sequential
                                           ;   ascending; <k> a constant or a pure expr

Literal escaping: printable 0x20..0x7E except `"` `\` `;` are literal;
everything else is `\xHH` (two upper-case hex digits); `;` MUST be
escaped (comments strip first, §2). The loader unescapes.

SEMANTICS — the statement IS the instruction sequence it replaces (the
WCMV / WCMP / WBLM plus the contiguous run of pure operand producers
before it: NLDAI/WLDAI/WMOV/XNLDA/XWLDA/LNLDA/LWLDA/XLEF/LLEF/XLEFB/
LLEFB/XLDB/LLDB/WLDB/ZEX/SEX and, for a varying destination, the
compiler's XNSTA length-word store), and the executor calls the P30
library (hw/strings/EagleString.hpp), which mirrors EagleSpecial's arms
byte for byte and writes the residues (StringsDesign §3) itself:

- `[@a, n] = piece`: `assign_fixed` — exactly n bytes written: the first
  min(n, len) from the piece, the rest 0x20 (source exhausted); the
  source truncated when longer.  Residues: ac0 = 0, ac1 = len − min(n,
  len), ac2 = a + n, ac3 = src + min(n, len), c = (len > n); ovr
  untouched (B-1).
- `[@a, n varying] = piece`: `assign_varying` — the piece is evaluated
  FIRST (its length word read), then the length word at `a` := min(len,
  n), then a copy of min(len, n) bytes to `a`+1 (never pads: the
  transferred count never exceeds the source).  Residues as above with
  the transferred count.  The compiler's min shape (`WSGE/WSLE + WMOV`
  diamond) stays in the CFG; the statement in the join computes the min
  itself and the residues make ac0..ac3 agree (P31 Census §5).
- `ac1 = cmp(s1, s2)`: `compare` — string 1 = the master's ac3/ac1, string
  2 = ac2/ac0; both read until both counts are exhausted, the shorter
  padded with blanks, stop at the first unequal pair; ac1 = −1/0/+1; ac0
  = string-2 count at the stop; ac2/ac3 = pointers at the stop, ONE PAST
  the failing byte on a mismatch (B-4 reproduced); c untouched.
- `words(@d, k) = words(@s, k)`: `block_move` — k words one at a time,
  addresses stepped after each store, which is what makes the 12
  self-overlapping fills of Quest (src = dst − 1 or − 2) well-defined;
  ac1 = 0, ac2 = s + k, ac3 = d + k; ac0, c untouched.
- A varying's length word is read as XNLDA reads it — SIGN-EXTENDED
  (EagleGeneral.cpp:51–52): 0xFFFF is the count −1, a one-byte DESCENDING
  string, and the master runs it (DISPLAY_INVENTORY 7016816B on the login
  path).  No length-range fault (P31 finding F-B1).

P32 (append chains; still ir 5 — no production added, `<n>` widened):
`[@d, n] = [@s, m]` IS `WCMV(dst_count = n, src_count = m, dst = d,
src = s)` through the library's `copy`; `n` and `m` may now be
expressions, and either operand may be the register itself
(`[@ac2, ac0] = [@ac3, ac1]` is the literal instruction, exact by
construction).  A continuation piece of a scratch chain is written at
the append cursor, `[@ac2, …] = piece` (StringsDesign §5.1's `append`,
register spelling).  A varying destination (`[@a, n varying]`, XNSTA
absorbed) is emitted only where `dst_count == src_count`, so the
library's `min(len, n)` is the master's stored length; otherwise the
XNSTA stays a statement and the copy is the fixed form at the data
address.  `len()`, `append()`, `substr()`, `min()` are NOT IR: `len` is
the count expression, `append` the `@ac2` spelling, SUBSTR a byte-offset
address (`(bp(ac3, 40) + ac1)`), a min the register the diamond leaves —
all readable-layer rewrites (P34).  Negative `bp()` displacements are
written `-0x…`.  Tail splits are bounded concatenation: `src_count`
carries the remaining room (`[@ac2, ac0] = [@ac3, ac1]` with ac1 = room).
The count expressions are evaluated in the statement's own context
(block-entry registers / memory at the statement) before the library
call; `check_treads` covers them.

LOADER REFUSES: an `ir 4` file; a constant `n` ≥ 32768; a
literal without a `0xW:b` constant address, outside the block's segment,
≥ 32 K, with an escape other than `\xHH`, or containing a raw `;`;
`[@a, varying]` as an lvalue; a varying destination without a capacity;
a `cmp` whose lvalue is not `ac1`; `words()` whose two counts differ
textually; t-place reads before write anywhere in the operands.

ir 8 NOTE — the `[@0xW:b, "text"]` LITERAL PIECE IS UNCHANGED, and
deliberately so. Its address is a byte address IN THE IMAGE and its bytes
are verified against memory; both presume the 1986 compiler put the bytes
there, which is true of every site in the book and false of every literal a
compiler of ours emits. A compiled literal is therefore NOT a literal piece:
it is an INITIALISED `v` (§5.10.1b) read as an ordinary fixed piece,
`[@bp(<name>, 0), n]`. Image literals keep their verification at full
strength; nothing here is relaxed to accommodate compiled ones.

EXECUTOR FAULTS (loud): a literal whose bytes differ from the image —
checked LAZILY, at the FIRST execution of each literal-bearing statement,
through `Memory::read_byte` (the normal path; a pre-scan would change
demand-page timing — user ruling): `IR literal mismatch [block, stmt] at
byte j of 0xW:b`; a negative `words()` count; every fault the library
throws (segment crossing per byte, G-3; WBLM indirect bit, G-2), with
`[IR block, stmt]` appended.

Execution model: registers are materialised to machine.ac before the
library call and re-read after (the library writes ac0–ac3 and c
directly, like an instruction's arm); statements do not advance
instruction_count and do not fire the Capture hook (as every statement);
the first execution of each string statement logs
`IRExec: first execution of string statement <kind> in block <pc> stmt <i>`
(coverage evidence, the battery's verdict lines).

Emitter (tools/lower.py `--strings-sites docs/Project31/p31.tsv
[--strings-sites32 docs/Project32/p32.tsv] --strings-slice {0..6}
--strings-census`): the per-site artifact is
produced by tools/string_sites.py `--p31` (the census tool's symbolic
evaluator renders the four operands and decides EMIT/REFUSE); lower.py
CONSUMES it — provenance (dis/blocks/mem sha256) checked against its own
inputs, every fold pc re-validated (in the site's block, before the site,
a pure producer or the absorbed XNSTA, nothing but LDAFP between the
first folded pc and the site) — and echoes the folded instructions after
`<-` in the statement's comment.  Slice 1 = the literal assignments,
2 = + the other located assignments, 3 = + cmp and words — 649 sites
(P32's correction, Sep 6: 652 → 649; docs/Project31/Census.md §11), 127
refused with the reason in the artifact.  Slices 4–6 (P32, p32.tsv,
which carries each row's slice and form): 4 = first pieces +
continuations (677), 5 = + copy-outs / bounded copies / SUBSTR pieces
(134), 6 = + CALLRESULT counts, residue counts (tail splits) and the 9
WCMPs P31 refused (133) — 944 sites, 0 refused; 1,593 string statements
in all, embeds 729 (book) / 2,608 (stock).  Slice 3 reproduces the P31
artifacts byte for byte; slice 0 the ir 4 artifacts except the version
line.  The `strings32` provenance line names p32.tsv.

### 5.9 Arena twins, claim, release (Project 33-B, ir 6; respelled `s@` in ir 7 — docs/Project29/StringsDesign.md §5.2/§6, docs/Project33/REPORT-B.md)

The master keeps a concatenation group's temporaries on its stack (WMSP
claims); the clone keeps each temp as a TWIN at a fixed arena address
(quest.arena, `QUEST_ARENA=<file>`; the segment [0x75000000, 0x75800000)):

    twin    := s@<block>.<k>               ; WORD address of the k-th claim's twin of the group
                                           ;   whose claims live in block <block> (hex8, k >= 1);
                                           ;   an ordinary constant in expressions: bp(s@b.k, o)
                                           ;   is its byte pointer, wp(s@b.k, o) a word inside it
    stmt    := acN = s@<block>.<k>         ; the master's `LDASP r; WADI 2,r` (the temp's base):
                                           ;   every slot store, reload, XLEFB, length-word store,
                                           ;   WSTB and XPEF push downstream then hits the arena
             | claim s@<block>.<k>, acN    ; the master's WMSP: the clone's wsp does NOT move;
                                           ;   FAULT if 4*acN (bytes) exceeds the twin's capacity
             | release s@<block>, acN      ; the master's `STASP N` (the group's end): asserts
                                           ;   acN == s@<block>.1 - 2 (the slot arithmetic agreed),
                                           ;   then acN := wsp (the master's residue, its restored wsp)

The 96 WCMVs of the 19 groups are ordinary §5.8 statements over twin
addresses (P32's forms; `[@bp(s@b.k, 0), n] = piece`, `[@s@b.k, n varying]
= …`). Loader rules: `s@` names resolve through quest.arena (REFUSE if
unset or unknown; the file's `arena <path> sha256=` line must match
QUEST_ARENA); `claim`/`release` need a register; `release` names the block
only. The checker (P33-A, `QUEST_STRINGS_CHECK=1`) binds one Mapper row per
twin at the master's WMSP hook and models the master's claims as stack
INSERTIONS the clone lacks (Mapper.md §1.4, P33-B corrections). Readable
layer: `s@b ≡ s@b.last`, the intermediate twins folded (ground is `s@b.k`).
SPELLING (P46, ir 7): the twins were `t@<block>.<k>` in ir 6; the `t@`
prefix collided in spelling with the t-places `t1…t255` (§5.4), which the
twins have nothing to do with, so ir 7 spells them `s@` (S for string). Pure
token rename — 236 tokens on 198 statements in each artifact, proven
token-only by regeneration (docs/Project46/evidence/stageA_rename.txt); the
loader refuses `ir 6`.

### 5.10 Declared storage, the variable form, symbolic blocks, the 0x76/0x77 spaces (Project 46 ir 7; Project 52 ir 8 — docs/Project44/DESIGN.md §4–§5, docs/Project46/{q001,a001}-plan-gate.md, docs/Project52/{q001,a001}-plan-gate.md)

The point of ir 7: a NAIVE program — a compiler's first output, with no
allocation model — is loadable and runnable. Its storage is declared
without an address and its blocks are named without an address; the loader
places both, in private synthetic spaces where aliasing is impossible.

The point of ir 8: that program is also READABLE, and expressive enough that
simple C compiles to it. A `v` looks like the variable it is (§5.10.4), a
pointer is a declared type rather than a spelling convention (§5.10.1a), a
routine's signature is declared rather than inferred from its call sites
(§5.10.1c), and a literal the image does not contain can be written down
(§5.10.1b). The cost is stated in §5.10.9 and accepted: access width and
signedness now come from the declaration rather than from the statement.

#### 5.10.1 Declarations

    decl   := v <ENTRY>.v<digits> <vtype>            ; file level, outside any block
    vtype  := i16 | u16 | i32 | u32                   ; 1, 1, 2, 2 words
            | char <n>                                ; fixed CHAR(n): ceil(n/2) words (§5.8 [@a, n])
            | varying <n>                             ; CHAR(n) VARYING: 1 + ceil(n/2) words
                                                      ;   (length word at the v's address, data at +1)
            | words <n>                               ; an aggregate the source addresses by offset
                                                      ;   (array, record, bit string): n words, uninterpreted
            | <ptype>                                 ; ir 8: a pointer — §5.10.1a
    <n>    := a constant 1..32767

A type carries SIZE to the allocator, and since ir 8 it also carries WIDTH,
SIGNEDNESS and POINTER KIND to the variable form and the tripwire
(§5.10.4/.5). `i16` and `u16` are still the same one-word cell, but they are
no longer interchangeable in the text: the declaration is what says whether
a read sign-extends. Every `v` is FIXED SIZE, because a placed `v` is
eventually laid exactly on top of the original's frame slot at 0x74 (DESIGN
§4.1); an arbitrary-length string temporary is a twin `s@b.k` (§5.9), not a
`v`, and assigning a twin to a `v` truncates or pads through the §5.8 forms.

Rules (loader; violations REFUSE): declared before its first reference, in
file order (single pass, as t-places); duplicate declaration; a reference
with no declaration; a `v` or `a` line inside a block; `<n>` outside
1..32767. Cross-entry references are LEGAL (`QUEST.1.b0` reading `QUEST.v3`
is a nested procedure reading its parent's local; the game is non-reentrant,
so every routine's storage exists in exactly one copy — DESIGN §4.1). In the
naive form that is how UPLEVEL access is spelled, and it is free: DESIGN
§4.1a, and §5.10.8's census of the 41 sites where the book pays for it.

#### 5.10.1a Pointer types (ir 8)

    ptype  := *i16 | *u16 | *i32 | *u32               ; WORD pointers
            | *char                                   ; BYTE pointer
            | *varying <n> | *words <n>               ; WORD pointers to an aggregate

**Two words in every case** — the machine has no narrow pointer. **ONE LEVEL
ONLY**: `**` refuses, and so does `*` on anything but the seven pointee
forms above. The book's apparent double indirection is an artefact of
addressing — it indexes by the ADDRESS OF a cell where ir 8 NAMES the cell —
so our types are one star shallower than the listing suggests. `?READ`'s
argument 2, a dummy holding a byte pointer, is `*char`, not a
pointer-to-pointer.

A pointer type carries two properties, and they are not equally binding:

- **KIND (word vs byte) is ENFORCED.** `M8[<word pointer>]` refuses;
  `M16`/`M32[<byte pointer>]` refuses. The kind also selects the instruction
  family at codegen: `XPEF` vs `XPEFB`, `XNLDA`/`XWLDA` vs
  `XLDB`/`WLDB`/`WSTB`.
- **POINTEE WIDTH is ADVISORY.** `M16` and `M32` on a word pointer are BOTH
  allowed — a wide is two consecutive words and `XNLDA`/`XWLDA` both take
  word addresses. The width is carried for the compiler and for `ircmp`, and
  is never checked.

`*varying <n>` and `*words <n>` exist because the program is full of them:
28 of the 55 non-argument `R[]` sites in `quest.ir2.book` are a local
holding the WORD address of a CHAR VARYING, through which the routine writes
the length word before passing the same pointer as a text argument
(§5.10.8). Without them the §5.8 tripwire refuses the only workaround, since
`check_piece` requires a varying piece's address to be a `varying` cell.

#### 5.10.1b Initialised `v` (ir 8)

    decl   += v <ENTRY>.v<digits> char <n> = "<text>"

The bytes are written by the LOADER at placement, escaped exactly as a §5.8
literal is (printable 0x20..0x7E except `"` `\` `;` literal, everything else
`\xHH`, `;` mandatory). `<n>` is the declared capacity; the text must be at
most `<n>` bytes and is blank-padded to `<n>`, as `assign_fixed` pads. Only
`char <n>` may be initialised; a varying's length word is written by the
program, not by the declaration.

This exists because **a compiled routine's string literal is nowhere.**
§5.8's literal piece names a byte address IN THE IMAGE and the executor
faults if the bytes disagree with memory — rules that are correct for the
book and inapplicable to anything we generate. An initialised `v` is read as
an ordinary fixed piece at `bp(<name>, 0)`, so no §5.8 rule is weakened for
image literals (§5.8, ir 8 NOTE).

Consequence, stated plainly so nobody meets it as a contradiction: **0x76 is
no longer uninitialised.** §5.10.7's "a `v` read before its first write reads
zero" holds for every `v` except an initialised one, whose bytes are present
from load.

#### 5.10.1c Argument cells and `arg_count` (ir 8)

    decl   += a <ENTRY>.a<digits> <vtype>             ; N >= 1
            | a <ENTRY>.arg_count u16

`a` cells are cells like `v`s — same type system, same placement, same
tripwire, same variable form. What they add is that a routine's SIGNATURE is
DECLARED, so the loader can check a call's arity and its argument pointer
KINDS against the callee rather than against a pushmap entry that a compiled
routine does not have.

Rules (loader; violations REFUSE): the `a` numbers must be CONTIGUOUS FROM 1
— a hole means the signature is wrong, which is precisely what these
declarations exist to catch; otherwise the `v` rules apply unchanged.

`arg_count` is **u16**: it is the LOW WORD of the LCALL marker
`(psr<<16)|argc` (the addrbook's own layout table). The `psr` half is machine
state no source construct touches; restoring it is a rewrite's business at
L2. NOTE for `ircmp`: the book READS that word sign-extended — REFRESH_SCREEN
block 70176A93, `ac0 = sx16(M16[wp(ac3, -9)])` — and a `u16` cell reads
zero-extended. `sx16 ≡ zx16` for every argc ≤ 0x7FFF, so nothing behaves
differently; the difference is textual and is recorded here rather than
rediscovered at L2.

**One optional-argument mechanism exists in this program and `arg_count` is
it.** Salvage F5 records two spellings; §5.10.8 shows the second is not an
optional-argument mechanism at all.

#### 5.10.1d The return cell (ir 8)

    decl   += a <ENTRY>.ret <vtype>                   ; a valued routine only

A value-returning PL/I function on this machine returns by SLOTPATCH: it
stores its result into the saved-ac0 image of its own frame so that `WRTN`
restores it — 32-bit at `wp(ac3,-8)`, 16-bit at `wp(ac3,-7)` (Salvage F4;
the addrbook's `slotpatch` flag marks exactly these 16 entries). `<ENTRY>.ret`
is the naive form's cell for that value: the callee writes it, the caller
reads it, and reintroducing the slotpatch is an L2 rewrite like every other.

A declared signature with no return is half a signature, and widening one
later would touch every call site, so the form is landed now even though no
routine yet compiled calls a valued game routine. A RUNTIME routine is
different: it returns in `ac0` by convention (RTConventions.md), which the
valued-call split of §5.10.6 already covers.

#### 5.10.2 Names

    qualified := <ENTRY> "." ("v" | "b" | "a") <digits>
               | <ENTRY> "." ("arg_count" | "ret")          ; ir 8, the two fixed names
    <ENTRY>   := an addrbook entry name (quest.addrbook, QUEST_ADDRESS_BOOK),
                 UPPERCASE, the `@ADDR` suffix DROPPED: QUEST, QUEST.1, FIRE.1
                 (from `FIRE.1@7016A3BD`), ALCHEMIST_HOME, C_A_LISTENER

Parsing splits on the LAST dotted component: `.v<digits>` / `.b<digits>` /
`.a<digits>` / `.arg_count` / `.ret` is the local, everything before it is
the entry (an entry's own suffix is `.<digits>`, never `.v`/`.b`/`.a`;
entries are uppercase, the local's first character lowercase, so even a
routine named `B12` could not collide). The namespace is ALL entry lines of
the addrbook — the 102 migrated AND the 28 `#`-commented (not migrated)
ones; a `nocall` ON-unit is still a routine a compiler may emit — 130 names,
unique after the suffix drop. `idx(E)` is the entry's 0-based position among
those lines in FILE ORDER, which the addrbook's own header already freezes
("keep the columns"). REFUSE: a lowercase entry, an entry not in the
addrbook, `.v`/`.b`/`.a` without digits, `.x<digits>`, any other shape.

#### 5.10.3 Placement (the loader owns it; nothing else does)

    | space | contents                          | assigned by                                   |
    |-------|-----------------------------------|-----------------------------------------------|
    | 0x70  | original code                     | —                                             |
    | 0x74  | locals and args, per routine      | M4a addrbook                                  |
    | 0x75  | string twins s@<block>.<k>        | quest.arena (§5.9)                            |
    | 0x76  | UNPLACED v AND `a` cells          | the IR loader, per-entry range, this section  |
    | 0x77  | UNPLACED symbolic blocks          | the IR loader, per-entry range, this section  |

Per-entry reserved range in each space: 0x10000 words (130 × 0x10000 =
0x820000 < 0x1000000; the whole 0x74 area is 40,428 words, so 65,536 words
per routine is ample).

- A symbolic block's address is a pure function of its name:
  `addr(<E>.b<k>) = 0x77000000 + idx(E)·0x10000 + k`, k < 0x10000 (refuse
  otherwise). No loader state, deterministic, invertible for diagnostics.
  It is assigned on FIRST SIGHT (a header or a label) and the file is
  refused at end if any label was never defined by a header.
- A `v` is allocated SEQUENTIALLY within `0x76000000 + idx(E)·0x10000`, in
  declaration order, by its size in words; overflowing the entry's range
  refuses. **ir 8: `a` cells (`a<N>`, `arg_count`, `ret`) are allocated from
  the same cursor, in the same declaration order** — they are cells like
  `v`s and there is one allocator, not two. An INITIALISED `v` (§5.10.1b) is
  allocated identically and its bytes are written into its placed words at
  load, before the first block runs. Every `v` gets its own private slot: with no placement input,
  two `v`s can never share an address, so the "live ranges disjoint"
  precondition of DESIGN §5.3 is vacuous here. The allocator asserts
  disjointness loudly anyway. NOTE, so nobody reads it as a check that
  passed: in ir 7 that assertion CANNOT fire — there is no input that
  could ask for a shared address. The REFUSAL of an aliasing placement
  belongs to the placement input (§8), when it exists.
- Numeric ORDER of symbolic blocks carries no meaning: every terminator is
  explicit (§4), so there is no fall-through for address order to encode.
- PLACEMENT IS NOT BINDING (DESIGN §5.2): placing a `v` (0x76 → 0x74, a
  future input) substitutes one constant for another in the statement text
  and changes nothing else. Binding a `v` to a register deletes loads and
  stores and is a transformation, outside this spec.
- Why ring 7: every M16/M32/R index is wrapped `(e & 0x0FFFFFFF) | seg`
  (§5.2; `Machine::copy_segment`), which replaces ONLY the top nibble, so a
  0x74/0x75/0x76/0x77 address referenced from any ring-7 block survives the
  wrap unchanged — bits 27:24 (the `4`, `5`, `6`, `7`) are preserved. A
  block's `seg` is `start & 0xF0000000` = 0x70000000 for a 0x77 block as
  for a 0x70 one. 0x4xxxxxxx for blocks was rejected because block
  addresses land in the PC and ring 4 has different protection semantics.

#### 5.10.4 THE VARIABLE FORM (ir 8)

**A cell name denotes the CELL'S CONTENTS, as a C variable does.** It is a
primary and it is an lvalue. Taking its address is `wp`/`bp` (§5.2); reading
through it, when it is a pointer, is `M8`/`M16`/`M32`.

| operation | ir 7 (old) | **ir 8** |
|---|---|---|
| read a local | `M32[PXY.v3]` | `PXY.v3` |
| write a local | `M32[PXY.v3] = e` | `PXY.v3 = e` |
| address of a local | `PXY.v3` or `wp(PXY.v3,0)` | `wp(PXY.v3, 0)` / `bp(PXY.v3, 0)` |
| read through a pointer | — | `M32[PXY.a1]` |
| write through a pointer | — | `M16[PXY.a1] = e` |

**Width and signedness come from the DECLARATION, not from the statement.**
This is the one place in the spec where that is true, and §5.10.9 says what
it costs.

| declared | a read is | a write is |
|---|---|---|
| `i16` | `sx16(M16[addr])` | `M16[addr] = e & 0xFFFF` |
| `u16` | `zx16(M16[addr])` | `M16[addr] = e & 0xFFFF` |
| `i32` `u32` and every `<ptype>` | `M32[addr]` | `M32[addr] = e` |

**Aggregates have no contents.** `char n`, `varying n` and `words n` name a
region, not a value, so a bare aggregate name as a value or an lvalue
REFUSES: it is legal only as the first operand of `wp`/`bp` and as an
address inside the §5.8 string forms. The alternative — letting it silently
mean "the first word" — is exactly the kind of implicit §5.1 exists to
prevent.

    QUEST.v0 = 3                                 ; an i16 write (truncating; the decl says so)
    ac0 = QUEST.v0                               ; an i16 read (sign-extended; the decl says so)
    QUEST.v1 = add(QUEST.v1, ac0)                ; an effectful store to a u32 cell
    ac2 = bp(QUEST.v2, 0)                        ; byte pointer into a char/varying v
    M16[QUEST.v6] = trunc16(ac0)                 ; a write THROUGH a *i16 pointer
    ac1 = M32[QUEST.v7]                          ; a read THROUGH a *i32 pointer
    QUEST.v7 = wp(QUEST.v1, 0)                   ; point v7 at v1 — no `&` needed
    [@QUEST.v3, 27 varying] = [@s@70166144.3, varying]   ; twin -> v, min(len, 27) truncates
    M16[wp(QUEST.v4, 5)] = 0                     ; word 5 of a `words 10` aggregate
    FAKE_OCEAN.a1 = INIT_SCREEN.a1               ; pass an incoming pointer through
    goto [QUEST.b3, QUEST.b7] (ac0 <s 0)         ; symbolic exits

The last-but-one line is worth reading twice, because it is the clearest
case of ir 8 reading BETTER than the original: the book spells that
pass-through `XPEF @[ac3+0xFFF4]`, a push of a slot's contents; ir 8 spells
it as the pointer copy it is. P53's diff has to account for the difference.

#### 5.10.5 The tripwire (ir 8; replaces ir 7's width tripwire)

ir 7's tripwire asked "does this `v` fit a direct `M<n>[v]` index". In ir 8
`M<n>[v]` no longer means that, so the rule is REPLACED, not extended
(loader; violations REFUSE):

    M8[<word pointer>]                REFUSE      ; kind
    M16[<byte pointer>]               REFUSE      ; kind
    M32[<byte pointer>]               REFUSE      ; kind
    M8[<byte pointer>]                allowed
    M16[<word pointer>]               allowed     ; pointee width is ADVISORY
    M32[<word pointer>]               allowed     ; a wide is two consecutive words
    M<n>[<cell that is not a pointer>]  REFUSE
    [@<cell>, … varying]              the cell must be `varying n` or `*varying n`
    [@<cell>, <n>]                     REFUSE — a fixed piece needs a BYTE pointer,
                                       `[@bp(<cell>, 0), n]`

An index of the form `wp/bp(<cell>, d)` is not width-checked (the offset is
the source's business), as in ir 7. The tripwire still exists for the reason
it was landed: it catches a compiler emitting the wrong width or the wrong
pointer kind AT THE IR BOUNDARY, where it is loud, instead of three projects
later as a behavioural divergence.

#### 5.10.6 What a symbolic block may contain — and calls out of one (ir 8)

Legal: statements (§5.1), string statements (§5.8, §5.9), `assert`, `goto`
(any labels), `ret`, and **since ir 8 `call` and `rt_call` with a SYMBOLIC
RETURN LABEL** (§6). Still REFUSED: `@addr` instructions, which are fetched
from real memory at their own address and so cannot come from a block that
has none.

ir 7 refused calls because they are validated against the LCALL word at a
real `site=`, and a compiled routine calling `?WRITE_SCREEN` has no site
(P46 gate finding F7; the refusal was recorded then as a SCOPE BOUNDARY, not
a design position). ir 8 lifts it. The structure already fitted: `call` and
`rt_call` are TERMINATORS, so a call already ends its block and the
continuation is already a separate block — at `site+4` for a book call, at
the symbolic label for a compiled one. The change is a symbolic target
filling a slot that existed.

**TWO ARGUMENT MECHANISMS, and the difference is not an inconsistency to
paper over — it reflects which end of the call we control.**

- **`rt_call` arguments go on the REAL STACK, as they always have.** The
  runtime reads argument *n* at `wsp−2n` from the LCALL marker
  (`RTBridge::arg_pointer`, hw/RTBridge.cpp:111; §6). Writing them into `a`
  cells would put them where the callee never looks. So an `rt_call` out of
  a symbolic block still pushes, right to left, through
  `Machine::wide_push` — we control the caller only.
- **game→game arguments go in the callee's `a` CELLS.** The caller writes
  `<CALLEE>.a1 … <CALLEE>.aN` and `<CALLEE>.arg_count`, then transfers. This
  is legal only because the game is non-reentrant, and it is the NAIVE form:
  the original's push-and-prologue machinery is a rewrite to reintroduce at
  L2, not something the compiler emits (same principle as pure-vs-effectful
  operators, P48 R2). We control both ends.

**THE VALUED-CALL SPLIT — a subset rule P53 needs stated, not discovered.**
Because a call is a terminator, **a call cannot sit inside a larger
expression.** `r = RANDOM_NUMBER$3(…)` is a call terminating one block and
`r = ac0` opening the next. A runtime routine returns in `ac0` by
convention (RTConventions.md); a valued GAME routine returns through
`<CALLEE>.ret` (§5.10.1d).

`ret` from a symbolic block runs WRTN with the synthetic pc = the block's
0x77 address (abort-message use only, as for 0x70).

**What ir 8 did not do, and P54 does:** ir 8 was spec and loader; a call
from a symbolic block now EXECUTES through the calling bridge (P54,
docs/Project54/REPORT.md §1 — read that section for what the mechanism
actually is; the design's "LCALL replica" was half of it).

#### 5.10.7 Executor and lockstep facts (normative)

- 0x76 is ORDINARY MEMORY: the clone process maps, RW / no exec, exactly
  the 0x76 pages the placed `v`s touch (the address-book and arena
  precedents, `os/OSProcess.cpp`); a read or write outside a mapped page
  faults in `Memory` exactly as any unmapped address would. An uninitialised
  `v` read before its first write reads zero (fresh pages), which is a
  property of the harness, not a promise of the source. **ir 8 AMENDMENT: it
  is no longer true that "nothing is initialised."** An initialised `v`
  (§5.10.1b) has its bytes written by the loader at placement and is
  readable before the first block runs. Said here explicitly so that a
  future reader meets an amendment rather than a contradiction.
- 0x77 is NEVER MAPPED. Nothing is fetched there: `Machine::run` asks the IR
  executor for the block BEFORE any memory access, so a 0x77 pc is a block
  identity by construction — `goto` returning a 0x77 label re-enters the
  same path. `IRExec::has(pc)` is the whole dispatch.
- Both spaces are clone-only, like every IR block (§1, §7): only the clone
  dispatches IR; the master never sees a 0x76 or 0x77 address.
- A 0x77 arrival is NOT a listed sync-list pc, so it does not tick the
  block ordinal; a 0x76 pointer in a register at a rendezvous decodes as
  Mapper form `None` and would MISMATCH a master stack pointer
  (Mapper.md §1.2). Both are consequences for SUBSTITUTION (P48), which
  will already be delisting the replaced routine's blocks; ir 7 states
  them and does nothing about them. Nothing in the book (`quest.ir2.*`)
  names a `v`, an `a` cell or a symbolic block — measured: zero `v` and
  zero `a` declarations in either artifact — so the strict surface is
  untouched by ir 7 and by ir 8 alike (§5.10.9).
- Placement diagnostics: `IRExec: v <name> type <t> words <w> at 0x76……`
  and `IRExec: block <name> at 0x77……` at load (one line each, stderr);
  first execution of a symbolic block logs its name with its address.

#### 5.10.8 Dereference, the bit-31 guarantee, and what the book measures (ir 8)

**The naive form spells a dereference plainly: fetch the pointer, fetch
through it.** `R[]` stays in the grammar because the book uses it; **the
compiler never emits it.** `R[e]` means "deref, then follow bit 31 until
clear" (§5.2), which is equivalent to a plain double fetch ONLY WHEN bit 31
is clear. Emitting `R[]` naively would assert that invariant at every
dereference instead of establishing it once.

    M32[M32[a]]  →  M32[R[a]]        is a REWRITE, precondition: bit 31 of
                                     the fetched word is clear

One rule, ~1,000 applications — the shape DESIGN §7.2a wants.

**THE PRECONDITION IS A PROPERTY OF THE FORMS, not a data assumption.**
Every address an ir 8 program can produce comes from `wp()`, `bp()`, or a
cell name. `wp` is `((b+d) & 0x0FFFFFFF) | seg` and `bp` is
`set_byte_segment(seg & 7, b*2 + d)`, with `seg` = 0x70000000 for every
block including a 0x77 one (§5.2, §5.10.3); a cell name is a 0x76 constant.
**Bit 31 is clear by construction in all three**, so the rewrite's
precondition holds for every address the program itself builds. The loader
adds a belt: **a top-bit-set literal in an ADDRESS POSITION refuses** — used
directly as an `M8`/`M16`/`M32`/`R` index, or assigned to a pointer-typed
cell.

The refusal is SCOPED to address positions deliberately. An unscoped ban on
top-bit-set literals would refuse the book 660 times (`quest.ir2.book` and
`quest.ir2.stock` alike): `ac1 = 0xFFFFEEEE ; NLDAI 61166,1;`,
`ac0 = add(ac0, 0xFFFFDB10) ; WNADI 0,56080;` — sign-extended 16-bit
immediates, not addresses. In an address position the count is **zero** in
both artifacts (literal M/R indices with bit 31 set: 0; `wp`/`bp` base
literals with bit 31 set: 0), so the scoped rule costs the strict surface
nothing.

**CENSUS 1 — the `R[]` sites of `quest.ir2.book`** (P52 gate §4.1; 1,032
lines, **1,035 occurrences**; stock 914/917):

| bucket | occurrences | where the value comes from | levels | is the cell written? |
|---|---|---|---|---|
| own-frame argument slots, `R[ac3 + -d]` and 2 spelled `R[wp(ac3,-12)]` | 939 | the caller pushed the address | 1 | — |
| UPLEVEL argument slots, `R[ac2 + -d]` | 41 | the static link, `ac2 = M32[wp(ac3,-6)]`, in a nested `.N@` entry | 1 | — |
| statics — `R[0x70000212]` ×22, `R[0x70000210]` ×3, `R[0x700007A0]` ×2 | **27** | a pointer word in the shared-data page | 1 | the pointer word, no; its pointee, yes (a counter at 70174209 and 701745E8) |
| positive frame offsets, 24 plus 4 spelled `R[wp(ac3,d)]` | **28** | the routine computes it itself | 1 | **YES — 42 writes across 11 cells** |

No `R[]` contains another `R[]` in either artifact: **one level, everywhere.**

Three consequences the spec keeps:

1. **The rewrite's precondition is PROVABLE for 1,008 of the 1,035 sites**
   and an assumption about data for 27. For arguments we pushed the address
   ourselves; for the 28 positive-offset sites the routine built the address
   from `wp`/`bp`, so those are provable too. Only the statics are assumed.
2. **The 41 uplevel sites are a DIFFERENT rewrite.** They are Salvage F2's
   triple indirection (link → arg slot → datum) in nested entries —
   ALCHEMIST_HOME.1 14, FIRE.3 14, BOAT.1 5, FIRE.2 3, MOVE_PLAYER.1 3,
   BARGAIN.1 1, BARGAIN.2 1. In the naive form they are FREE: the child
   names the parent's `a` cell cross-entry and no link exists (DESIGN
   §4.1a). Their L2 rewrite reintroduces the static link; it is not the
   dereference rewrite. So "one rule, many applications" covers 994 sites,
   not all of them.
3. **A local's address value IS assigned, and often** — the 28 positive-
   offset sites are 11 cells written 42 times, twelve of them DIED's slot
   +8. That is what `<ptype>` and `*varying <n>` are for (§5.10.1a); it is
   not an edge case.

**CENSUS 2 — the optional-argument mechanism** (P52 gate §4.2). Salvage F5
records two spellings. **The second is not an optional-argument mechanism.**
RETURN_MESSAGE @70176FDD tests `M32[wp(ac3,-16)]` for null, and (a) −16 is
argument 3, which is supplied at BOTH arities, so a null there cannot
discriminate 3 from 6; (b) the body reads only arguments 1, 2 and 3 and
never touches 4, 5 or 6, so `mixed:3/6` produces no arity-dependent code at
all; (c) all five call sites supply a non-null argument 3, so the
default-message arm (Salvage F11's literal at 0x70000CCD) is dead in this
program. It is a null-POINTER test on a supplied by-reference argument.

**So exactly one optional-argument mechanism exists, and it is the count
read:** REFRESH_SCREEN block 70176A93, `ac0 = sx16(M16[wp(ac3, -9)])`,
tested `== 0`. That is what `arg_count` (§5.10.1c) is for, and there is no
second case to support. The evidence is one witness; the spec says so
rather than implying two.

#### 5.10.9 ir 7 → ir 8: what the reversal costs, and which ir 7 files still load

In ir 7 a `v` name was an ADDRESS CONSTANT (P46 a001 R3). ir 8 reverses that.
The reversal was relitigated and won on the day it was made; DESIGN §4.1c
records it, and the argument that lost — that a typed value form puts
signedness into storage — is not wrong, it is **the accepted cost**:

**`ircmp` must consult declarations when comparing against the book.** The
book spells an access explicitly, `M32[wp(ac3,12)]`; ir 8 spells it
`PXY.v3`, and only the declaration says how wide and how signed that is.
Expanding names before comparing is now `ircmp`'s job. This compounds the
slot-bijection obligation P46 found (DESIGN §5.2): the book spells a local
register-relative and a placed cell is absolute.

The one observation from the old note that SURVIVES: **no `&` operator is
needed.** `wp`/`bp` already carry the word/byte distinction (§5.2).

**COMPATIBILITY (loader):** an `ir 7` header is accepted **if and only if
the file declares no `v` and no `a`**. The construct whose meaning moved is
then absent, so such a file's meaning is provably unchanged and it loads as
ir 8 reads it. `quest.ir2.book` and `quest.ir2.stock` declare zero of each
and therefore keep loading untouched — which is what lets ir 8 land without
regenerating an artifact. This is a narrow COMPATIBILITY WINDOW, not a
second dialect: an `ir 7` file that declares a `v` is refused, loudly,
because its `v`s mean something this loader does not implement. `ir 6` and
earlier are refused as always.

#### 5.10.10 Worked example (a symbolic-only program; the self-test's shape)

    ir 8
    mode stock
    v QUEST.v0 i16
    v QUEST.v1 u32
    v QUEST.v2 varying 8
    v QUEST.v3 char 11 = "HELLO WORLD"    ; an initialised v: the loader writes the bytes
    v QUEST.v4 *u32                       ; a word pointer
    a QUEST.a1 *i16                       ; argument 1 is a pointer to an i16
    a QUEST.arg_count u16

    block QUEST.b0
      QUEST.v0 = 3 ; counter
      QUEST.v1 = 0
      QUEST.v4 = wp(QUEST.v1, 0)          ; point v4 at v1 — no `&` needed
      goto [QUEST.b1] 0

    block QUEST.b1
      ac0 = QUEST.v0                      ; i16: sign-extended, because the DECLARATION says so
      M32[QUEST.v4] = add(M32[QUEST.v4], ac0)   ; accumulate THROUGH the pointer
      QUEST.v0 = ac0 - 1                  ; i16 write: truncates, because the declaration says so
      goto [QUEST.b2, QUEST.b1] ((ac0 - 1) >s 0)   ; backward edge; b2 is a forward reference

    block QUEST.b2
      [@QUEST.v2, 8 varying] = [@bp(QUEST.v3, 0), 11]   ; the initialised v, truncated to 8
      M16[QUEST.a1] = trunc16(QUEST.v1)   ; write out through the i16 parameter
      ret

    blocks 3

Loaded with `QUEST_ADDRESS_BOOK=quest.addrbook` (QUEST is idx 0): the three
blocks are 0x77000000, 0x77000001, 0x77000002; v0 at 0x76000000 (1 word),
v1 at 0x76000001 (2), v2 at 0x76000003 (5), v3 at 0x76000008 (6), v4 at
0x7600000E (2), a1 at 0x76000010 (2), arg_count at 0x76000012 (1) — one
cursor, declaration order. No `blocks` provenance line is needed (no numeric
label). The loop runs three times; at the `ret` v1 holds 6, v2 holds length
8 and `HELLO WO`, and the i16 the caller pointed `a1` at holds 6.

Read `M32[QUEST.v4] = add(M32[QUEST.v4], ac0)` against ir 7's
`M32[QUEST.v1] = add(M32[QUEST.v1], 1)`: the ir 7 line accumulated INTO v1
by naming its address; the ir 8 line accumulates into whatever v4 points at,
and `QUEST.v1 = …` would be the direct write. The two spellings no longer
mean the same thing, which is the whole of §5.10.9.

### 5.6 Class cap — what lower.py emits

Everything in Project26/Census.md buckets (a), (b) and the ruled-in
(c) set: loads/stores (X/L × N/W LDA/STA, modes 0–3, direct/indirect),
XLEF/LLEF, NLDAI/WLDAI, WMOV, byte addressing (XLEFB/LLEFB values,
XLDB/XSTB/LLDB/LSTB/WLDB/WSTB — P25), §6's argpush stores (P25); the
skip family WSEQ/WSNE/WSLT/WSLE/WSGT/WSGE/WUSGT/WUSGE + the word-I and
wide-I forms and WSZB/WSKBO/XNISZ as `goto [fall, skip] test`
(signedness per skip is the emulator's cast, per mnemonic — Census.md
§2a; XX==YY compares against 0); direct XJMP and WBR as `goto [L] 0`;
the effectful family (WADD WSUB WADC WINC WNEG WADI WSBI WADDI WNADI
XWADD LWADD XWSUB LWSUB XWADI XWSBI WMUL XWMUL LWMUL WDIV CVWN NADD
NSUB NNEG NADI NSBI NADDI XNADD LNADD XNSUB LNSUB XNADI LNADI XNSBI
LNSBI NMUL XNMUL LNMUL, WHLV as root `ash(x, -1)`); the word layer
(WCOM WAND WIOR WXOR WANDI WIORI WXORI ANDI WLSI WLSHI WMOVR SEX ZEX);
WBTZ/WBTO via ind(); LDAFP/LDASP reads; CRYTO; WXCH and the 23 borrow
brackets via t-places; XNDO/XWDO; the Nova no-load (`#`) skip forms as
tests derived mechanically from NovaCompute.cpp's CC/op/SS/KKK tables
(Census.md §2d); since P28 (ir 4): the 67 Nova LOAD forms (pure — the
no-load decomposition plus `c = <carry>; acY = <16-bit result>`; the
high half is what NovaCompute.cpp:63–66 leaves, zero except that the
SS=1 rotate keeps bit 16 = old bit 15 — UNDEFINED per the manual,
HWFindings_Sep5.md §3/§7, a don't-care replicated for the strict
surface; a `SKP` form is `goto [pc+2] 0`), LNDO (as XNDO with the
L-form EA and pc+4 fall-through, EagleGeneral.cpp:225–236), the LDSP
pair (§6), and the 987 runtime call sites as `rt_call` (§6).
Everything else stays an instruction — in particular indirect XJMP,
DIVX/WDIVS, the two LDSP-fed `DERR 17` sinks (terminal, verified
pairs), the string/WMSP/stack-write family (since ir 5: minus the 652
located-string sites of §5.8; the append chains, WSTB, tail splits and
CALLRESULT pieces are P32, the WMSP temps and STASP P33), calls
(undecorated), frames, floats. The cap widens by extraction, never by assumption.

### 5.7 Worked example (an emitted block, ir 3 grammar — unchanged in ir 4)

    block 7015C2A6 seg 0x70000000
      ac1 = 0x000002AE ; NLDAI 686 (0x02AE),1;
      ac0 = mul(ac0, ac1) ; WMUL 1,0;
      ac2 = 0x00000010 ; NLDAI 16 (0x0010),2;
      ac0 = mul(ac0, ac2) ; WMUL 2,0;
      ac0 = add(ac0, 0xFFFFDB11) ; WNADI 0,56081 (0xDB11);
      ac1 = M32[0x70000210] ; LWLDA 1,[0x70000210];
      M16[ind(ac1) + lsh(ac0, -4)] = M16[ind(ac1) + lsh(ac0, -4)] & ~lsh(0x8000, 0 - (ac0 & 15)) ; WBTZ 1,0;
      goto [7015C2B2] 0 ; fall-through

    block 7015C2BB seg 0x70000000
      ac0 = sx16(M16[0x7000021A]) ; LNLDA 0,[0x7000021A];
      t1 = ((ac0 & 0xFFFF) | lsh(c, 16)) ; MOV.L# 0,0,SNC;
      goto [7015C2BF, 7015C2C0] ((lsh(t1, -15) & 1) == 1) ; MOV.L# 0,0,SNC;

    block 7015D923 seg 0x70000000                ; (ir 4) an rt_call block
      ...
      M32[wp(ac3, 44)] = ac2 ; XWSTA 2,[ac3+0x2C];
      ac2 = wp(ac3, 50) ; XLEF 2,[ac3+0x32];       ; register argument (ac2)
      rt_call ?UNSIGNED_TO_CHAR(wp(ac3, 44)) site=7015D930 ; LCALL [0x7017DA75],1; # ?UNSIGNED_TO_CHAR  <- XPEF [ac3+0x2C];

Reading the third block: the XPEF at 7015D92C is folded into the
terminator's argument (echoed after `<-`); the interleaved XLEF stays
an ordinary statement in program order; the executor pushes wp(ac3,44)
and runs the LCALL at 7015D930, exiting to the callee — the block at
7015D934 (site+4) resumes on return.

Reading the second block: the Nova `#` form writes nothing, so it is a
pure test; t1 holds NovaCompute's 17-bit source (carry-in from `c`
because CC=0), the L shift's carry is bit 15 of it, and SNC skips when
that carry is 1 — exits [no-skip, skip] in ascending pc order.

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
- `rt_call <callee>(e1, …, eN) site=<s>` (P28, ir 4;
  docs/Project28/Census.md, RTConventions.md) — the game→runtime LCALL
  at `s` with its N argument pushes (XPEF/LPEF/WPSH; the site is always
  block-final, 987/987) folded in. Executor: evaluate e1…eN (pure —
  order unobservable), materialize registers, `machine.pc = s`, verify
  the declared beliefs against the instruction words (LCALL opcode;
  argc field == N; callee symbol == the LCALL's resolved target — loud
  throw on any mismatch), then push eN FIRST and e1 LAST, each through
  `Machine::wide_push` — the SAME helper XPEF/LPEF/WPSH call
  (EagleStack.cpp:665–701, :601), owner of the wsp>wsl overflow fault —
  then execute the LCALL instruction at `s` through the normal fetch/
  decode/execute path (as `call` does: hooks, ovk/ovr check, count) and
  return its new_pc. The runtime then runs exactly as today (emulated,
  logging stub, or native body — the LCALL body's own registry lookup).
  **Evaluation order is RIGHT TO LEFT** because the callee reads
  argument n at wsp−2n from the LCALL marker (`RTBridge::arg_pointer`,
  hw/RTBridge.cpp:111; marker `(psr<<16)|argc` pushed by LCALL,
  EagleStack.cpp:239–244; WSAVS saves ac0 ac1 ac2 wfp ac3|c above it,
  :421–425, so arg n sits at wfp−10−2n): the LAST push is argument 1.
  The pushmap's `# XPEF arg2` (lower pc) / `# LPEFB arg1` (higher pc)
  comments record the same fact for game→game sites. Worked example
  (site 7015C047): `XPEF [ac3+0x14]; LPEF [0x70000260]; LCALL
  [0x7017E27A],2` → `rt_call ?WRITE_SCREEN(0x70000260, wp(ac3, 20))
  site=7015C047`: the executor pushes wp(ac3,20) then 0x70000260; the
  callee's arg 1 is the channel word at 0x70000260 and arg 2 the text at
  ac3+20 (rt/write_screen.hpp: channel, text). Arguments render with
  the P25 grammar and nothing new: XPEF/LPEF → `wp(base, d)`, a word
  constant, or `R[base + d]`; WPSH x,a → the register values `acx…aca`
  ascending (ac x pushed first = the HIGHER-numbered argument);
  XPEFB/LPEFB → `bp(...)`/`0xW:b` (none occur at runtime sites). The
  emitter folds an argument inline only when the expression reads no
  state an interleaved statement writes (987/987 in the Sep 5 census —
  every interleaved XLEF writes ac2, every such argument reads ac3);
  interleaved XLEF/XWSTA (the ac2 register argument of
  ?UNSIGNED_TO_CHAR; compiler spills) lower as the ordinary statements
  they are, in program order, before the rt_call, which does not
  declare them — RTConventions.md documents which registers each
  callee reads on entry / writes on return. Emitter refuse list (each
  censused, the site stays an instruction): window not [pushes +
  XLEF/XWSTA]; argc != wides captured; window crossing the block
  start; argc outside the callee's known set (RTConventions.md, ruling
  F2); pef_value cannot render a push; an indirect argument with an
  interleaved store; an argument reading a register an interleaved
  XLEF writes (the t-place form is legal but not emitted — 0 sites);
  block successors != [site+4]. No wsp arithmetic lives in the IR or in
  IRExec; stack-register writes stay refused. Real stack in BOTH modes
  (no book slots, no argpush machinery); block-ordinal accounting and
  the ovk/ovr check are unchanged — the callee's blocks are not IR
  blocks. Recorded difference (Census.md F6): a wsp>wsl fault names
  `pc=` the LCALL site on the clone and the overflowing push's pc on
  the master — never fired, accepted.
- LDSP (P28, option A1; EagleGeneral.cpp:251–260): `assert((lo <=s acX)
  && (acX <=s hi), "DERR 17 @<sink>")` then `goto [L(lo) … L(hi)] acX −
  lo`, where L(k) is the table's target for index k and a −1 entry's
  label is the fall-through `DERR 17` sink block (pc+3), which stays a
  listed, embedded, TERMINAL instruction — a verified terminal pair.
  Out of range detaches by assert (P27's pairing). Table bounds and
  entries come from the dis's rendering of the table; the emitter
  refuses if the successor set disagrees.
- `ret` — executes WRTN's fixed opcode (0x87A9) through the normal
  decode path (WRTN is address-independent; the synthetic address is
  the block start, reaching only abort messages). Frame pop, psr/ovk
  restore, carry from bit 31 of the return slot — all shared code.
- `goto [L0..Lk] e` — pure exit: evaluate e (a strict index; out of
  range FAULTS), materialize registers, return L[e]. Lowered skips use
  [no-skip, skip] with the test yielding 1 for skip (the CFG lists the
  two successors ascending in that order); lowered XNDO/XWDO use
  [fall, loop-target]. `goto L` is sugar for `goto [L] 0`.
- **`call <ENTRY> args=<n> ret=<ENTRY>.b<k>` (ir 8, the NAIVE game→game
  call).** No `site=`, no `marker=`: there is no LCALL word, so there are no
  beliefs to cross-validate against one. The beliefs it DOES declare are
  checked against the CALLEE'S DECLARATIONS (§5.10.1c) — `args=<n>` must
  equal the callee's `a` count, and the ARGUMENT-KIND CHECK runs (P54, per
  docs/Project54/a003 item 2 — it was specified here before it existed):
  for each `a` cell of pointer type, the LAST write to it in the CALLING
  BLOCK before the call must be of the matching KIND wherever that kind is
  MANIFEST — `wp()` is a word pointer, `bp()` a byte pointer, a pointer cell
  carries its declared kind. What the check does NOT claim: a value of
  unknowable kind (a constant, a memory read, a register) passes, and a
  write in an earlier block is not examined. The M-form tripwire (§5.10.5)
  still catches a wrong kind where the pointer is USED; this catches it
  where it is PASSED. Arguments travel in the callee's `a` cells, written by the caller,
  with `<CALLEE>.arg_count` set; the exit is `ret=`, an ordinary symbolic
  label. A valued callee returns through `<CALLEE>.ret` (§5.10.1d). Legal
  only because the game is non-reentrant. **Execution (P54):** a naive
  `call <ENTRY>` transfers to `<ENTRY>.b0`, which must be a block of the
  same file; the loader refuses otherwise. Calling an un-compiled Eagle
  routine from a symbolic block is not this form (it needs the real-stack
  protocol and the M4a area writes — P50). The bridge pushes the marker
  with argc 0 — nothing is on the stack, `<CALLEE>.arg_count` carries the
  arity — and the WSAVS image (frame size 0, `ovk` from the callee's
  addrbook variant), so the callee's `ret` is the ordinary WRTN
  (docs/Project54/REPORT.md §1).
- **`rt_call <callee>(e1, …, eN) ret=<ENTRY>.b<k>` (ir 8, the naive runtime
  call).** Identical to the `site=` form above in everything that concerns
  the ARGUMENTS — they are evaluated right to left and pushed through
  `Machine::wide_push`, because the runtime reads argument *n* at `wsp−2n`
  from the LCALL marker and would never look in an `a` cell. What is absent
  is the site: the callee symbol is resolved from the runtime symbol table
  rather than from an LCALL's target, and the exit is `ret=`. The two
  mechanisms differ because we control both ends of a game→game call and
  only the caller's end of a runtime call.
- **`X.CB` and the callee rule (ir 8).** The `rt_call` callee rule is
  widened: a callee that is not a `?` symbol is accepted when it resolves
  into the runtime range AND its argument list is EMPTY — the register
  convention, with the setup as ordinary preceding statements. `X.CB
  @7017E708` is the case that forced it (Salvage F12: ac2 = the destination
  word address, ac0 = a byte pointer to the character form, ac1 = its
  length, then an undecorated `LCALL [0x7017E708],0`); both book sites,
  7016AA41 and 701703A6, are already block-final with the three register
  arguments as ordinary statements, so the shape `rt_call` validates fits
  without a second production. A new production would have duplicated the
  whole belief-check apparatus for one callee, and `RTConventions.md` is
  already the home of record for any runtime routine including the
  `X.*`/`I.*`/`O.*`/`D.*` helpers — the IR follows the same widening rather
  than treating `?` as a type distinction it was never meant to be. Note
  that a COMPILED `BITS("001")` also needs the character form to exist,
  which is §5.10.1b's business, not this rule's.
- **Built in P54.** ir 8 specified and validated these three forms; the
  calling bridge (P54, docs/Project54/REPORT.md §1–§2) makes them EXECUTE:
  `rt_call` pushes its arguments and the marker, resolves the callee from
  the symbol table and dispatches as LCALL would; the naive `call`
  replicates LCALL and the callee's WSAVS together and enters `b0`.

Scope by decoration ("no mixed metaphors", user ruling): a decorated
site's pushes lower ONLY if every decorated push of the site is
expressible and in the site's block; otherwise the whole push
sequence AND its call stay instructions (uniform accounting per
site). As of P25 all 566 decorated sites lower (566/566;
Project25/ByteEA.md is the ledger): B-form pushes emit byte-pointer
VALUES, one WPSH x,a emits its wides as ascending group stores
M32[slot+2k] = ac((x+k)&3) (AC[XX] at the base slot — the emulated
hook's verified ordering, EagleStack.cpp P18 tranche B). Borrow
brackets (the pushmap's `borrow` lines) are NOT part of a site's
accounting (args= never counted them); since P26 they lower to
t-place save/restore pairs (§5.4) in both modes — the P25 form (@addr
WPSH/WPOP instruction pairs) is superseded.

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
- Statements: pure statements evaluate the rhs then assign; effectful
  statements evaluate the pure arguments, call the shared helper (which
  writes machine.c/ovr directly), assign, then run the `ovk && ovr`
  check. Statements do not advance instruction_count and do not fire
  the Capture hook (accepted since P23: the pair gate compares block
  structure, not per-statement counts). t-places live in the block
  context and die with it.
- `goto [..] e`: evaluate e, range-check (fault), materialize, return
  the label. No hook, no count.
- Statement memory faults are rethrown with [block, statement index,
  store address] context; grammar faults (§5.1) name the block.
- Debug: QUEST_IR_DEBUG_BLOCK=<hex> prints per-statement ac state for
  that block. First execution of each block logs once to stderr
  (coverage evidence).
- QUEST_IR requires -lockstep (refused otherwise: only the clone
  dispatches IR; a non-lockstep run would silently ignore it). The
  self-tests load a file directly (`IRExec::load_file`) into a scratch
  Machine with `lockstep_role = CLONE` and drive `Machine::run` — the
  same dispatch path, no lockstep peer (§5.10.7, docs/Project46/REPORT.md).
- Symbolic blocks (ir 7) execute exactly as numeric ones; `Ctx.seg` is
  0x70000000; nothing is read at the block's address.
- The VARIABLE FORM (ir 8) costs the executor almost nothing, by design. A
  cell reference carries its placed address, its width and its signedness
  from the PARSER, so evaluating one is a `read_word`/`read_wide` plus the
  declared extension, and assigning one is the matching store; the executor
  never consults the declaration table. The `wp`/`bp` operand-kind overload
  (§5.2) is likewise resolved statically, so the two spellings reach the
  executor as different nodes and no kind test runs per evaluation.

## 8. Reserved / roadmap

ir 8 reservations (P52): the EXECUTION side of a call out of a symbolic
block — the LCALL replica and the calling bridge (P50/P53); the `L2`
rewrites ir 8 names and does not perform — `M32[M32[a]] → M32[R[a]]`
(§5.10.8), the static-link reintroduction for the 41 uplevel sites, the
slotpatch for `<ENTRY>.ret` (§5.10.1d), and the push-and-prologue machinery
a game→game call replaces with `a` cells (§5.10.6); a pointer of more than
one level, if the program ever needs one (it does not: §5.10.1a);
initialisation of anything but `char <n>` (§5.10.1b).

ir 7 reservations (P46), still open: a PLACEMENT INPUT (a `v` → 0x74
address, the oracle of DESIGN §5.1) — when it exists, the loader REFUSES two
`v`s placed at one address unless a liveness proof accompanies it (DESIGN
§5.3; that proof is a transformation's, not the loader's); a symbolic
`<ENTRY>.b<k>` for a block that IS in the book (placement of blocks,
0x77 → 0x70, the block-merge/split transformations of DESIGN §6). CLOSED by
ir 8: `call`/`rt_call` inside symbolic blocks (§5.10.6); `@addr` inside one
stays refused and is not a reservation but a category error.

`save`; M1 (bit addressing, IQ3 — when it lands, bit pointers get the
function-style literal `bitp(w, n)` (n = 0..31), matching the wp/bp
precedent; the colon form `0xW:b` is byte-select FOREVER and is not to
be overloaded — user ruling, Aug 29 2026); stack-register WRITES (wfp
wsp wsb wsl — grammatically registers, refused until the RT-call
decoration project); a pure (flag-free) arithmetic shift primary
(none needed by the census; `ash` is root-effectful). Byte addressing
LANDED in P25 (the parked formula was wrong for L-forms — Project25/
ByteEA.md §2, METHOD §11). The `#` family, t-places, conditional
exits and borrow→t-place conversion LANDED in P26 (`#*`/`#/` retired
unimplemented with the family — `mul`/`div` own the flag semantics).
Flag-conversion (add→+ where flags are provably dead) is parked with
direction ruled: MathDesign §5.

## 9. Version history

ir 8 (Project 52, Sep 12 2026 — docs/Project52/{PROMPT,q001-plan-gate,
a001-plan-gate,REPORT}.md; DESIGN §4.1c, which had argued the other way and
was superseded the same day). **THE VARIABLE FORM**: a `<cell>` name denotes
the cell's CONTENTS, at the width and signedness of its DECLARATION, and is
both a primary and an lvalue; the address is `wp`/`bp`, which resolve by
operand kind and therefore make an `&` operator unnecessary (§5.2, §5.10.4).
**POINTER VTYPES** `*i16 *u16 *i32 *u32 *char *varying n *words n`, one
level only, two words each, KIND enforced and pointee width advisory
(§5.10.1a); `*varying`/`*words` were added at the gate because 28 of the 55
non-argument `R[]` sites hold the word address of a CHAR VARYING and the
§5.8 tripwire closes every workaround. **ARGUMENT CELLS** `a <ENTRY>.a<N>`,
`a <ENTRY>.arg_count u16` and `a <ENTRY>.ret <vtype>` — cells like `v`s,
same placement cursor, same tripwire — which make a routine's signature
DECLARED so a call's arity and pointer kinds are checked against the callee
instead of against a pushmap entry a compiled routine does not have
(§5.10.1c/.1d). **INITIALISED `v`s** (`v <E>.v<k> char <n> = "text"`, bytes
written by the loader at placement, §5.10.1b), because a compiled routine's
string literal exists nowhere and §5.8's literal piece names an address in
the IMAGE — the gate's blocking finding, and the reason 0x76 is no longer
uninitialised. **`trunc8`**, the store-intent twin of `zx8` — a name, not
new expressiveness; the prompt's claim that the mask set had no byte
truncate was wrong (`zx8` is `& 0xFF`, §5.3). **`call`/`rt_call` LEGAL IN A
SYMBOLIC BLOCK** with a symbolic `ret=` label, lifting P46's F7 scope
boundary, with TWO argument mechanisms stated rather than conflated —
`a` cells for game→game, the real stack for `rt_call` — and the
valued-call split recorded as a subset rule (§5.10.6, §6). The `rt_call`
callee rule widened to admit `X.CB` and the other register-convention
runtime helpers (§6). The `M32[M32[a]] → M32[R[a]]` REWRITE and its
precondition, with the bit-31 guarantee derived from `wp`/`bp` rather than
assumed, and the literal refusal SCOPED to address positions — unscoped it
would have refused the book 660 times (§5.10.8). Both censuses of record are
in §5.10.8: 1,035 `R[]` occurrences on 1,032 lines (939 own-frame arguments,
41 uplevel, 27 statics, 28 positive frame offsets written 42 times), and the
correction that **only one optional-argument mechanism exists** —
REFRESH_SCREEN's `arg_count` read; RETURN_MESSAGE's null test is a
null-pointer test on a supplied argument, so Salvage F5 is corrected. The
strict surface is untouched: the book names no `v`, no `a` and no symbolic
block, and §5.10.9's compatibility rule accepts an `ir 7` header exactly
when the file declares neither, so `quest.ir2.{book,stock}` load unchanged
and NO ARTIFACT WAS REGENERATED. C-side rename `SUB` → `RANGE_CHECK`
(`game/quest_rt.h` and `game/routines/*.c`; the IR does not name it — it
lowers to `assert(e)`).

ir 7 (Project 46, Sep 12 2026 — docs/Project44/DESIGN.md §4–§6,
docs/Project46/{PROMPT,q001-plan-gate,a001-plan-gate,REPORT}.md). Two
stages, one version. Stage A: the arena twins respelled `t@<block>.<k>` →
`s@<block>.<k>` (§5.9), `quest.arena`, and the P33 per-site artifacts;
nothing else moves. Regenerated through the P33-B chain of record
(strhooks.py → arena.py → string_sites.py --p33 → lower.py), never by
editing artifacts; the diff against a `sed`-renamed scratch copy of the ir
6 artifacts is exactly the version line and the `strings33`/`arena`
provenance lines (their inputs changed); 236 tokens on 198 statements per
artifact (docs/Project46/evidence/stageA_rename.txt). Stages B–D: declared
storage `v <ENTRY>.v<k> <vtype>` (i16 u16 i32 u32, char n, varying n,
words n), a `v` name as an address constant (§5.1 primary), symbolic blocks
`block <ENTRY>.b<k>` with symbolic `goto` labels (forward references
legal), placement by the loader at 0x76 (sequential per entry) and 0x77
(`base + idx·0x10000 + k`), names from the addrbook's 130 entry lines in
file order, the width tripwire, the `blocks` provenance line conditional
on a numeric block or label, refusals of `@addr`/`call`/`rt_call` in
symbolic blocks and of literal 0x76/0x77 constants (§5.10). The strict
surface is untouched: the book names no `v` and no symbolic block, and a
renamed ir 6 program loads and runs identically (task 050, 16/16 legs,
0 div). The loader refuses `ir 6` (regenerate artifacts and binaries
together, as always). Self-test: tests/run_vform_selftest.sh.

ir 6 (Project 33-B, Sep 6 2026 — docs/Project33/REPORT-B.md): the arena twins
`t@<block>.<k>`, `claim`, `release` (§5.9); the loader refuses ir 5; the
`strings33` and `arena` provenance lines. `--strings-slice 7` is the artifact
of record (1,822 string statements; WCMV/WMSP/STASP embeds 0/0/0; embeds 557
book / 2,436 stock); slice 6 reproduces P32's artifacts except the header.

ir 5 (Project 31, Sep 6 2026 — docs/Project31/{Census,REPORT}.md,
docs/Project29/StringsDesign.md): located strings — `[@a, n]`, `[@a, n
varying]`, `[@a, varying]` (read), the literal `[@0xW:b, "text"]`; the
statements `<located> = <piece>`, `ac1 = cmp(<piece>, <piece>)`,
`words(@d, k) = words(@s, k)` executed on the P30 library
(hw/strings/EagleString) with the §3 residues; the `strings` provenance
line; lazy literal verification.  652 WCMV/WCMP/WBLM sites lowered (528
`v = 'lit'`, 74 `v = t` in every pad/truncate form incl. the 10 min-shape
joins, 7 `fixed = 'lit'`, 31 compares, 12 word fills); embeds 2,322 →
1,670 (book), 4,201 → 3,549 (stock); sync list unchanged.  Superset of
ir 4; the loader refuses `ir 4` files (regenerate artifacts and binaries
together, as always).

ir 5, P32 amendment (append chains, Sep 6 2026 —
docs/Project32/{Census,REPORT}.md): `<n>` widened to any pure expr (no
production added, so no version bump); per-operand expression-or-
register rendering with the continuation cursor `@ac2`; the `strings32`
provenance line; 944 more WCMV/WCMP sites lowered (first pieces,
continuations, copy-outs, bounded copies, SUBSTR pieces, CALLRESULT and
residue counts, the 9 P31-refused compares) — 1,593 string statements,
embeds 1,673 → 729 (book), 3,552 → 2,608 (stock).  Also the P31
correction of the same day (652 → 649: docs/Project31/Census.md §11).

ir 1 (Project 23 phase 1): @pc-prefixed statements, `embed` keyword,
`end` / `end fall`, per-statement pcs. Superseded; loaders refuse it.
ir 2 (Project 23 phase 2): addressless statements, @addr instructions only,
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

ir 4 (Project 28, Sep 5 2026 — docs/Project28/{Census,RTConventions,
REPORT}.md): `rt_call <callee>(e1,…,eN) site=<s>` TERMINATOR for the
987 game→runtime LCALL sites (right-to-left evaluation, real stack in
both modes, pushes only through `wide_push`, the LCALL run through the
normal instruction path as `call` does; loader/executor belief checks
against the site's words and the callee symbol); LNDO lowered as XNDO
with the L-form EA (pc+4 fall-through); the LDSP pair as range-assert
+ `goto` table with −1 entries labelled to the terminal DERR sink; the
67 Nova LOAD forms lowered pure with the high half as the emulator
leaves it (UNDEFINED per the manual — a don't-care, not a contract);
Nova `SKP` as `goto [pc+2] 0`. Superset of ir 3; the loader refuses
`ir 3` files (regenerate artifacts and binaries together, as always).
Embeds 6,258 → 2,322 (book), 8,137 → 4,201 (stock).

ir 3 (Project 26, Sep 5 2026 — docs/Project26/{MathDesign,Census,
REPORT}.md): `goto [labels] e` terminator (plain `goto L` kept as
parser sugar; the dump form is `goto [L] 0`); strict 0/1 booleans
(`tf`, `==`/`!=`, mandatory-suffix `<s <=s >s >=s <u <=u >u >=u`, eager
`&& || !`); word layer (`& | ^ ~`, `lsh`); pure `/s /u %s %u` with loud
faults; the effectful root-only family add/sub/mul/div/cvwn/ash/nadd/
nsub/nmul defined as the shared EagleInstruction helpers (div and cvwn
hoisted from EagleCompute's inline bodies); `#+ #- #* #/` RETIRED
(refused); `c`/`ovr` readable and root-assignable; stack-register
reads (`wfp wsp wsb wsl`, writes refused); `ind(e)`; t-places (block-
local, single-assignment, loader-checked); flat chains must be
class-homogeneous (loader-enforced, no precedence table). NOT a
superset of ir 2 (the `#` family and the plain-goto dump form are
gone); loaders refuse `ir 2` files — regenerate artifacts and
binaries together, as always.

ir 3 P27 note (Sep 5 2026, no grammar change): DERR clusters fold to
`assert(cond, "DERR nn @pc"); goto [K] 0` in the guard block (§4a);
interior blocks delisted via the shipped `quest.synclist.p27`; 2,271
of 2,273 DERR embeds gone (the 2 LDSP-fed sinks are P28); `assert` is
now emitted by lower.py (P25 made it hand-authorable only).
