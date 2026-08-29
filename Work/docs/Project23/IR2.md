> NOTE: superseded as a standalone reference by Work/docs/IR.md
> (consolidated spec). This file remains the session's ruling record.

# IR 2 — grammar revision 2. BINDING SPEC (user rulings, Aug 29 2026)

Supersedes the statement grammar of IRPhase1.md §6 + P23 REPORT §3.
Everything not changed here carries over from IR 1 (class cap, embed
semantics, provenance, refuse-on-anything, #-ops = shared helpers,
ovk/ovr check, executor segment wrap, R[] implying the indirect bit,
lower.py TOTALITY: any block it cannot express is OMITTED, absent =
emulated).

## 1. Line forms

A quest.ir file is: header lines, then blocks separated by BLANK LINES
(no `end` keyword — rhymes with quest.blocks), then a trailer.

    ir 2
    source  <path> sha256=<hex>          (dis; + blocks/pushmap/argmap)
    ...
    block <hex8> seg <hex>
    <lines...>
                                          <- blank line separates blocks
    blocks <count>                        <- trailer, validated at load

Block lines, distinguished by first token:

    @<hex8> <mnemonic...>      INSTRUCTION: literal machine instruction
                               at that address. Fetched/decoded/executed
                               from memory; the text is audit trail and
                               is NOT parsed for semantics. ONLY
                               instructions carry addresses (user
                               ruling: single-entry blocks need no
                               internal addresses).
    <lvalue> = <expr>          STATEMENT: IR 1 expression statement,
                               now ADDRESSLESS. One machine instruction
                               may lower to several statements (e.g.
                               WPSH): no address bookkeeping needed.
    save <hex>                 IR op: WSAVS/WSSVS frame open, operand =
                               frame size word.  [rev 2: emitted only
                               where lower.py chooses; initially may
                               remain @instructions]
    call <tgt> args=<n> marker=<hex> site=<hex8> ret=<hex8>
                               IR op: decorated LCALL or XCALL.
                               TERMINATOR. site names the wrapped call
                               instruction (executor runs it there — no
                               length knowledge anywhere); ret is a
                               validated belief only (ac3 comes from
                               the instruction; disagreement surfaces
                               at the next pair). [Amended this
                               session: site= replaced the ret-4
                               derivation, which baked in LCALL's
                               length and excluded XCALL for no
                               semantic reason.]
    ret                        IR op: WRTN. TERMINATOR. No operands —
                               target/psr/carry come from the frame.
    goto <hex8>                IR op: unconditional exit. TERMINATOR.
                               Covers (a) fall-through into the next
                               block (was `end fall`), (b) lowered
                               unconditional WBR. `; comment` keeps
                               provenance distinguishable.
    ; ...                      comment to end of line, anywhere.

## 2. Terminator rule (replaces `end` and its variants)

Every block's LAST line must be exactly one of: a control-transferring
@instruction (call/jump/return/skip class — executed, exit = its
new_pc), `call`, `ret`, or `goto`. Loader refuses otherwise. This is
also the practical truncation net; the trailer count closes the
boundary-shear residual.

## 3. Continuation tripwire (decoder-length, no +len annotation)

For every NON-final @instruction, executed new_pc must equal
addr + Disassembler::word_length(decoded format) — cross-validates the
execute path against the decode table at runtime (user ruling: length
comes from the decoder, not an annotation). Fault/OS edges
(new_pc outside [game_start, game_stop)) exit the block instead;
in-range mismatch THROWS. Requires the tinyImmediateWideIndirect
word_length fix (landed; the C++ mirror of the Java listing defect).

## 4. IR operation semantics — anchored to shared code

Like #-ops: `call`/`save`/`ret` execute through the same code paths the
emulated instruction would drive; declared operands are VALIDATED
BELIEFS, not inputs the executor trusts blindly.

- `call tgt args=n marker=m ret=r` (decorated LCALL sites only):
  - Loader cross-validation: tgt == dis operand target; r == pc after
    the LCALL (dis adjacency); (site pc, m) present as a `call` entry
    in the pushmap; n == count of that site's `push` entries.
  - Executor: performs the decorated LCALL protocol via the shared
    path (marker write to m, marker push — wsp MOVES for the marker,
    per EagleStack — args_written flag, ac3 = r, transfer to tgt),
    PLUS the copied-args accounting `stack_offset += 2*n` moved here
    from per-push note_arg_write (user design: statements are pure
    stores; the one call-shaped action lives at the call). The
    no-live-record assert moves here too.
  - Extraction task before implementation: read the decorated LCALL
    case fresh (EagleStack ~l.179) and the marker-push details; do NOT
    implement from this paragraph's summary.
- `ret`: WRTN frame-pop via shared path (psr restore incl. ovk, carry
  from bit 31 of the return slot, wsp/wfp restore). Exit = frame pc.
- `save x`: WSAVS with frame size x via shared path (frame push, ovk=1,
  stack-fault edge preserved -> out-of-range exit like any fault edge).
- `goto t`: pure exit; loader validates t is a listed block start.

Scope by decoration (user ruling, "no mixed metaphors"): only
pushmap-decorated LCALLs become `call`; undecorated remain
@instructions. A decorated site lowers its arg pushes to statements
only if EVERY decorated push at the site is expressible; else the
whole push sequence + LCALL stay @instructions (uniform accounting).

## 5. XPEF-family statements (rides on rev 2, user design)

Decorated XPEF/LPEF at a lowering site: `M32[<slot-const>] = <ea>` —
pure store, slot constant from the pushmap. XPEFB/LPEFB: byte-pointer
value `((base<<1) + disp) & 0x1FFFFFFF | (seg<<29)` — grammar gains
`<<` (pure op). The byte EA is a VALUE (not an M/R index): executor
segment wrap does NOT apply. LPEFB `@` (indirect byte form, e.g.
7015C2B4 @[0x60001998]): extract eagle_l_byte_indexed's indirect
handling before lowering; until then those sites stay embedded (which
per §4 keeps their whole site embedded).

## 6. Attribution downgrade (accepted cost)

Statements have no pc: the ovk/ovr throw and debug hooks attribute to
block + nearest preceding @instruction (or block entry). The overflow
path has never fired (METHOD §3); exception-string symmetry with the
master is only observable if it ever does — noted, accepted.
QUEST_IR_DEBUG_PC becomes QUEST_IR_DEBUG_BLOCK (block start, prints
per-statement ac state).

## 7. Gate

Whole-game emission (--all) on the repaired listings + split CFG;
local scripted battery (login + creation + turns), K=1, strict
surface; zero divergences; call/ret/goto and XPEF statements must
demonstrably execute (first-execution log + census). Report appends
to docs/Project23/REPORT.md.
