# Project 19 — M4b tranches C & D: XCALL/nested + RETURN_MESSAGE (REPORT)

**Sessions:** Aug 23 2026. **Status: LANDED.** All 566 arg-bearing
game→game call sites are decorated (535 P18 + 26 XCALL + 5
RETURN_MESSAGE); the 188 zero-arg sites need nothing. **M4b is
complete.** Batteries: task 026 (full 7-leg regression on the combined
map, GREEN), task 027 (targeted C/D coverage, GREEN). Exactly as the
SCOPING predicted: hook replication + verification, no new mechanism, no
mapper/ruling changes.

## 1. What was built

- **`quest.pushmap.C`** (26 XCALL sites, 41 pushes), **`.D`** (5
  RETURN_MESSAGE sites, 21 pushes incl. four 2-wide and one 3-wide
  WPSH), **`.ABCD`** (combined: 566 calls, 1313 pushes, no pc
  collisions). Generator `tools/gen_pushmap_cd.py`: the same
  (callee,argN)→slot transcription as P18, **cross-validated against an
  independent disassembly window-walk** (nearest-call push = arg1, WPSH
  covers the next w arg numbers) — the two paths agree on every pc;
  slot-in-region and marker==wfp−10 checked at generation AND load time.
  Loader accepts 566/566, 0 rejects (task 026 load check).
- **The XCALL marker hook** (EagleStack.cpp `case XCALL`): the identical
  block to `case LCALL` — P18's XPEFB/LPEFB replication precedent. The
  pushed word was hoisted into `value` (computed exactly as LCALL
  computes it); marker written to wfp−10, still pushed (call-marker
  ruling), resolved-target-vs-book fail-loud check, `args_written` set,
  stack_offset untouched (P17 ruling). Trace tag "XCALL".
- **Tranche D needs NO code**: all 5 RETURN_MESSAGE sites are LCALLs;
  their WPSH windows ride the P18-B multi-slot hook and 3-field grammar.
- **Test scaffolding** (both marked TEMPORARY, os/OSContextShared.cpp):
  `QUEST_FAIL_OPEN` extended to ?SOPEN, and `QUEST_FAIL_SSHPT=1`
  failing SYSCALL 044 for the clients. Both are LOCAL calls
  (LockstepMediator) so injected failures are symmetric. Neither touches
  game or mapper code.

## 2. Verification ask #1 — XCALL marker/argc convention (VERIFIED, live)

Code-identity first: XCALL pushes the same `(psr<<16)|argc` /
`argc&0x7FFF` word LCALL pushes, and the write-mode WSAVS is
call-opcode-agnostic — it reads argc from the AREA marker. Live (task
027 lp leg, driver L→P): all **9** LIST_PLAYERS.3 XCALL sites fired,
div=0. Window specimen:

    ARGWR pc=7016ED6B slot=74005E60 value=74005ABD off=2
    XCALL pc=7016ED6E slot=74005E62 value=80000001 off=2
    WSAVS LIST_PLAYERS.3@7016F556 mode=W area_wfp=74005E6C argc=1
          ... link=74005AAA

- slot = wfp−10−2·1 = 74005E60 ✓; marker = wfp−10 = 74005E62 ✓;
- marker word 0x80000001 = (psr<<16)|1 with psr=0x8000 (OVK) — the
  SAME form the rmD LCALL marker shows (0x80000006 for argc 6): the
  convention is identical across the two opcodes;
- `link=74005AAA` = LIST_PLAYERS' area frame: the M4a static-link
  machinery is intact under arg redirection (the SCOPING's "FRAME
  semantics untouched" claim, observed);
- the arg VALUE is itself a pointer (XPEF [ac3+0x13] → 74005ABD =
  caller's area-frame local) — redirected pointer, correct dereference,
  div=0 through nine distinct sites × repeated per-record calls.

lp leg totals: div=0, 0 aborts, 3685 redirected writes, wWSAVS 959 vs
wWRTN 958 (one in-flight at kill, the P18 play-leg pattern).

## 3. Verification ask #3 — RETURN_MESSAGE noreturn (VERIFIED, live)

Task 026's rm leg disproved the first trigger theory: an ?SOPEN failure
routes via ?LIB_ERROR → ?DEFAULT_ERROR_HANDLER → ?FATAL (clean detach,
div=0 — an incidental error-path regression pass with the full map
live). Reading INIT_SHARED_DATA settled it: the RETURN_MESSAGE branch
hangs off the INLINE check of **SYSCALL 044** at 7015BE59. With
`QUEST_FAIL_SSHPT=1` (task 027 rmD leg) the documented sequence ran
end-to-end on both engines:

    ARGWR pc=7015BE69 slot=740075C0 value=7015BD7B   (arg6)
    ARGWR pc=7015BE6B slot=740075C2 value=00000000   (WPSH wide 1: arg5)
    ARGWR pc=7015BE6B slot=740075C4 value=00000000   (WPSH wide 2: arg4 — ASCENDING)
    ARGWR pc=7015BE6D slot=740075C6 value=700010A6   (arg3)
    ARGWR pc=7015BE6F slot=740075C8 value=701518F9   (arg2)
    ARGWR pc=7015BE72 slot=740075CA value=700010A0   (arg1)
    LCALL pc=7015BE74 slot=740075CC value=80000006   (marker, argc 6)
    WSAVS RETURN_MESSAGE mode=W area_wfp=740075D6 argc=6 ...
    ...
    Lockstep: ordinal 0 RETIRED at terminal syscall ?RETURN

Offset closed at 2·argc=12 and was consumed at the write-mode WSAVS;
the body (screen writes, delay) ran under ordinary lockstep at div=0 —
the mid-body checkpoint condition SCOPING asked to confirm; the 0310
retire absorbed the missing WRTN. **wWSAVS − wWRTN = exactly +1**, the
predicted signature of the never-returning record, and it is MOOT after
retire as designed. div=0, 0 aborts.

## 4. Verification ask #2 — pass-by-ref pointers (structural + analog)

The one pass-by-reference site (RETURN_MESSAGE,3 @70169B82 in LOCK_FILE)
was NOT exercised: reading the block shows it is a lock-consistency
fatal, entered only from corrupt-lock checks (70169B94/9A/A2/A5) — no
clean external trigger exists (task 026 rm2's user-file failure never
reaches it; the m driver never opens the file at all). Its correctness
rests on:
- the structural argument (SCOPING, confirmed by inspection): the three
  temp-building WPSHes (70169B77/7C/7F) are NOT in the map — the temps
  stay on the REAL stack; only the final `WPSH 0,2` (the three pointer
  VALUES) is redirected, and the values are register contents unchanged
  by redirection, so the callee's dereferences resolve identically on
  both engines;
- the live-verified analog: the lp leg's arg IS a redirected pointer
  (dereferenced correctly at div=0), and the 3-wide WPSH ascending hook
  is P18-proven (TERRAIN) and re-proven here at 2 wides with values.

## 5. Coverage — honest accounting

Across tasks 026+027 (10 legs): div=0, 0 i2, 0 probes, 0 m4b/mapper
aborts everywhere.

- **Tranche C: 9/26 sites live** (all nine LIST_PLAYERS.3). The other 17
  are decorated + load-validated but driver-unreachable: BARGAIN.1 ×5
  (needs an adjacent being), BOAT.1 ×3, CAST.4 ×2, KILL_PLAYER.4 ×1
  (the kp driver attempt didn't reach the XCALL; div=0), OP_EDIT ×6
  (operator path). Same acceptance basis as P18 (51/515 fired): the
  uniform slot law, generation cross-validation, and load-time checks
  carry the unexercised sites; any future firing is protected by the
  fail-loud target-vs-book check.
- **Tranche D: 1/5 sites live** (7015BE74, the fullest one: argc 6, a
  2-wide WPSH, noreturn). Unexercised: LOGON ×2 (?LOOKUP_PORT/?CONNECT
  failure), UPDATE_USER_DATA_FILE ×1, LOCK_FILE ×1 (§4). All are the
  same LCALL + XPEF/LPEF/WPSH shape as the verified site.

## 6. Battery ledger

- 026 (ABCD regression): m/fo/inj/abort/play all GREEN and equal to the
  P18 baselines (e.g. play: 85696 redirected writes, 22639/22638
  write-mode WSAVS/WRTN, 51 L-sites); rm = the ?LIB_ERROR→?FATAL
  error-path pass; rm2 = m-equivalent (USER_DATA_FILE never opened by
  that driver — a leg-design dud, superseded by 027).
- 027: lp (C evidence, §2), kp (attempt, div=0, no coverage), rmD (D
  evidence, §3).

## 7. State / next

- **M4b COMPLETE: 566/566 arg-bearing sites decorated, 0 divergences.**
- Committed: pushmap.C/.D/.ABCD, gen_pushmap_cd.py, the XCALL hook, the
  two TEMPORARY os knobs (remove with the other QUEST_FAIL scaffolding
  when the experiments close), drive.py kp mode, tasks 026/027 +
  results.
- NEXT (per CURRENT_STATE): **M4c** — in-body stack residue: MSP dyn
  allocs (see the parallel M4cNotes.md WMSP/STASP sprintf findings),
  WPSH/WPOP brackets.
