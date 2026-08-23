# M4c preliminary notes — in-body stack residue (WMSP / STASP / save-restore)

Investigated Aug 23 2026 (coordinator, exploratory — NOT yet a plan).

## The stack-pointer-modifying instructions in QUEST (complete set)
By how they move wsp:
- **Frame** (structured, call/return-bracketed): WSAVS (130), WSAVR (2)
  `wsp += 2·frame` on entry; WRTN (166) `wsp = wfp` then `−2·frame` on
  return; WPOP (23) partial `−2·frame` (frame-borrow restore).
- **Push** (incremental +2 each): XPEF (2935), LPEF (1151), XPEFB (77),
  LPEFB (1), WPSH (59). These are what M4b redirects.
- **Dynamic alloc**: WMSP (57) `wsp += 2·AC[reg]` — register-sized
  (NOT a constant; the operand selects a register holding the amount).
  No negative/free form.
- **Explicit set**: STASP (19) `wsp = AC[reg]` — the ONLY instruction
  that loads an arbitrary computed value straight into wsp.
- (LDASP (62) READS wsp into a register; does not write it.)
- Bare MSP: 0 in QUEST (WMSP only).

## The WMSP/STASP save-restore bracket (confirmed, key M4c finding)
WMSP allocates a dynamic in-body buffer; the space is given back by an
EXPLICIT STASP mid-body (NOT only by WRTN teardown, as first assumed).

**WMSP (57) outnumbers STASP (19) because cleanup is BATCHED: a run of
WMSP allocations is closed by ONE STASP that restores wsp below the whole
group. Group size VARIES (1 to 5 WMSP per STASP) — it is NOT a fixed
3:1.** #STASP per routine = #groups (DISPLAY_SCREEN 3 groups, DIED /
GET_QUEST 2, the rest 1). Verified:
- STORE: FIVE WMSPs in one group, one STASP (7017a141..7017a1d1), and
  the STASP is well before the WRTN.
- DISPLAY_SCREEN: three groups of three, each closed by its own STASP.
- The 1:1 routines (DISPLAY_CAVE, HELP, OBSERVE) are single-alloc groups.

**WRTN does NOT reclaim MSP space** (earlier "WRTN backstop" note was
wrong — nemmart caught it). Every WMSP group is explicitly closed by its
STASP BEFORE the routine returns; by the time WRTN runs, wsp is already
back down. So MSP allocation is always balanced by an explicit STASP
restore within the body, never left for frame teardown.

    LDASP r; WMSP a;   ; capture wsp, allocate buffer 1
    LDASP r; WMSP b;   ; capture wsp, allocate buffer 2
    ... (1..5 allocations in the group) ...
    ... use buffers (?WRITE_SCREEN etc.) ...
    STASP 0            ; ONE restore: wsp = saved pre-group wsp → whole group freed

The per-WMSP LDASP captures the running wsp (buffers are built relative
to their own base); the single STASP at the group end restores wsp to a
saved pre-group value, reclaiming the whole batch at once. The saved
value round-trips through the frame (XWSTA/XWLDA [wfp+off]) with a small
WSBI adjust, as in the single-buffer bracket at 7016614b..701661aa
(a ?WRITE_SCREEN buffer):

    LDASP 2            ; capture current wsp into AC2
    WMSP 0             ; allocate buffer (wsp += 2·AC0)
    XWSTA 2,[ac3+0xA]  ; SAVE the captured pre-alloc wsp into the frame
    ... build/use buffer; ?WRITE_SCREEN ...
    XWLDA 1,[ac3+0xA]  ; RELOAD the saved wsp
    WSBI 2,1           ; small arithmetic adjust
    STASP 1            ; wsp = restored value → buffer reclaimed

So: **STASP is the "give WMSP space back" instruction**, restoring a wsp
value the routine saved in its own frame — one STASP per allocation
GROUP, not per WMSP. WRTN (wsp = wfp) is the final backstop for anything
not explicitly restored. Most STASP sites follow a ?WRITE_SCREEN call
(dynamic screen-write buffers).

## What the WMSP groups ARE (nemmart's read): sprintf-into-a-buffer
These are the DG PL/I equivalent of `sprintf` then write: the WMSP group
BUILDS ONE formatted output buffer on the stack (the format-and-fill code
renders all the args — strings/numbers of runtime-varying width — INTO
that single buffer), then `?WRITE_SCREEN` (0x7017E27A, a RUNTIME routine)
DUMPS THE WHOLE BUFFER to the screen in one call. So the argc-2 call is
(pointer-to-buffer, screen-handle) and it emits the entire formatted
buffer; the WMSP count is however many allocations the fill code needed
to build that one buffer, NOT a per-arg or per-call-arg count (STORE: 5
WMSPs build the buffer, one argc-2 dump). (The argc-5 ?WRITE_SCREEN form
is a different, fixed-field call shape, not the buffer-build path.)

**M4c consequence — this bracket is TRANSPARENT to lockstep, likely
needs NO offset handling.** The writer is RUNTIME (out of M4b game→game
scope, never redirected), the buffer-build is deterministic stock
computation that moves BOTH engines' wsp identically, and the group is
self-contained (STASP closes it before WRTN). So WMSP/STASP contributes
nothing to master/clone divergence on its own. The ONLY thing M4c must
check: a WMSP group opening while an M4b game→game arg window is already
live on the stack (the interaction case) — the offset from the open
window must survive across the group's LDASP-capture/STASP-restore.
Absent that overlap, WMSP/STASP can be left alone.

## M4c implication (for when M4c is planned — NOT now)
This is a THIRD stack-lifecycle pattern beside arg-pushes (M4b) and
frame-borrows (WPSH/WPOP). Under M4b redirection the clone's wsp may
carry a stack_offset (differ from master's). The bracket:
- LDASP captures the clone's (possibly offset) wsp;
- it round-trips through frame MEMORY and a WSBI adjust;
- STASP reinstates it.
For lockstep to hold, the master/clone wsp relationship at capture must
be faithfully reinstated at STASP. Likely folds into the stack_offset
model (WMSP contributes 2·AC[reg] at runtime; STASP restores a saved
cursor), BUT the memory round-trip + arithmetic means it needs its own
verification — a STASP that lands wsp at a point where the offset no
longer matches would diverge. NOT designed yet; flagged for M4c.

## Open questions for M4c planning
- Do all 19 STASP sites fit the save-restore shape, or are some
  different (e.g. a computed wsp not from a saved value)? (Sampled ~12;
  all fit. Confirm the rest.)
- Does WMSP need decoration under M4c at all, or does the save/restore
  bracket keep the buffer purely in-frame (clone == master naturally)
  since WMSP moves BOTH engines' wsp identically? If WMSP is never
  redirected (both engines allocate the same buffer on their own stacks),
  it may need NO offset contribution — only the interaction with an
  already-open M4b window matters. RESOLVE before building.
