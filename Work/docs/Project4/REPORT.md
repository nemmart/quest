# Project 4 — REPORT (O?AREA, P?DEFON, R?SIGNAL, DEF?ON)

Single-session project (Aug 12 2026), not a parallel-merge session: the
tree edits SharedProtocol reserves for the integration pass
(translation_table, Makefile) were made directly and are listed here
for audit.

## 1. Status

| Routine | Status |
|---|---|
| O?AREA | derived / translated / master-capture-confirmed (bit-level, DERIVATION §2) / registered / DORMANT pending DEF?ON lift |
| P?DEFON | derived / translated / registered / DORMANT pending DEF?ON lift + positive-type fault injection |
| R?SIGNAL | derived / translated / plain-path master-capture-confirmed (bit-level, DERIVATION §5.5) / registered / DORMANT pending DEF?ON lift |
| DEF?ON | derived / translated / STAGED-UNREGISTERED (terminal_table entry — registration belongs to the lift, §5 below) |

Regression: CONVERSION trigger and QUEST_FAIL_OPEN trigger both re-run
under lockstep post-registration, 0 divergences each, behavior
byte-identical to pre-registration (wrappers provably unreached — the
dormancy argument, DERIVATION §4).

## 2. Tree changes (the would-be integration entries, already merged)

- `runtime/o_area.{cpp,hpp}` — rt::o_area + emu_rt::oq_area (LEAF).
- `runtime/p_defon.{cpp,hpp}` — emu_rt::pq_defon (TRANSFER on the
  resignal branch, via native O?SIGNAL; normal return on type==2).
- `hw/RTStubs.cpp`: `{ "O?AREA", emu_rt::oq_area }`,
  `{ "P?DEFON", emu_rt::pq_defon }`, `{ "R?SIGNAL", emu_rt::rq_signal }`
  appended to translation_table (no ordering constraints), plus a
  COMMENTED `{ "DEF?ON", emu_rt::defq_on }` row with a do-not-enable
  banner, plus the four runtime includes.
- `runtime/r_signal.{cpp,hpp}` — emu_rt::rq_signal (LEAF on the live
  path; anomaly path falls back whole). R.SIGNAL (EF51): zero callers,
  not implemented.
- `runtime/def_on.{cpp,hpp}` — emu_rt::defq_on (STAGED; TRANSFER via
  P?DEFON on the resignal path, single native_return boundary
  otherwise).
- `Makefile`: the four .cpp entries.
- Build: warning-free, g++ C++17 -O2 -Wall -Wextra.

## 3. Interfaces I expose / I consume

Expose: `rt::o_area(hw::Machine&) -> uint32_t` (= wsb-0x40) — the
named accessor for the signal area; candidate frozen contract for the
DEF?ON/R?SIGNAL session (mirrors rt::t_area's role).

Consume: `rt::walker_gate_open`, `rt::select_frames` (o_signal.hpp,
unchanged); the registered native O?SIGNAL via
`native_registry.lookup(0x7017EDED)` — the lib_error.cpp composition
precedent, same pc3 convention. Deviation from SharedProtocol worth
flagging: the has-handler prediction is `select_frames(new_type,
*arg1)` directly, NOT `rt::signal_has_handler` — the frozen helper
hardwires (-1, 0), which is wrong for P?DEFON's type-6 resignal
(DERIVATION §3.6.4).

### 3a. Additional interface notes (continuation)

def_on.cpp pre-runs p_defon.cpp's fallback gates before dispatching it
(the cascading-pre-check pattern); the grep anchor rule in §5 now
covers three files: o_signal.cpp, p_defon.cpp, def_on.cpp.

## 4. Shared-doc corrections

a. **Layering.md P?DEFON caller row / NextSession "C?INIT (init +
   P?DEFON...)" reading**: the docs are correct, but the grouping
   invites the inverted reading (P?DEFON called from C?INIT at init).
   The edge runs P?DEFON → C?INIT; P?DEFON's sole caller is DEF?ON.
   Session-planning consequence: my "P?DEFON validates on normal
   startup" claim to the user was WRONG (corrected mid-session,
   recorded per METHOD §11) — nothing in this cluster validates on a
   startup path.
b. **Disassembler immediates masquerade**: `WSNEI 0,65535` at DEF?ON
   ef29 is a type == -1 test (WSEQI/WSNEI sign-extend the 16-bit
   immediate). Any future reader tabulating DEF?ON's branches from the
   listing text alone will mis-key this. Same class as the -0x40
   displacement printing as +0x7FC0.
c. **R.SIGREC has a dynamic caller** — Project 1's census ("ZERO
   static callers", o_signal.hpp banner) is correct as stated but
   incomplete as reachability: R?SIGNAL's anomaly path dispatches
   through the [0x70000124] vector, installed by I.GINIT at startup,
   whose word0 = 0x7017EE02 = R.SIGREC (DERIVATION §5.2, confirmed
   live in the run3 DEST window). "Deliberately not implemented"
   remains right — the path has never fired — but any future scan
   claiming R.SIGREC unreachable must account for the vector.
d. **Layering.md's R?SIGNAL word count** (226) is the symbol extent;
   the code body is 59 words, the rest is ?SNAP's traceback strings.
e. **Capture-tool sharp edge re-confirmed the hard way**: one run lost
   to QUEST_CAPTURE without QUEST_CAPTURE_DEST (clone permission-fault
   on the default entry-ac2 window at startup — divergence at I.INIT,
   insns 51 vs 57, exception "Page does not have read permission ...
   E000218B"). The existing gotcha line is accurate; obey it.

## 5. Open questions / integration hazards — THE DEF?ON-LIFT CHECKLIST

- **The lift session's ordered moves**: (1) uncomment DEF?ON's
  translation_table row; (2) remove "DEF?ON" from terminal_table and
  decide the new terminal frontier (?FATAL stays; terminal syscalls
  per Layering ruling 5); (3) audit the run-loop and dispatch-site
  interactions for a formerly-terminal translated entry (Machine.cpp
  ~299-337: terminal_bits precede translated_bits — both roles' paths
  change when the bit clears); (4) build the fault injector (a
  synthetic positive-type unhandled condition — none exists in
  gameplay; this IS the parked provocation criterion); (5) capture-diff
  every wrapper NATIVE vs master RETURN per shape, re-derive the
  c=1/entry-c splits (WADC vs NLDAI paths) against captures — the
  likeliest bits to be wrong, resting on emulator-source reading
  alone; (6) re-run both standard triggers expecting "(native)"
  rtcalls and a DEEPER detach.
- **DEF?ON's -1 path natively requires the resume flag** ([area+0x16]
  bit15) SET — never observed. Witnessing a native DEF?ON RESUME under
  lockstep is the Layering validation hook; the injector should
  construct it deliberately.
- P?DEFON's resignal pre-checks duplicate the sibling's fallback
  conditions by construction; if o_signal.cpp ever grows a NEW fallback
  reason, p_defon.cpp's gate list must grow with it. Grep anchor:
  "(native-fallback:" in both files.
- The type==2 RESUME path returns normally into DEF?ON (ef26 WRTN) —
  if it ever fires natively, lockstep verifies a DEF?ON RESUMPTION,
  exactly the event Layering.md's validation hook wants witnessed.
- rt::o_area vs the raw wsb-0x40 offsets throughout o_signal.cpp:
  left as-is (no churn in validated files); the DEF?ON session may
  want to unify.

## 6. Validation evidence

- run1 (CONVERSION): 0 divergences; rtcalls tail
  `O?SIGNAL(native) ret=7017E3EF`, `I.GOTO(native) ret=7015FBAF`;
  clean write-back; rtcov QUEST1 1418 / QUEST2 921 of 9828.
- run2 (QUEST_FAIL_OPEN + capture): 0 divergences;
  `?LIB_ERROR(native)` (signal 1) then
  `?LIB_ERROR(native-fallback: no-handler/terminal-bound)` (signal 2);
  clone Thread halt @1,172,198 insns; master death path through
  ?FATAL; O?AREA master ENTRY/RETURN pair seq=36 matching the
  translation's footprint bit-for-bit (values in DERIVATION §2).
- Exact commands: SessionPlan.md session record (this session).
