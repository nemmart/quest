# Project 23 — IR Phase 1: lower.py, quest.ir, IRExec. REPORT

Session: Aug 28-29 2026. Spec: IRPhase1.md (binding subset of P22's
IRDesign.md §8). Status: LANDED — pilot gate green, see §6.

## 1. What landed

- `c_src/tools/lower.py` — quest.dis + quest.blocks (+ pushmap, argmap)
  → provenance-stamped `quest.ir`. Register-faithful 1:1, class-capped;
  everything outside the cap is an embedded statement; TOTAL (an
  all-embed block is valid output). Refuses loudly on: excluded blocks
  (7015BD6B, ENQT/DEQUE text, XCT sites), dis/blocks text mismatch,
  non-monotonic pcs, pc-relative or L-absolute EAs outside the block
  segment, multi-successor fall-through, unparseable operands.
- `c_src/hw/IRExec.{hpp,cpp}` — loader + block interpreter. Loader
  refuses on: missing/mismatched blocks provenance (sha256 of the file
  QUEST_BLOCKS actually names), unlisted block starts, first-pc /
  monotonicity / seg violations, argstore lines (validation not yet
  implemented — refuse beats loading unvalidated), t-places, M1/M8,
  #*/#/, unknown directives, QUEST_IR without -lockstep. Dispatch:
  block present in quest.ir = CLONE executes it as IR; absent =
  emulated; the master always emulates (Machine.cpp run_steps, top of
  the fetch path so ordinal counting and every rendezvous see block
  exits exactly as instruction results).
- Checker (Lockstep.cpp): the P22 TEMPORARY insn-count delta term is
  REMOVED (the P23 obligation); deltas remain in traces and pair
  reports. `ovr` ADDED to the pair compare surface next to c — see §4.
- `EagleInstruction::add/sub` are now `static` (no behavior change) so
  IRExec calls them directly.

## 2. Semantics rulings (user, this session)

- **#-ops ARE the shared helpers.** `#+`/`#-` are DEFINED as calls to
  the same `EagleInstruction::add()/sub()` the emulated instructions
  use. quest.ir carries no formulas; when the helpers change, IR
  changes with them. (This supersedes IRPhase1.md §3's per-op formula
  tables; lower.py only CLASSIFIES.) Motivated by the wide-carry
  finding — see WideCarry.md: the emulator's wide carry is `>>31`
  (result sign bit), the DG manual says ALU carry-out; the fix is
  parked as its own task because it flips WSUB x,x / WADC x,x residue
  that Project 1/2 translations hard-derived.
- **OVK fault edge mirrored.** WSAVS bodies run with ovk=1 (i.e. all
  game code), so every #-statement ends with the loop's `ovk && ovr`
  check and the identical "Overflow occurred at %08X" string. The
  check has provably never fired in play (METHOD §3) and stays that
  way symmetrically.

## 3. Grammar amendments (vs IRPhase1.md §6)

Emerged from the pilot; all recorded here as the normative deltas:

a. **Executor-level segment wrap.** Every M/R index evaluates as
   `(e & 0x0FFFFFFF) | seg` with `seg` declared once in the block
   header (`block <pc> seg <hex>`). lower.py refuses any absolute or
   pc-folded EA outside the block's segment, making the uniform wrap
   provably identity-or-hardware-exact (hardware skips the wrap only
   for L-absolute and pc-relative, where it is then idempotent).
   Statements read `M32[ac3 + 2]`, not segment arithmetic.
b. **R implies the indirect bit.** `R[e]` = deref e, then follow bit
   31 (executor: `eagle_resolve_indirect(wrap(e) | 0x80000000)`), and
   an R RESULT used as a memory index is NOT re-wrapped (resolved
   chain pointers are full addresses). lower.py emits R only where the
   instruction's indirect bit is set.
c. **Fall-through blocks: `end fall <pc>`.** Blocks split by incoming
   edges end in an ordinary instruction with no terminator to embed;
   lower.py emits the block's single n-successor (refusing if not
   unique) and IRExec exits there after the last statement.
d. **Embed continuation semantics (final form).** An embedded
   instruction may continue to any LATER statement pc (interior guard
   skips: WSGTI/WSGT over a DERR shadow, kept in-block by the CFG when
   paths rejoin); ANY other pc — forward, backward, calls into OS
   space (the program-entry block's LJSR to 0x700001B8 killed the
   first stricter rule) — is a block exit returned to the loop.
   Dispatch fires only at block ENTRIES, so even a mid-block target
   simply emulates from there, and every exit is validated by the
   next K=1 pair against the master.

## 4. Checker surface correction

IRPhase1.md asserted c AND ovr were compared at pairs. Reality: only c
was (Lockstep.cpp regs_differ). ovr is now compared too — it is the
#-correctness check. All-emulated legs are unaffected (both engines
derive ovr identically); the pilot leg ran green with it armed.
Recorded as a correction per METHOD §11.

## 5. Defects found and fixed during the pilot

- **lower.py WLDAI/WADDI operand order.** Dis prints `WLDAI ac,value`
  and `WADDI ac,value (0xhex)` — ac FIRST, opposite of NLDAI. Both
  rules originally never matched (silent embed — safe, TOTAL held) and
  are fixed. WSBI's printed immediate is the EFFECTIVE value (a live
  `WSBI 4,0` proves nn+1, not the raw field). NOTE: no pilot block
  contains WADDI, so that rule is verified against EagleCompute source
  only, not by the gate.
- **run_block exit materialization (the TERRAIN ac2 divergence).**
  Locals were never written back to machine.ac on the fall-through
  exit, so expression statements after a block's last embed committed
  stale registers at the pair. Embed-terminated blocks were immune
  (the final embed materializes), which is why RANDOM/READ_IN ran
  clean and the fault first showed in TERRAIN's 7017C8AF. Fixed:
  materialize at the fall exit. Diagnosed by single-block isolation +
  statement bisection + an instrumented run showing the IR's own read
  returning the master's value while a stale one got committed.

## 6. Validation (METHOD §15 local minimal gate)

All legs: local scripted driver (`tools/pilot_driver.py`), -lockstep
-silent, QUEST_SYNC_K=1, strict 030-shape surface (registers incl. c
AND ovr, wsp shadow, fp state, pc, structure; insn-count term removed
by design). Scratch-copied QUEST/ per run.

- Wiring leg: READ_IN blocks (701766ED all-embed with an embedded LJSR
  L1→L2 crossing mid-barrier; 701766F6 with a live `#-` flag write) —
  IR-executed through login + character creation, 0 divergences.
- Full pilot leg (final, on shipping code): 166 blocks — RANDOM (5),
  READ_IN (2), DIST (2), OWNS-region (50), TERRAIN-region (107); 202
  expression statements, 278 embeds. Login + creation + scripted turns:
  101 blocks executed as IR, 0 divergences; routine census RANDOM x8,
  DIST x1918, OWNS x10, TERRAIN x8. Exercised: #+/#- under the strict
  c/ovr compare, mid-block embed barriers incl. LJSR crossings,
  interior skips, R[] indirect loads, fall-through exits, WSLE/WBR/WRTN
  terminators, syscall-adjacent embeds.
- ~65 pilot blocks did not execute this session (path-dependent
  alternates in the OWNS/TERRAIN regions) — unexercised, not failed;
  they load, validate, and would dispatch. Breadth belongs to the next
  project's baseline per METHOD §15.

## 6b. Whole-game leg (user direction, same session)

`lower.py --all` attempts every listed block, SKIPPING (censused) any
that refuses — omission is always safe under absent-=-emulated.
Result: 13472 of 13495 blocks emitted (25426 expr, 36031 embed);
23 skipped, all for "@-prefix vs indirect bit disagree" — the Java
disassembler's `offset > 0x8000` strictly-greater display quirk
(indirect flag with zero displacement prints the bit but no `@`);
flagged per METHOD §14 as a listing defect for follow-up, those
blocks stay emulated meanwhile. Battery: full boot + login + creation
+ scripted turns with all 13472 blocks armed, K=1, strict surface:
**1887 distinct blocks executed as IR, 0 divergences.** The one
failure en route was the embed backward-exit throw (§3d), fixed to
exit-return.

## 7. Deliverables & how to run

- `c_src/tools/lower.py`, `c_src/hw/IRExec.{hpp,cpp}`, dispatch in
  `Machine.cpp`, checker edits in `Lockstep.cpp`, `quest.ir.pilot`
  (the gate's exact IR), `tools/pilot_driver.py`.
- Run: emit with lower.py (see §6 pilot list or any listed blocks),
  then `QUEST_IR=<file>` alongside the usual QUEST_BLOCKS /
  QUEST_SYNC_LIST envs under -lockstep. `QUEST_IR_DEBUG_PC=<hex>`
  prints ac state at a statement (env-gated). "IRExec: first execution
  of block X" on stderr is the coverage evidence.

## 8. Open items → next session

- Wide-carry correction task (WideCarry.md): parked patch + residue
  re-derivation audit + WADC carry evidence question.
- argstore emission + loader/pushmap cross-validation (no pilot needed
  it; loader currently refuses the annotation).
- IQ3 (M1/M8 encodings) before lowering bit/byte ops; #*, #/ per
  session health; t-places are P24.
- Sandbox notes for future sessions: (1) the tool kills process
  groups on command timeout — launch background runs with `setsid`
  and poll in separate short commands; (2) `pkill -f <pattern>`
  matches the invoking shell's OWN command line and kills it mid-
  chain — use `pkill -f "[e]mulator ..."` bracket patterns. Both
  failure modes produce stale artifacts that lie (METHOD §10);
  verify freshness (mtime, loaded-block count, binary strings)
  before believing a log.
- Disassembler listing defect (METHOD §14): the @/bit15 disagreement
  on 23 blocks (see §6b) — fix the Java tool's `> 0x8000` to
  `>= 0x8000` (or print @ from the bit), regenerate, diff, re-emit.

## 9. IR grammar rev 2 — LANDED (same session, spec: IR2.md)

User-driven redesign: only literal instructions carry addresses
(`@addr mnemonic`); statements are addressless (multi-statement
instructions need no bookkeeping); blocks separated by blank lines
(no `end`); `blocks <count>` trailer; non-statement operations
`call`/`ret`/`goto` (`save` reserved, deferred: WSAVS reads its frame
word from memory). Continuation tripwire uses the DECODER's
word_length (no +len annotation) — required fixing the C++ mirror of
the Java tinyImmediateWordIndirect defect (new tinyImmediateWideIndirect
class, L-forms = 3 words; landed alongside the user's Java fix and the
regenerated listings, whose §14 diff-audit came back exactly
on-prediction: 14 phantoms gone, real instructions recovered at
7017530F and 70177CEC, contested 70177CED resolved as phantom chain).

Semantics: `call` = execute the actual decorated LCALL at ret-4 via
the embed path (byte-exact: marker value/push, callee-verification
abort, ovr clear) plus batched `note_arg_write(args)` where args =
elided WIDES (pushmap lines carry a wides field; both parsers now
refuse unparseable lines rather than skip — a silent-skip vs
loader-count mismatch cost one battery). `ret` = WRTN's fixed opcode
0x87A9 through normal decode (address-independent, verified).
`goto` = pure exit, target loader-validated. Arg pushes at lowerable
LCALL sites become plain `M32[slot] = <ea-value>` stores (user
design); WPSH/multi-wide, XCALL, borrow-decorated, and B-form sites
stay instructions this tranche.

MODE DISCIPLINE (found the hard way): decorated-site lowering encodes
M4-book-run semantics. In a stock run the area pages are not even
mapped (the argpush store faults) and the emulated clone pushes stock.
quest.ir now declares `mode stock|book`; the loader REFUSES book-mode
IR without QUEST_ADDRESS_BOOK+QUEST_PUSH_MAP. Stock-mode IR is valid
in both configurations (decorated sites embedded = config-faithful).
Memory faults in statements now carry [block, stmt index, store addr]
context. QUEST_IR_DEBUG_PC -> QUEST_IR_DEBUG_BLOCK.

Gate (both legs full login+creation+turns, K=1, strict surface, 0
divergences): BOOK leg quest.ir2.book (17983 blocks: 25426 expr,
31187 instr, 1017 argpush, 423 call, 165 ret, 3242 goto): 2201 blocks
executed as IR; executed blocks contained 33 call / 43 ret / 504 goto
/ 60 argpush statements — all four operations demonstrably live.
STOCK leg quest.ir2.stock: 2187 blocks executed, 0 divergences.

Open next: `save`; WPSH/XCALL/borrow/B-form sites; the 26 skipped
blocks (@/bit15 listing quirk — same fix-and-regenerate treatment);
`end if` conditional exits (P24 boundary); wide-carry task.

## 10. call site= amendment (user ruling: length doesn't belong to call)

The rev-2 `call` derived its instruction site as ret-4, silently
encoding LCALL's length and excluding XCALL. Amended: the site is a
declared operand (`site=<pc>`), ret= is a pure validated belief, the
executor runs the instruction at site with zero length knowledge, and
XCALL sites join the lowered set (443 calls, was 423; XCALL's dis
shape — leading fields, pc-relative target with folded absolute —
needed a more robust target parse). Ledger of the 566 decorated
sites: 443 lowered, 96 B-form-push blocked (incl. all 6 remaining
XCALLs), 25 WPSH multi-wide, 2 borrow-blocked. Book battery:
2180 blocks as IR, 0 divergences, full session.

## 11. Borrows disposition (user ruling): the P24 t-place pilot

The 23 WPSH/WPOP borrow brackets are the scratch-register idiom —
spill one register to get a temp, reload after. Ruling: do NOT build
borrow/restore IR ops or their cross-block stack_offset accounting;
that would be machinery for plumbing t-places are meant to delete.
Today the brackets (and the 2 decorated call blocks containing them)
stay embedded — the emulated hooks handle them. In P24 the bracket
becomes the temp-introduction warm-up: spill to a fresh t-place,
interior uses of the register split between the live-across original
value and the temp, and the store/slot/offset bookkeeping vanishes.
23 sites, single register, statically paired (quest.pushmap.borrows),
interiors CFG-clean after the skip split. Pre-P24 census owed: any
crossing inside a bracket interior (borrowed value in a book slot vs
a t-place is observable across a crossing).
