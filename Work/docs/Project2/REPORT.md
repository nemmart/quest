# Project 2 REPORT — ?LIB_ERROR / ?LIB_ERROR_CODE / ?DEFAULT_ERROR_HANDLER

Code: `runtime/lib_error.{hpp,cpp}` (compile-checked standalone:
`g++ -c -std=c++17 -I. runtime/lib_error.cpp` from `c_src/`, zero
warnings with -Wall). Derivation: `docs/Project2/DERIVATION.md`.
The tree is reverted: `make` output is byte-identical in behavior
(temporary local registration used for validation was removed).

## 1. Status

| Routine | Status |
|---|---|
| ?LIB_ERROR | derived / translated / **capture-validated + boundary-pair-validated** on both live paths (alloc+message via the 2-arg ?OPEN_FILE site; free+no-message via a 1-arg ?WRITE_SCREEN site). Fallback path lockstep-validated live. Final hop (calling translated O?SIGNAL, and the WRTN-return replay) blocked(integration): needs Project 1's O?SIGNAL row AND their strong `rt::signal_has_handler` (weak default keeps everything falling back — safe — until then). |
| ?DEFAULT_ERROR_HANDLER | derived / translated / capture-validated **as part of the ?LIB_ERROR unit** (its body runs inside the native ?LIB_ERROR path; that is its only live entry). Its standalone dispatch wrapper is unreachable today by construction (see §5) — structurally correct, untested standalone. |
| ?LIB_ERROR_CODE | derived / translated / compile-checked. blocked(no live trigger): its single caller is LOGON handler #2 (?CONNECT failure), which no available fault injection reaches. Structure is the validated ?UDIV32 saved-ac0-patch pattern plus a fully derived 6-word T?AREA residue. |

Design (DERIVATION.md "Dispatch and pairing design"): the trio is one
unit. Native ?LIB_ERROR runs everything — install, latch, code store,
old-buffer free, message alloc+copy, the ?DEFAULT_ERROR_HANDLER body —
to the exact O?SIGNAL LCALL-dispatch state, then composes with
O?SIGNAL through the native registry. Pre-integration every call falls
back at entry (validated live, zero side effects); post-integration it
goes native automatically with **no code change**.

## 2. translation_table entries (registration order)

```c
  { "?LIB_ERROR", emu_rt::lib_error },                       // TRANSFER: ends at O?SIGNAL dispatch (calls the registered
                                                             // translation as plain C++; passes its transfer pc through),
                                                             // or falls back whole pre-integration. Full WRTN return path
                                                             // implemented for the O?SIGNAL-returns case.
  { "?LIB_ERROR_CODE", emu_rt::lib_error_code },             // LEAF: saved-ac0 patch, 0 args.
  { "?DEFAULT_ERROR_HANDLER", emu_rt::default_error_handler },// TRANSFER: reachable only via the 0x7017E3CD XCALL, which
                                                             // today always fires inside a fallback span (nested rule
                                                             // returns immediately); registered for dispatch-surface
                                                             // completeness and the post-integration standalone case.
```

Makefile: add `runtime/lib_error.cpp \` to SRCS (I used the line after
`runtime/t_area.cpp \` during validation).

Ordering constraints: none among the three; ?LIB_ERROR requires
I.ALLOC/I.FREEW rows present (they are) and benefits from O?SIGNAL's
row (Project 1) but is safe without it.

## 3. Interfaces I expose / I consume

**Expose**: `emu_rt::lib_error / lib_error_code / default_error_handler
(hw::Machine&) -> uint32_t` — standard dispatch shape, nothing else.

**Consume**:
- `rt::t_area(machine)` (frozen contract) — record base is
  `t_area()+8`; see DERIVATION.md for the layout table.
- `emu_rt::i_freew` / `emu_rt::i_alloc` called as plain C++ with
  machine state staged to the exact emulated LJSR values (including
  temporary `machine.wfp = F`, `machine.wsp = F+4`). Their gates are
  replicated at ?LIB_ERROR entry (via exported `rt::HEAP_*`,
  `rt::heap_class_size`, `rt::heap_lock_contended/has_waiters`) so the
  inner calls provably take their native paths.
- `hw::RTStubs::translated_bits / active / start / stop /
  entry_address / log_call`; `hw::RTBridge` (LCALL_FRAME ctor,
  `emulate_frame`, `native_return`, `native_transfer`);
  `debug::Capture::native_footprint`.

**Contract proposed to Project 1 (the only cross-project seam)**: no
new `rt::` signature. ?LIB_ERROR delivers the EXACT O?SIGNAL
LCALL-dispatch state (DERIVATION.md "Boundary state": pc target
0x7017EDED, ac0=code, ac1=-1, ac2=0, ac3=0x7017E3EF, c=0, psr
ovk=1/ovr=0, wsp=F+30, wfp=F+16, three by-ref args at [wsp-2/-4/-6] →
[-1, 0, code], all frame/residue words in place) and then calls
`machine.process->native_registry.lookup(0x7017EDED)(machine)` — i.e.
Project 1's registered wrapper must be standard dispatch-shaped, must
arm `rt_pending_return` itself on its own fallback, and its return
value is interpreted as: `0x7017E3EF` = normal return (I replay both
WRTNs natively and return to ?LIB_ERROR's caller); anything else =
transfer pc passed straight through (handler dispatch, DEF?ON, or
fallback continuation — all three compose with run_steps' pairing
rules; the DEF?ON case was observed live from the emulated side, §6).
**Deviation to call out loudly: none** — this is exactly the LCALL
dispatch semantic; the registry is the interface.

**Plus one new signature (from Project 1's review corrections)**:
`bool rt::signal_has_handler(hw::Machine& machine, int32_t code)` —
pure reads only, true iff the signal walk starting at [wsb-0x40] would
find a handler for this condition code. Declared in lib_error.hpp; a
WEAK conservative default (returns false → fall back whole) ships in
lib_error.cpp, so the merge links with or without Project 1's strong
definition, and the no-handler/DEF?ON case can never enter a native
span. Project 1 owns the strong definition (it is the same pure-read
walk their own entry gate needs per their correction).

## 3a. Project 1 review corrections — application record

1. **"No RT-range terminal transfers; predict terminal-bound at entry
   on pure reads and fall back whole"** — APPLIED. My code never
   transferred to DEF?ON/?FATAL directly, but the o_signal
   pass-through was exposed to the mid-span-fallback count-skew the
   correction describes. Fix: a `rt::signal_has_handler(machine,
   code)` gate at both ?LIB_ERROR's and ?DEFAULT_ERROR_HANDLER's
   entries (pure reads, before any store), with a weak-linked
   conservative default (false) until Project 1's strong definition
   lands (§3). The VALIDATE rig's transfer to 0x7017EDED remains — it
   is the documented intentionally-divergent harvesting tool,
   unreachable in production (env unset).
2. **"Nested-span guard: return entry address without re-arming"** —
   ALREADY PRESENT (independently derived; first line of all three
   wrappers, exactly the specified form) and the underlying hazard is
   documented in §4.2 below. No change needed.
3. **Conversion trigger** — ADOPTED as validation run D (§6): it
   covers the one path combination the fault-injection runs missed.

## 4. Shared-doc corrections

1. **`native_registry.lookup(entry) != nullptr` does NOT mean
   "translated"** — every RT entry has a log-and-continue STUB
   registered, so lookup is non-null for all of them.
   `RTStubs::translated_bits` is the correct predicate. Evidence: my
   first build gated on lookup nullity; the "o_signal" call invoked the
   O?SIGNAL stub, which logged and returned its entry address — every
   validation run went native regardless of the intended gate (rtcalls
   showed `?LIB_ERROR(native)` where a fallback was configured).
   Nothing in RTStubs.hpp/SharedProtocol.md warns about this;
   **Project 1's handler dispatch (O.SET's XCALL through node[+6])
   must use the same translated_bits test if it ever needs
   "is X native"**.
2. **Nested-fallback clobber hazard** (latent, in-tree):
   `i_alloc.cpp`/`i_lock.cpp` fallbacks arm `rt_pending_return`
   UNCONDITIONALLY. If such a dispatch ever fires inside another
   routine's emulated-fallback span (registry dispatch happens
   regardless of an active span), the outer pending is overwritten and
   the clone's span ends at the inner return while the master's
   run-to-return continues — structural divergence. Unreachable for
   the heap family today (gate composition), but ?DEFAULT_ERROR_HANDLER
   hits the nested case DETERMINISTICALLY (the e3CD XCALL inside every
   emulated ?LIB_ERROR fallback), hence the nested-span rule in
   lib_error.cpp: `if(machine.rt_pending_return != 0) return
   entry_address(name);` with no re-arm. Recommend adopting the
   conditional arm in METHOD.md §12 as the standard fallback protocol.
3. **Capture one-shot arming never re-arms for non-returning
   routines**: ?LIB_ERROR/?DEFAULT_ERROR_HANDLER/O?SIGNAL never reach
   their armed return pc (handled signals I.GOTO out), so the FIRST
   signal in a session consumes the arming forever ("nested entry …
   ignored" for all later signals). One A/B pair per session per run;
   plan multiple runs. (Tooling observation, not edited.)
4. **QUEST_TERMINAL at the same pc as QUEST_CAPTURE suppresses the
   ENTRY snapshot** — the terminal check in run_steps returns before
   the next iteration's Capture::check. Use separate runs for terminal
   pairing vs capture ground truth.
5. ERROR_PROCESSING.md's "17 one-arg ?LIB_ERROR callers": there are
   18 sites — 17 one-arg plus the two-arg 0x7017DDAE inside ?OPEN_FILE
   (the message-forwarding site; it was the prompt's unattributed
   "~7017DDAF caller").

## 5. Open questions / integration hazards

- **Merge-pass check**: after Project 1's O?SIGNAL row lands, re-run
  the end-to-end trigger (§6 commands) WITHOUT
  `QUEST_LIBERROR_VALIDATE`: expect a clean native_span pair at the
  range exit (the game-handler pc, e.g. LIST_PLAYERS' catalog-#13
  resume), not a divergence; then the store-garbage trigger (O_ON.md)
  for the conversion-error path; then an unhandled-signal case pairing
  terminally at DEF?ON 0x7017EF05.
- **O?SIGNAL-returns path** (both WRTN replays, `call_stack->
  native_return`) is implemented but unreachable until integration —
  needs a targeted test if any live path ever returns from O?SIGNAL.
- **?LIB_ERROR_CODE** needs a ?CONNECT-failure injection (none exists)
  or an integration-time harness; risk is low (leaf, 6 instructions,
  validated pattern) but it ships live-unvalidated.
- The `QUEST_LIBERROR_VALIDATE` env (occurrence-N deliberate-
  divergence rig) and its static call counter are validation-only and
  inert in production (unset → 0 → never matches); the merge pass may
  delete both once the chain validates end-to-end.
- ?DEFAULT_ERROR_HANDLER's standalone wrapper becomes REACHABLE with
  pending==0 only if some future native routine XCALL-dispatches it
  outside a span; its gates then require O?SIGNAL translated — fine —
  but it has no headroom-40 guard like ?LIB_ERROR (only +24; correct
  for its own depth).

## 6. Validation evidence

All runs: `Work/c_src` build with the TEMPORARY local registration
(three rows + Makefile line, since reverted), game data scratch-copied
to /tmp/QRUN, lockstep + silent, scripted driver (login CL / Claude /
quest / Y / space / F, then `L`, `P` [, `L`, `P`]), trigger
`QUEST_FAIL_OPEN=USER_DATA_FILE`.

**Run A — boundary pair, alloc+message path** (terminal rig):

```
QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_LIBERROR_VALIDATE=1 \
QUEST_TERMINAL=7017EDED QUEST_CAPTURE=7017E3D2 \
./emulator -lockstep -silent QUEST QUEST_SERVER @QUEST @QUEST
```

One final pair at 7017EDED, both terminal, REGISTERS IDENTICAL:
`master … pc=7017EDED insns=231 ac0=0000100A ac1=FFFFFFFF ac2=00000000
ac3=7017E3EF c=0 wsp=70001510 wfp=70001502 psr=8000` vs `clone …
insns=14` + same everything else. The count delta is the expected,
documented pre-integration mismatch (native_span exemption needs the
real O?SIGNAL hop); the report's flagged divergence IS the evidence.
Master backtrace confirms the derived chain: LIST_PLAYERS →
?OPEN_FILE+0x87 (2-arg site) → ?LIB_ERROR+0x93 →
?DEFAULT_ERROR_HANDLER+0x19 → O?SIGNAL.

**Run B — memory footprint, alloc+message path**:

```
QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_LIBERROR_VALIDATE=1 \
QUEST_CAPTURE=7017EDED QUEST_CAPTURE_DEST=7000106B \
./emulator -lockstep -silent QUEST QUEST_SERVER @QUEST @QUEST
```

Master `ENTRY pc=7017EDED` block vs clone boundary `NATIVE` block:
registers, flags, wsp/wfp, the full 92-word region at base 70001508
(= F+22) and the 18-word area window at B=7000106B are word-for-word
identical, including [F+22]=0000100A (code), the three XPEF arg refs,
LCALL word 8000|0003, surviving T?AREA residue [F+32]=F+16 /
[F+34]=7017E3D8, and area [B]=8000 latch, [B+1]=0000100A,
[B+3]=70017642 (buffer). The XCALL-instant blocks (master ENTRY at
7017E3D2 from the run-A configuration, base 700014F0 = F-2) also
match: len narrow 000F at [F+2] with stale [F+3]=0002, alloc result
70017642 at [F+4], and the e3C2 T?AREA frame [F+8..F+16] =
area / 0 / E002EC95 (WCMV dst end) / F / 7017E3C6.

Two real bugs were found BY these diffs and fixed: (i) the e3C2
T?AREA tail-frame residue was missing at the XCALL instant; (ii)
`RTBridge::arg_pointer/arg_word` resolve against LIVE machine.wsp —
after staging wsp for the inner heap calls, arg 2 read [F] (the
return-address wide) instead of [F-14]: msgptr=7017DDB2, n went
negative, and the message copy silently did not run. Post-staging arg
reads now address the slot from the entry wsp directly. Debug values
after fix: `newbuf=70017642 msgptr=70001390 dstb=E002EC86
srcb=E0002722 n=F dst_end=E002EC95` — dst end equals the master's.

**Run C — fallback path + free/no-message path** (occurrence rig):

```
QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_LIBERROR_VALIDATE=2 \
QUEST_CAPTURE=7017E3D2 QUEST_CAPTURE_DEST=7000106B \
./emulator -lockstep -silent -trace quest.trace -types rtcalls \
QUEST QUEST_SERVER @QUEST @QUEST
```

- Signal 1 (2-arg): `rtcalls … ?LIB_ERROR(native-fallback:
  o-signal-untranslated) ret=7017DDB2` — the SHIPPED pre-integration
  behavior, exercised live: the whole signal ran emulated on both
  engines, the LIST_PLAYERS handler fired and the game continued.
  Zero divergences through it.
- Signal 2 arrived unprompted: ?WRITE_SCREEN's SYSCALL 0303 failed
  (code 0x2004) inside the handler's screen refresh — a ONE-ARG site
  (ret=7017E306) with [B+3] holding signal 1's buffer, i.e. exactly
  the free + no-message path: `rtcalls … ?LIB_ERROR(native)
  ret=7017E306` followed by `I.FREEW(native) ret=7017E381` (the
  staged inner call). Clone NATIVE blocks: the I.FREEW retract residue
  per I_ALLOC.md (A_RET_FREE/lock/psr|1/block/size 000E/block+size/
  F_i/A_UNLOCK_RET/A_RET_WATER/scratch) with area [B+1]=2004 already
  stored and [B+3] still 70017642 mid-free (write order per the
  body); boundary block ac0=00002004 ac1=FFFFFFFF ac2=0 ac3=7017E3EF
  c=0, wsp=wfp+14, [F+30]=8000|0003, [F+34]=7017E3D8, **[F+36]=
  70017642 — the derived surviving I.FREEW water scratch** — and
  dest [B+1]=00002004, [B+3]=00000000 (zeroed after the free).
- The master ran signal 2's unhandled chain to **DEF?ON 0x7017EF05**
  (run-to-return terminal escape), demonstrating the exact
  pass-through/terminal composition the integration will pair on.

**Run D — no-free + no-message path, live handled signal (Project 1's
conversion trigger; no fault injection)**:

```
QUEST_LIBERROR_VALIDATE=1 QUEST_TERMINAL=7017EDED \
QUEST_CAPTURE=7017E3D2 QUEST_CAPTURE_DEST=7000106B \
./emulator -lockstep -silent -trace quest.trace -types rtcalls \
QUEST QUEST_SERVER @QUEST @QUEST
# driver: login, M, n, "abc" at "For how many turns?"
```

`rtcalls … ?LIB_ERROR(native) ret=7017DA12` — the ?CHAR_TO_UNSIGNED
wrapper's 1-arg site, code 0x00011611, first signal ([B+3]==0: no
free, no message). Terminal pair at 7017EDED REGISTER-IDENTICAL:
`ac0=00011611 ac1=FFFFFFFF ac2=00000000 ac3=7017E3EF c=0
wsp=700011D0 wfp=700011C2 psr=8000` both sides (insns 129 vs 42, the
expected exemption gap). XCALL-instant capture diff (master ENTRY at
7017E3D2 vs clone NATIVE, base 700011B0): WORD-FOR-WORD identical,
including the entry-carry bit in [F]=F017DA12, the untouched stale
no-message locals [F+2..F+5], and the e3C2 T?AREA residue
[F+10]=00000001 (entry ac1) / [F+12]=7000106B (B). With runs B and C
this completes the path matrix: alloc+message, free+no-message,
no-free+no-message.

Capture files from the runs are preserved at /tmp/QRUN/capture-*.txt
(QUEST1 = master, QUEST2 = clone) with logs and quest.trace.
