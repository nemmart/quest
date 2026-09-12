# Project 54 — q001: PLAN GATE (Part 1 report, Sep 12 2026)

No code and no existing doc changed. Everything below was measured on `main`
@ `fe6865a`. Rulings needed are marked **RULING**; corrections to the prompt's
claims are **C-n**; the four mechanism claims are **M1–M4**.

Reading order followed as given. `docs/attic/` was not opened. P53 owns
`compiler/` and I do not touch it; nothing in this plan needs it before
Stage B (§4), where the PICK_X_Y IR is hand-written and said so.

---

## 1. The four mechanism claims, verified against the source

| | claim | verdict | where |
|---|---|---|---|
| **M1** | `rt_call` (and `call`) is a TERMINATOR; the continuation is already a separate block | **HELD** | IR.md §3 (:118–130), §5.10.6 (:970–976); the executor asserts it — `IRExec.cpp:1823` (`interior call`), `:1846–1847` (`interior rt_call`); the loader's terminator rule `:620` |
| **M2** | an LCALL replica exists at `RTStubs.cpp:563` | **HELD, HALF** | the comment is `:563–566`, the code is `:591–594`: `wide_push((psr<<16)|argc)`, `ac[3] = pc`, `ovr = 0`, `call_stack->call(...)`. It replicates the CALLER's half of LCALL (`EagleStack.cpp:239–245, :273–274, :284`) and nothing else. **It does not replicate the callee's WSAVS** (`EagleStack.cpp:419–438`), and a symbolic callee has no WSAVS instruction — see C-1 |
| **M3** | returning into 0x77 works by construction: `Machine::run` asks `IRExec::has(pc)` before any memory access | **HELD, with two qualifiers** | `Machine.cpp:276–278` is the test, `:287` is the first fetch; `has()` is the whole dispatch (IR.md §5.10.7). Qualifier 1: gated on `lockstep_role == CLONE` (`:276`) — the rig sets it, the master never dispatches. Qualifier 2: a NATIVE return sets `native_break` (`RTBridge.cpp:171`) and `Machine::run` then **ends the batch** at the 0x77 continuation (`Machine.cpp:399–411`, returns `pc`); the block runs on the NEXT `run_steps`. Works, but the self-test must drive `run_steps` in a loop, and under lockstep it is a rendezvous at a pc the master does not have (P50's problem, noted in §6) |
| **M4** | `RTBridge.cpp:170` pops the shadow frame | **HELD** | `native_return()` `:163–173`: psr from the marker, `wsp -= 2+2*argc` (callee-pops), ac0..ac2 from `saved_ac`, `ac3 = wfp`, shadow pop `:170`, `native_break`, returns `return_addr` — which is **`ac[3]` captured at construction** (`:21`, `:101`). So a bridge that sets `ac3` to the 0x77 label needs to do nothing else for the native flavour. The OTHER native flavour, `EagleIntegration` (`EagleIntegration.cpp:8–18` pushes a real WSAVS image; `wrtn()` `:34–56` pops it and callee-pops via the marker) also returns whatever `ac3` held. Both honour the label |

**Two facts the claims did not state, both load-bearing:**

- **`?RANDOM_NUMBER` is a LOGGING STUB, not a native.** It is in `RT_STUB_LIST`
  (`RTStubs.cpp:190` → `log_and_continue`, `:696–699`, returns the entry) and
  is **absent from `translation_table`** (`:343–377`). So in the game an
  `rt_call ?RANDOM_NUMBER` out of a symbolic block goes: bridge → stub logs →
  **emulated body at 7017DE33** → its own WSAVS → D.MOD → `XWSTA 0,[ac3+0x7FF8]`
  (= wp(ac3,−8), the F4 slotpatch: `quest-rt.dis` 7017DE5B) → **WRTN** into
  ac3 = the 0x77 label. The "result in ac0" for a runtime routine IS the F4
  slotpatch restored by WRTN. `emu_rt::random_number` exists on disk but is
  dead: not in any translation table, references `machine.native_context`
  which `Machine.hpp` does not declare, and `emu_rt/` is not on the
  self-test link line (`run_vform_selftest.sh:17`).
- **The rig has no symbol table and no `OSProcess`** (`vform_selftest.cpp:183`:
  `Machine(nullptr, nullptr, nullptr, &memory)`). The `site=` form resolves
  the callee from the LCALL word and only cross-checks the symbol `if
  (machine.symbols)` (`IRExec.cpp:1884`). A symbolic `rt_call` has no word:
  the symbol table is REQUIRED at run time, and the native registry lives on
  `machine.process` (`OSProcess.hpp:48`), which needs a real `.PR` file
  (`OSProcess.cpp:43–98`). This forces a small test seam — R3.

---

## 2. The two argument mechanisms, as I will build them

### 2.1 `rt_call <callee>(e1…eN) ret=<E>.b<k>` — arguments on the REAL STACK

Executor arm, modelled line for line on the `site=` arm (`IRExec.cpp:1858–1896`)
and on LCALL (`EagleStack.cpp:238–306`), minus the belief check (no word to
check) and plus the dispatch tail LCALL does after the push:

1. evaluate e1…eN (pure), materialise ac0–3, `machine.pc = blk->start` (the
   0x77 address: `wide_push` folds `copy_segment(pc, wsp)` — seg 7 — and a
   stack fault names the block).
2. push eN first … e1 last through `Machine::wide_push` (`:1892–1893`,
   unchanged).
3. resolve `entry = machine.symbols->address_for_name(callee)`; throw by name
   if there is no table or no symbol. `get_segment(entry) != 7` → `ILLEGAL
   CALL` (`EagleStack.cpp:282–283`).
4. `wide_push((psr<<16) | N)`; `ac3 = addr(<E>.b<k>)`; `ovr = 0`;
   `call_stack->call(entry, ac3, blk->start, N)` — the M2 replica with the
   label in place of the injection pc.
5. dispatch tail, verbatim from `EagleStack.cpp:286–306`: `native =
   registry.lookup(entry)`; if native and `rt_pending_return != 0` → return
   `entry` (nested-in-fallback guard); if native and
   `RTStubs::defer_dispatch(entry)` → `pending_native = native`, return
   `entry` (crossings rendezvous); if native → `return native(machine)`;
   else return `entry` (emulate).

The marker is always the psr-saving form. LCALL's `argc & 0x8000` variant
(`EagleStack.cpp:241–244`) is not producible from the IR and I have not
censused whether any runtime site uses it (C-9).

**Stack at the moment of transfer** (W = caller's wsp before the call):

    W+2      eN          ← RTBridge::arg_pointer(N) = wsp-2N
    …
    W+2N     e1          ← arg_pointer(1) = wsp-2
    W+2N+2   (psr<<16)|N ← wsp; RTBridge reads it at wsp (:27, :104)
    ac3 = 0x77<ret>, ovr = 0; the callee's WSAVS (emulated) or
    emulate_frame residue (native, RTBridge.cpp:137–149) goes above.

### 2.2 `call <ENTRY> args=<n> ret=<E>.b<k>` — arguments in the callee's `a` CELLS

The caller wrote `<CALLEE>.a1…aN` and `.arg_count` as ordinary statements
(loader-checked, `IRExec.cpp:1346–1356`). The bridge does NOT touch them.
What it must do is what LCALL **and** WSAVS together do, because the callee's
`ret` is WRTN (IR.md §5.10.6 :995–996; `IRExec.cpp:1858–1875`) and WRTN pops a
six-wide frame from `wfp` (`EagleStack.cpp:472–499`). With nothing pushed,
`ret` would pop the caller's stack as a frame. So:

1. materialise; `machine.pc = blk->start`.
2. **LCALL replica:** `wide_push((psr<<16) | 0)` — argc **0**, see R2;
   `ac3 = addr(<E>.b<k>)`; `ovr = 0`;
   `call_stack->call(addr(<CALLEE>.b0), ac3, blk->start, 0)`.
3. **WSAVS replica** (`EagleStack.cpp:422–438`), factored as
   `IRExec::enter_frame(machine, variant)` so P50's Eagle→compiled entry
   can reuse it: push ac0, ac1, ac2, wfp, ac3|c<<31; `ac3 = wfp = wsp`;
   frame_size 0 (locals live at 0x76, nothing to claim, nothing to zero);
   `ovk = (variant == WSAVR) ? 0 : 1`; `call_stack->augment(wfp, 0)`. The
   variant comes from the addrbook's column 7 (2 of 102 live entries are
   WSAVR), which `load_entries` (`IRExec.cpp:162–187`) currently drops —
   ~6 lines to keep it.
4. return `addr(<CALLEE>.b0)` — R1.

**Stack at the moment of transfer:**

    W+2    (psr<<16)|0      the marker; WRTN's argc = 0, nothing to pop
    W+4    ac0              ← wp(ac3,-8): where an L2 slotpatch would land
    W+6    ac1              ← wp(ac3,-6)
    W+8    ac2              ← wp(ac3,-4)
    W+10   wfp              ← wp(ac3,-2)
    W+12   0x77<ret> | c<<31  = wfp = ac3 = wsp
    no claim above; the callee's cells are at 0x76.

This is byte-for-byte the shape `RTBridge::emulate_frame` writes
(`RTBridge.cpp:141–148`) and WRTN reads, so the caller's ac0..ac2, wfp, c
and psr come back exactly as after an original call.

---

## 3. What happens on RETURN

| call | value travels in | what the bridge does at return |
|---|---|---|
| `rt_call`, native callee | `set_return_ac(0, v)` (`RTBridge.cpp:133`) → `native_return` restores ac0 from it (`:167`) | **nothing** — `return_addr` is the label; the continuation opens with `x = ac0` (the valued-call split, already loader-enforced) |
| `rt_call`, emulated callee | F4 slotpatch into wp(ac3,−8) by the callee (`quest-rt.dis` 7017DE5B) → WRTN pops it into ac0 (`EagleStack.cpp:487`) | **nothing** — WRTN returns `ac3|c` = the label (`:483, :498`); callee-pops 2·argc (`:491–493`) |
| valued game→game | **`<CALLEE>.ret`**, written by the callee's own statement before `ret` (IR.md §5.10.1d :799–814) | **nothing** — WRTN restores the CALLER's ac0 from the replica image, so ac0 is NOT the result; the caller reads the cell. The slotpatch is an L2 rewrite by spec and the bridge does not fake one at L1. The frame's ac0 slot (W+4) is exactly where that rewrite would put it |
| void game→game | — | as above, minus the cell |

**How `ret` cells are filled:** by the callee, `<CALLEE>.ret = e` as an
ordinary variable-form store — a compiler (P53) emission, not a bridge act.
The bridge never reads or writes `.ret`, `.a<N>` or `.arg_count`; the loader
already checks they are declared and the arities agree.

---

## 4. The self-test plan

New `tests/bridge_selftest.cpp` + `tests/run_bridge_selftest.sh`, the
`vform_selftest.cpp` rig copied (`:179–205`) and extended with a
`SymbolTable`, a `NativeRegistry` (R3), and a `run_steps` loop that
re-enters after each `native_break` (M3 qualifier 2). `vform_selftest.cpp`
leg 3b (`:399–414`) currently asserts the refusal `does not execute in ir 8`;
it flips to a positive case in the same push (R5).

Legs, each with the expectation stated before it runs:

1. **teeth (load)** — naive `call X` with no `X.b0` defined → refuse (R1);
   the existing arity / `arg_count` / `X.CB` refusals retained.
2. **teeth (run)** — symbolic `rt_call` with no symbol table, and with an
   unknown symbol → loud throw naming the callee.
3. **`rt_call` → native** (test native built on `RTBridge`): three arguments
   — two values and `wp(v,0)`; the native asserts `arg_pointer(1) == e1`
   (order), writes through arg 3, `set_return_ac(0, 42)`, `native_return()`.
   Continuation stores ac0 into a `v`. Checks: v == 42, the cell written,
   `wsp == W`, `ac3 == caller wfp`, ac1/ac2/c preserved, program reaches its
   `ret`.
4. **`rt_call` → EMULATED** — a hand-assembled leaf in the rig's code page
   (`WSAVS 0x0000; NLDAI k,0; XWSTA 0,[ac3+0x7FF8]; WRTN`, words from the
   image/Decoder table): proves WRTN into a 0x77 label through a REAL WSAVS
   frame, callee-pops argc, ac0 = the slotpatched value. This is the path
   `?RANDOM_NUMBER` takes in the game.
5. **game→game, void** — caller writes `a1 = wp(v,0)`, `arg_count`, `call`;
   callee `M16[a1] = …; ret`; continuation checks the CALLER's cell, `wsp ==
   W`, ac0..ac2 restored; the callee's `ovk` observed by a nested `rt_call`
   to a recording native, once for a WSAVS entry and once for a WSAVR entry
   (the synthetic addrbook gets one of each).
6. **game→game, valued** — callee writes `<CALLEE>.ret`; caller reads it;
   **and** asserts ac0 after the call is the caller's pre-call ac0, not the
   result.
7. **nested** — caller → game callee → `rt_call` native → back → `ret` →
   caller: stack and shadow stack balanced at depth 2.
8. **PICK_X_Y end to end** (Stage B). Hand-written ir 8 PICK_X_Y — three
   `rt_call ?RANDOM_NUMBER(wp(t,0), wp(t,0), wp(seed))` with six 32-bit
   temps, `RANGE_CHECK`, the two `M16[a1]`/`M16[a2]` write-backs, the retry
   loop — plus a REPOSITION-shaped caller: two `i16` locals, `a1 = wp(v0,0)`,
   `a2 = wp(v1,0)`, `arg_count = 2`, `call PICK_X_Y args=2`. The rig maps a
   page for SD_PTR (0x70000210) / OBJ_PTR (0x70000212) targets with a
   synthetic REGION table (stride 9: one region unused, one wrong class, one
   good near a rectangle edge so the first draw bails at the gate) and a
   test-native `?RANDOM_NUMBER` whose LCG is `rt/random_number.cpp:9–19`
   copied. Host oracle: the same C, run on the host over the same table and
   seed → (x, y, seed, call count). **Compared: the CALLER's v0, v1, the seed
   word, the call count** — not PICK_X_Y's state.
   - **identity-green**: all four agree → PASS.
   - **identity-red (a)**: the same IR with one extra
     `rt_call ?RANDOM_NUMBER` inserted (result discarded) → x, y still agree,
     the seed and count do not → the comparator must report **FAIL**.
   - **identity-red (b)**: gate constant 0x3BF5 → 0x3BF4 → the comparator
     must report **FAIL** (on x, or on seed/count, depending on the draw).
   The comparator is one function; each red leg is a case that passes only
   if that function fails.
9. **machinery teeth** (script, as P46/P52's broken-allocator build): rebuild
   `IRExec.cpp` with `-DP54_BROKEN_BRIDGE` (the game→game marker pushed with
   argc = n, i.e. WRTN pops 2n words that were never pushed) and require the
   self-test **RED on the stack-balance check**.
10. **strict surface**: `quest.ir2.book` and `quest.ir2.stock` load unchanged
    through the loader (the P52 probe); the 16-leg battery per R6.

Where the identity legs sit relative to the prompt: the LOCKSTEP identity
leg — the master emulating PICK_X_Y while the clone runs it through the
bridge — is the P50 harness's first component and `docs/Project50/` has a
PROMPT and no design. Leg 8's oracle is the rig-level identity pair; I say
so in the REPORT rather than dressing it as a lockstep result.

---

## 5. Sizing (line-cited)

| file | estimate | basis |
|---|---|---|
| `hw/IRExec.cpp` | **~150** | the `site=` `rt_call` arm is 39 lines (`:1858–1896`); the naive arm is that minus the belief block (14) plus the dispatch tail (~20 from `EagleStack.cpp:286–306`) → ~45. The game→game arm ~25 + `enter_frame` ~20 (from `EagleStack.cpp:422–438`). Loader: variant capture ~6, `b0`-defined check ~6, entry-address plumbing ~10. Seam (R3) ~8. Comments to P52's standard ~30 |
| `hw/IRExec.hpp` | ~12 | `enter_frame`, `rt_registry_override`, one struct field |
| `tests/bridge_selftest.cpp` | **~600** | rig 60 (copied) + symbols/registry 30; legs 1–7 ~200; PICK_X_Y IR ~80, caller ~15, oracle ~40, table + rig pages ~40, red variants ~40; harness 60 |
| `tests/run_bridge_selftest.sh` | ~35 | `run_vform_selftest.sh` is 29 |
| `tests/vform_selftest.cpp` | −15 / +25 | leg 3b flips |
| `docs/Project54/REPORT.md` | doc | — |

P52 ran ~20% over on the loader and UNDER on the executor for a reason
that carries here: the cell addresses and widths are already in the parsed
nodes, so neither arm consults the declaration table at run time. Where I
expect to run over is leg 4's hand assembly and leg 8's table, not the
bridge.

Stages, each pushed to `p54-bridge`: **A** bridge + legs 1–7 + strict-surface
load; **B** PICK_X_Y + identity legs + machinery teeth; **C** battery (per
R6) + REPORT.

---

## 6. What does not survive contact

- **C-1 (M2).** The existing replica is the caller's half only. A symbolic
  callee has no WSAVS, and `ret` is WRTN, so game→game needs a WSAVS replica
  too or the return pops the caller's stack as a frame. ~20 lines, new, and
  it is the same prologue P50's Eagle→compiled entry will need — hence
  `enter_frame` as a named function rather than inline.
- **C-2 (M3).** Two qualifiers: CLONE-only, and a native return ends the
  batch at the 0x77 continuation. The second is invisible in the rig with a
  `run_steps` loop; under lockstep it puts a rendezvous at a 0x77 pc the
  master never reaches (IR.md §5.10.7 already records that a 0x77 arrival
  ticks no ordinal). P50's, not mine; recorded so it is not rediscovered.
- **C-3.** The spec never says which block a `call <ENTRY>` enters; the
  loader checks arity and `arg_count` but not that the callee has any block.
  → R1.
- **C-4.** No symbol table, no registry in the rig → R3.
- **C-5.** `?RANDOM_NUMBER` is emulated, not native (§1). The PICK_X_Y leg's
  callee is therefore a test native, and the WRTN-into-0x77 path is proved
  separately by leg 4. The real 7017DE33 (D.MOD, float) runs only in a
  lockstep leg with the image mapped — P50/battery, not the self-test.
- **C-6.** The naive marker's argc is a choice the prompt did not make → R2.
- **C-7.** The callee's `ovk` (WSAVS vs WSAVR) has no source in the IR; the
  addrbook has it and `load_entries` drops it. Kept, ~6 lines.
- **C-8.** `RTStubs.cpp:563` is the comment; the code is `:591–594`.
- **C-9.** Not measured: whether any runtime LCALL site uses the
  `argc & 0x8000` (no-psr) marker form. The bridge emits the psr form only,
  as `inject_fire` does; if a site needs the other, it is an IR addition and
  a STOP.
- **Held unchanged:** M1, M4, the prompt's account of `arg_pointer`
  (`RTBridge.cpp:111–114`), the valued-call split, "arguments in `a` cells
  are legal only because the game is non-reentrant", and the F4 slot
  addresses (W+4 = wp(ac3,−8) for 32-bit, W+5 = wp(ac3,−7) for 16-bit, which
  `EagleIntegration::wrtn` `:36–40` confirms by writing fp−8).

---

## 7. Rulings needed

**R1 — the entry block.** `call <ENTRY>` transfers to `<ENTRY>.b0`; the
loader refuses a naive call whose callee has no `b0` in the file (message
naming it), and refuses it also when the callee is an un-compiled game
routine (no `b0`) — calling ORIGINAL Eagle code from a symbolic block needs
the real-stack argument protocol and the M4a area writes, which is P50's.
*Recommendation:* rule it so and record one line in IR.md §6 (P52's file —
I cannot edit it). Alternative: an explicit `entry` marker in the grammar —
a grammar change, a STOP, and more than the problem needs.

**R2 — the naive marker's argc.** (a) argc = 0: nothing was pushed, WRTN
pops nothing, `arg_count` carries the arity in its cell. (b) argc = n with n
zero-wide tombstones so the stack shape matches the original's. (a) is one
line, honest about what is on the stack, and the shape difference is the
whole point of "naive"; (b) buys a shape nothing reads at L1 and makes the
broken-bridge teeth (leg 9) harder to state. *Recommendation:* (a).

**R3 — the test seam.** A static `IRExec::rt_registry_override`
(`NativeRegistry*`, null in the emulator) consulted instead of
`machine.process->native_registry` when set; names always from
`machine.symbols`, which the rig now supplies. Eight lines, no behaviour
change when unset. Alternative: build an `OSProcess` in the rig — needs a
`.PR` file and the FS layer; wrong weight for a self-test.
*Recommendation:* the seam.

**R4 — the dispatch tail.** Duplicate the 20-line native-dispatch tail from
`EagleStack.cpp:286–306` into the IRExec arm with a citation, rather than
factoring it out of the LCALL/XCALL/LJSR arms — a refactor of EagleStack is
inside the strict surface for no gain. *Recommendation:* duplicate, cited.

**R5 — self-test placement.** New file + script; `vform_selftest` leg 3b
flips from asserting the refusal to asserting execution. The 129-case count
moves by that one leg. *Recommendation:* as stated.

**R6 — the battery.** Boundary 3 requires the 16-leg battery not to regress;
that needs the runner. Prompt hazards: `bin/runner.sh` fixed but not
deployed, and the box's disk. Is the runner deployed, and may I queue one
battery task (055) at Stage C? If not, the REPORT will carry the two-artifact
load probe plus a local in-container leg if one fits, and say the battery is
outstanding — as P52 did. *Recommendation:* tell me the runner's state; I
queue only if it is deployed and the disk was checked.

**R7 — leg 8's callee.** A test-native `?RANDOM_NUMBER` with the LCG copied
from `rt/random_number.cpp`, plus a host oracle, is what the self-test can
prove; the emulated 7017DE33 is a lockstep matter. *Recommendation:*
accept; the seed comparison is still real (the same memory word, advanced
the same number of times by the same recurrence).

STOP. Waiting for `a001`.
