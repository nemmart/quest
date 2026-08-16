# Shared Protocol — Parallel M3 Translation Projects

Read this FIRST, whichever project you are. It freezes the interfaces the
three sessions share, so the merge works. Binding alongside METHOD.md.
Plan context: M3Plan.md. Machinery: TerminalDetach.md.

## The three projects

| | Routines | Words | Prompt |
|---|---|---|---|
| Project 1 | O.SEARCH→O.SET cluster (0x7017EDDD–EF05) | 296 | docs/Project1/PROMPT.md |
| Project 2 | ?LIB_ERROR, ?LIB_ERROR_CODE, ?DEFAULT_ERROR_HANDLER | 196 | docs/Project2/PROMPT.md |
| Project 3 | T?AREA, I.PROLOG, I.EPILOG, I.GOTO | 124 | docs/Project3/PROMPT.md |

## Hard merge rules

1. **Write only your own files.** Each project creates
   `runtime/<name>.{cpp,hpp}` for its routines and ONE derivation doc
   `docs/ProjectN/DERIVATION.md`. Do not edit any other source file, any
   shared doc, or another project's directory. Exceptions: you may append
   to `docs/ProjectN/NOTES.md` (your scratch) and you MUST write
   `docs/ProjectN/REPORT.md` (below).
2. **Nobody edits `hw/RTStubs.cpp`'s `translation_table` or the
   Makefile.** Report your would-be entries in REPORT.md; the integration
   pass merges all three and runs the end-to-end chain validation. (Until
   merged, your routines stay stubbed in the tree — that is expected.)
3. **Compile check**: you may add a temporary local target or use
   `g++ -c -std=c++17 -I. runtime/<file>.cpp` to prove your code builds.
   Leave the tree's `make` output byte-identical in behavior.
4. **Corrections to shared docs** (errors you find in METHOD.md,
   UNIMPLEMENTED.md, disassembly claims, this file...) go in REPORT.md
   under "Shared-doc corrections", not into the docs themselves.

## Frozen interface 1: rt::t_area (IMPLEMENTED — use it, don't re-derive)

`runtime/t_area.hpp`: `uint32_t rt::t_area(hw::Machine&)` returns
`machine.wsb - 0x29` — the per-task condition area address (derived from
T?AREA's body: `LDASB; XLEF 0,[sb-0x29]`; result returned in ac0 via the
saved-ac0 slot patch). Any native routine that needs the area calls
`rt::t_area(machine)` as plain C++. Project 3 owns the `emu_rt` wrapper
(frame residue + slot patch) and its empirical validation; Projects 1–2
just call the pure function.

Area fields observed so far (VERIFY against captures before relying on
them; offsets are word offsets from the area address):
`[+0x1]` last error code (?LIB_ERROR stores the signaled code there),
`[+0x3]` ?LIB_ERROR's message-buffer pointer (free-then-alloc),
`[+0x1E]` handler procedure address (?DEFAULT_ERROR_HANDLER installed by
?LIB_ERROR when `[+0x1E]`==0), `[+0x20]` its companion word (zeroed
alongside). `XLEF 2,[ac2+0x8]` after every T?AREA call suggests callers
actually work at area+0x8 — resolve and document what the +0x8 base is.

## Frozen interface 2: transfer pairing (IMPLEMENTED)

> **Addendum (Aug 13 2026, crossings-only checker —
> docs/CrossingsChecker.md).** Every rule in this section survives
> unchanged; wrappers were not modified. One ADDITION wrapper authors
> should know: dispatch to a TRANSLATED L2 entry is now DEFERRED at
> the call site — the pair rendezvouses AT the entry pc (argument
> state compared) BEFORE your wrapper runs, and your
> native_return/native_transfer still produces the exit rendezvous as
> before. Non-L2 leaf translations keep immediate dispatch and
> exit-only pairing. The nested-in-fallback guard below is unchanged
> and remains mandatory.

The condition chain does not return normally. The mechanism, already in
the tree and regression-tested:

- **Clone side**: end a native routine that transfers control (handler
  dispatch, unwind resume, LJSR continuation) with
  `return RTBridge::native_transfer(machine, target_pc);`
  The wrapper is responsible for ALL register/stack state at the target —
  there is no single convention; derive each site's from the disassembly.
- **Master side** (nothing for you to do): run-to-return now terminates
  when the emulated body's pc equals the pending return OR leaves the RT
  range `[RTStubs::start, RTStubs::stop)`. Handler dispatches and unwind
  resumes land in game code, so both engines' batches end at the identical
  pc and pair as a native_span.
- **RT-internal transfer targets** (one translated routine transferring
  to another RT entry, e.g. native ?DEFAULT_ERROR_HANDLER → O?SIGNAL):
  while the CALLEE is still emulated on the clone this is FORBIDDEN
  (no re-entering emulation mid-routine — METHOD.md §7). Design your
  wrapper to call the sibling's `rt::` function as plain C++ where the
  sibling is in YOUR project; where it is in ANOTHER project, code
  against the `rt::` signature you agree in REPORT.md ("Interfaces I
  expose / I consume") and note that the path validates only at
  integration. Keep a fallback-to-emulation entry gate meanwhile if a
  partial validation is possible.
- **Terminal targets INSIDE the RT range (?FATAL, I.STOP; at the time of
  writing also DEF?ON, since lifted to verified L2): do
  NOT native_transfer to them** (CORRECTED Aug 2026, found by
  Project 1): the master ends its run-to-return there via the terminal
  exception (terminal flag, no native_span) while a transferring clone
  ends via native_break (native_span, no terminal) — compare_pair sees
  span+terminal mismatch plus an instruction-count delta: structural
  divergence with identical machine state. Correct composition: decide
  on PURE READS before any store, then FALL BACK to emulation from
  entry (arm rt_pending_return) so both engines emulate to the
  terminal with equal counts. `native_transfer` remains correct for
  GAME-RANGE targets (handler dispatch, unwind resume, LJSR
  continuations), where the range-exit rule gives native_span on both
  sides. Never implement terminal subtrees.
- **Nested-in-fallback guard** (Project 1 finding): on the clone, if
  `machine.rt_pending_return != 0` at your wrapper's dispatch, an
  outer fallback span is re-emulating — return
  `RTStubs::entry_address(<name>)` WITHOUT re-arming
  rt_pending_return, so the inner routine emulates inside the span and
  both engines' instruction counts stay equal (they are compared when
  the span ends at a terminal). The integration pass audits all
  translations for this guard.

## Validation rules (unchanged, METHOD.md §5–6, §12)

- Derive from the disassembly; Opus-era `rt/`, `emu_rt/`, `types/` are
  reference-only.
- Empirical captures before gameplay: `QUEST_CAPTURE=<entry-pc-hex>`,
  `QUEST_CAPTURE_DEST=<word-addr-hex>`; diff master RETURN vs clone
  NATIVE blocks to 0 differing words. For transfer routines, capture the
  master's state at the RANGE-EXIT pc (the capture hook fires on pc
  match — use the observed target from a master-only run).
- Lockstep gameplay validation for whatever your project can exercise
  ALONE (your prompt says what that is); chain-level validation is the
  integration pass, not your responsibility.
- Every fallback path arms `machine.rt_pending_return = machine.ac[3]`
  (METHOD.md §12) — note this constant applies to RETURNING fallbacks;
  a fallback in a transfer-capable routine still works because the
  master's range-exit rule covers both endings.

## Triggers and environment

- Signal triggers, cheapest first: **`M` + direction (n/s/e/w) + `abc`
  at the "For how many turns?" prompt** — CONVERSION, handled, fires
  same-turn with NO navigation and NO fault injection (validated:
  full chain through native I.GOTO, ret 7015FBAF, 0 divergences);
  `QUEST_FAIL_OPEN=USER_DATA_FILE` + login + `L`→`P` (failed open,
  handled, I.GOTO ret 7016EC74; pressing continue can reach the
  unhandled second-signal path — now a clean DEF?ON detach); store
  "ABC" at a purchase prompt (third shape, needs map navigation).
- Environment gotchas (turn cadence ~49 s/turn on a slow container, the
  L sub-menu, ESC = quit, login sequence, stdbuf, scratch-copy QUEST/):
  NextSession.md "Environment gotchas". Believe them.
- `QUEST_TERMINAL=<hex-pc>` — test terminal point, useful for
  master-only controlled runs.

## Integration outcomes (Aug 2026 — all three projects merged)

All three merged, chain validated end-to-end: both triggers run the
full raise-to-resume chain native on the clone (0 divergences); the
unhandled second signal predicts terminal-bound via the strong
`rt::signal_has_handler` (select_frames(-1,0), owned by o_signal.cpp),
falls back whole, and pairs terminally at DEF?ON. Two integration
fixes worth knowing: (1) the central nested-span guard initially
covered only LCALL/XCALL — the LJSR/XJSR dispatch sites in
EagleGeneral.cpp were unguarded, found live as native I.FREEW (LJSR'd
at 0x7017E37E) running inside the emulated ?LIB_ERROR fallback span
(93-instruction count skew at the DEF?ON terminal pair); all four
sites are now guarded. (2) `native_registry.lookup(entry) != nullptr`
does NOT mean translated — every entry has a stub; use
`RTStubs::translated_bits` (Project 2's correction).

## REPORT.md format (required, per project)

1. **Status** — per routine: derived / translated / capture-validated /
   lockstep-validated / blocked(why).
2. **translation_table entries** — exact `{ "SYMBOL", emu_rt::fn }` rows
   to merge, in registration order, with LEAF/TRANSFER/terminal notes.
3. **Interfaces I expose / I consume** — `rt::` signatures, with any
   deviations from this protocol called out loudly.
4. **Shared-doc corrections** — errors found, with evidence.
5. **Open questions / integration hazards** — anything the merge pass
   must check (ordering constraints, shared globals, capture commands to
   re-run end-to-end, expected values FROM THE EXACT COMMANDS —
   METHOD.md §10).
6. **Validation evidence** — capture diffs, lockstep run summaries,
   trigger transcripts.
