# Project 54 — REPORT: the calling bridge

Worker session, Sep 12 2026, branch `p54-bridge`. Gate `q001-plan-gate.md`,
ruling `a001-plan-gate.md` (all seven granted; M2's correction recorded).

**Outcome.** A call out of a symbolic block EXECUTES and returns into a 0x77
block, both ways: `rt_call` with its arguments on the real stack (native and
emulated callees), and the naive game→game `call` with its arguments in the
callee's `a` cells. **PICK_X_Y runs end to end** from a REPOSITION-shaped
caller, hand-written in ir 8 (P53's emitter was not ready), and the caller's
cells, the seed word and the draw count agree with a host oracle on three
seeds; both identity-red legs fail the comparison as they must. The
broken-bridge build is RED on the stack-balance check. `bridge_selftest`
GREEN **86 cases**; `vform_selftest` GREEN **133 cases** (129 → 133: leg 3b
flipped from asserting the refusal to asserting execution, a001 R5). Both
artifacts load unchanged; one local `m` leg is 0-div. **The 16-leg battery is
outstanding** (a001 R6: runner not deployed, task 054 in flight).

---

## 1. The calling mechanism, as it actually is

Collected here per a001's request, because this is a better account than
the design had and P50/P55 need it in one place.

1. **A call is a terminator; the continuation is a block.** Held (IR.md
   §5.10.6; `IRExec.cpp` interior-call asserts). The bridge fills `ac3` with
   the continuation's placed 0x77 address and nothing else about control flow
   changes.

2. **The "LCALL replica" was half of LCALL.** `RTStubs::inject_fire`
   replicates the caller's side — marker `(psr<<16)|argc`, `ac3`, `ovr = 0`,
   the shadow push — and not the callee's WSAVS. That is fine for a runtime
   callee, which has its own WSAVS (emulated) or writes its frame as residue
   (native). **A symbolic callee has no WSAVS word, and its `ret` is WRTN**,
   which pops a six-wide frame from `wfp`. So the game→game bridge does both
   halves: the LCALL replica, then `IRExec::enter_frame` — the WSAVS image
   (ac0, ac1, ac2, wfp, ac3|c), `ac3 = wfp = wsp`, frame size 0, `ovk` from
   the addrbook variant, `call_stack->augment`. `enter_frame` is a named
   function because it is also the prologue P50's Eagle→compiled entry will
   need.

3. **Returning into 0x77 works by construction, with two qualifiers.**
   `Machine::run` asks `IRExec::has(pc)` before any fetch — but only on the
   CLONE, and a NATIVE return sets `native_break`, which ENDS THE BATCH at
   the 0x77 continuation; the block runs on the next `run_steps`. In the
   self-test that is a `drive()` loop. Under lockstep it is a rendezvous at a
   pc the master never reaches — P50's, recorded, not absorbed.

4. **`native_return` honours whatever `ac3` held at entry.** Both native
   flavours (`RTBridge`, `EagleIntegration`) return to the captured `ac3`, so
   the bridge does nothing at return for a runtime callee.

5. **`?RANDOM_NUMBER` is a logging stub, not a native.** It is in
   `RT_STUB_LIST` and absent from `translation_table`, so in the game an
   `rt_call ?RANDOM_NUMBER` goes stub → emulated 7017DE33 → its own WSAVS →
   D.MOD → `XWSTA 0,[ac3+0x7FF8]` (wp(ac3,−8): **Salvage F4's slotpatch**) →
   WRTN into the 0x77 label. "A runtime routine returns in ac0" IS the F4
   slotpatch restored by WRTN. `emu_rt::random_number` is dead code (not in
   any table, references an undeclared `machine.native_context`, not linked).

6. **The valued game call does not slotpatch at L1.** WRTN restores the
   CALLER's ac0 from the replica image; the value travels in `<CALLEE>.ret`,
   written by the callee's own statement. Leg 6 asserts both halves: the
   caller reads 42 from the cell, and ac0 after the call is the caller's
   0xC0C0. The image's ac0 slot (W+4 = wp(ac3,−8)) is exactly where an L2
   rewrite would land the slotpatch.

**The stack at transfer**, W = the caller's wsp before the call:

    rt_call, N args                   game->game, n args in cells
    W+2     eN     <- arg_pointer(N)  W+2    (psr<<16)|0    marker, argc 0 (R2)
    ...                                W+4    ac0            <- wp(ac3,-8)
    W+2N    e1     <- arg_pointer(1)  W+6    ac1
    W+2N+2  (psr<<16)|N  = wsp        W+8    ac2
    ac3 = 0x77<ret>, ovr = 0           W+10   wfp
    (callee's WSAVS / residue above)   W+12   0x77<ret>|c<<31 = wfp = ac3 = wsp

---

## 2. Both argument mechanisms, as built (`hw/IRExec.cpp`)

**`rt_call <callee>(e1…eN) ret=<E>.b<k>`** — evaluate the arguments (pure),
materialise, `pc = the block's 0x77 address` (so `wide_push` folds seg 7 and
a stack fault names the block), resolve `entry =
symbols->address_for_name(callee)` (throw by name with no table or no
symbol; `ILLEGAL CALL` outside seg 7), push eN first … e1 last, push the
psr-saving marker, `ac3 = ret`, `ovr = 0`, shadow push, then the dispatch
tail duplicated from `EagleStack.cpp:286–306` (a001 R4): native lookup,
nested-in-fallback guard, `defer_dispatch` → `pending_native`, else
`native(machine)`, else return `entry` to emulate. The registry is
`machine.process->native_registry`, or `IRExec::rt_registry_override` when
set (a001 R3; null in the emulator).

**`call <ENTRY> args=<n> ret=<E>.b<k>`** — the caller's preceding statements
wrote `<CALLEE>.a1…aN` and `.arg_count`; the bridge does not touch them.
Materialise, marker `(psr<<16)|0`, `ac3 = ret`, `ovr = 0`, shadow push,
`enter_frame(machine, callee_wsavr)`, return `addr(<CALLEE>.b0)`.

**Loader additions.** `load_entries` keeps the addrbook's variant column
(2 of 102 live entries are WSAVR); a naive call REFUSES when the callee has
no `b0` in the file (a001 R1) — which also refuses calling an un-compiled
Eagle routine from a symbolic block, P50's; a post-pass binds each naive
call's target block and variant once all headers are known.

**For IR.md §6 (a001 R1 asked for the wording; P52's file, so it is
yours):**

> A naive `call <ENTRY>` transfers to `<ENTRY>.b0`, which must be a block of
> the same file; the loader refuses otherwise. Calling an un-compiled Eagle
> routine from a symbolic block is not this form (it needs the real-stack
> protocol and the M4a area writes — P50). The bridge pushes the marker with
> argc 0 — nothing is on the stack, `<CALLEE>.arg_count` carries the arity —
> and the WSAVS image (frame size 0, `ovk` from the callee's addrbook
> variant), so the callee's `ret` is the ordinary WRTN (P54).

---

## 3. The self-test (`tests/bridge_selftest.cpp`, `run_bridge_selftest.sh`)

| leg | what it proves | key assertions |
|---|---|---|
| 1 teeth (load) | R1 | a call to an entry with no `b0` refuses; so does `call PICK_X_Y` with no blocks |
| 2 teeth (run) | resolution | no symbol table / unknown symbol → throw naming the callee |
| 3 `rt_call` → native | the real-stack protocol | `arg_pointer(1) == e1`, `(2) == e2`, `(3)` a 0x76 cell; write-back through arg 3; 42 in ac0 in the continuation; ac1/ac2/c preserved; `ac3 == wfp` |
| 4 `rt_call` → emulated | WRTN into 0x77 | a hand-assembled leaf `WSAVS 0; NLDAI 77,0; XWSTA 0,[ac3+0x7FF8]; WRTN` (words from the Decoder patterns): ac0 = 77 via the slotpatch, callee-pops argc 2 |
| 5 game→game void | cells + `enter_frame` | −5 and 9 written through the callers' pointers; caller's ac0 restored; `ovk` 1 inside the WSAVS callee and 0 inside the WSAVR callee (an `?OVK_PROBE` native reads it); caller's psr restored |
| 6 game→game valued | `.ret`, no slotpatch | 42 from `BETA.ret`; ac0 after = the caller's |
| 7 nested | depth 2 | GAMMA → ALPHA → `?TEST_ADD` → back → `ret` → GAMMA: 42+99 in the outer cell, stack and shadow balanced |
| 8 PICK_X_Y | end to end | §4 |
| 9 script | teeth | `-DP54_BROKEN_BRIDGE` (marker argc = n) → RED, 15 stack-balance failures |

**The stack-balance check is at the CONTINUATION** — `<entry>.v20 = wsp`
before the call, `.v21 = wsp` in the `ret=` block, compared. The first
version compared the END-of-program wsp and was blind: WRTN begins with
`wsp = wfp`, so the top-level `ret` erases any drift. **The broken-bridge
build was GREEN and said so** — the seventh recorded instance of a check
that compares nothing, and the first caught by the teeth build in the same
session it was written. Kept, and noted in the test.

The rig gives the Machine a `SymbolTable` (`CallStack::call` dereferences
it; the vform rig now does the same) and a `NativeRegistry` through the R3
seam; `drive()` re-enters `run_steps` after every `native_break`.

---

## 4. PICK_X_Y end to end, including the seed

Hand-written ir 8 from `game/routines/PICK_X_Y.c`: three
`rt_call ?RANDOM_NUMBER(wp(v1,0), wp(v2,0), wp(ac2,0x28))` with 32-bit temps
(RTConventions: FIXED BIN(31)), `RANGE_CHECK` as an `assert`, the two
`M16[a1]`/`M16[a2]` write-backs, the four-term gate as a skip chain of
blocks, the retry loop. The caller is REPOSITION-shaped: two `i16` locals,
`a1 = wp(v0,0)`, `a2 = wp(v1,0)`, `arg_count = 2`, `call PICK_X_Y args=2`.
The rig maps SD/OBJ scratch with a three-region table (unused / wrong class
/ good, near two rectangle edges so ±20 draws fall outside about half the
time) and a test-native `?RANDOM_NUMBER` whose LCG is
`rt/random_number.cpp:9–19` verbatim (a001 R7). The oracle is the same C on
the host. **Compared: the CALLER's x and y, the seed word, the draw count.**

| seed | oracle x, y | draws | bails unused/class/rect | green | red (a) extra draw after the write-backs | red (b) `*x` off by one |
|---|---|---|---|---|---|---|
| 12345 | 3BFD, 3FC9 | 20 | 4/1/4 | agree | FAIL: seed, count (x, y right) | FAIL: x |
| 7 | 3BFA, 3FC4 | 16 | 4/3/2 | agree | FAIL: seed, count | FAIL: x |
| 60000001 | 3C0B, 3FD1 | 4 | 0/1/0 | agree | FAIL: seed, count | FAIL: x |

Red (a) is the prompt's case exactly: right coordinates, wrong seed. Red (b)
fails on x on all three (the gate happened not to flip; either way it
fails).

**A finding about the seed comparison itself (measured, not asserted).**
The first red (a) was an extra DISCARDED draw at the loop head, and on all
three seeds it was **invisible**: same x, same y, same seed, same count. The
reason is structural, not luck. The seed after N draws depends only on N.
The perturbed loop is the original delayed by one skip, and the two walks
COALESCE on the seed sequence as soon as the original bails once at the
skipped position (two of the three region values bail); once coalesced they
accept the same point at the same position, so the count — and therefore
the seed, and the coordinates, which are always the last two draws —
coincide. **Seed + count comparison catches a lost or duplicated draw only
when the walks fail to coalesce.** That is the P50 Q3 argument in one
example: a merged or reordered test can return identical x, y AND seed, and
only a logged call sequence (callee, arguments, order) sees it. The case
stays in the self-test as a measured print, not an assertion.

---

## 5. Sizing: estimate vs actual

| | gate estimate | actual |
|---|---|---|
| `hw/IRExec.cpp` | ~150 | **+122 / −17** |
| `hw/IRExec.hpp` | ~12 | +19 / −1 |
| `tests/bridge_selftest.cpp` | ~600 | **742** |
| `run_bridge_selftest.sh` | ~35 | 26 |
| `tests/vform_selftest.cpp` | −15 / +25 | +51 / −17 |

Under on the bridge — the two arms are 45 and 25 lines as estimated, and the
loader work was smaller than predicted because the post-pass binds target
and variant in six lines. Over on the self-test, in the two places the gate
named (the hand-assembled leaf, the PICK_X_Y table and oracle) plus one it
did not: the fourth PICK_X_Y variant and the continuation-point balance
cells, both products of §3/§4's findings.

---

## 6. Strict surface and regression evidence

- `quest.ir2.book` and `quest.ir2.stock` **LOAD unchanged** through the P54
  loader: 13,507 blocks each, 0 cells, 0 symbolic blocks (a load probe with
  the LAUNCH environment: `quest.blocks.split`, `quest.synclist.split`,
  `quest.pushmap.M4`, `quest.arena`). No artifact touched.
- **One local `m` leg**, book config, K=50, `QUEST_STRINGS_CHECK=1`: **0
  divergences**, 6,244 pairs, endpoint `DETACHED at 7017FCE8 (I.STOP)` — the
  m leg's expected end — driver "all steps settled on the prompt", and **0**
  `0x76 space` mapping lines (the book declares no cells, so the loader maps
  nothing new). Run in this container, not on the runner.
- `run_helpers_selftest`, `run_strings_selftest`, `run_strhooks_selftest`:
  GREEN, teeth confirmed. `run_vform_selftest`: GREEN 133, broken-allocator
  build RED.
- **Not run, and said plainly: the 16-leg battery.** a001 R6: the runner is
  not deployed and task 054 was in flight; the integrator queues it once 054
  lands. What the local leg cannot show is the other fifteen configurations
  (stock, emu, inject, abort, K=1, derr, forced).

---

## 7. What did not survive contact

- **The end-of-program stack check compared nothing** (§3). Caught by the
  broken build before the first commit. The check now reads `wsp` at the
  continuation, where the drift is visible.
- **The in-loop discarded draw is invisible to the seed** (§4). The prompt's
  "compare the seed, not only the outputs" is necessary and not sufficient;
  red (a) moved to an extra draw after the write-backs, which the seed and
  the count both see, and the in-loop case is kept as a measurement.
- **`CallStack::call` dereferences the symbol table.** The vform rig passed
  `nullptr` and never called it (no call had ever executed); the flipped leg
  3b would have segfaulted. Both rigs now own a `SymbolTable`.
- **Everything in the gate's §1–§3 held as written**: M1, M4, the two
  qualifiers on M3, the half of M2, the two argument mechanisms, the stack
  pictures (leg 3's `arg_pointer` checks and leg 4's callee-pop are the
  measurements), and "the bridge does nothing at return" in all three cases.
- **Not measured, still:** whether any runtime LCALL site uses the
  `argc & 0x8000` marker form (gate C-9). The bridge emits the psr form.

---

## 8. Files

Branch `p54-bridge`: `emulation/hw/IRExec.{cpp,hpp}`,
`emulation/tests/{bridge_selftest.cpp,run_bridge_selftest.sh,vform_selftest.cpp}`,
`docs/Project54/{q001-plan-gate,REPORT}.md`. Not touched: `compiler/**`,
`game/**`, `docs/IR.md` (the §6 wording is in §2 above for the integrator),
`Disassembled/**`, every artifact, `tasks/`, `bin/`, `EagleStack.cpp`.
