# Tranches C & D — rescoped: mostly hook-replication + verification, NOT new mechanism

Investigated Aug 23 2026 (coordinator). The "C&D are the special hard
tranches" framing was inflated. Both largely reduce to what P18 already
did for XPEFB/LPEFB (replicate an existing hook) + battery verification.

## Tranche C — 26 XCALL/nested sites — NOT special
- Args are pushed with plain XPEF/LPEF/WPSH, straight-line, same window
  shape as tranche A (verified: OP_EDIT.8 @70174771 argc4 = four XPEFs
  then XCALL 0,4; also BARGAIN.1/BOAT.1/CAST.4/LIST_PLAYERS.3/OP_EDIT.4/
  6/8). Callees are book-live with real arg slots (wfp-10-2N unchanged).
- The "static link / nested procedure" concern is about the callee's
  FRAME semantics (static link via the XCALL operands/registers) — M4a
  already migrated these; arg redirection does not touch it.
- ONLY real difference from tranche A: the opcode is XCALL not LCALL. The
  marker-write + args_written-flag hook in `case LCALL` (EagleStack.cpp)
  must be replicated into `case XCALL` — the P18 XPEFB/LPEFB pattern.
- XCALL operand form is `XCALL DL,argc,[target]` — argc is IN the
  instruction (e.g. XCALL 0,4). VERIFY the marker/argc convention the
  write-mode WSAVS expects matches what XCALL provides.
- (37 of 63 XCALL sites are argc-0 -> nothing to redirect.)

## Tranche D — 5 RETURN_MESSAGE sites — one pass-by-ref, noreturn ALREADY handled
- 4 of 5 (7015BE74, 70175EC8, 70175EFF, 7017D7D9) are ordinary XPEF/LPEF/
  WPSH windows (argc 6) — tranche-A/B-shaped.
- 1 (70169B82, argc 3) is pass-by-reference: WPSH r,r/LDASP r builds
  three stack TEMPORARIES + loads their ADDRESSES, then WPSH 0,2 pushes
  the addresses as args. Census scopes args to the final WPSH (pointer
  pushes), excluding temp-builds. Redirecting the POINTERS to the area is
  fine: pointer VALUES are unchanged and still point at the stack temps,
  which stay on the stack — indirection resolves either way. VERIFY.
- Noreturn is absorbed by existing machinery. RETURN_MESSAGE ends in
  SYSCALL 0310 (process-terminate), never WRTN. But 0310 is a terminal/
  retire event: RTStubs.cpp calls Lockstep::retire_ordinal for every 0310
  in game code (Machine.cpp notes the 0x7017700F site). Sequence: write-
  mode WSAVS opens a record -> body runs (ordinary lockstep; stack_offset
  handles any mid-body checkpoint) -> 0310 retires the ordinal -> clone
  halts. The never-popped record + live stack_offset are MOOT after
  retire — no further comparison. The missing WRTN doesn't matter.
- Confirm only: a checkpoint between RETURN_MESSAGE's WSAVS and its 0310
  stays clean — the normal open-record mid-window case stack_offset
  already covers; expected clean, confirm in the battery.

## Net: the final M4b project
- Replicate the marker/flag hook into `case XCALL` (like the XPEFB fix).
- Extend the push_map to the 26 C + 5 D sites (same generator).
- Battery. VERIFY (not build): XCALL marker/argc convention; pass-by-ref
  pointer resolution; clean checkpoints in RETURN_MESSAGE's body.
- If any verification surfaces a real gap, STOP AND REPORT — but expect
  replication + confirmation, not new mechanism. Closes M4b (535 + 31 =
  566 arg-bearing sites; 188 zero-arg need nothing).
