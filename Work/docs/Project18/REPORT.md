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

---

# Project 18 — COMPLETION HALF (emulator side + batteries, Aug 23 2026)

**Status: LANDED. Tranches A (515) + B (20) decorated and GREEN.**
Tasks 023 (divergence capture), 024 (A battery), 025 (A+B battery).

## 7. The 021 driven divergence — root cause and fix

Task 023 (a driven m leg that preserved $R/out, which 021 hadn't)
captured the dump: divergence at the FIRST write-mode call, site
GET_INPUT,1 @ 701760C4, clone shadow_wsp 700011A8 vs master wsp
700011A6 (= +2·argc), argwr=0 despite the site's push pc (701760C2,
XPEFB) being in the map.

Root cause — an implementation gap, not a site-class or mechanism
problem: **XPEFB and LPEFB had no caller_write hook.** P16 hooked only
XPEF/LPEF (DIST's window uses only those), so any decorated site whose
arg push is a B-variant pushed stock while its LCALL still ran the
write-mode path — the WSAVS then assumed elided args that were in fact
on the stack. 80 of map A's 1127 push entries are XPEFB/LPEFB; boot was
clean in 022 only because no affected site became the active write-mode
call without driver input. Fix: the identical 6-line hook replicated
into both cases (EagleStack.cpp). No mapper/ruling changes; the
FINDING doc's exclusion hypothesis was not needed.

## 8. Tranche A battery (task 024) — GREEN

div=0 / i2=0 / probes=0 / m4b_aborts=0 / mapper_aborts=0 on all five
legs (m, fo, inj @ normal driver speed, abort, play @ 10x). Write-mode
WSAVS == write-mode WRTN: 627/627 (m), 16490/16489 (play; one call
in-flight at kill). Coverage: 51/515 decorated call sites fired, 91
distinct push pcs redirected, ~49k redirected arg writes in play.
Unfired sites are the usual coverage backlog (drivers don't reach all
content), not a blocker — same posture as M4a.

## 9. Tranche B — loader grammar + WPSH multi-slot hook

- **Loader** (AddressBook.cpp): 3-field `push <pc> <base_slot> <wides>`
  accepted (wides ∈ [1,3]); EVERY written slot base..base+2·(wides−1)
  validated inside the callee's arg region; absent 3rd field = 1.
  quest.pushmap.AB loads clean: 1251 push redirects, 535 decorated
  calls, 0 rejects.
- **Hook** (EagleStack.cpp case WPSH): on a caller-map hit, write
  AC[XX] to the base (LOWEST) slot and ASCEND for `wides` slots
  (AC[XX] = highest arg number, per §4), note_arg_write(m, wides), no
  push. Fail-loud cross-check: map wides must equal the instruction's
  AC group size ((AA−XX mod 4)+1).

## 10. Tranche A+B battery (task 025) — GREEN, values verified

div=0 / 0 / 0 / 0 / 0 on all five legs. Write-mode WSAVS == WRTN:
630/630 (m, fo), 647/645 (inj, 2 in-flight), 22781/22780 (play),
abort leg terminates mid-call by design. 53 decorated call sites
fired; 86k redirected arg writes in play; copy mode coexisting
throughout (div=0 covers byte-identity of everything undecorated).

**The multi-slot evidence** (per §4's warning that descending order
corrupts values at div=0). TERRAIN,9 window @ 7015C518, play leg:

    ARGWR pc=7015C508 slot=740098C0 value=7015BF19 off=2    # arg9 XPEF
    ARGWR pc=7015C50A slot=740098C2 value=701802AE off=2    # WPSH AC0→arg8
    ARGWR pc=7015C50A slot=740098C4 value=000002AE off=2    # WPSH AC1→arg7
    ARGWR pc=7015C50A slot=740098C6 value=701802AE off=2    # WPSH AC2→arg6
    ARGWR pc=7015C50B slot=740098C8 value=7400000D off=10   # arg5 (2+6+2)
    ...
    ARGWR pc=7015C515 slot=740098D0 value=70000216 off=18   # arg1 = 2·argc
    LCALL pc=7015C518 slot=740098D2 value=80000009 off=18   # marker wfp−10
    WSAVS TERRAIN mode=W argc=9 ...

Consecutive ascending slots from one WPSH pc; offset steps 2→10 across
the WPSH (+2·wides=6) and closes at 18 = 2·argc; marker at wfp−10 with
the argc-9 marker word. Site 70171182's group carries three DISTINCT
values (00003C18 / 000002AE / 00003E72) — a descending-order bug would
have transposed them into TERRAIN's pointer args, so div=0 over 86k
writes plus the offset arithmetic verifies VALUES, not just wsp. 6 of
the 20 WPSH windows exercised (coverage note).

## 11. State / next

- main: XPEFB/LPEFB hooks, WPSH multi-slot hook, 3-field loader,
  tasks 023–025 + results. CheckerHistory Gen-4/5 addendum appended;
  CURRENT_STATE updated.
- Decorated: 535 of 754 game→game sites. Remaining: tranche C — 26
  XCALL/nested (static-link interaction), tranche D — 5 RETURN_MESSAGE
  (pass-by-ref pointers, [[noreturn]]) — NEXT PROJECT. 188 zero-arg
  sites need no decoration.
