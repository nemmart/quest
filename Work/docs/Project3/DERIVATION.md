# Project 3 — T?AREA, I.EPILOG, I.PROLOG, I.GOTO: Translation Derivation

> **P24 WIDE-CARRY CORRECTION (Aug 29 2026).** Every carry VALUE in
> this document that derives from the wide helpers reflects the OLD
> emulator's `>>31` bug (docs/Project23/WideCarry.md, fixed by Project
> 24). The corrected semantics: `WSUB x,x` -> c=**1** (no borrow);
> `WADC x,x` -> c=**0** (user ruling — no ALU carry-out of x + ~x);
> `WSBI n,x` carries per genuine no-borrow (count-0 decrement borrows:
> c=0; count-1 does not: c=1 — the reverse of the old values); `WINC x`
> -> c=1 iff x==0xFFFFFFFF (old: never on that operand shape). Narrow
> (Nova/`>>16`) carries are unchanged. Annotations below are NOT
> rewritten inline (METHOD §11); read every wide-derived c through this
> mapping. The re-derived native staging lives in runtime/ (P24
> report); this doc remains the record of the derivation METHOD.



Status: TRANSLATED AND VALIDATED (this session). All four routines
capture-validated to ZERO differing words on their live shapes and
exercised under `-lockstep` gameplay (login sessions + the
LIST_PLAYERS fault injection) with zero divergences. One I.GOTO shape
(store-"ABC"/CONVERSION) remains unexercised natively — see "Open
items". Implementation: `runtime/frames.{cpp,hpp}` (I.PROLOG /
I.EPILOG / I.GOTO) and the `emu_rt::t_area` wrapper added to
`runtime/t_area.{cpp,hpp}`.

Derived instruction-by-instruction from `Disassembled/quest-rt.dis`
(METHOD.md §1); all instruction semantics pinned from the emulator
source, never from intent (§5). Every hex constant below was
re-checked against the disassembly this session.

## Entry facts

| Symbol | Address | Words | Called via | Frame | Ends |
|---|---|---|---|---|---|
| T?AREA | 0x7017ED93 | 8 | LCALL, 0 args | WSAVS 0x0000 | WRTN (normal) |
| I.PROLOG | 0x7017E733 | 29 | LJSR + 4 inline data words | none | WPOPJ → entry-ac3+4 (= LJSR pc+7) |
| I.EPILOG | 0x7017E77D | 7 | LJSR | none | WRTN against the CALLER's frame (returns from the caller) |
| I.GOTO | 0x7017EC7C | 80 (incl. the EC9D stub) | LJSR | none | WRTN through the cut frame → landing stub → XJMP label |

None of the three frame routines contains a WSAVS/WSSVS: they build no
frame of their own, push no shadow call-stack entry (LJSR pushes
nothing — EagleGeneral.cpp LJSR; the WSSVS caller-detection never
runs), and RTBridge's frame machinery does not apply. Each wrapper
replicates the body's exact register/flag/memory footprint directly
and ends with `RTBridge::native_transfer` (SharedProtocol.md frozen
interface 2). T?AREA is a conventional LCALL/WSAVS/WRTN leaf and uses
the standard bridge (the ?UDIV32 slot-patch precedent).

Call-site census (quest.dis): I.PROLOG 18 sites, I.EPILOG 25,
I.GOTO 26 — all LJSR, all in game code; zero RT-internal callers of
any of the three. T?AREA: LCALL'd RT-internally (?LIB_ERROR,
?LIB_ERROR_CODE, ?DEFAULT_ERROR_HANDLER sites at 0x7017E33C/E375/
E37E/E381/E3C2/E3D4 and more), zero game callers.

## THE CONDITION FRAME LAYOUT (shared contract)

I.PROLOG builds the condition frame INSIDE its caller's WSAVS frame —
the game routine that LJSRs I.PROLOG immediately after its own WSAVS.
Offsets are word offsets from that routine's frame pointer (wfp at
I.PROLOG entry; also the value stored to [wsb-0x40]):

| Slot | Written by I.PROLOG | Content | Consumers |
|---|---|---|---|
| [frame+0x2] | XWSTA after WPSH 0,1 | wsp snapshot = entry wsp + 4 | I.GOTO's landing stub: `STASP` restores wsp from it on unwind into this frame |
| [frame+0x4] | from inline [ac3+0x0] (wide) | 0 at all 18 sites | none observed |
| [frame+0x6] | from inline [ac3+0x2] (narrow, sign-extended) | 0 at all 18 sites | none observed |
| [frame+0x8] | old [wsb-0x40] | ENCLOSING condition frame (chain link) | I.EPILOG (pop), I.GOTO's walk (head advance), EE62 select loop (O_ON.md's "[frame+0x8]" walk — confirmed, same slot) |
| [frame+0xA] | XWSTA of wsp-2 | POINTER to the chain head slot (= entry wsp + 2, on the stack) | O.ON / O.REVERT / EE7A helper: `@[frame+0xA]` (O_ON.md — confirmed, same slot) |
| [frame+0xC..] | display loop | static-link frame pointers, count = inline [ac3+0x3] − 1 iterations | nested-procedure addressing (never populated in Quest — see below) |
| stack [wsp0+2] | WPSH (ac0=0) | the chain HEAD SLOT itself, initialized 0 (empty handler chain) | O.ON's allocate path stores the new node pointer here via @[frame+0xA] |
| stack [wsp0+4] | WPSH | entry ac1 | dead (residue) |
| stack [wsp0+6] | XPEF | entry_ac3+4 (continuation EA; popped by WPOPJ) | dead after the pop (residue above final wsp) |
| [wsb-0x40] | XWSTA | ← frame (this frame becomes the chain head) | the whole condition system |

Cross-checks: every consumer offset documented in O_ON.md (the
@[frame+0xA] head-slot pointer, the [frame+0x8] enclosing-frame walk in
the EE62 select loop and I.GOTO) matches this table; O.ON's
frame-extension path (allocating the 8-word node above [frame+0x2]'s
recorded wsp) is consistent with the head slot living at entry wsp+2.
Project 1's O.SET deep walker should be checked against [frame+0x8]
(enclosing link) and [wsb-0x40] (head) when its derivation lands — no
Project 1 REPORT existed at this writing.

Inline-data survey (all 18 sites, from quest.dis): every site passes
`0000 0000 / 0000 / 000{0|1}` — [frame+4] and [frame+6] are always 0,
and the display count is 0 or 1. The display loop body runs count−1
times (the count is pre-decremented before the WSGT test), i.e. NEVER
in Quest. The loop is still ported exactly because its register/carry
side effects differ between the two counts (below).

## Per-routine derivation

### T?AREA (0x7017ED93)

    WSAVS 0x0000; LDASB 2; XLEF 0,[ac2+0x7FD7]; XWSTA 0,[ac3+0x7FF8]; WRTN

Displacements are 15-bit sign-extended (Machine::eagle_x_resolve_indirect):
0x7FD7 = −0x29, 0x7FF8 = −0x8. So: ac2 = wsb; ac0 = wsb−0x29 (XLEF
loads the ADDRESS); store to [frame−8] = the saved-ac0 slot of the
WSAVS image (ret|c at [frame], wfp −2, ac2 −4, ac1 −6, ac0 −8,
psr|argc −10) — the slot-patch return idiom, byte-for-byte the
?UDIV32 precedent. WRTN then restores the patched ac0 = the area
address; ac1/ac2/carry return to entry values. The frozen
`rt::t_area` (`machine.wsb − 0x29`) matches the body exactly.

Wrapper: standard RTBridge; `set_return_ac(0, rt::t_area(machine))`
before `emulate_frame()`; `native_return()`. Fallback (never observed:
all sites are `LCALL [..],0`): argc≠0 → arm `rt_pending_return`, run
emulated (METHOD.md §12).

**The area+0x8 question (SharedProtocol.md) — RESOLVED.** Every
T?AREA consumer executes the identical rebase `WMOV 0,2; XLEF
2,[ac2+0x8]` and then works at offsets +0x1 (last error code), +0x3
(message-buffer pointer), +0x1E (handler proc), +0x20 (companion) —
i.e. SharedProtocol's field offsets are offsets FROM area+0x8. A
search of the whole RT range finds NO accessor into area+0x0..0x7
(the only 0x7FD7-displacement is T?AREA itself; the 0x7FDE hit at
0x7017F6F4 is frame-relative in an unrelated routine). Conclusion:
T?AREA's contract is "return wsb−0x29"; words +0..+7 are a dead/
reserved header, and the LIVE error record begins at area+0x8 =
wsb−0x21. ([wsb−0x2A], one word below the area, is O?SIGNAL's
4th-arg save per O_ON.md — independently owned, not part of this
record.) Recommendation: Projects 1–2 should treat `rt::t_area() + 8`
as the record base and keep field offsets as documented.

### I.PROLOG (0x7017E733)

Entry: ac3 = LJSR pc+3 (points at the 4 inline data words), wfp/wsp =
the caller's. Body, with emulator-pinned semantics:

    WSUB 0,0            ac0=0; CARRY CLEARED (sub formula: (0−0)>>31 = 0)
    WPSH 0,1            push ac0 (the head slot, =0), push ac1
    LDAFP 2             ac2 = wfp
    LDASP 0             ac0 = wsp (= entry wsp+4)
    XWSTA 0,[ac2+0x2]   [frame+2] = wsp snapshot
    WSBI 2,0            ac0 −= 2 (immediate is XX+1)
    XWSTA 0,[ac2+0xA]   [frame+0xA] = &head slot
    XWLDA 0,[ac3+0x0]   inline wide → ac0
    XWSTA 0,[ac2+0x4]   → [frame+4]
    XNLDA 0,[ac3+0x2]   inline narrow, SIGN-EXTENDED (XNLDA: (w<<16)>>16)
    XWSTA 0,[ac2+0x6]   → [frame+6]
    XNLDA 1,[ac3+0x3]   display count → ac1
    XPEF [ac3+0x4]      push continuation EA = ac3+4
    LDASB 3             ac3 = wsb
    XWLDA 0,[ac3+0x7FC0]  ac0 = old head ([wsb−0x40])
    XWSTA 2,[ac3+0x7FC0]  head = this frame
    XWSTA 0,[ac2+0x8]     [frame+8] = old head
    WBR → 0x7017E76E      common tail (shared with I.WPROLO/I.DISPLA)
    XWLDA 3,[ac2+0x7FFA]  ac3 = [frame−6] = caller's SAVED ac1 (its static link)
    XLEF 2,[ac2+0xC]      ac2 = frame+0xC
    loop: WSBI 1,1        ac1 −= 1 (borrow SETS carry when ac1 was 0)
          WSGT 1,1        skip-if ac1>0 (WSGT x,x compares against 0 —
                          EagleCompute: dst = (XX!=YY)?ac[YY]:0)
          WBR exit        taken when ac1 ≤ 0
          XWLDA 3,[ac3+0x7FFA]  ac3 = [ac3−6] (next static link)
          XWSTA 3,[ac2+0x0]     store display entry
          WADI 2,2              ac2 += 2
          WBR loop
    LDAFP 3             ac3 = wfp
    WPOPJ               pop the XPEF wide → jump to entry_ac3+4

End state (replicated exactly, including flags): ac0 = old head;
ac1 = −1 (count 0) or 0 (count 1); ac2 = frame+0xC (+2/iteration);
ac3 = wfp; **carry = 1 for count 0** (the WSBI borrow), **0 for count
1**; ovr sticky-unchanged in practice (all operands positive); wsp =
entry+4; the XPEF wide at [entry wsp+6] remains as residue above
final wsp. Continuation = LJSR pc + 7 (LJSR sets ac3 = pc+3;
EagleGeneral.cpp:180).

The carry difference is not theoretical: the count-0 shape (e.g.
READ_IN's site 0x701766EF) exits c=1, and the continuation code's
`WSUB 1,1; WADC 0,0` (computing O.ON's catch-all args ac0=−1, ac1=0)
happens to kill it — but the capture windows verify the flag itself.

### I.EPILOG (0x7017E77D)

    LDAFP 3; XWLDA 0,[ac3+0x8]; LDASB 2; XWSTA 0,[ac2+0x7FC0]; WRTN

Unlink ([wsb−0x40] = [frame+8]) and then WRTN — but wfp is still the
CALLER's frame, so this WRTN is the caller's own return: it restores
the caller's caller's registers from the frame image, restores psr
from the frame word ([frame−10], set_psr of the high half), pops
2×argc argument words, sets c from bit 31 of the ret|c wide, and
resumes at the caller's return address. "Never returns conventionally"
= the LJSR return address in ac3 is simply never used. The WRTN
replica in frames.cpp mirrors EagleStack.cpp:140-154 pop-for-pop,
including `call_stack->call_return(ret)` — which pops the game
routine's shadow entry silently (the return address matches the entry
LCALL recorded).

Footprint: ONE wide ([wsb−0x40]) plus the register/psr/wsp end state.
No residue (nothing pushed).

### I.GOTO (0x7017EC7C) — the unwind

Entry: ac0 = target frame pointer, ac2 = label pc, ac3 = LJSR return.
Caller shapes (26 sites, two idioms): handler procedures load the
target from their own saved-ac1 slot (`XWLDA 0,[ac3+0x7FFA]`) or from
live ac1 (`WMOV 1,0`) — either way the dispatcher passed the
ESTABLISHER's frame in ac1 when XCALLing the handler; the label is an
`XLEF 2,[pc+d]` code address (directly executable — verified: labels
like 0x7015D2E3 disassemble as game code).

Three shapes:

1. **Local** (ac0 == wfp): `WMOV 3,1; LDAFP 3;` then `XJMP [ac2+0x0]`
   at 0x7017ECA0 — jump straight to the label. No stack or wsp
   changes; ac1 = entry ac3, ac3 = wfp; carry untouched.

2. **Unwind** (the live shape): after pushing ac1/ac2 (residue),
   walk the saved-wfp chain from wfp:

       ec84: WSEQ 3,1 / ac1 = [ac3+0x8] when the frame being LEFT is
             the current chain head (head advances past it)
       ec88: cursor = ac3; ac3 = [cursor−2] (saved wfp)
       ec8b: WSLT 3,2 — NOT(below < cursor) → 0x7017ECA2 (bad/foreign)
       ec8d: WSGT 3,3 — NOT(below > 0)     → 0x7017ECBD (error 0x11614)
       ec8f: WSEQ 3,0 — below == target    → exit; else loop

   Exit: [wsb−0x40] = walked head; **patch [cursor+0x0] (the ret|c
   slot) = 0x7017EC9D** (XLEF 0,[pc+8]); pop the pushed label; **patch
   [cursor−0x4] (the saved-ac2 slot) = label**; `STAFP 2` (wfp =
   cursor); WRTN. The WRTN pops the cursor frame: wfp/ac3 become the
   TARGET frame, ac2 becomes the label (patched slot), ac0/ac1/psr
   from the cursor's saved slots, **c = 0** (bit 31 of the patched ret
   is clear), and control lands on the patched return address —

   **The landing stub.** 0x7017EC9D prints as `memory_data` in
   quest-rt.dis (`E309 0002 A659`) but is code (disassembled per
   METHOD.md §4 with the rebuilt Tools):

       7017ec9d XWLDA 0,[ac3+0x2]   ac0 = [target_frame+2]
       7017ec9f STASP 0             wsp = the I.PROLOG snapshot
       (falls through into 7017eca0 XJMP [ac2+0x0] → the label)

   This closes the loop on the frame layout: [frame+2] exists so the
   unwind can restore the stack pointer to exactly the state
   I.PROLOG recorded, discarding everything above the target frame.
   quest-rt.addrs should reclassify 7017EC9D..7017EC9F as code (the
   same StartStop static-reachability gap as the RTWorklist "hidden
   live code" items — reached only via the patched return address).

3. **Error/foreign** (0x7017ECA2: below ≥ cursor — segment-masking
   logic against [0x70000124], a cross-stack/task target; 0x7017ECBD/
   ECC1: corrupt chain → `O.SERROR` with code 0x11614/0x11635, which
   throws): never observed live; zero calls in every recorded session.

Wrapper strategy: the walk is READ-ONLY until the exit patches, so a
pre-walk classifies the shape before the first side effect; shapes
under (3) fall back to emulation symmetrically (METHOD.md §7/§12 —
`rt_pending_return` armed; the master's range-exit rule covers a body
that signals). A 4096-step guard on the pre-walk also falls back
(corrupt-chain paranoia; the emulated body would loop the same walk).
The live path then replicates: pushes, walk with head tracking, both
patches, wfp cut, the WRTN replica (whose `call_return(0x7017EC9D)`
prints the same benign mismatch notice as the master —
UNIMPLEMENTED.md §8 — and pops one shadow entry, keeping the two
engines' shadow stacks in lockstep), the stub's wsp restore, and
`native_transfer(label)`.

### Pairing (all four)

Master enters a translated entry → arms `rt_pending_return = ac3` and
run-to-returns. T?AREA's body returns to ac3 (pc match). The three
frame routines end at game-range pcs (continuation / caller's return /
label), terminating the master's batch by the RANGE-EXIT rule
(Machine.cpp run_steps; SharedProtocol.md frozen interface 2) at
precisely the pc the clone's `native_transfer` returns. Verified
empirically: zero divergences across every session this run,
including the full signal chain.

## Validation evidence

Method: `QUEST_CAPTURE` on the master at the routine's exit
architectural point, diffed word-for-word (registers, flags, 92-word
wsp−8 region, 18-word DEST window) against the clone wrapper's
`native_footprint` block at the same point (same base by
construction), using an exact-diff script. Plus `-lockstep` gameplay
with the pair gate comparing pc/ac0-3/carry at every rendezvous.
Sessions used the documented scripted login (CL/Claude/quest/Y/any/F)
over port 8781.

| Routine | Capture point | Windows | Result |
|---|---|---|---|
| T?AREA | ENTRY 0x7017ED93 / RETURN 0x7017E340 (?LIB_ERROR site) vs NATIVE | wsp−8 region (frame image incl. patched ac0 slot) | **0 differing words**; the 3 differing register LINES in the NATIVE block are the snapshot-timing artifact (taken before `native_return` restores ac0/ac3/wsp) — the restored values are exactly what the pair gate verified on all 17 calls |
| I.PROLOG | master snapshot at continuation 0x701766F6 (READ_IN) vs NATIVE | wsp−8 region; DEST=frame (0x700010E0); DEST=[wsb−0x40] (0x7000104C) | **0 differing words** in all three windows, three runs; c=1 borrow confirmed |
| I.EPILOG | master snapshot at caller-return 0x7015C063 vs NATIVE | wsp−8 region; DEST=[wsb−0x40] | **0 differing words** |
| I.GOTO | master snapshot at label 0x7016F1C4 (handler #13) vs NATIVE | wsp−8 region (post-STASP stack top); DEST | **0 differing words**: registers, c=0, psr, restored wsp, both patched slots, walked head |

Live counts (zero divergences in every run): login sessions —
I.PROLOG ×6, I.EPILOG ×5, O.ON/O.REVERT interleaved, clean ESC quit
with data write-back. `QUEST_FAIL_OPEN=USER_DATA_FILE` + `L`→`P`
(LIST_PLAYERS, ON_ERROR_CATALOG #13) — ?LIB_ERROR ×2 (emulated),
T?AREA ×17 (native), ?DEFAULT_ERROR_HANDLER ×2, I.GOTO ×1 (native,
ret=0x7016EC74, label 0x7016F1C4), game RESUMED and continued to a
clean quit. The benign shadow notice (`call return address; 7017EC9D,
stack return address: 7017EE40`) appears once per engine — identical
evolution.

Wsb for the main client task observed = 0x7000108C (area
0x70001063 = wsb−0x29 ✓).

## Open items

1. **Second I.GOTO shape — store-"ABC" (CONVERSION, handler #26,
   label 0x7017A520): NOT exercised natively this session.** The
   documented trigger requires map navigation to a store; at this
   container's turn cadence (>75 s/move, movement produced no
   response within budget) it was abandoned after a bounded attempt.
   Standing evidence: RTWorklist Play Session 2 ran this exact chain
   under lockstep (emulated I.GOTO) and it exercises the same single
   live path (the EC84 walk + patches + stub) that shape #13
   validated word-exactly; only the walk depth, handler frame, and
   label differ. Instructions for an interactive session: play to any
   store tile, type `ABC` at the purchase prompt; run with
   `QUEST_CAPTURE=7017A520 QUEST_CAPTURE_DEST=70001098` and diff the
   master snapshot against the clone's NATIVE block (pc=70179BF7).
   The `L`→`A` (allies) injection path was tried as a cheap second
   shape and does NOT fire (?OPEN_FILE never issued — allies listing
   short-circuits with zero allies).
2. **Cross-stack shape (0x7017ECA2 path)**: unreached; falls back by
   design. If it ever fires, the fallback is symmetric and logged
   `(native-fallback: non-descending or non-positive frame link)`.
3. **quest-rt.addrs**: 0x7017EC9D..0x7017EC9F should be reclassified
   `code` (the landing stub) — a Disassembled/ regeneration item, not
   done here (shared artifact).

## Capture-tool sharp edge (integration note, not fixed here)

`debug::Capture::native_footprint` with no `QUEST_CAPTURE_DEST` uses
the CURRENT ac2 as the DEST window. For wrappers that call it after
restoring caller registers (this project's, and any future transfer
routine), ac2 may legitimately hold a byte pointer or any non-address
(observed: 0xE000218B after I.EPILOG's WRTN replica at READ_IN),
and the 18-word read THROWS `Page does not have read permission`,
killing the clone mid-batch. Every Project 3 validation run therefore
sets `QUEST_CAPTURE_DEST` explicitly. Proposed hardening (debug/ is
shared — not edited): guard the dest reads, or skip the dest window
when `dest` is outside mapped pages. Until then: **always set
`QUEST_CAPTURE_DEST` when capturing with these translations
registered.**
