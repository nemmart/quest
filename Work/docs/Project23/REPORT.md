# Project 23 — the IR: lower.py, quest.ir, IRExec. FINAL REPORT

Session: Aug 28–29 2026 (one long interactive session with the user).
Binding specs: IRPhase1.md (phase 1) and IR2.md (grammar rev 2, written
mid-session from user rulings). Status: LANDED through grammar rev 2,
whole-game emission, both configuration modes, all gates green.

## 0. Where this leaves the system

The clone can execute the game as an intermediate representation. In
the current book-mode artifact (`c_src/quest.ir2.book`): 17,983 of
18,009 blocks emitted (99.86%), containing 25,426 expression
statements, 31,145 embedded instructions, 1,039 arg-slot stores, 443
call operations, 165 rets, 3,242 gotos. Local batteries (full boot →
login → character creation → scripted turns, K=1, strict 030-shape
surface) run with zero divergences in both stock and book modes, with
~2,200 distinct blocks executing as IR per session. The master remains
a pure emulator of the 1986 binary; every IR construct is verified
against it at every block boundary.

## 1. What was built

- `c_src/tools/lower.py` — quest.dis + quest.blocks + pushmap + argmap
  → provenance-stamped quest.ir. Register-faithful, class-capped,
  TOTAL: any block it cannot express is OMITTED (absent = emulated =
  always safe). `--all` whole-game mode skips-with-census; `--book`
  selects decorated-site lowering (see §5). Refuses loudly on
  malformed inputs — including its own input files (a lesson, §7).
- `c_src/hw/IRExec.{hpp,cpp}` — loader (validation + provenance +
  refuse-on-anything) and block interpreter. Dispatch in
  Machine::run_steps: a block present in QUEST_IR runs as IR on the
  CLONE; the master always emulates.
- `c_src/tools/split_skips.py` — CFG rewrite splitting every skip and
  interior WBR into its own block (§3).
- Checker: P22's TEMPORARY insn-count delta term removed (the P23
  obligation); `ovr` added to the pair compare surface.
- C++ disassembler: tinyImmediateWideIndirect class split (§6).
- Docs: IRPhase1 rulings, IR2.md, WideCarry.md, this report.

## 2. P22 decisions overruled or redesigned (read this section first
   if you are the next session)

P22's IRDesign.md §8 and its artifacts were the starting point; the
session deliberately revised several of its decisions. For each: what
P22 did, what replaced it, and why.

a. **Interior skips (P22 CFG) → ALL skips split (user ruling).**
   P22's block builder kept the skip-over-DERR guard idiom interior
   (single successor; the shadow "never returns"), ~4,500 blocks.
   Overruled: every conditional-length instruction now terminates its
   block (split_skips.py; 13,494 → 18,009 blocks; synclist
   regenerated as identity). Rationale: interior skips make trailing
   statements conditional, poisoning the straight-line reasoning P24
   t-places need; and the accommodation IRExec initially carried for
   them ("forward-skip continuation") weakened the tripwire that
   later caught real listing defects. Future passes can convert the
   skip/DERR shapes to static asserts. The skip-class table is
   MECHANICALLY extracted from the emulator source (two regex nets
   over conditional returns) after a hand-built list missed WUSGE and
   cost a battery: hand lists lie, source extraction doesn't.
b. **Per-op c/ovr formula tables (IRPhase1 §3) → #-ops ARE the shared
   helpers (user ruling).** lower.py only classifies; IRExec calls the
   same EagleInstruction::add/sub the emulated instructions call.
   Trigger: the wide-carry finding (WideCarry.md) — the emulator's
   wide carry is `>>31` (result sign bit) where the DG manual says ALU
   carry-out. Baking formulas into quest.ir would have frozen the
   defect; sharing code means fixing the helper fixes the IR.
c. **"c and ovr compared at pairs" (IRPhase1 claim) → corrected.**
   Reality: only c was compared. ovr is now on the surface (it is the
   #-correctness check). All-emulated legs unaffected; recorded per
   METHOD §11.
d. **Insn-count delta term → removed** as designed (P22 marked it
   TEMPORARY). An IR clone does not execute the master's instruction
   stream; deltas remain in traces.
e. **METHOD §5's "WSUB x,x clears carry" → reclassified.** That
   sentence documents the OLD (buggy) wide-carry behavior, not
   hardware; it needs a correction note when the parked wide-carry
   task lands (WideCarry.md).
f. **IRPhase1 §6 statement grammar → IR grammar rev 2 (IR2.md),**
   redesigned live with the user (§4 below). IRPhase1's @pc-prefixed
   statements, `end`/`end fall`, and embed keyword are all gone.
g. **P22 quest.dis/quest.blocks → regenerated** after the LNADI/LNSBI
   listing defect (§6). All prior sessions read those files with 14
   phantom lines and two swallowed instructions in them.

## 3. The CFG rebuild and what its strictness caught

After the split, IRExec's embed contract became: a non-final
instruction must continue to exactly the next statement (fault/OS
edges — e.g. the program-entry WSAVS taking its stack-fault edge to
0x700001B8 — exit the block; anything else in game range THROWS).
That tripwire, within minutes of arming, surfaced in order: 27
merged-routine interior WBRs (splitter extended), the incomplete
hand-built skip table (WUSGE; mechanical extraction), and the
LNADI/LNSBI listing defect (§6). The final continuation check (rev 2)
uses the DECODER's word_length — execute path cross-validated against
the decode table at runtime, no annotations (user ruling: length
comes from the disassembler, not the IR).

## 4. IR grammar rev 2 (IR2.md is binding; the redesign in brief)

User rulings, in sequence: (1) statements should not carry addresses —
single-entry blocks need no internal identities; only LITERAL
INSTRUCTIONS are `@addr mnemonic` lines (this also dissolves the
multi-statement-per-instruction bookkeeping WPSH will need); (2) no
`+len` annotation — the decoder knows lengths; (3) `goto <pc>` as the
unconditional exit (covers fall-through and lowered WBR; `end fall`
gone); (4) no `end` keyword — blank-line separation like
quest.blocks, `blocks <count>` trailer as the truncation net; (5)
non-statement IR ops in the same keyword-led family: `call`, `ret`,
`goto` (`save` reserved; WSAVS reads its frame word from memory so it
genuinely needs an address — deferred); (6) `call` carries
`site=<pc>` naming the wrapped instruction — deriving the site as
ret−4 had silently baked in LCALL's length and excluded XCALL for no
semantic reason ("why does call need to know length?"); `ret=` is a
validated belief only. Operands are declarations cross-checked at
load (pushmap: marker + args-as-elided-WIDES) and at runtime (the
executed call's own callee-verification abort; ac3 vs ret at the next
pair).

Executor anchoring: `call` executes the actual decorated LCALL/XCALL
at site via the embed path (marker value/push, args_written, ovr
clear — byte-exact) plus the batched `note_arg_write(args)` replacing
per-push notes (user design: arg pushes are PURE stores
`M32[slot] = <ea>`; the one call-shaped action lives at the call).
`ret` executes WRTN's fixed opcode 0x87A9 through normal decode
(address-independent, verified). Attribution downgrade accepted: the
never-fired ovk/ovr throw names block, not statement (IR2 §6).

## 5. Mode discipline (found the hard way)

Decorated-site lowering encodes the M4 book world. The first book-IR
battery ran in a STOCK configuration (no QUEST_ADDRESS_BOOK /
QUEST_PUSH_MAP): the area pages weren't mapped and the argpush store
faulted — while the emulated clone at those sites would have pushed
stock. quest.ir now declares `mode stock|book`; the loader refuses
book-mode IR without the book envs. Stock-mode IR (decorated sites
left as instructions) is valid under BOTH configurations. All prior
IR-1 batteries were stock and correct-by-embedding.

Decorated call-site ledger (566 in quest.pushmap.M4): 443 lowered
(417 LCALL + 20 XCALL + 6 others of the LCALL family), 96 blocked on
B-form pushes (XPEFB/LPEFB byte pointers — extraction deferred, §8),
25 WPSH multi-wide, 2 borrow-adjacent. Unlowered sites stay embedded:
correct in both modes.

## 6. The disassembler defects (both toolchains fixed)

The strict CFG surfaced that the Java disassembler sized LNADI/LNSBI
at 2 words (shared "tinyImmediateWordIndirect" class with the X
forms) where the L-forms are 3 — behaviorally proven by the master
itself before any fix (LNSBI at 70178839 → +3, live). Effect: 14
phantom lines (linear-sweep resume inside immediates), one phantom
chain, TWO swallowed real instructions, one phantom-derived block
start. User fixed the Java class (split; LWADI/LWSBI confirmed absent
from both decoders — METHOD §3), regenerated all listings; the §14
diff-audit came back EXACTLY on-prediction: 26 deletions / 14
additions, nothing outside the predicted classes, real instructions
recovered at 7017530F (LWLDA) and 70177CEC (WBR −33), the contested
branch-target region resolved as phantom chain (no overlapping code).
The same defect existed in the C++ trace disassembler's word_length
and was fixed identically (new tinyImmediateWideIndirect = 3) — which
is also what makes the rev-2 continuation check trustworthy.

Remaining listing defect, FLAGGED not fixed: the @/bit-15 disagreement
(disassembler prints `@` from `offset > 0x8000`, strictly-greater) —
26 blocks skip-censused; same fix-and-regenerate treatment owed.

## 7. Defects in this project's own code, and their lessons

- **Exit materialization** (the TERRAIN ac2 divergence): locals never
  written back to machine.ac on fall-through exits; embed-terminated
  blocks were immune, which is why pilots ran clean. Lesson: every
  exit path materializes; trap-dump registers can show stale locals.
- **Embed exits may go anywhere** (calls to OS space broke the first
  strict rule; the split CFG restored strictness with the
  out-of-game-range fault-edge exception).
- **Pushmap parser skip**: an anchored regex silently dropped WPSH
  lines carrying a wides field; the loader counted them; the
  cross-check refused. Both parsers now DIE on unparseable lines.
  args= means elided WIDES (note_arg_write scales by it). `borrow`
  lines parsed and quarantined.
- Sandbox: (1) command timeout kills the process group — setsid +
  short poll commands; (2) `pkill -f <pat>` matches the invoking
  shell's own cmdline — use `[b]racket` patterns. Both produce stale
  artifacts that lie; verify freshness (mtimes, loaded-block counts)
  before believing any log (METHOD §10; two incidents avoided, one
  invalid result discarded mid-investigation).

## 8. TODO / next session

1. **Wide-carry re-verification (parked task, WideCarry.md).** A
   PRIOR session verified no wide-produced carry reaches control flow
   (dataflow census: carry-live-in ∩ wide producers) but that session
   went sideways after the check — REDO the check and record it
   before landing the parked patch. If confirmed empty, the
   correction shrinks to the helper fix + WADC evidence question +
   the lib_error staged-carry audit; worst plausible in-game impact
   is minor multiprecision math.
2. B-form byte-EA extraction (eagle_{x,l}_byte_indexed incl. the
   indirect LPEFB at 7015C2B4) → unlocks 96 call sites; grammar gains
   `<<`; byte pointers are VALUES (no executor wrap).
3. WPSH multi-wide arg sites (25): one instruction → several
   addressless stores (the grammar was shaped for exactly this) +
   wides accounting already in place. quest.wpsh_wpop has the data.
4. `save` op (needs an address story or reimplementation decision).
5. @/bit-15 listing fix + regeneration + diff-audit (26 blocks).
6. Borrows = P24 t-place pilot (user ruling): 23 WPSH/WPOP scratch-
   register brackets; do NOT build borrow/restore ops — in P24, spill
   to a fresh t-place and delete the plumbing. Pre-P24 census owed:
   crossings inside bracket interiors.
7. `end if` conditional exits: P24 boundary (skips meet expressions).
8. Housekeeping: CURRENT_STATE.md / NextSession.md not updated this
   session (user handled state docs separately); the runner-box /
   repo battery path was consciously NOT used — METHOD §15 local
   gates carried the whole session.

## 9. Gates run (all green, all local, K=1, strict surface)

IR-1 pilot (166 blocks; RANDOM/READ_IN/DIST/OWNS/TERRAIN): 101
executed, 0 div. IR-1 whole game (13,472 blocks, pre-split CFG):
1,887 executed, 0 div. Split CFG + repaired listings (17,986): 2,226
executed, 0 div. Rev-2 book (17,983): 2,201 executed — 33 call / 43
ret / 504 goto / 60 argpush statements live in the executed set — 0
div. Rev-2 stock: 2,187 executed, 0 div. Rev-2 site=/XCALL book:
2,180 executed, 0 div. Every battery: full boot + login + character
creation + scripted turns via tools/pilot_driver.py-style local
drivers on a scratch QUEST/ copy.

## 10. Reviewer notes (integration review, Aug 29 2026)

Independent review verdict: GREEN. Verified: the compare_pair verdict
(insn-count term gone, ovr present, c retained); 7015BD6B and the
ENQT/DEQUE skip edges as block boundaries in quest.blocks.split; the
shipped quest.ir2.book's provenance sha256s against the shipped
Disassembled/quest.dis and c_src/quest.blocks.split (exact); the
wide-carry patch NOT applied to the tree; and a fresh behavioral
spot-check — K=1 strict book-mode leg (boot → login → creation →
turns), 2,184 IR blocks executed, 0 divergences, only the benign
server-startup segment fault.

Three counting corrections to §0 (METHOD §11 — recorded, not edited
away): (1) embedded instructions in the shipped book artifact number
31,116, not 31,145; (2) "3,242 gotos" is the lowered-WBR count — total
`goto` lines are 3,895, of which 653 are fall-throughs; (3) the 26
omitted blocks are the 7015BD6B exclusion plus 25 @/bit-15-censused
blocks (§6's "26 blocks skip-censused" for @/bit-15 alone overcounts
by the exclusion). None affect any gate or conclusion.

Integration performed with this review: CURRENT_STATE.md P23 entry,
NextSession.md banner, Run.md (operative split blocks/synclist pair +
QUEST_IR env documentation).
