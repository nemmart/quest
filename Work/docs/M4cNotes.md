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

**WMSP (57) far outnumbers STASP (19) — ~3:1 — because cleanup is
BATCHED: multiple WMSP allocations are reclaimed by ONE STASP that
restores wsp below the whole group.** Verified in DISPLAY_SCREEN (9
WMSP, 3 STASP), which has three groups each of the shape:

    LDASP r; WMSP a;   ; capture wsp, allocate buffer 1
    LDASP r; WMSP b;   ; capture wsp, allocate buffer 2
    LDASP r; WMSP c;   ; capture wsp, allocate buffer 3
    ... use buffers (?WRITE_SCREEN etc.) ...
    STASP 0            ; ONE restore: wsp = saved pre-group wsp → all 3 freed

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
