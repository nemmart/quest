# Project 8 — REPORT (Phase 2: the stack-free L2, Stages A+B)

Per SharedProtocol.md REPORT format. Solo implementation session,
Aug 13–14 2026. Code changes only in c_src/runtime/* (new:
error_handler.hpp split into three implementations), Launch.cpp
(-handler flag), Makefile, hw/RTStubs.* (H7 arming), and mode gates in
runtime/frames.cpp + runtime/o_signal.cpp. NO checker changes
(CheckerHistory.md: no new generation, no Generation-2 correction
needed). NO contract or NativeDesign edits — corrections are recorded
below as corrections (§4), per the prompt's discipline.

## 1. Status

| Stage | Status |
|---|---|
| A — error_handler_api carve-out, mv_error_handler only | **DONE, gate green** (§2) — pure refactor, zero behavior change |
| B — native_error_handler + check_error_handler, matrix in CHECK mode | **DONE, matrix green** (§3) — one genuine design gap caught and fixed by the check machinery itself (§4 c1) |
| C — flip default to native, matrix in NATIVE mode | **BLOCKED — two independent design-level failures of native-alone mode, both traced to root cause (§6). Needs a user ruling before any further work.** |

Landed default: `-handler=check` semantics (HandlerMode::CHECK) — the
both-in-parallel configuration, permanently valid per the prompt
("staging is instantiation"; the comparison machinery is not
scaffolding). mv = the attic run (`-handler=mv`, verified green
post-landing); native = reachable (`-handler=native`) but fails as
designed (§6).

## 2. Stage A evidence (mv only, pure refactor)

Warning-free build. All runs `-lockstep`, default (mv) mode, scratch
QUEST copies, drivers docs/Project1/drive.py / drive_move2.py.

| Run | Result |
|---|---|
| Login + L→P (drive.py) | 0 divergences |
| M-trigger (drive_move2.py, abc CONVERSION raise) | 0 div; rtcalls chain = the documented shape: ?LIB_ERROR(native) → O?SIGNAL(native) → I.GOTO(native) → O.REVERT → I.EPILOG → I.STOP detach; every refactored entry dispatched native |
| QUEST_FAIL_OPEN=USER_DATA_FILE + drive.py | 0 div; both signals through the documented path |
| QUEST_INJECT=7016EC74:-1:0x2006 (shape 1) | 0 div; late detached ?FATAL 7017F036 — matches the documented driver-timing variance row |
| QUEST_INJECT=70176AA7:-1:0x2006 (shape 2) | 0 div; detached ?FATAL 7017F036 exactly as documented |
| QUEST_INJECT=70176AA7:-1:0x2006:RESUME (shape 3) | 0 div; resumed, played on, I.STOP detach |
| QUEST_TERMINAL=7016EC74:ABORT | 0 div; TERMINAL-ABORT banner, FS::save_all suppressed |
| QUEST_CAPTURE=7017ED9B (O.ON) DEST=70180400 | 7 master-RETURN vs clone-NATIVE pairs, **0 differing footprint words** — the one entry whose write order Stage A reshuffled (image → api node-writes → helper residue; address-disjoint) proven bit-identical |

Flag behavior: -handler=mv accepted; native/check rejected with a
Stage-B message (later enabled); unknown values rejected.

## 3. Stage B evidence (CHECK mode = default)

check_error_handler: mv and native both execute per call; COMPARED
outcome fields verified equal BEFORE the common wrapper stages
registers; mismatch = loud throw naming method, field, both values.
Side-effect ownership strict: mv owns every real-memory write, native
owns TaskL2State. Read methods (sig_record, resume_flag, has_handler)
are compared too — every read cross-checks the re-hosting.

| Run (CHECK mode) | Result |
|---|---|
| Login + L→P | 0 div, 0 mismatches |
| M-trigger | first run: **check MISMATCH caught (§4 c1)**; after fix: 0 div, 0 mismatches |
| FAIL_OPEN | 0 div, 0 mismatches (terminal-bound cascade symmetric: mv maintains memory) |
| Inject shape 1 | 0 div; detached ?FATAL 7017F036 (documented variance row) |
| Inject shape 2 | 0 div; detached ?FATAL 7017F036 exactly |
| Inject shape 3 (RESUME) | 0 div; fired ×2, play continued — **H6 native tail proven** (check mode takes the native tail path, §5 H6) |
| QUEST_TERMINAL=7016EC74:ABORT | 0 div; TERMINAL-ABORT banner, save suppressed |
| **H7** QUEST_BAD_TOKEN=1 + M-trigger | ONE banner: `L2 ABORT-INTENDED: I.GOTO bad-chain shape (token ac0=00000100, link 00000000 under cursor 70001098, entry pc=7017EC7C)`; write-back suppressed; `aborting` silenced the checker (0 divergence spam) — the A3 composition end to end, once, on purpose |
| ?LIB_ERROR B-window capture (FAIL_OPEN + QUEST_CAPTURE=7017E33A DEST=70001060) | 0 div. Tool finding: the capture instrument cannot produce same-instant A/B pairs for TRANSFER-shaped entries (one-shot arming + no RETURN block; master yields a single pre-body ENTRY). The clone's post-wrapper window shows the written B record {8000, 100A} = the documented fail-open code; the run's 0 divergences with both engines' L1 reading B through compare-on-read mediation is the effective SHARED-PROTOCOL evidence. Footprint captures of contract-private storage: retired per E7–E10 (conformance statement) |
| mv attic re-verify (post-landing, -handler=mv login) | 0 div |

Not exercised: store-"ABC" node recycling (REVERT-then-re-ON at the
same record). No scripted driver reaches a store; the driven runs
bracket I.PROLOG..I.EPILOG per turn, so every O.ON allocates into a
fresh record. Disposition: the reuse/backstop outcomes are compared by
check_error_handler on EVERY O.ON call, and CHECK is the landed
default — the recycling shape is permanently guarded the first time
real play produces it. Carried on the revisit list (§7).

## 4. Corrections (as corrections; no doc edited)

- **c1 — NativeDesign §1 cut row is incomplete; found by the check
  machinery on its first M-trigger run** (`cut outcome mismatch on
  wsp_restore: mv=70001188 native=70001180`). O.ON's allocate-path
  write `caller_frame[+2] := frame−4` (Contract §3.4, labeled
  "bookkeeping") is the establisher's LIVE landing-snapshot update:
  frame−4 = O.ON entry wsp + 8 = the post-allocate wsp, so a later
  unwind restores wsp ABOVE the node kept alive on the stack. The
  design's EstablisherRecord.wsp_snapshot description ("entry wsp+4,
  I.PROLOG's write") never connected that second writer. Fix: native
  allocate updates rec.wsp_snapshot = entry_wsp+8 (reuse path: no
  update). The contract's write LIST was complete; its semantic
  annotation ("bookkeeping") undersells a normative role — contract
  friction, no edit made.
- **c2 — Contract §3.3 Outputs row vs validated code** (carried from
  Stage A): the unwind row says ac0 = cursor's saved-ac0 at the
  label, but the capture-validated landing stub loads ac0 =
  [target+2]. Code wins; row uncorrected in place, flagged here.

## 5. Hazard spine disposition (H2–H7)

- **H2 token pin** — ac1 = the real establisher frame address,
  exactly today's value (native select returns record.frame; check
  compares it as "frame (token)" on every select). Nothing
  dereferences it for handler state. No id minting — that is M4's
  move (§7).
- **H3 wsp reservations** — normative reservations stay in COMMON
  wrapper code (I.PROLOG +4, O.ON allocate +8, I.GOTO snapshot
  restore staged from the compared CutOutcome.wsp_restore). Login
  regression green in mv/check; c1 is precisely H3's snapshot
  interacting with the allocate reservation — caught by comparison,
  not by blindness.
- **H4 resume-flag staleness** — flag written ONLY by the O?SIGNAL
  entry path (api.set_resume_flag from o_qsignal's argc>3 store);
  shorthands never write it; def_on_would_run_native still takes no
  flag parameter and reads through the api. In check mode every read
  compares mv's cell against native's struct — staleness semantics
  verified equal on every gate evaluation in every green run.
- **H5 fallback-gate parity** — the predicate lists live once and are
  consulted through the api in all modes; terminal-bound decisions in
  the green matrix produced identical fallback choices (shape 2 /
  FAIL_OPEN cascades paired to their documented detaches).
- **H6 DISPATCH_RET** — DONE, the one pairing-adjacent change, proven
  on shape 3. In native/check modes the arm-whole-span-emulation
  continuation is replaced: after DEF?ON's native resume returns
  DISPATCH_RET, the wrapper reads the resume flag through the api
  (live shape: sign bit set, guaranteed by DEF?ON's own gates —
  flag==0 throws instead of guessing at cold escalation branches),
  restores wfp to the raiser's frame and wsp to the bridge entry
  position, and takes bridge.native_return() — the EE44 WRTN staged
  image-free from saved state. Pairing is the standard translated-L2
  exit crossing at the raiser's post-call pc; the master's
  run-to-return ends there unchanged; NO checker change. mv mode
  keeps the bit-faithful arm-span attic path. The genuine
  handler-WRTN arrival at EE40 keeps its depth-0 rendezvous
  (untouched, dormant on live paths); note: in native-alone mode that
  path would emulate the tail against unwritten cells — moot while
  Stage C is blocked, listed in §7.
- **H7 new inject shape** — DONE as env QUEST_BAD_TOKEN=1 (one-shot,
  consumed at the first non-local I.GOTO, native/check only — in mv
  mode a clone-side corruption would desynchronize the attic instead
  of exercising the abort). Evidence row in §3: one banner, token and
  pc named, save suppressed, checker silenced.

Rulings applied as given: (a) I.EPILOG mismatched pop → abort_world
(save=false)+throw in native_error_handler::disestablish (fires in
check mode too — it is the ruling, not a comparison); (b) walker
outputs dropped entirely — native record_raise hosts the triple only;
(c) no chain cap; (d) I.GOTO bad-chain shapes ABORT-INTENDED in
native/check (both the non-descending/non-positive-link branch and
the walk guard), mv keeps the bit-faithful fallback as the living
attic; (e) the error_handler_api landing shape exactly as specified —
outcome structs compared before staging, residue fields named.

## 6. Stage C blockers — native-alone mode fails, two independent root causes

Both are design-level consequences of Registers E8/E9/E10 ("the clone
writes nothing to contract-private storage"), which check mode masks
because mv maintains the memory. Both need a user ruling; neither has
an in-scope fix (re-materializing cells, translating the terminal
cascade, and checker changes are all outside this project's charter).

**B1 — stale-stack reuse makes E8/E9 empirically unsound (bites plain
login).** Native login diverges at a shared gate-0 syscall stub
(trap_pc 7017FDED): ac0 master=700010E8, clone=80000000, all other
state equal. Traced with a temporary probe (since removed): the
arrival's ac3 pins the computing routine near 7017E2C8 — a syscall
packet-builder writing through ac3=700010E8. That packet occupies
early-stack space where a frame previously lived; mv's L2 wrappers
wrote that frame's contract-private cells (node words, relocated
image, frame slots); the stack popped; LOGON reused the space for the
packet; the builder's dataflow passes through a cell it does not
itself initialize. The master's value there is DETERMINISTIC STALE
STACK — the original program's actual behavior includes its garbage —
while the native clone still holds the boot-era frame word
(0x80000000 = a psr<<16 frame wide). Conclusion: "contents unwritten"
is unsound for cells that later become L1-visible through ordinary
stack reuse without an intervening write. The register-file compare
catches it at the next crossing, as designed.

**B2 — whole-fallback re-emulation cannot pair at terminal cascades.**
Shape 2 / FAIL_OPEN's second signal fall back to emulating the
original O?SIGNAL body toward the ?FATAL terminal. The emulated
select/walker loops read [wsb−0x40]: the master walks its real chain;
the native clone reads 0 (never written) and walks nothing.
Instruction counts skew, and terminal pairs compare counts strictly
(the count exemption needs native_span on both sides —
hw/Lockstep.cpp; the very reason the terminal-bound fallback exists).
Analysis grounded in Contract §11 ("between crossings it may do
anything" — but the crossing/count obligations at the terminal are
exactly what symmetric emulation was providing). Not independently
runnable as evidence: B1 fires first on every native run (login
precedes any cascade), so B2 rests on the contract text plus the
o_signal.cpp pairing comment.

Options sketched for the ruling (not decided here): (i) native mode
retains the bit-faithful cell writes while TaskL2State stays the sole
READ authority — weakens E8/E9/E10 from "unwritten" to "semantically
unread", collapses B1 and B2 simultaneously, keeps the M4 seeds; (ii)
defer the native-alone flip to M4, where frames stop being real and
the checker generation changes anyway — CHECK remains the shipping
configuration for M3b; (iii) something else. Check mode is fully
validated either way and is the landed default.

## 7. M4 revisit list

- Token re-mint as a static-area id when frames stop existing (H2's
  bought freedom; the address side-table concern from NativeDesign §2
  goes away with the frame population).
- Chain records' frame/wsp fields become the shadow-accounting seeds
  (CheckerHistory.md Generation 3).
- B1/B2 resolution interacts with M4 directly: once frames are not
  real, stale-stack determinism (B1) must be re-derived for whatever
  replaces the stack, and the terminal-cascade accounting (B2) falls
  into the Generation-3 checker discussion.
- The handler-WRTN-return tail path (EE40 depth-0 rendezvous) in any
  future native-alone mode: emulates against unwritten cells today;
  dormant (live handlers unwind via I.GOTO); needs either translation
  or the B1-style ruling.
- store-"ABC" recycling shape: first real-play occurrence is guarded
  by the check default; drive it deliberately if a store-reaching
  script ever exists.
- Consolidation of select_frames / signal_walker / chain_search
  (exported but mv-internal since Stage A).

## 8. Interfaces touched

- runtime/error_handler.hpp — error_handler_api, HandlerMode, outcome
  structs (COMPARED vs MV-RESIDUE fields annotated), factory decl.
- runtime/mv_error_handler.{hpp,cpp} — Stage A verbatim lifts;
  factory/mode global moved out in Stage B.
- runtime/native_error_handler.{hpp,cpp} — NEW: TaskL2State keyed by
  (process, wsb), lazy; rulings a/d abort sites; c1 fix.
- runtime/check_error_handler.cpp — NEW: the comparison composite +
  handler_mode global + error_handler() factory. Default CHECK.
- runtime/frames.cpp — i_goto mode gates (ruling d aborts), H7
  one-shot; o_signal.cpp — H6 native tail behind the mode gate;
  def_on.cpp / p_defon.cpp / o_on.cpp — unchanged since Stage A (api
  consumers).
- hw/RTStubs.{hpp,cpp} — bad_token_armed + QUEST_BAD_TOKEN parse.
- Launch.cpp -handler=mv|native|check; Makefile.
- hw/Machine.cpp — NO landed change (temporary probes added and
  removed within the session; post-removal mv and check logins
  re-verified green).
