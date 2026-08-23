# Project 18 — M4b widening, tranches A & B (COORDINATOR REPORT, generation half)

**Session:** Aug 22 2026 (coordinator). **Status: the push_map for
tranches A (515 sites) and B (20 WPSH sites) is GENERATED, VALIDATED,
and COMMITTED. The emulator-side work (WPSH multi-slot hook + battery)
is queued to the runner as task 021.** No mechanism changes — this
applies the P16/P17-proven M4b to all flat-LCALL arg sites.

## 1. Scope (verified counts, reconciled against quest.callsites)

Of 754 game→game call sites:
- **Tranche A — 515**: flat LCALL, args all single-word (XPEF/LPEF/
  XPEFB/LPEFB). The DIST case × 515; no new mechanism.
- **Tranche B — 20**: flat LCALL to TERRAIN/TERRITORY whose windows
  include a WPSH writing 2–3 args in one instruction. New code path:
  the WPSH multi-slot store hook (see §4).
- Excluded (tranche C&D, next project): 26 XCALL/nested, 5
  RETURN_MESSAGE. Skipped: 188 zero-arg CLEAN-EMPTY (nothing to
  redirect). A+B+C+D+empty = 754. ✓

## 2. The push_map — a direct transcription of the argmap

Key structural fact: a push_map entry is a pure function of
(callee, arg-number) → area slot, keyed by the push PC:
`slot = callee_wfp − 10 − 2N`. It does NOT depend on which LCALL the
push belongs to — a given PC pushes a fixed arg of a fixed callee, whose
frame sits at a fixed area address. So the argmap already IS the
push_map. Generated files (committed):
- `Work/c_src/quest.pushmap.A` — 515 sites
- `Work/c_src/quest.pushmap.B` — 20 WPSH sites
- `Work/c_src/quest.pushmap.AB` — combined
- `Work/c_src/tools/gen_pushmap.py` — generator

## 3. Validation (done at generation time, before the runner)

- **Every site resolved exactly**: all 535 sites' arg windows matched
  their book argc; 0 skipped, 0 argc mismatches.
- **Every slot in range**: each push slot lies inside its callee's arg
  region [wfp−10−2·argc, wfp−10); every marker == wfp−10. 0 errors.
- **Cross-validation (two independent generators agree)**: a
  call-window-walking generator and the direct (callee,argN)→slot
  transcription produce IDENTICAL maps — independent confirmation the
  slots are right.
- **Grounded against the known-good DIST site** (P16/P17 green): argN at
  wfp−10−2N, arg pushed first = lowest addr, arg1 (nearest call) =
  highest. TERRAIN's slots follow the same law exactly.

## 4. The WPSH multi-slot ordering (the tranche-B subtlety — verified)

WPSH XX,AA pushes AC[XX]..AC[AA]; the stack grows UP, so AC[XX] (pushed
FIRST) lands at the LOWEST address. Args are pushed in reverse (argN
first, arg1 last/nearest-call), so within a WPSH group AC[XX] = the
HIGHEST arg number = the lowest slot. For TERRAIN's `WPSH 0,2` over args
6/7/8: **AC0→arg8 (lowest slot C2), AC1→arg7 (C4), AC2→arg6 (C6)**. The
map gives the base (lowest) slot; the hook must write AC[XX] there and
ascend. Writing it descending silently corrupts the multi-slot args with
NO wsp divergence (div stays 0, VALUES wrong) — so tranche-B
verification must confirm the callee reads correct VALUES, not merely
div=0. The generated quest.pushmap.B encodes base=lowest ascending.

**The WPSH store hook does NOT exist yet** (verified: EagleStack.cpp
XPEF/LPEF hooks hardcode note_arg_write(m,1); `case WPSH` has no
caller_write hook). Tranche B requires implementing it: on a WPSH
caller-map hit, write `wides` consecutive words ascending from the base
slot + note_arg_write(m, wides). Tranche A needs NO new code.

## 5. Runner task 021 (queued)

Two-step battery (A first, then A+B) so B's new hook is isolated from
A's 515 sites:
- **Step A**: load quest.pushmap.A (no new code), full battery — expect
  div=0, and report which of the 515 sites fired per leg (coverage).
- **Step B**: after the WPSH hook is built, load .AB, full battery —
  expect div=0 AND correct multi-slot values (show a TERRAIN WPSH window
  trace: 3 words to C2/C4/C6, offset += 6).
NOTE: step B's build is a code change; if this session's runner cannot
build the hook, step A validates independently and step B is handed to a
build session with the maps + §4 ordering spec in hand.

## 6. State

- main: push_map.A/.B/.AB + gen_pushmap.py + this report committed.
- Maps are load-time-validatable by the existing loader (push slot in
  arg region; marker == some wfp−10; containing routine book-live).
- NEXT after A&B land: tranche C (XCALL/nested — static-link
  interaction) + D (RETURN_MESSAGE — pass-by-ref pointers, [[noreturn]]).
