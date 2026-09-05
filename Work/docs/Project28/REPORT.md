# Project 28 — `rt_call`: the 987 runtime call sites decorated (ir 4); embeds 6,258 → 2,322

Session Sep 5 2026, solo implementation; plan gate (Census.md) and
landing reviewed by the user. TREE VINTAGE: main @ 9972b85 (P27 merged
bd3369c; artifacts verified against docs/Provenance.md). Branch:
p28-rt-call. Battery: task 041 (040's 15 legs + the P28 verdict lines,
via bin/task_source.sh) — queued; local gates in §5. Plan-gate record:
docs/Project28/Census.md; conventions: RTConventions.md; ledger:
rt_call.ledger (987 emitted, 0 refused).

## 1. Outcome

**987/987 game→runtime LCALL sites lowered** to the ir 4 terminator
`rt_call ?NAME(e1,…,eN) site=<pc>` in both modes — real stack, pushes
through `Machine::wide_push`, the LCALL run through the normal
instruction path exactly as `call` does; no EagleStack change. Plus
the three P26 leftovers: LNDO 7015C0C7 (as XNDO with the L-form EA),
the LDSP pair (range assert + `goto` table, −1 entries labelled to the
terminal DERR sinks, option A1), the 67 Nova LOAD forms (pure, high
half as the emulator leaves it), and the 8 `ADC c,c,SKP` unconditional
skips as `goto [pc+2] 0`. **Embeds 6,258 → 2,322 (book), 8,137 → 4,201
(stock)** — exactly the plan-gate prediction; the 2 remaining DERR
embeds are the LDSP sinks by ruling. Blocks 13,507 (unchanged), gotos
11,400 → 11,442, asserts 2,271 → 2,273, statements 40,382 → 40,587;
sync list unchanged (quest.synclist.p27). `--rt-slice 0` without
`--leftovers` reproduces the P27 artifacts byte-for-byte (header
aside) — the regression check.

## 2. Rulings taken (user, Sep 5)

Plan gate (Census.md §2): **F1** rt_call is a TERMINATOR mirroring
`call` (the CFG ends every site's block at the LCALL with a `c <callee>
n <site+4>` edge — the prompt's "statement, fall-through continues"
described a CFG that does not exist); **F2** argc ∈ the callee's known
set (RTConventions.md; ?WRITE_SCREEN {2,5}, ?READ {4,6,7}, ?WRITE
{3,6} read argc from the frame word); **F6** `machine.pc = site` before
the pushes, the master-vs-clone `pc=` text difference on a never-fired
stack fault recorded, not carried in the grammar; **F7** no EagleStack
hoist — run the LCALL at `site` through run_instr; **LDSP A1**
(verified terminal pair kept; variant B would need a terminal-assert
grammar and trades to the detach pairing); Nova per-shape pure lowering,
no `nova()` helper; prediction 2,322/4,201 as the landing bar.
Post-gate: the finding on Nova SS=1 bit 16 is a benign
undefined-high-half case (HWFindings_Sep5.md §7), not a bug.

## 3. Findings

- Every RT site is block-final (987/987), all 2,879 arguments are
  inline-able (0 t-places), 18 callees not ~15, WPSH 3 not 8, no byte
  pointers reach the runtime (Census.md §1–§2).
- LDSP table 70160191 has 43 of 65 entries = −1 — the prompt's plain
  `assert(range); goto` form could not express it (Census.md §4).
- 68 Nova loads in blocks.split; the 68th (7015BD6B SUB.ZR) is inside
  the permanently excluded block — hence P26's 67.
- NovaCompute.cpp SS=1 stores a 17-bit value (bit 16 = old bit 15) into
  the loaded ac; hardware: high half UNDEFINED; dead in QUEST (no L
  form is followed by a result-test skip; every live L-loaded register
  is overwritten or SEX'd before a wide read). Replicated for the strict
  surface; recorded HWFindings §7.
- Emitter mechanics surfaced by the slices: parse_blocks did not read
  the `c … n …` edge line, so call blocks had no successor list (fixed:
  the return pc IS the successor); the `ADC c,c,SKP` idiom has ONE
  successor (pc+2; the skipped word is dead, not a block start) so it
  cannot use `skip_exits` (fixed: canonical `goto [pc+2] 0`).
- Correction (METHOD §11): the Sep 5 flag that `machine.pc = site`
  makes the stack-fault text match the master's was wrong — the master
  names the push's pc. Recorded as a known difference (Census.md F6).
- Provenance note (METHOD §10): ~170 lines of rt_call scaffolding in
  lower.py (`rt_window`, `RT_ARGC`, `rt_slice_ok`, `parse_ldsp_tables`)
  were found uncommitted in the working tree after the plan-gate
  commit, unattributed — most likely this session's own budget-
  truncated work with the context compacted away. Reviewed line by
  line against tools/rt_sites.py and adopted; every line is owned.

## 4. Implementation

- **tools/lower.py** (ir 4): `--rt-slice {0,1,2,3}` (off / contiguous
  ?WRITE_SCREEN 723 / every contiguous 887 / all 987), `--leftovers`
  (LNDO, LDSP, Nova loads), `--rt-census` (per-site ledger).
  `rt_window` walks the pushes backward from the block-final LCALL,
  classifies interleaved XLEF/XWSTA, renders each argument with
  `pef_value` (the P25 grammar — nothing new), refuses per the list in
  IR.md §6 (argc set, cross-block, unrenderable, indirect+store,
  register read an interleaved XLEF writes, successors ≠ [site+4]).
  Push pcs fold into the terminator and are echoed after `<-` in its
  comment; interleaved instructions lower as the statements they are.
  LNDO shares the XNDO path (wide EA, next = pc+4 checked). LDSP reads
  the dis's table rendering (`LDSP_TABLES`), checks entry count and the
  successor set, emits assert + goto with −1 → sink. `nova_test` gains
  the load form (`c = <carry>; acY = <res>`), SKP → `goto [pc+2] 0`.
  `parse_blocks` reads `c <callee> n <ret>` edge lines.
- **hw/IRExec.cpp/.hpp** (ir 4): `Stmt::RT_CALL` with `argv`; parser
  (balanced-paren argument split, `?` callee, `site=` hex8, site after
  the block's last instruction, site+4 listed, args pure via the
  existing Parser, t-reads checked); executor evaluates args, sets
  `machine.pc = site`, verifies the site word is an LCALL (pattern
  `101ii11011001001`) with argc == N and the callee symbol == the
  LCALL's resolved target (loud throw otherwise), pushes eN…e1 via
  `wide_push`, then `return run_instr(site)`. Terminator rule and
  version header updated; ir 3 files refused.
- **docs/IR.md** → ir 4 (§1, §2, §3, §4, §5.1, §5.4, §5.6, §5.7 third
  block, §6 rt_call + LDSP, §9). **docs/HWFindings_Sep5.md** §7.
  **docs/Provenance.md** post-P28 table + regen command. Run.md note.
- Artifacts: quest.ir2.book 294f81d3…, quest.ir2.stock 1cf12f33…;
  docs/Project28/rt_call.ledger.
- tasks/041-p28-rt-call.sh: 040's 15 legs (source p28-rt-call) + verdict
  lines: embeds 2322/4201, rt_call 987/987, ledger 987/0, leftover
  embeds 0, rt_call coverage by callee with WRITE_SCREEN and
  RANDOM_NUMBER required live, leftover block coverage; the P27 block
  keeps derr/derr-emu and asserts now 2,273.

## 5. Validation (local, METHOD §15, this 1-core box; K=1 strict unless noted)

| leg | slice | cfg | pairs | IR blocks live | div | rt_call sites live |
|---|---|---|---:|---:|---:|---|
| k1fo | 1 (723 WRITE_SCREEN) | book | 311,664 | 1,349 | 0 | 50 WRITE_SCREEN |
| k1fo | 1 | stock | 302,904 | — | 0 | — |
| k1fo | 2 (887 contiguous) | book | 313,770 | — | 0 | — |
| k1fo | 2 | stock | 318,228 | 1,355 | 0 | 67 (WRITE_SCREEN 50, READ 1, OPEN_FILE 3, CLOSE_FILE 1, …) |
| k1fo | 3 (all 987) | book | 298,939 | 1,341 | 0 | — |
| k1fo | 3 | stock | 302,667 | 1,360 | 0 | — |
| k1fo | 3 + leftovers | book | 304,338 | 1,392 | 0 | 74 incl. 7 interleaved ?UNSIGNED_TO_CHAR |
| k1fo | 3 + leftovers | stock | 309,388 | 1,340 | 0 | — |
| k1play | 3 + leftovers | book | 308,732 | 1,550 | 0 (end I.STOP) | 90: WRITE_SCREEN 64, UNSIGNED_TO_CHAR 9, OPEN_SHARED_IO_FILE 3/3, GET_SHARED_PAGE 3/3, DELAY 1, CHAR_TO_UNSIGNED 1 |

Every leg ended clean/I.STOP with 0 divergences and no IR refusal or
belief-check throw. The interleaved-window sites (ac2 register argument
+ spills) are validated live (7–9 ?UNSIGNED_TO_CHAR sites, native body
reading `entry_ac(2)`). ?RANDOM_NUMBER was NOT reached by the K=1 play
leg — at K=1 the scripted drain window does not complete a turn (040's
K=50 play legs reach 6 RANDOM_NUMBER sites; the battery's play/play-st
legs at K=50 are where the verdict line requires it). Leftover blocks:
the SQR31 Nova loads (7015BD20, 7015BD3F) live in every leg; LNDO /
LDSP / the carry-consumer Nova blocks are turn-cadence paths (040
reached 70160E64… only in the K=50 play legs) — carried to the battery.
### 5a. K=50 play leg (040's configuration), book, ir 4 artifacts of record

**0 divergences over 5,229,383 pairs**, 1,987 IR blocks live, clean end.
rt_call sites live 86/987: **?RANDOM_NUMBER 6/111** (the ac0-result +
seed-write-back callee, verified strict at every K=50 pair), ?WRITE_SCREEN
57, ?UNSIGNED_TO_CHAR 7 (interleaved windows), ?OPEN_SHARED_IO_FILE 3/3
(the WPSH argument), ?GET_SHARED_PAGE 3/3, ?CREATE_TASK 1/1,
?AWAIT_CONSOLE_INTERRUPT 1/1, ?READ 1, ?CHAR_TO_UNSIGNED 1, ?OPEN_FILE 2.
The lowered Nova LOAD blocks 70160E64 / 70160E65 / 70160E73 / 70160E74
(`ADD.O 0,0,SBN` / `ADC.C 0,0,SNC` / `SUB.CL 0,0` — the carry-consumer
sites of CarryCensus.md) are LIVE — the load form, its `c =` write and
the SS=1 high-half replication all verified on the strict surface. No IR
assert, no belief-check throw, no refusal. LNDO 7015C0C5 and the LDSP
blocks were not reached (init-loop and menu paths outside the scripted
legs — carried on census classification, as P25's borrow blocks were).

### 5b. Negative tests (loader posture, METHOD §8)

Three hand-corrupted copies of quest.ir2.book: `site=7015C046` → loader
`REFUSE: rt_call return pc (site+4) is not a listed block start`;
callee `WRITE_SCREEN` (no `?`) → `REFUSE: rt_call callee must be a
runtime symbol`; one argument dropped from the 2-arg ?WRITE_SCREEN site
7015C047 → loads (argc is a runtime belief), and at the first execution
the executor's word check throws — LOCKSTEP DIVERGENCE at trap_pc
7015C047 on the clone at pair 41, master continuing into ?WRITE_SCREEN.
Loud at the site, never a silent value.

## 6. Tempting adjacencies NOT taken (boundary 1)

Game→game calls (P25's), WMSP/STASP dynamic allocation and the 3
pass-by-reference temps at 70169B77, string ops (WCMV), checker
changes, runtime translation, the t-place form of an rt_call argument
(legal, 0 sites, not emitted), an LDSP memory belief check at load,
delisting the LDSP sinks (variant B), carrying push pcs for the
stack-fault text.

## 7. TODO / next session

- Integrate after 041: merge p28-rt-call; Provenance is already
  written for the branch's artifacts.
- Remaining embeds 2,322 (book) = calls 1,313 (undecorated LJSR/XJSR/
  LCALL to game routines not in the pushmap? — census next) + string/
  WMSP/stack-write family 1,765 − … : a fresh census by mnemonic is the
  next project's step 1.
- F2-b (assert-detach + kind-2 terminal → TERMINAL-ABORT) still open
  from P27; the LDSP out-of-range assert is another customer.
