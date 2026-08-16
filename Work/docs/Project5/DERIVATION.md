# Project 5 — DERIVATION (injection staging + lift pairing facts)

New byte-level facts only; everything else is in Project4/DERIVATION.md.

## 1. The resume flag is the 4th O?SIGNAL argument's sign bit

DEF?ON's resume test (ef47-ef49) reads the NARROW word at
[area+0x16] and skips on bit15 clear. [area+0x16] is the HIGH word of
the wide at [wsb-0x2A] — the store O?SIGNAL's entry path writes on
EVERY call (`flag = argc>3 ? *arg4 (narrow, sign-extended) : 0`,
Project2 derivation, o_signal.cpp EDF9). Consequences:

- A negative 4th argument (narrow -1 sign-extends to wide -1, high
  word 0xFFFF, bit15 set) arms the resume branch; any argc<=3 raise
  DISARMS it (stores 0).
- The PROMPT's ":RESUME = set bit15 of narrow [area+0x16] at
  injection time" cannot work: the injected call itself overwrites
  the poked bit before DEF?ON reads it. :RESUME is therefore
  implemented as a FOUR-ARG raise with a -1 flag cell — the shape a
  real resumable raise would have.
- The SHORTHAND entries (O.SCONVE etc.) never store the flag: a
  shorthand-raised signal reads whatever the previous raise left.
  Faithful in both engines (same bytes); noted for the record.
- "Consumed or sticky": the resume path writes nothing to the flag;
  it is STICKY UNTIL THE NEXT RAISE, and every raise rewrites it —
  effectively per-raise. Observed in shape 3: post-resume play
  continued with the flag still -1 and no ill effect.

## 2. Injection staging (the synthesized call site)

Template: DEF?ON's resignal staging (ef37-ef3d: value cells in
locals, XPEF the cell EAs argN-first, LCALL) + EagleStack LCALL
(frame wide (psr<<16)|argc, ac3=return, ovr=0, shadow
call_stack->call, then the registry lookup with the central
nested-span guard). The injector has no locals, so the value cells
are wide-pushed onto the stack first and their addresses pushed as
the args — layout identical in kind to a real PL/1 site (cells where
locals would be, below the arg pointers). Return address = the
injection pc, so a handled signal's unwind or a native resume puts
the game exactly where it was. The identical code runs on both
roles; only the clone's non-empty registry makes it dispatch native —
the same asymmetry a real LCALL has.

Registry lookup at O?SIGNAL entry on the master returns null
(OSProcess.cpp: register_all is CLONE-only), so the master sets
pc=0x7017EDED and the run loop's rt_sync entry block arms
rt_pending_return=ac[3] (the injection pc) exactly as for a real
call into a translated routine.

## 3. Lift pairing facts (the step-3 audit, empirically confirmed)

- Machine.cpp order of checks (both roles): terminal_bits precede
  translated_bits in the rt_sync entry block AND in the
  run-to-return escape. Removing DEF?ON's terminal bit means a
  fallback span passing DEF?ON's entry now CONTINUES emulating
  (in-range, != pending) through DEF?ON's body to ?FATAL's entry —
  the terminal escape fires there, on both engines, with equal
  in-span counts. Confirmed: trigger-2 post-lift detaches at
  7017F036 (?FATAL), one level deeper, 0 divergences.
- A native prefix before a TERMINAL pair skews the compared counts.
  Therefore native O?SIGNAL may dispatch native DEF?ON on exhaustion
  ONLY when DEF?ON's outcome is a shared native boundary. The
  pure-read predicate rt::def_on_would_run_native (def_on.cpp)
  mirrors defq_on's decision phase: true for the C?INIT resume,
  a found resignal, or the -1/plain-walk/flag-negative resume; false
  otherwise — in which case O?SIGNAL falls back WHOLE (the pre-lift
  behavior, now with an accurate log reason).
- DEF?ON's native RESUME returns to DISPATCH_RET (0x7017EE40), the
  emulated handler-returned tail — NOT a boundary. o_signal's
  exhaustion dispatch detects pc3==DISPATCH_RET, clears the inner
  native_break, and arms rt_pending_return to O?SIGNAL's own entry
  return (bridge.entry_return()): the clone emulates the EE40 tail
  and the unwind to the raiser's post-call pc, where the master's
  run-to-return also ends — a count-exempt native_span pair.
  Confirmed: shape 3, 0 divergences, DEF?ON(native) ret=7017EE40.

## 4. Sites (resolved from the trigger-2 death traceback, octal→hex)

Death chain: QUEST+0x5DB → START_TURN+0x7E2 → **REFRESH_SCREEN+0x14
(0x70176AA7)** → ?CLOSE_FILE(7017E306) → ?LIB_ERROR — the historical
unscoped raise site; REFRESH_SCREEN runs at login and every turn, so
it fires without QUEST_FAIL_OPEN. In-scope site: **0x7016EC74** in
LIST_PLAYERS — the pc immediately after its O.ON registration at
EC53 (also the ON-unit's unwind label; both roles of the same pc),
visited on every normal L→P.
