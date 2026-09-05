# quest.ir — THE IR SPECIFICATION (consolidated, standalone)

Version: **ir 3** (Project 26, Sep 5 2026 — the math & control grammar:
`goto [labels] e` terminator, strict booleans, s/u comparisons, word
layer, the effectful op family replacing `#+`/`#-`, t-places, stack-
register reads). This document is self-contained and normative; it
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

    ir 3
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
    goto [<hex8>, ...] <expr>      Exit. TERMINATOR (P26). The expr is
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
    goto <hex8>                    Parser SUGAR for `goto [<hex8>] 0`
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
  refused.
- Instruction addresses within a block strictly increase.
- TERMINATOR RULE: the last line of every block is an instruction,
  `call`, `ret`, or `goto`. (A final instruction's control transfer —
  branch, skip, return, call, fault — IS the exit.) A lowered skip is
  a `goto [fall, skip] test`; nothing follows a terminator.
- t-places (P26) are BLOCK-LOCAL, SINGLE-ASSIGNMENT: the loader refuses
  a read before the write and a second write in the same block
  (straight-line blocks make definite assignment exact).
- Anything unrecognized refuses. No silent skips, ever — including in
  the emitter's own input parsers.
- SYNC LIST (P27, ir 3 note — no grammar change): the loader validates
  block starts AND goto labels against the SHIPPED sync list
  (QUEST_SYNC_LIST), not against quest.blocks. A translation that
  removes blocks ships a list without them (BlockSyncDesign.md rules
  1–2), and any IR line naming a delisted pc refuses at load.

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
list `c_src/quest.synclist.p27` (identity minus interiors of the
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
               (wfp wsp wsb wsl are grammatically registers but WRITES are
               RESERVED — the loader refuses them until the RT-call
               decoration project owns the wsp/wsl overflow gate; P26
               emits stack-register READS only)
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
    primary := acN | tN (N = 1..255) | c | ovr | wfp | wsp | wsb | wsl |
               constant (0x… or signed decimal) | byte-pointer literal
               0xW:b (b in {0,1}) | M8[e] | M16[e] | M32[e] | R[e] |
               ind(e) | wp(e, e) | bp(e, e) | lsh(e, amount) | tf(e) |
               sx16(e) | zx16(e) | zx8(e) | trunc16(e) | ( e )
    effop   := add(a, b) | sub(a, b) | mul(a, b) | div(a, b) | cvwn(a) |
               ash(a, amount) | nadd(a, b) | nsub(a, b) | nmul(a, b)

    REFUSED AT LOAD: bare `< <= > >=`; bare `/ %`; the whole `#` family
    (`#+ #- #* #/`, retired in ir 3); C `<<`/`>>` at any tier (ruling
    R9: ash/lsh are the only shift vocabulary); functional
    and()/or()/xor()/com(); an effectful op anywhere but statement
    root; mixed-class or chained-comparison chains; t read-before-write
    or double write; stack-register writes; `goto [L] k` with k != 0;
    M1 (reserved). Anything else unrecognized: refuse.

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
  :512). Writes are reserved (refused) until the RT-call decoration
  project owns the wsp/wsl overflow gate.
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
(Census.md §2d). Everything else stays an instruction — in particular
the 67 Nova LOAD forms (deferred pending the user's manual check on
the high-half convention), indirect XJMP, LNDO (the dis omits its
register field), DIVX/WDIVS, DERR, the string/WMSP/stack-write family,
calls (undecorated), frames, floats. The cap widens by extraction,
never by assumption.

### 5.7 Worked example (an emitted block, ir 3)

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
- `ret` — executes WRTN's fixed opcode (0x87A9) through the normal
  decode path (WRTN is address-independent; the synthetic address is
  the block start, reaching only abort messages). Frame pop, psr/ovk
  restore, carry from bit 31 of the return slot — all shared code.
- `goto [L0..Lk] e` — pure exit: evaluate e (a strict index; out of
  range FAULTS), materialize registers, return L[e]. Lowered skips use
  [no-skip, skip] with the test yielding 1 for skip (the CFG lists the
  two successors ascending in that order); lowered XNDO/XWDO use
  [fall, loop-target]. `goto L` is sugar for `goto [L] 0`.

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
  dispatches IR; a non-lockstep run would silently ignore it).

## 8. Reserved / roadmap

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
