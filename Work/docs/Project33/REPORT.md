# Project 33-A — the checker half of the string design (dark), + F2-b + the ABORT readout

Session Sep 6 2026, solo implementation; plan gate and landing reviewed by
the user. **TREE VINTAGE: main @ 8738975** (= 64f0cc3, the P30 merge, +
the P33-A prompt); uploaded Work/Disassembled byte-identical to the repo;
Provenance prefixes verified (quest.dis 5c1db5fb…, blocks.split
1d3baaf6…, pushmap.M4 b8953659…, synclist.p27 af1be42f…, ir2.book
294f81d3…, ir2.stock 1cf12f33…). Branch: **p33a-checker-strings**. Gate:
task 045 (040's 15 legs + k1fo-off + forced, all self-tests as
preconditions) — queued; local legs in §5. Design of record:
docs/Project29/StringsDesign.md §6, docs/Mapper.md §1.4, this report's
§2 rulings.

## 1. Outcome

Everything StringsDesign §6 puts in the checker is built, wired and
proven with no clone-side string yet in existence — P33-B lands `p@b`
on a checker that already binds, unmaps, accounts and mismatches
correctly. Nothing is observable with the flag off; with it on and no
clone-side strings the strict surface is unchanged (0 div on every leg
run so far). Bundled: **F2-b** (a folded DERR is a verified terminal
pair again) and the **ABORT readout** (now prints `00000011 7015C48E`).

- **`emulation/quest.strhooks`** (`tools/strhooks.py`, from the P31
  regenerated census — vendored as docs/Project33/inputs/census_raw.p31.txt,
  sha 5bdedab0…, P31 branch 3a230e5 — + blocks.split + quest.dis):
  19 `row` lines (block = the block containing the first WMSP — 19
  distinct, the identity of `p@b`; PROVISIONAL arena layout: 4096 B
  capacity, rows 0x1000 words apart from 0x75000000), 57 `wmsp`, 19
  `stasp`, 1 `onpop` (the I.GOTO landing-stub STASP **7017EC9F**), 3
  `unwind` (WRTNs that are cuts: I.GOTO 7017EC9C / 7017ECBC, R?SIGNAL
  7017EF88). Provenance header with the three input shas.
- **`hw/strings/StrHooks.{hpp,cpp}`** — the loader (`QUEST_STRHOOKS`,
  required by `QUEST_STRINGS_CHECK=1`; INJECT discipline: malformed or
  missing → refuse to launch; refuses duplicate pcs, a block with ≠ 1
  first claim, a claim set ≠ nclaims, a row without a STASP, a duplicate
  block; at the first attach decodes the word at every hooked pc and
  refuses anything that is not WMSP/STASP/WRTN); the per-Machine
  `MachineHooks` (BOTH roles — ruling S1); the per-ordinal arena event
  queue (master queues bind/unmap, clone drains at the top of its next
  `run_steps`); the shutdown verdict lines.
- **Hooks in the instruction arms** (EagleStack.cpp WMSP / STASP / WRTN,
  one null-gated line each; frames.cpp's clone twins in `wrtn()` and
  `i_goto`). A WMSP embedded in an IR block (`IRExec::run_instr →
  ins->execute`) fires exactly like one the master fetched — the reason
  the hooks could not live in the dispatch loop.
- **compare_pair**: wsp term `master_wsp − Δ_m(master_wfp) ==
  (shadow_wsp + checkpoint) − Δ_c(clone_wfp)` behind the flag; AC
  compares unchanged (`equivalent()` already knows arena rows); the
  divergence dump names the arena row behind any arena-valued clone AC
  (`hits UNMAPPED arena row block=…` / mapped-but-wrong / no row) and
  prints both Δs. **F2-b**: `assert_detach` records a one-shot pending
  assert; `Lockstep::terminal_abort_pending` (factored for the test)
  consumes it at the top of the next compare_pair and, for a kind-2
  master half, `abort_world`s with `TERMINAL-ABORT at 7017ED1C … clone
  IR assert at 7015C48B (<report>)`. **Readout**: `w` / `w−2`.
- **Mapper**: `arena_bind`'s wfp check is "real-stack frame address (not
  0, not area, not arena)" — ruling S2. **ClaimDelta**: range and
  predicate frame exits. **RTStubs**: `QUEST_POKE=<pc>:<ac>:<v>[:CLONE|:MASTER]`.
  **Trace**: types `arena`, `strings` registered. **Makefile**: StrHooks.cpp.
- **Tests**: `tests/strhooks_selftest.cpp` + `run_strhooks_selftest.sh`
  — 64 cases GREEN through the REAL instruction arms (words written at
  the synthetic table's pcs, decoded and executed by EagleStack);
  `-DP33_BROKEN_DELTA_SIGN` build RED (5 failures: the STASP's Δ==0
  check catches the sign). P30's strings self-test still GREEN /
  broken RED with the rewritten wfp case.

## 2. Rulings taken (user, Sep 6, plan gate)

- **S1 — symmetric Δ.** 18 of 19 groups have listed block entries (the
  min-skip cut, the return block) and 11 the `?WRITE_SCREEN` RT-entry
  rendezvous between their claims and their STASP, so the prompt's
  "delta 0 at every rendezvous" was false: the master's wsp carries the
  claims at those pairs and, today, so does the clone's. Each Machine
  keeps its own ClaimDelta on its own wfp and the compare is on the
  claim-free wsps. Today `Δ_m ≡ Δ_c` (identical to the old check);
  after P33-B `Δ_c = 0` and it is §6.3's equation verbatim; a group
  P33-B refuses (still-claiming clone) stays exact. EagleStack.cpp added
  to the files for the three hook lines (the `zero_claim` precedent).
- **S2 — rows keyed on the MASTER's wfp, living in the CLONE's mapper.**
  All 12 claim routines are book-redirected (`dyn`), so the clone's wfp
  is an area address and P30's `wfp == owner_->wfp` assert would have
  aborted on the first bind. The hook asserts `wfp == master->wfp` by
  construction; the mapper checks "real-stack address". Events cross
  from master to clone by a per-ordinal queue (worker order: master
  half, clone half, compare — nothing between).
- **S3 — the `≥` rule** at every frame exit (WRTN, and the ON-pop
  STASP for the restored frame and above); the three unwind WRTNs are
  table rows; the clone's native `i_goto`/`wrtn` are the twins.
- Bind address **recorded, rebound at every WMSP** of the block (each
  claim's `wsp_before + 2` is the `LDASP r; WADI 2,r` value the code
  keeps; the last one is what the group pushes / copies from — 19/19
  read in quest.dis). No census-size arithmetic at runtime.
- The teeth: the STASP restores `sp@block` (the first claim's
  `wsp_before`); Δ == 0 after every STASP; Δ == 0 at every ORDINARY
  frame exit (refined in §3, item 3).
- Artifact: layout columns in the same file, provisional, header says
  so. Loader decodes at launch. F2-b ordering and the readout `w/w−2`
  as planned. Task 045 as planned.

## 3. Findings (design-vs-reality, all recorded here and in
## CheckerHistory / EmulatorDivergences as noted)

1. **The landing-stub STASP is 7017EC9F, not 7017EC9E.** The stub is
   `XWLDA 0,[ac3+2]` (two words, EC9D–EC9E) then `STASP 0` (EC9F,
   word A659). The plan said EC9E; the decode-at-attach check would
   have refused the table — it is the reason that check exists. Tool
   and table carry EC9F.
2. **The `≥` rule must be evaluated in MASTER coordinates.** First live
   run (k1fo book K=1, flag on): the clone's `?WRITE_SCREEN` return
   "discarded" DISPLAY_INVENTORY's live claims and a leaf routine's
   WRTN (SQR31?3, real-stack frame 700010A0) unmapped DISPLAY_SCREEN's
   row — every claim routine's clone frame is an AREA address
   (0x74…), numerically above every real-stack frame. The order is
   `Mapper::frame_precedes` (Ruling A's instrument), asked while both
   records still live, so the hooks fire BEFORE `area_wrtn_fixup` /
   `area_unwind_to`. On the master the mapper is empty and the order is
   numeric, as designed. `ClaimDelta::frame_exit_where` takes the
   predicate for the same reason.
3. **Δ == 0 at frame exit is asserted for ordinary WRTNs only.** An
   unwind cut (the three `unwind` rows, the ON-pop, the clone's native
   `i_goto`) legitimately discards outstanding claims — a signal raised
   from the `?WRITE_SCREEN` inside a group. Those are erased silently
   and counted (`discarded_claims` in the verdict line; 0 in every leg
   so far).
4. **R?SIGNAL/?ERROR 7017EF70–EF88 (`STAFP 2; WRTN` through a walked-to
   frame) is a frame-restoring path that is neither I.GOTO nor
   terminal**: it is reachable from `DEF?ON` (7017EF41), which the
   Aug 12 lift made ordinary verified L2 — the prompt's "DEF?ON paths
   are TERMINAL" premise is stale (TerminalDetach.md has said so since
   Aug 12). Its live shape (P5 RESUME) takes the plain-WRTN branch
   (EF8D); the STAFP branch has never been observed. It ends in a
   WRTN, so the `≥` rule covers it: table row `unwind 7017EF88`.
   Dismissed after reading: `T.INIT` STAFP (task creation), `I.SFALT`
   `STAFP 0` (→ ?FATAL, terminal), `O.ON` `LDASP 3; STAFP 3`
   (ascending — establishes, never pops), the three WPOPB sites
   (SWAT.REX / I.SFCON / I.FFALT pop only their own trap frame),
   lib_error.cpp's `replay_wrtn` (pops ?LIB_ERROR's own RT frames,
   which own no rows and hold no claims).
5. **The readout was one wide too deep in P27's note as well.**
   `wide_push` is `wsp += 2; write at wsp`; DERR pushes address then
   code; DERR.TRP's own reads are `XWLDA 3,[wsp'−2]` / `[wsp'−4]` after
   its `WPSH 3,3` — code at `w`, address at `w−2`. `w−2/w−4` (P27 §3)
   would have printed the address and whatever lay below it. Proven by
   the derr-emu leg: `top stack wides: 00000011 7015C48E`.
6. `max_delta` on k1fo is 1034 wides: DISPLAY_SCREEN's three groups run
   in a loop with `N[fp+238]`-sized claims — the loop-rebind edge
   (§6.5) is exercised on every login (rebind counts in the hundreds)
   and has not mismatched (the dead registers are overwritten before
   the next rendezvous, as the design predicted).

## 4. Implementation notes

- `Machine::strhooks` is a pointer, null unless armed; `StrHooks::attach`
  (top of `run_steps`) creates it, configures the arena rows once per
  machine, verifies the table once per process, and drains the clone's
  queue. `MachineHooks::master()` is `role == MASTER` — only the master
  emits events; roles exist only under `-lockstep`.
- The WMSP hook cross-checks the instruction's effect against the
  register (`wsp_after − wsp_before == 2·ac`) before `claim(wfp, ac)`
  — the design's "from the master's ac" and the emulator's own arithmetic
  must agree. Claims must arrive in order (n=1 binds, n>1 rebinds in the
  same frame); a STASP must see all `nclaims` and restore `sp@block`;
  a stray STASP for a block bound in another frame aborts. All aborts
  go through `abort_world(save=false)` under lockstep, throw otherwise
  (the test rig).
- The hook table validates pcs by DECODING (`Decoder::decode` on the
  word at the pc, `EagleStack::oper`), stronger than a listing check:
  the table and the binary cannot disagree.
- `QUEST_POKE` with `:CLONE` still arms both processes; the master's
  shot never fires (role test at the poke site). Unchanged otherwise.
- Files touched: hw/strings/{StrHooks.*, ClaimDelta.*}, hw/{Machine.*,
  Mapper.cpp, Lockstep.*, RTStubs.*, EagleStack.cpp}, runtime/frames.cpp,
  os/Trace.cpp (two type names), Makefile, tools/strhooks.py,
  quest.strhooks, tests/{strhooks_selftest.cpp, run_strhooks_selftest.sh,
  strings_selftest.cpp}, docs/Project33/, tasks/045. NOT touched:
  lower.py, IRExec, IR.md, string_sites.py, any instruction semantics.

## 5. Validation (local, 1-core container; METHOD §15 — ≈6 min of legs)

| leg | cfg | result |
|---|---|---|
| strhooks self-test | — | GREEN 64 cases; `-DP33_BROKEN_DELTA_SIGN` RED (5) |
| strings self-test (P30) | — | GREEN 18,371; broken RED 1,326 |
| k1fo, flag ON | book K=1 failopen | **0 div**, 302,845 pairs, clean; table verified (80 pcs); master bind 4 / rebind 324 / unmap 2 / claim 328 / release 109 / frame_exit 868, clone unmap 101; max_delta 1034; discarded 0 |
| k1fo, flag OFF | book K=1 failopen | **0 div**, 327,506 pairs, clean; no StrHooks line (dark) |
| derr, flag ON | book K=1, POKE 7015C48B:0:11 | clone `IR ASSERT FAILED [block 7015C48B stmt 0]`; **`TERMINAL-ABORT at 7017ED1C, verified on both engines: master at the kind-2 terminal, clone IR assert at 7015C48B (…)`**, WORLD-ABORT |
| derr-emu, flag ON | emu K=1, same poke | **`TERMINAL-ABORT at 7017ED1C … (top stack wides: 00000011 7015C48E — …)`** |
| forced | book K=1, POKE 7015C48B:2:0x75000000:CLONE | poke fired on the clone only; **LOCKSTEP DIVERGENCE** with `ac2: clone value 75000000 hits UNMAPPED arena row block=70166144 arena=75000000 — no master temp for p@70166144`, `delta_master=0 delta_clone=0` |

Task 045 (`tasks/045-p33a-checker-strings.sh`, committed on main so
the runner sees it): 040's 15 legs with the checker ON + k1fo-off +
forced; self-tests as preconditions; verdict lines = the StrHooks
counters per leg, table-verified count (want 16), k1fo-off dark (0
lines), derr F2-b line, derr-emu readout, forced row line. Bar: 17/17
green, 0 div on the 16 non-forced legs, forced = exactly 1 div naming
row 70166144.

## 6. Corrections recorded (METHOD §11)

- P27 REPORT §3's readout fix (`w−2/w−4`) — corrected to `w/w−2` (§3.5).
- The P33-A prompt's "DERR/DEF?ON/?FATAL paths are TERMINAL" — DEF?ON is
  verified L2 since the Aug 12 lift; R?SIGNAL's frame-restoring branch
  is reachable from it (§3.4).
- The P33-A prompt's 6(b) "delta 0 at every rendezvous" — false for
  18/19 groups (§2, S1).
- Plan-gate pc for the landing-stub STASP (EC9E) — EC9F (§3.1).

## 7. For P33-B

- The clone's `p@b = ""` needs no bind of its own: the row is bound,
  with the pushed address, before the clone's batch runs the block.
  `p@b = ""` is `arena_set_length(row, 0)` (bind already zeroed it);
  appends are `arena_set_length`. `release` = `assert(wsp == ac1)`; the
  clone must NOT run the STASP hook for a lowered group (it will not —
  the IR statement is not the instruction), and Δ_c stays 0 there.
- A group P33-B refuses stays an embedded `@WMSP … @STASP` and keeps
  claiming on the clone with the hooks firing — exact under symmetric Δ.
- Replace the `cap=` column from lowering; keep 0x1000-word spacing or
  re-lay the arena (the loader and `configure_arena` enforce I1).
- The verdict-line counters are the regression reference: bind/rebind/
  release counts must not change when a group is lowered (the master's
  side), unmap counts likewise; the clone's claim count must drop by
  the lowered groups' claims.


## Integrator note — task 045/045b (Sep 6)

045 exhausted three attempts on a link error in tests/run_helpers_selftest.sh
(it linked hw/*.o only; the hooks are referenced from hw/*.o) — fixed
on the branch (a05ad0f). 045b: all 32 legs OK, 0 div; table verified on
all 16 checker-ON legs; k1fo-off dark (0 StrHooks lines); F2-b
TERMINAL-ABORT with both pcs; readout `00000011 7015C48E`; forced poke
→ DIVERGENCE naming the unmapped row p@70166144; hooks fire on both
engines (max_delta 1034, discarded_claims 0). The single verdict FAIL
was `clone_assert=2 (want 1)`: the grep counted the TERMINAL-ABORT line,
which quotes the assert text; anchored in the task for the record.
Merged on the leg evidence.
