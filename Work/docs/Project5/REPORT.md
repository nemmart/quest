# Project 5 — REPORT (DEF?ON lift + fault injector)

## 1. Status

| Item | Status |
|---|---|
| Part A lift (register DEF?ON, terminal_table change, audit) | DONE, both triggers 0 divergences, detach at 7017F036 (?FATAL) |
| Part B injector (QUEST_INJECT) | DONE, symmetric, one shot per process |
| Shape 1 handled | PASS — native chain + handler + I.GOTO unwind, 0 div |
| Shape 2 unhandled cascade | PASS — predicate-forced whole fallback, ?FATAL detach, authentic master death, 0 div |
| Shape 3 resume branch | PASS — **first execution in the program's history**: DEF?ON(native) ret=7017EE40, game continued, 0 div |
| DEF?ON lockstep status | Native paths VALIDATED for: exhaustion dispatch, -1 resume, terminal-bound fallback prediction. Still cold: type>0 (P?DEFON composite), type==2 C?INIT resume, [-5..-2] resignal — see §5 |

## 2. Tree edits (solo-session audit list)

- `hw/RTStubs.cpp`: DEF?ON translation_table row enabled; "DEF?ON"
  removed from terminal_table (comment records the lift); QUEST_INJECT
  parse in initialize(); RTStubs::inject_fire(); CallStack include.
- `hw/RTStubs.hpp`: injector statics + inject_fire declaration
  (tripwire-commented).
- `hw/Machine.cpp`: run_steps injection hook (after the QUEST_TERMINAL
  check, before the native_break block — so a clone-side native
  transfer from the injected call flows into the existing break
  handling); OSProcess include.
- `hw/RTBridge.hpp`: `entry_return()` accessor (the captured LCALL/
  XCALL return address; needed by the resume continuation).
- `os/OSProcess.hpp/.cpp`: `inject_armed` flag, armed for QUEST
  clients only (both roles and single-machine; never QUEST_SERVER).
- `runtime/o_signal.cpp`: exhaustion path — predicate-gated registry
  dispatch of translated DEF?ON; resume-continuation handling
  (pc3==DISPATCH_RET → clear break, arm pending to entry_return);
  fallback reason reworded to "(no handler — terminal-bound,
  emulating for terminal pairing)" (DEF?ON is no longer the terminal).
- `runtime/def_on.cpp/.hpp`: rt::def_on_would_run_native (pure-read
  outcome predicate, kept beside defq_on's gates); staged banner
  updated to REGISTERED.
- `runtime/p_defon.cpp`: removed an unused static helper (`rd`) that
  had been generating the tree's only warning — a Project 4 blemish
  the full-build grep pattern missed (the PROMPT's gotcha about
  warning-grep false-positives cut both ways).

## 3. Interfaces I expose / I consume

Expose: `rt::def_on_would_run_native(machine, type, key2)` — reads
the flag itself from [wsb-0x2A] (see DERIVATION §1; do NOT add a flag
parameter back: o_qsignal's store precedes prediction, shorthands
correctly read stale). `RTBridge::entry_return()`.
Consume: unchanged Project 1/4 interfaces. HAZARD RULE EXTENDED: the
fallback-gate lists must stay identical across o_signal.cpp's
predicate use, def_on.cpp's gates+predicate, and p_defon.cpp — a new
fallback reason in any one must be added to all (grep anchor
"(native-fallback:"), or a predicate/gate disagreement surfaces as
count skew at a terminal pair.

## 4. Shared-doc / spec corrections

a. **The PROMPT's :RESUME mechanism ("set bit15 at injection time")
   cannot work**: O?SIGNAL overwrites the flag wide on every call.
   Implemented instead as a 4-arg raise with a -1 flag cell — the
   authentic resumable-raise shape (DERIVATION §1).
b. **The PROMPT's shape-1 "type=positive" guess**: every observed
   game handler is a type=-1 catch-all (chain_search exact-match;
   all 26 O.ON sites); handledness is controlled by SITE SCOPE, not
   type. Shape 1 injects -1 at an in-scope pc; a positive type
   anywhere gives the resignal cascade instead (a bonus shape,
   exercised implicitly by shape 2's analysis, natively still cold).
c. o_qsignal logs "(native)" before signal_dispatch can still fall
   back, so a predicate fallback produces two rtcalls lines for one
   call (seen in shape 2). Cosmetic; pre-existing logging order.

## 5. Open questions / integration hazards

- **Still-cold DEF?ON native paths**: type>0 through P?DEFON (needs a
  registered handler for the RESIGNAL's type — no game handler
  matches type 6/positive; would need a handler-registration
  injection, a different mechanism), type==2 C?INIT resume (needs a
  type-2 raise inside... same limitation), [-5..-2] resignal.
  Recommend PARKING: they are guarded by the same predicate/gate
  pattern that shapes 1-3 validated, and their first live firing
  under lockstep will be verified, not silent.
- **?LIB_ERROR's pre-check not lifted**: lib_error still falls back
  whole on signal_has_handler(-1,0)==false even when DEF?ON would
  resume natively. Correct (symmetric) but conservative; lifting it
  means threading the same predicate + resume continuation through
  lib_error's two gates. Deferred deliberately — the natural triggers
  depend on it and this session validated them unchanged.
- The injector is one-shot per process by design; multi-shot would
  need re-arm semantics and interacts with the disarm-on-fire
  ordering under lockstep (both engines disarm independently at the
  same pc — symmetric).
- compare_pair's count exemption for native_span pairs is load-
  bearing for the resume continuation; if the exemption rules change,
  shape 3 is the regression test.

## 6. Validation evidence (exact commands; expected values from these runs)

All from `Work/c_src` builds (warning-free), scratch dirs with fresh
`cp -r QUEST`, `-lockstep -silent`, driver in the same shell, login
CL/Claude/quest/Y/space/F.

- **Lift regression 1** (dir p5a): no env; M→n→abc driver
  (Project1/drive_move2.py). Expect: `grep -c "LOCKSTEP DIVERGENCE"
  log` = 0; trace tail `O?SIGNAL(native) ret=7017E3EF`,
  `I.GOTO(native) ret=7015FBAF`.
- **Lift regression 2** (dir p5b): `QUEST_FAIL_OPEN=USER_DATA_FILE`,
  L→P→continue driver, wait ≥20 s after "driver done". Expect: 0
  divergences; stderr `Lockstep: ordinal 0 DETACHED at 7017F036
  (?FATAL) — clone halted, master continues unverified`; the octal
  death traceback on the driver terminal ending `from fp=0,
  pc=16005765124`; trace `?LIB_ERROR(native-fallback:
  no-handler/terminal-bound) ret=7017E306`. (One run of this trigger
  showed run-to-run variance — the second signal handled elsewhere
  and no death; re-run reproduces the cascade.)
*(Note, Aug 13 2026: under the crossings-only checker —
docs/CrossingsChecker.md — every injected raise now pairs AT the
O?SIGNAL entry before dispatch, so each shape's pair sequence below
gained exactly ONE entry pair (7017EDED, strict counts) versus these
transcripts. Deliberate improvement, not drift; do not chase it when
diffing.)*

- **Shape 1** (dir s1): `QUEST_INJECT=7016EC74:-1:0x2006`, L→P
  driver. Expect: 0 divergences; two stderr `INJECT firing at
  7016EC74` lines (master+clone); trace `O?SIGNAL(native)
  ret=7016EC74` then `I.GOTO(native) ret=7016EC74`; terminal shows
  the LIST_PLAYERS handler's "Couldn't access user_data_file"; game
  continues; clean ESC exit.
- **Shape 2** (dir s2): `QUEST_INJECT=70176AA7:-1:0x2006`, login-only
  driver. Expect: 0 divergences; two INJECT-firing lines; trace
  `O?SIGNAL(native)` then `O?SIGNAL(native-fallback: no handler —
  terminal-bound, emulating for terminal pairing)`; stderr DETACHED
  at 7017F036; authentic master death.
- **Shape 3** (dir s3): `QUEST_INJECT=70176AA7:-1:0x2006:RESUME`,
  login + M→n driver. Expect: 0 divergences; NO detach; trace
  `O?SIGNAL(native) ret=70176AA7` then `DEF?ON(native)
  ret=7017EE40`; play continues to the direction/turns prompts.

## 7. The decision this project settles

The parked M3 provocation criterion is answered by construction: M3
error-path validation = the two natural triggers + the three injected
shapes, each at least once under lockstep. Sufficiency opinion, as
requested: SUFFICIENT for the lift's claims (every dispatch decision
the predicate can take was exercised on at least one side, and every
exercised path paired at 0 divergences), NOT sufficient as a claim
about DEF?ON's full surface — the three cold paths in §5 remain
derivation-validated only. Since each is fail-safe (predicate false →
symmetric emulation) and self-announcing when first reached
natively, I recommend accepting the criterion and parking the cold
paths rather than building handler-registration injection now.
