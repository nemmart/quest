# Project 1 — REPORT (O.SEARCH → O.SET cluster)

Deliverables: `runtime/o_signal.{hpp,cpp}` (in the tree, compiles
clean with `g++ -c -std=c++17 -Wall -Wextra -I. runtime/o_signal.cpp`),
`docs/Project1/DERIVATION.md`. Tree state at handoff: Makefile and
hw/RTStubs.cpp REVERTED to their originals (verified with diff against
pre-edit backups); the temporary local registrations used for
validation are listed below.

## 1. Status

| Routine | Status |
|---|---|
| O?SIGNAL | translated, capture-validated, **lockstep-validated** (both dispatch outcomes: found-handler transfer + no-handler fallback/detach) |
| O.SET | translated, capture-validated, **lockstep-validated** (in isolation: clone-native at the emulated EE38 XCALL, 2 pairs, 0 differing words) |
| O.SERROR | translated; validation = shared signal_dispatch path (identical code); its 1-line prefix (type=-1, code=caller ac2) unexercised — no live heap-corruption trigger |
| O.SCONVE / O.SSUBSC / O.SFIXED / O.SZEROD / O.SOVERF / O.SUNDER | translated; same shared-path status; no live provocation path exists — the store-"ABC" trigger routes via ?CHAR_TO_UNSIGNED → O?SIGNAL, and both game X.CB call sites convert digit constants (correction 4d) |
| O.SEARCH | derived only (DERIVATION §4); NOT translated — single caller is I.FFALT, a fault vector the emulator replaces with a C++ throw; no validation path exists (METHOD §9) |
| O.SIGNAL (0x7017EDE7), R.SIGREC (0x7017EE02) | derived only; **zero static callers anywhere**; NOT translated |

## 2. translation_table entries (registration order)

```
  { "O?SIGNAL", emu_rt::o_qsignal },   // TRANSFER (found) / fallback (no handler, terminal)
  { "O.SET",    emu_rt::o_set },       // LEAF (native_return)
  { "O.SERROR", emu_rt::o_serror },    // TRANSFER / fallback, as O?SIGNAL
  { "O.SCONVE", emu_rt::o_sconve },    // ditto
  { "O.SSUBSC", emu_rt::o_ssubsc },    // ditto
  { "O.SFIXED", emu_rt::o_sfixed },    // ditto
  { "O.SZEROD", emu_rt::o_szerod },    // ditto
  { "O.SOVERF", emu_rt::o_soverf },    // ditto
  { "O.SUNDER", emu_rt::o_sunder },    // ditto
```

Makefile: add `runtime/o_signal.cpp` to SRCS (I placed it after
runtime/o_on.cpp during testing). RTStubs.cpp include:
`#include "../runtime/o_signal.hpp"`.

Ordering constraints: none among these; they only depend on
rt::chain_search (runtime/o_on.hpp, already merged) and rt::t_area is
NOT used (the cluster addresses the task area via wsb directly, as the
disassembly does).

## 3. Interfaces I expose / I consume

Expose (plain C++, usable by other native code):
- `rt::walker_gate_open(hw::Machine&) -> bool` — the I?LINEID enable
  test (wide at 0x7017EEA0 > 0).
- `rt::signal_walker(hw::Machine&, WalkerResult&)` — EE9D live path,
  pure reads.
- `rt::select_frames(hw::Machine&, type, raw_key2, SelectResult&)` —
  EE62 select loop, pure reads (uses rt::chain_search per frame).
- `emu_rt::` entries per the table above.

Consume:
- `rt::chain_search` from runtime/o_on.hpp — reused verbatim (the EE7A
  helper my select loop XJSRs is word-for-word the one O_ON.md
  resolution 6 derived and validated).
- RTBridge LCALL_FRAME convention (every implemented entry is
  LCALL/XCALL + WSAVS 0), emulate_frame / native_return /
  native_transfer, RTStubs::entry_address / log_call.

Deviation from SharedProtocol called out LOUDLY: **the "terminal paths
compose with transfer" instruction is not implementable as written**
— see correction 4c.

## 4. Shared-doc corrections

a) **PROMPT.md extent table / quest.symbols accounting**: an unlisted
   entry **R.SIGREC at 0x7017EE02 (5 words)** sits between O?SIGNAL
   and O.SUNDER; O?SIGNAL is 21 words (EDED–EE01), not EDED–EE06.
   R.SIGREC (`WMOV 1,3; WSUB 1,1; WSSVS 0; WBR →EE38`) and O.SIGNAL
   (EDE7) both have zero static callers in game+RT.

b) **The I?LINEID branch is statically dead, and the guard is not the
   one on record.** M3Plan ("guarded by a mid-walk comparison against
   a distinguished frame cached from a per-task global") and the
   PROMPT describe the EEB2 comparison as the guard. The actual gate
   is the walker's FIRST instruction pair: `WLDAI ac0,0x00000000`
   (raw C689 0000 0000 at 0x7017EE9F–EEA1; the disassembler omits
   WLDAI's register field) followed by `WSGT 0,0` — and same-register
   skips compare against literal zero (EagleCompute:
   `dst=(XX!=YY)?ac[YY]:0`), so with the immediate 0 the branch to
   EEEA is unconditional and EVERYTHING in EEA4–EEE9, including the
   LCALL I?LINEID and the EEB2 comparison, is unreachable in this
   binary. This explains the empirical I?LINEID ×0. The locked
   terminal-branch decision survives in cheaper form: the native
   reads the wide at 0x7017EEA0 (as the emulated WLDAI does on every
   execution) and falls back to emulation from entry if it is ever
   positive. Evidence: HexDump of the .PR at 7017EE9D–EEA3 +
   EagleCompute.cpp WSGT source.

c) **SharedProtocol "Terminal paths compose with transfer" is wrong
   as written.** "A native routine that discovers it is heading to
   DEF?ON ... simply native_transfers to that entry — the terminal
   machinery detaches the clone at arrival" diverges structurally:
   DEF?ON is a terminal entry INSIDE the RT range; the master ends
   its run-to-return there via the terminal exception
   (terminal_reached, native_span NOT set — hw/Machine.cpp), while
   the transferring clone ends via native_break (native_span, NOT
   terminal), and compare_pair requires terminal on both sides and
   compares instruction counts unless BOTH are native_span. Observed:
   master insns=87 vs clone insns=17, identical registers/pc/stack —
   pure structure. Correct composition for an RT-range terminal
   target: the native wrapper FALLS BACK to emulation (decided on
   pure reads before any store) so both engines emulate to the
   terminal with equal counts, plus a nested-in-fallback guard (see
   hazard 5b). native_transfer remains correct for game-range targets
   (handler dispatch), where range-exit gives native_span on both
   sides. Evidence: run-B divergence report (reproduced in §6).

d) **The store-"ABC" CONVERSION trigger does not exercise O.SCONVE.**
   RTWorklist Play Session 2's own record shows the chain:
   ?CHAR_TO_UNSIGNED → C.INDEX → ?LIB_ERROR → (O?SIGNAL) — the same
   entry the FAIL_OPEN trigger validates. Moreover O.SCONVE appears
   to be UNREACHABLE IN PRACTICE: its only caller is X.CB's
   digit-check failure branch (7017E714 → E71E), and both game X.CB
   call sites convert fixed digit CONSTANTS — GET_INPUT+0xC converts
   "001" (bytes 30 30 31 at 7016A9B9) and MOVE_IN_CAVE+0xCB converts
   "1" (31 at 7017024D) — which can never fail. No input-derived
   X.CB conversion exists in the game. Consequences: M3Plan's "X.CB
   (live, 153/session) calls O.SCONVE" is statically true but
   dynamically vacuous; the shorthand entries have NO live
   provocation path at all (matching their zero-or-dead caller
   inventory, §1/DERIVATION §1), so their validation status is as
   good as it can get without synthetic patching. CONFIRMED LIVE:
   the auto-move count prompt ("M", direction, then "abc" at "For
   how many turns?") fires the CONVERSION signal and the rtcalls
   trace shows ?CHAR_TO_UNSIGNED -> ?LIB_ERROR -> O?SIGNAL(native)
   -> I.GOTO — no O.SCONVE.

e) Minor: ON_ERROR_CATALOG condition codes are supplemented at
   runtime — ?LIB_ERROR signals with the OS error code as the
   condition code (observed 0x100A failed-open, 0x2004 write-path),
   not only the 0x1160x/0x1161x constants the shorthand entries load.
   (No doc is wrong; noting for anyone matching codes in captures.)

f) O_ON.md's sketched signal-path section is confirmed exact where it
   guessed: frame walk via [frame+0x8]; the WPSH 1,1 exists to
   preserve key2 across the helper's ac1 clobbers (LDATS re-load per
   iteration). The walker's outputs go to [wsb-0x36]/[wsb-0x38],
   which have NO static readers anywhere (dead stores, replicated
   regardless).

## 5. Open questions / integration hazards

a) **rtcalls visibility difference**: native o_qsignal subsumes O.SET;
   the emulated flow logs a separate `O.SET ret=7017EE3B` line per
   signal, the native flow does not (and DEF?ON lines disappear since
   the no-handler case re-emulates from O?SIGNAL entry with the
   fallback tag instead). Any tooling that counts rtcalls lines must
   expect this.

b) **Nested-in-fallback guard (new pattern, other projects likely
   need it)**: on the clone, `machine.rt_pending_return != 0` at
   native dispatch means an outer fallback span is being re-emulated;
   an inner registered routine must ALSO return
   `RTStubs::entry_address(name)` — WITHOUT re-arming
   rt_pending_return (re-arming retargets the outer span's return) —
   or the master's absorbed emulation and the clone's native run skew
   the span's instruction counts, which matters whenever the span
   ends count-compared (terminal pairs). Projects 2/3's routines sit
   on the same signal chain and will be called inside my no-handler
   fallback span (?LIB_ERROR → ?DEFAULT_ERROR_HANDLER emulated bodies
   XCALL/LCALL T?AREA, I.FREEW, O?SIGNAL...). T?AREA/heap natives
   already in the tree do NOT have this guard — they were never
   inside a count-compared span before my fallback introduced one:
   **the integration pass must audit every translation for it** (the
   existing I.FREEW(native) at seq 1135 of the vanilla trace runs
   inside ?LIB_ERROR's body — if ?LIB_ERROR is native with a fallback
   path (Project 2), the same hazard fires there).
   NOTE: my own guard makes the nested entries emulate, which is why
   run E's terminal pair balanced. The clean lockstep runs in §6
   include I.FREEW/I.ALLOC natives running inside the NON-fallback
   parts of the flow, unaffected.

c) **Clone shadow-call-stack depth**: the found-path transfer skips
   the XCALL O.SET push/pop pair (net 0) and the dispatch XCALL's
   push, so the clone's debug::CallStack runs one entry SHALLOWER
   than the master per natively-dispatched signal (both leak stale
   entries — handlers GOTO past their frames — so no underflow risk;
   call_return mismatches print warnings, not throws). Cosmetic
   (backtrace quality on the clone only), but worth knowing when
   reading clone backtraces.

d) **Frame-walk cycle guards**: rt::signal_walker and
   rt::select_frames throw `"...frame chain exceeds 1024 (cycle?)"`
   rather than spinning (METHOD §8). A genuine cycle would hang the
   emulated master; the clone-side throw at least names it as a
   divergence.

e) O.SEARCH / O.SIGNAL / R.SIGREC deliberately unregistered (§1). If
   I.FFALT ever becomes reachable, O.SEARCH's derivation is complete
   in DERIVATION §4.

## 6. Validation evidence

Environment: scripted driver (`/home/claude/drive.py`): connect 8781,
login CL/Claude/quest/Y/space/F, then `L` `P`, ESC. Scratch-copied
QUEST/ per run. All runs
`QUEST_FAIL_OPEN=USER_DATA_FILE ... -lockstep -silent QUEST_SERVER
@QUEST @QUEST`. Trigger yields TWO signals: #1 failed-open in
LIST_PLAYERS → handler 0x7016EC57 (catalog #13) → I.GOTO resume;
#2 via REFRESH_SCREEN/?WRITE_SCREEN → chain exhausted → DEF?ON.
wsb = 0x7000108C (T?AREA capture: RETURN ac0=0x70001063 + 0x29).

1. Ground truth (vanilla, `-trace -types rtcalls`): sequence
   `?LIB_ERROR → 8×T?AREA → ?DEFAULT_ERROR_HANDLER → O?SIGNAL(ret
   7017E3EF) → O.SET(ret 7017EE3B) → [dispatch] → handler ?WRITE_SCREEN
   → I.GOTO(ret 7016EC74) → ... → second signal → ... → DEF?ON(ret
   7017EE40)` then `Lockstep: ordinal 0 DETACHED at 7017EF05
   (DEF?ON)`. The DEF?ON ret=7017EE40 line confirms the dispatch
   XCALL's ac3.
2. Master expected values (vanilla lockstep + capture; METHOD §10 —
   all values below are from these exact runs):
   - `QUEST_CAPTURE=7017EE56 QUEST_CAPTURE_DEST=7000104C`: two
     ENTRY/RETURN pairs. Signal 1: E=0x7000151C, RETURN shows O.SET's
     WSAVS image [E+2..E+B] = (-1, 0, 0x100A, 0x7000151A,
     0x7017EE3B), walker WSSVR image [E+C..E+17] = (0x80000000, -1,
     0, 0x100A, 0x70001526, 0x7017EE5A), locals untouched, task-area
     stores (-1, 0, 0x100A) at [wsb-0x3E..-0x3A], walker outputs 0/0,
     post-WRTN wsp=E-2, ac3=wfp. Signal 2 same shapes with
     code=0x2004.
   - `QUEST_CAPTURE=7016EC57` (dispatch arrival, signal 1):
     ac0=FFFFFFFF ac1=70001158(frame) ac2=7016EC57 ac3=7017EE40 c=0
     ovk=1 ovr=0 wsp=E_sig+12 wfp=E_sig+10; memory [E_sig+12]=fw,
     [E_sig+14]=key2 backup 0, helper image with node 0x700014A4
     patched into saved-ac1 and ret 0x7017EE6A (found: NOT
     incremented), walker image beneath.
   - `QUEST_CAPTURE=7017EF05` (DEF?ON arrival, signal 2): ac1=0,
     ac2=7017EF05, ac3=7017EE40, helper ret 0x7017EE6B (ISZTS'd,
     not-found), scratch 0 — every word per DERIVATION §5.
3. Divergence that produced correction 4c (translations registered,
   original transfer-to-DEF?ON design): identical
   pc/registers/wsp/wfp/psr both sides at 7017EF05, master insns=87
   clone insns=17, master backtrace shows the EE3D dispatch frame,
   clone's does not. Fixed as described; kept as evidence.
4. Final validated runs (temporary local registrations):
   - Full set registered, `QUEST_CAPTURE=7016EC57`: **0 divergences**,
     signal-1 transfer paired (lockstep trace shows the native_span
     pair at pc=7016EC57), master ENTRY vs clone NATIVE diff:
     **0 differing words / 110 compared** (92-word frame window +
     18-word task-area window), all registers identical. Signal 2:
     `O?SIGNAL(native-fallback: no handler — DEF?ON terminal path...)`
     then `O.SET(native-skip: inside fallback span)` then clean
     detach at 7017EF05.
   - O.SET only registered (isolation): **0 divergences**, two
     clone-native o_set dispatches at the emulated EE38 XCALL, master
     RETURN vs clone NATIVE: **0 differing words** on both signals
     (register deltas = documented pre-native_return snapshot
     timing; post-return state verified by the passing pair gate).
   - rtcalls lines: `O?SIGNAL(native) ret=7017E3EF` ×2 per session.
   - Third live shape (auto-move CONVERSION; fast trigger, no
     FAIL_OPEN needed): login, `M`, direction `n`, wait ~90 s for the
     move turn ("For how many turns?" appears), type `abc` + CR.
     rtcalls: `?CHAR_TO_UNSIGNED ret=7015FBC6` → `?LIB_ERROR ret=
     7017DA12` → `O?SIGNAL(native) ret=7017E3EF` → `I.GOTO ret=
     7015FBAF` (AUTO_MOVE's catalog-#2 handler; game resumes to the
     command prompt). 0 divergences. Distinct ?LIB_ERROR call site
     and handler from the FAIL_OPEN shape — a second found-path
     dispatch validated live, and direct confirmation of correction
     4d (no O.SCONVE involvement).
5. Tree reverted: Makefile and hw/RTStubs.cpp diff clean against
   pre-edit backups; vanilla `make` relinks identically.

Re-run recipe for the integration pass (exact commands):

```
cd Work/c_src && make
cp -r QUEST QUEST_RUN
QUEST_CAPTURE=7016EC57 QUEST_CAPTURE_DEST=7000104C \
QUEST_FAIL_OPEN=USER_DATA_FILE stdbuf -o0 -e0 \
./emulator -lockstep -silent -trace rt.trace -types rtcalls \
  QUEST_RUN QUEST_SERVER @QUEST @QUEST > run.log 2> run.err &
# drive: telnet 8781; CL/Claude/quest/Y/<space>/F; L; P; ESC
# expect: run.err contains "DETACHED at 7017EF05" and no "DIVERGENCE";
# rt.trace contains two "O?SIGNAL(native)" lines and one
# "O?SIGNAL(native-fallback: no handler" line;
# capture-QUEST1 ENTRY@7016EC57 vs capture-QUEST2 NATIVE (pc=7017E3EB
# block) diff to 0 words.
```
