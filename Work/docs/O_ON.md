# O.ON / O.REVERT (+ condition-system architecture) — Translation Derivation

Status: DERIVATION IN PROGRESS (this session). Strategy agreed with
user: translate O.ON + O.REVERT natively; I.PROLOG and the signal path
(O?SIGNAL, O.SET, EE62 select loop, I.GOTO) REMAIN EMULATED. This is
the Task D transition architecture: the native registrars maintain the
chain in emulated stack frames so the emulated walkers keep working.

## Entry facts

| Symbol | Address | Called via | Frame |
|---|---|---|---|
| O.ON | 0x7017ED9B | LJSR (returns pc+3) | WSSVS 0x0004 |
| O.REVERT | 0x7017EDCB | LJSR | WSSVR 0x0000 |
| helper (unnamed) | 0x7017EE7A | XJSR from O.ON/O.REVERT/EE62 | WSSVR 0x0000, SKIP-RETURN |
| O.SET | 0x7017EE56 | XCALL from signal path | WSAVS 0x0000 |
| select loop | 0x7017EE62 | XPSHJ (WPOPJ returns) | none (raw stack) |
| deep walker | 0x7017EE9D | XJSR from O.SET | WSSVR 0x0005 |
| O?SIGNAL | 0x7017EDED | LCALL, 3-4 args | WSAVS 0x0000 |
| O.S* shorthands | 0x7017EE07..EE33 | (entered by ?LIB_ERROR paths) | WSAVS 0x0000, load fixed codes, join at EE35/EE37/EE38 |

0x7017EE7A/EE62/EE9D/EE56 are in quest-rt.addrs code range
7017ED8F..7017EF50 already; "symbol not found" in the disassembly is a
SYMBOL-TABLE gap (unnamed internals), not a classification error.

## New instruction semantics (pinned from emulator source)

- **WSSVS/WSSVR fs** (EagleStack): pushes SIX wides — psr<<16 FIRST,
  then ac0, ac1, ac2, wfp, ac3|c<<31; ac3=wfp=wsp; wsp+=fs*2;
  ovk=1 (WSSVS) / 0 (WSSVR). Saved slots relative to the new ac3:
  ret at [0], wfp at [-2], ac2 [-4], ac1 [-6], ac0 [-8], psr [-10].
  NOTE the psr wide has argc=0 in its low half, so WRTN against a
  WSSVS frame pops zero args — WRTN is shared by both frame types.
  **Consequence: XWLDA n,[ac3+0x7FFE] in a WSSVS body reads the
  SAVED CALLER wfp — the implicit "current frame" argument.**
- **LDASB**: ac=wsb (stack base). Task-local condition area lives at
  NEGATIVE offsets from wsb: [wsb-0x40] (chain-related, read by
  O.REVERT/EE62), [wsb-0x3E,-0x3C,-0x3A] (O.SET saves ac0-2),
  [wsb-0x2A] (O?SIGNAL's optional 4th arg).
- **LDASP/STASP, LDAFP/STAFP**: read/write wsp / wfp directly (O.ON's
  frame-extension path). LDATS/STATS: read/write the wide AT wsp.
  ISZTS: increment wide at wsp, skip if zero.
- **WPSH x,y / WPOP x,y**: push/pop register range (x..y mod 4).
  WPOPJ: pop a wide, jump to it. WXCH: exchange registers.
- **XPSHJ**: push pc+2, jump — callee returns via WPOPJ (used for the
  EE62 select loop).
- **Skip-return convention (the helper at EE7A)**: after WPOP restores
  wsp to the frame base, the ret|c wide is at TOS; **ISZTS increments
  the saved return address**, so WRTN returns to caller+1. Callers
  place a WBR at [ljsr_ret] and the alternate path at [ljsr_ret+1]:
  O.ON 7017EDA1 (found)→EDA4 / 7017EDA2 (not-found)→...→EDAF;
  O.REVERT 7017EDD6/EDD7 likewise.

## Chain-node layout (from writer + search + select sides)

- node[+0]  next-node link (0 / non-positive terminates: WSGT 2,2 guard)
- node[+2]  condition-type key; **0 is a wildcard**: the search stashes
  the node via STATS as a backstop while continuing to look for an
  exact match (exact semantics TO BE CONFIRMED in implementation)
- node[+4]  second key (compared vs caller ac1; frame identity?)
- node[+6]  handler code address (the signal path XCALLs through it)
- node[+8]  area the re-registration WBLM copies into (6 words,
  backward copy, count 0xFFF4=-12)
- Chain head: reached via **@[frame+0xA]** — the frame slot at
  [wfp+0xA] holds a POINTER to the head slot (I.PROLOG initializes
  this; I.PROLOG stays emulated so we only ever FOLLOW it natively).

## O.ON decoded flow (0x7017ED9B)

Inputs: ac0 = condition type (-1 catch-all in all 26 Quest handlers),
ac1 = (key2 / handler descriptor arg — CONFIRM), ac2 = clobbered,
implicit: saved caller wfp at [ac3-2]. WSSVS 4 (4 wides of locals).

1. ac2 = caller wfp; XJSR helper (chain search with keys ac0/ac1).
2. FOUND (ret+0 → WBR to EDA4): re-registration — XLEF 2,[ac3+0x7FF8]
   (= &saved-ac0, i.e. the 6-wide SAVED REGISTER BLOCK psr..ac1 is
   the descriptor source? CONFIRM exact WBLM regs/direction),
   WMOV 1,3 (ac3 = found node from helper's ac1), XLEF 3,[ac3+0x2],
   NLDAI 6,1, WBLM — copy 6 words; then ac3 restored from
   [ac3+0x7FF8]?? (XLEF 3,[ac3+0x7FF8] — CONFIRM: ac3 was clobbered,
   this XLEF indexes the NEW ac3 — re-derive carefully) → exit.
3. NOT FOUND (ret+1 → EDA2, WSNE 1,1 tests helper's ac1): if a
   backstop/insert point exists (ac1!=0 → EDAF): **frame extension**:
   NLDAI 0xFFF4,1 (count -12), WINC 3,2, XLEF 3,[ac2+0x8], WBLM
   (backward-copy 6 wides into the area above node[+8]?),
   LDASP 3/STAFP 3 (grow the frame: wfp=wsp), then link: ac2 =
   re-read saved wfp [ac3+0x7FFE], node fields written at [ac2+0x2],
   old head = @[ac2+0x800A] → stored to new node[+0], new node ptr
   stored to @[ac2+0x800A] (head update). CONFIRM the exact
   node-base register mapping in this path.
4. Common exit EDC5: WSLE 0,0 (skip if ac0<=0 — condition type
   negative = catch-all?), else zero local [4]; WRTN.

OPEN: exact descriptor layout O.ON copies (which 6 words, from
where), the EDA4-path register dance, the EDAF-path node addressing,
and whether local [4] matters downstream.

## O.REVERT decoded flow (0x7017EDCB)

WSSVR 0. ac3' = saved caller wfp... (XWLDA 3,[ac3+0x7FFE]); LDASB 2;
ac2 = [wsb-0x40]; WSEQ 2,3 — if the task-area value equals the caller
frame ptr, skip the search (WBR to WRTN); else XJSR helper
(skip-return): found → WMOV 1,3 (node), WSUB 0,0, XWSTA 0,[ac3+0x2] —
**zeroes node[+2]** (deactivates the handler in place — REVERT does
NOT unlink, it wildcards/clears the key — CONFIRM key semantics vs
the search's zero-as-wildcard reading!); not-found → plain WRTN.

NOTE the tension: search treats node[+2]==0 as a WILDCARD backstop,
but REVERT CLEARS node[+2] to deactivate. These can't both be right —
resolving this (what zero means in [+2] vs the search's STATS branch)
is the first implementation task. Likely: [+2]=0 means "inactive" and
the STATS branch keys on something else — re-derive EE86..EE93
precisely.

## Signal path (REMAINS EMULATED — contract the native must serve)

O?SIGNAL/O.S* → common at EE35..EE38: XCALL O.SET (EE56: XJSR deep
walker EE9D, then saves ac0-2 to [wsb-0x3E,-0x3C,-0x3A]) → XPSHJ EE62
select loop: from [wsb-0x40] frame, per frame XJSR the EE7A search;
found → ac2=node[+6] (WXCH/XWLDA), WPOPJ → XCALL 0,0,[ac2+0x0]-style
dispatch into the handler at EE3D; not-found → LDATS/next frame via
[frame+0x8]?? (EE6B..EE6E: LDATS 1; XWLDA 2,[ac2+0x8]; loop) —
CONFIRM: the FRAME chain walk uses [frame+0x8] as the enclosing-frame
link. Exhausted → WPOP, LLEF 2,[EF05] default, WSUB 1,1, WPOPJ.

## Validation assets

- Every login fires I.PROLOG→O.ON brackets ~10x (scripted driver
  covers it: initials CL / name Claude / password quest / Y / any key
  / class F / O / D / ESC on port 8781).
- Full chain fire (search+dispatch+I.GOTO+O.REVERT): **type garbage
  (e.g. ABC) at a store purchase prompt** (reproducible trigger,
  RTWorklist Play Session 2; catalog handler #26, resume 0x7017A520).
- Captures: QUEST_CAPTURE=7017ED9B works for entry (LJSR sets ac3 —
  CONFIRM in EagleGeneral LJSR before trusting arming), but O.ON
  writes OUTSIDE its frame (caller frame, chain nodes, task area) —
  Capture needs a second window (chain area) before footprint diffs
  are meaningful. Plan: capture caller-frame region + [wsb-0x50..wsb]
  task area + the node neighborhood.

## Bridge design notes (for implementation)

- New convention path (do NOT force into RTBridge's LCALL model —
  cross-review warning): WSSVS/WSSVR frame image (6 wides, psr low
  half = 0 args), LJSR return (ac3 = pc+3 — CONFIRM), no arg pops at
  WRTN. Suggest a small RTBridgeSS sibling or a mode flag with its
  own emulate_frame/native_return.
- O.ON's frame-extension path changes wsp/wfp — the native must
  REPLICATE the machine-state change (not just memory): after
  native return the caller's frame is BIGGER. This is a first: a
  native routine with persistent machine-state side effects beyond
  registers. native_return must not undo it.
- The helper search must be a plain C++ function used by both native
  O.ON and native O.REVERT (subtree rule: no emulation re-entry).
- Master run-to-return arms rt_pending_return=ac[3] at entry; for the
  skip-return the master's body returns to ac3 OR ac3+1 — run-to-
  return matches a SINGLE pc. FIX REQUIRED: arm BOTH (e.g. treat
  pc==ret or pc==ret+1 as the return event) or key on wsp-restoration.
  Design this before implementing (pairing correctness).

## RESOLUTIONS (same session, full decode)

1. **LJSR**: ac3=pc+3; NO frame-word push, NO ovr clear → separate
   bridge convention required (confirmed EagleGeneral).
2. **O.ON register arguments**: ac0 = condition type (wide), ac1 =
   key2, ac2 = handler address. The 6-word descriptor IS the entry
   ac0/ac1/ac2 as saved by WSSVS.
3. **Node = 8 words**: [+0]link, [+2]key1/type, [+4]key2, [+6]handler.
4. **EDA4 (reuse) path**: WBLM copies 6 words FORWARD from &saved-ac0
   (frame-8) to node[+2..7]; then XLEF 3,[ac3+0x7FF8] deliberately
   sets ac3=node so the common tail's XWSTA 1,[ac3+0x4] writes
   node[+4]=0 when ac0<=0 (catch-all). WRTN is safe with ac3
   clobbered — it uses machine.wfp, not ac3.
5. **EDAF (allocate) path**: WBLM count=-12 backward-copies the 6-wide
   saved block from [frame-10..frame+1] to [frame-2..frame+9] (source
   low words retain their values — the abandoned block at frame-10
   becomes the node with type/key2/handler ALREADY in place);
   LDASP/STAFP moves wfp to frame+8 (the relocated ret wide);
   caller_frame[+2] := frame-4 (bookkeeping pointer, replicate
   blindly); node[+0] := old head via @[caller_frame+0xA]; head :=
   node (=frame-10). WRTN then restores caller state from the
   RELOCATED block; final wsp = entry_wsp+8 (the node persists on the
   stack — the machine-state side effect the native must replicate).
   Normal/reuse path: final wsp = entry_wsp.
6. **Helper (EE7A) contract**: preamble BEFORE its frame save: if
   ac0<=0 then ac1:=0 (catch-all ignores key2). Walk from
   @[frame_arg+0xA]; every node with [+2]==0 STATS-overwrites the
   backstop slot (LAST zero node wins); match = [+2]==key1 &&
   [+4]==key2 → return ret+0, ac1=node; exhausted (link<=0) → ISZTS
   increments its saved ret (skip-return), ac1=backstop-or-0.
   Residue: its own 6-wide WSSVR image above the caller's locals
   (pushed psr has the caller's ovk state), scratch wide (final =
   last backstop or 0), saved-ac1 slot overwritten with the result,
   ret wide possibly incremented. All deterministic — replicate.
7. **O.REVERT**: gate WSEQ [wsb-0x40] == caller wfp; else no-op.
   Found → node[+2]=0. Not-found → no-op. Reads [wsb-0x40] only
   (I.PROLOG maintains it — stays emulated).
8. **O.ON locals are never written** (WSSVS 4 allocates 4 wides of
   untouched residue; the earlier "local [4]" reading was node[+4]).
9. **Pairing**: skip-returns are internal to the subtree; both
   routines return to LJSR pc+3 — run-to-return and native-span
   pairing work UNCHANGED. (Supersedes the "FIX REQUIRED" note
   above.)
10. **frame[+0xA]** holds a POINTER to the chain-head slot;
    frame[+8] is the enclosing-frame link (signal walk); both
    written by emulated I.PROLOG — native code only follows them.

## Implementation plan (agreed: O.ON + O.REVERT native, rest emulated)

- RTBridge: add an SS-convention mode (or RTBridgeSS): entry capture
  without frame word (argc=0), emulate_frame_ss writing the 6-wide
  image (psr first), native_return_ss(final_wsp) with caller state =
  entry state and explicit final wsp (entry_wsp normally,
  entry_wsp+8 on O.ON-allocate).
- runtime/o_on.{hpp,cpp}: rt::chain_search (shared, exact, incl.
  backstop and residue bookkeeping data), emu_rt::o_on, o_revert.
- Capture: add windows for caller frame (machine.wfp at entry) and
  the resolved chain-head area; validate with login sessions (O.ON
  ~10x) and the store-garbage trigger (full chain incl. REVERT at
  the documented catalog-#26 path).

## VALIDATION (this session)

- Bug found by lockstep: native_return_ss must NOT pop the shadow
  call stack — for LJSR routines WSSVS itself pushes the shadow frame
  (EagleStack caller-pattern detection) and it never runs natively.
- Bug found by footprint diff: the helper preamble's WSUB 1,1 also
  CLEARS CARRY (when type<=0), so the helper's saved ret wide has c=0
  for catch-all searches, not the entry carry. Single-bit diff caught.
- Scripted lockstep login: 5 native O.ON + 2 native O.REVERT, zero
  divergences; emulated I.PROLOG (6) / I.EPILOG (5) interleave and
  tear down native-built chain state cleanly. Reuse path exercised
  (REVERT then re-ON at the same frame) alongside allocates.
- Footprint diffs (return-pc-matched pairing; clone header pc+3 ==
  master RETURN pc): O.ON 5 pairs, 0 differing words (allocate +
  reuse); O.REVERT 2 pairs, 0 differing words.
- DONE — user playtest, signal path end-to-end (rt_trace, 491k lines,
  0 divergences, 9 native O.ON + 5 native O.REVERT): conversion error
  at the store fired ?LIB_ERROR -> ?DEFAULT_ERROR_HANDLER -> emulated
  O?SIGNAL/O.SET -> I.GOTO dispatch through the NATIVE-built chain ->
  O.REVERT(native) at ret=7017A525 (catalog #26 resume) -> re-register
  via O.ON(native) and clean resume of the store loop (price sites
  7017A0DA/7017A116). The mixed-mode condition system round-trips.

Status: COMPLETE — implemented, scripted-validated, playtest-validated
end-to-end including live signal dispatch. Registered in
translation_table (hw/RTStubs.cpp); code in runtime/o_on.{hpp,cpp};
SS convention in hw/RTBridge (emulate_frame_ss / native_return_ss).
