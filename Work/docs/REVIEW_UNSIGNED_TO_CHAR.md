# Cross-Session Review — ?UNSIGNED_TO_CHAR Implementation

Reviewed by the parallel session that wrote the original derivation doc
(docs/UNSIGNED_TO_CHAR.md) and the ?FILL_WORDS / ?UDIV32 translations.
The implementing session carries the version of record; this file is
the review record folded in. The reviewed diff was applied to the
reviewer's tree, built warning-free, and regression-smoked green
(0 divergences, 3 translations registered, native fills unaffected).

## Verdict: APPROVED

The implementation is careful, capture-backed, and internally
consistent. It corrected an error in the derivation doc — ?UDIV32's
remainder store is XWSTA (WIDE), so frame words [0xC..0xD] are both
written every iteration and [0xD] carries no residue. (The reviewer's
udiv32 wrapper was already wide via set_arg_wide, so only the doc was
wrong. Independent derivation catching documentation drift in both
directions is exactly why the cross-review exists.)

## Fragile claims — all VERIFIED against emulator source

1. **WSAVS** (EagleStack): advances wsp by frame_size*2 after the five
   pushes (so the inner-?UDIV32 pushes land at fb+52.. exactly as the
   residue replication assumes) AND sets ovk=1 (WSAVR sets 0) — the
   "(psr|0x8000) ovk set by the outer WSAVS" comment matches the
   source literally.
2. **XLEFB [ac3+disp]** (Machine::eagle_x_byte_indexed): byte address
   = ac3*2 + sign-extended disp — write_frame_byte's
   frame_base*2+byte_offset model is correct, odd offsets included.
3. **ADD.O# 0,0,SEZ** (NovaCompute): '#' no-load suppresses BOTH the
   result and the carry update (if(N==0) guards both stores), and the
   .O carry-preset arithmetic makes the skip fire iff
   (2*flag)&0xFFFF==0 — so flag values 0 and 0x8000 both mean "no
   width flag", exactly as implemented, and the inner-entry carry
   model survives the instruction.
4. **WSEQ 0,0** (EagleCompute): same-register encodes
   compare-with-zero (dst=(XX!=YY)?ac[YY]:0) → skip if ac0==0. Loop
   exit reading correct. Bonus: this convention retroactively explains
   the WSGT 0,0 in DisassembleBlocks' ASSERT patterns.

## Fixed during review

- A stray empty statement (`;`) after the width clamp — a
  transcription artifact in the reviewer's applied copy; NOT present
  in the version-of-record tree (verified by grep). No change needed.

## Recorded (no action now)

- **Fallback-path pairing limitation** (applies to all three
  wrappers): the arity-fallback (`return entry_address(...)`)
  re-enters emulation. If the fallen-back routine's CALLEES are
  translated, the clone's emulated body dispatches native mid-routine
  (native_break at the inner return) while the master runs-to-return
  at the OUTER return → pc-mismatch divergence. Unreachable today
  (arities are fixed at every call site), but fallbacks are
  divergence-safe only while the routine's callees are unregistered.
  Revisit if a fallback ever fires, or if fallback-capable routines
  gain translated callees. (Also noted in SessionPlan.md.)
- **Capture arming across master+clone** relies on pair ordering
  (master's batch runs first, so the master claims the RETURN
  snapshot and the clone's arrival finds it disarmed). Works; fragile
  by design; fine for a derivation tool. (Comment added in
  Capture.cpp.)
- debug/Capture and RTBridge entry_ac / entry_carry /
  write_frame_byte are keepers — Capture especially should be the
  standard step-1 for every future translation (SessionPlan checklist
  already says captures are the safety net; now there's a tool).

## Validation still owed

- User store session (both new translations fire on every displayed
  number), ideally one run with QUEST_CAPTURE=7017DA75 to diff the
  master RETURN blocks against the clone NATIVE blocks word-for-word.
- The padded/width path (explicit width arg, zero padding,
  truncation) and the argc==1 default-base path... note: argc==1 IS
  the empirically dominant path (all 9 scripted-session calls);
  it is the argc==2/3 paths with no live observations. Note which
  call sites use 3 args and whether play reaches them; if not, they
  remain correct-by-derivation only.
