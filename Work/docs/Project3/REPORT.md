# Project 3 — REPORT

Session: Aug 11, 2026. Scope per docs/Project3/PROMPT.md: T?AREA
(wrapper + validation), I.EPILOG, I.PROLOG, I.GOTO. Full derivation
and evidence: docs/Project3/DERIVATION.md.

## Status

| Routine | State |
|---|---|
| T?AREA | TRANSLATED + VALIDATED (capture 0-diff; 17 native calls in lockstep, pair-gate clean) |
| I.PROLOG | TRANSLATED + VALIDATED (capture 0-diff ×3 windows; live in every session) |
| I.EPILOG | TRANSLATED + VALIDATED (capture 0-diff; live in every session) |
| I.GOTO | TRANSLATED + VALIDATED on the handled-signal shape (#13, capture 0-diff, full signal chain resumed under lockstep). Store-"ABC" shape (#26) not exercised natively — turn cadence made map navigation infeasible in this container; instructions + precedent argument in DERIVATION "Open items". Error/cross-stack shapes: side-effect-free fallback by design. |

Zero lockstep divergences in every run. The signal-unwind case that
UNIMPLEMENTED.md lists as the last run-to-return gap is closed by the
range-exit rule + these translations (verified live: injection →
?LIB_ERROR → T?AREA×17 → handler → native I.GOTO → resume → clean
quit).

## Files

Written (mine): `runtime/frames.{cpp,hpp}`,
`docs/Project3/{DERIVATION.md,REPORT.md}`.

Edited (flagged deviation, approved this session): `runtime/t_area.cpp`
and `runtime/t_area.hpp` — ADDITIVE only: the `emu_rt::t_area` wrapper
and its declaration. The frozen `rt::t_area` signature and body are
untouched.

Not edited: hw/, debug/, os/, Makefile, RTStubs.cpp — temporary
validation edits reverted (below).

## Temporary registrations (REVERTED; integrator should re-add)

`hw/RTStubs.cpp`:
- includes: `#include "../runtime/t_area.hpp"`,
  `#include "../runtime/frames.hpp"`
- `translation_table[]` rows:

      { "T?AREA", emu_rt::t_area },
      { "I.PROLOG", emu_rt::i_prolog },
      { "I.EPILOG", emu_rt::i_epilog },
      { "I.GOTO", emu_rt::i_goto },

`Makefile`: `runtime/frames.cpp` added to the source list (after
`runtime/t_area.cpp`).

All validation in this report was run with exactly these six lines
present; they were reverted afterward and the tree rebuilt clean.

## Proposed hw/debug diff (NOT applied): CallStack::native_unwind

PROMPT.md asked for a decision on what the native unwind does to
debug::CallStack. DECISION IMPLEMENTED: the native I.GOTO calls the
existing public `call_stack->call_return(0x7017EC9D)` — exactly what
the emulated WRTN does — so master and clone shadow stacks evolve
identically, including the benign mismatch notice (UNIMPLEMENTED.md
§8) and the stale entries for frames skipped by the cut (later WRTNs
pop them with further benign notices, again identically on both
engines). Backtraces therefore stay exactly as sane as they are today
on the emulated path, and no shared file changes.

For the record, the cleaner-backtrace alternative, if ever wanted:

    --- debug/CallStack.hpp
    @@ class CallStack {
       void call_return(int32_t return_address);
       void native_return(int32_t return_address);   // return from a native translation (no WSAVS frame)
    +  // Non-local unwind (native I.GOTO): pop every shadow frame whose
    +  // recorded frame pointer lies above target_wfp, silently. Keeps
    +  // backtraces exact through a native unwind instead of leaving
    +  // stale entries + mismatch notices.
    +  void native_unwind(int32_t target_wfp);

    --- debug/CallStack.cpp
    @@ after native_return():
    +void CallStack::native_unwind(int32_t target_wfp) {
    +  while(!call_stack.empty() &&
    +        call_stack.back().frame_pointer > target_wfp)
    +    call_stack.pop_back();
    +}

(`frame_pointer` = the value recorded by `augment`; entries never
augmented — LJSR-only frames — have none and would need the entry
default checked before adoption.) RECOMMENDATION: do NOT adopt while
the master runs the emulated body — the clone's shadow state would go
quiet/clean while the master keeps the stale-entry behavior, and
every later benign notice would appear on one engine only, which
reads like a divergence in session logs. Revisit only if the emulated
WRTN path is also routed through it.

## Integration notes / findings for other projects

1. **Capture sharp edge** (hit twice this session): with translations
   registered and `QUEST_CAPTURE` set but `QUEST_CAPTURE_DEST` unset,
   `native_footprint`'s ac2-based DEST window can read an unmapped
   address (ac2 after register restoration is caller data, e.g. byte
   pointer 0xE000218B) and the throw kills the clone mid-batch,
   presenting as a LOCKSTEP DIVERGENCE with
   `exception=Page does not have read permission`. Workaround
   mandatory: always set `QUEST_CAPTURE_DEST`. Proposed hardening in
   DERIVATION.md (debug/ not edited).
2. **area+0x8 resolved** (SharedProtocol.md open question): the live
   error record starts at area+0x8 = wsb−0x21; area+0..7 has no
   accessor anywhere in the RT range; SharedProtocol's field offsets
   (+0x1/+0x3/+0x1E/+0x20) are from the +0x8 base. Projects 1–2:
   use `rt::t_area(machine) + 8` as the record base.
3. **Frame layout table** (DERIVATION.md) is the shared contract;
   O_ON.md's @[frame+0xA] and [frame+0x8] facts confirmed against it.
   Project 1 should cross-check O.SET's deep walker when derived.
4. **quest-rt.addrs**: 0x7017EC9D..9F is code (I.GOTO's landing
   stub), currently classified data — regeneration item.
5. **Injection reach**: `L`→`P` fires handler #13 (the cheap I.GOTO
   trigger). `L`→`A` does NOT fire the injection (no ?OPEN_FILE with
   zero allies). Two signals fire per L→P session; only the first
   reaches I.GOTO (the second ?DEFAULT_ERROR_HANDLER resolves without
   unwinding — consistent with catalog #13 being "handler 1 of 2").
6. **Second-shape validation recipe** (interactive session, minutes):
   store tile → type `ABC` at the purchase prompt, with
   `QUEST_CAPTURE=7017A520 QUEST_CAPTURE_DEST=70001098`; diff master
   snapshot vs clone NATIVE (pc=70179BF7).

## M3 exit-criteria mapping (M3Plan.md, this project's share)

- Four routines translated, derived instruction-by-instruction: DONE.
- Capture-validated to zero differing words before gameplay: DONE
  (per-routine table in DERIVATION.md).
- Lockstep play with translations registered, zero divergences,
  signal path exercised end-to-end with native unwind + resume: DONE
  (one of the two documented I.GOTO triggers; the second documented
  as an open item with recipe).
- Fallbacks arm rt_pending_return: DONE (T?AREA argc guard; I.GOTO
  pre-walk classification, side-effect-free).
- Shared files untouched in the deliverable; temporary registration
  reverted; CallStack decision documented with the proposed diff:
  DONE.

## Integration addendum (reviewer, Aug 11 2026)

Open item 1 CLOSED: the `M` + direction + `abc` trigger (user-supplied)
fires the CONVERSION chain same-turn with no navigation; native I.GOTO
ran a second distinct shape (ret 0x7015FBAF, different handler/label/
walk than #13's 0x7016EC74) under lockstep, 0 divergences, on the
P3-integrated tree. Store shape remains a third data point if ever
wanted, no longer needed for validation confidence.
