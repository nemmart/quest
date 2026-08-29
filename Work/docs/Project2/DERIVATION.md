# ?LIB_ERROR / ?LIB_ERROR_CODE / ?DEFAULT_ERROR_HANDLER — Translation Derivation

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



Project 2 (parallel-session protocol: docs/SharedProtocol.md). Code:
runtime/lib_error.{hpp,cpp}. Status and validation evidence: REPORT.md.

## Entry facts

| Symbol | Address | Called via | Frame | Words |
|---|---|---|---|---|
| ?LIB_ERROR | 0x7017E33A | LCALL, 1 or 2 args | WSAVS 0x0002 | 152 |
| ?LIB_ERROR_CODE | 0x7017DE25 | LCALL, 0 args | WSAVS 0x0000 | 14 |
| ?DEFAULT_ERROR_HANDLER | 0x7017E3D2 | XCALL 0,0,[ac2+0x0] from ?LIB_ERROR's tail (only dispatch site) | WSAVS 0x0003 | 30 |

No XCT, no interior data words, no hidden entries: one `memory_data`
alignment word precedes 0x7017DE25 and 0x7017E3D2; rtcov shows no
side entries (all three bodies are straight-line with two forward
branches in ?LIB_ERROR).

### Call-site inventory (complete, both binaries)

18 `LCALL [0x7017E33A]` sites, **all in the RT library**: 17 one-arg,
1 two-arg. The two-arg site — the previously unattributed "~7017DDAF
caller" (rtcalls return 7017DDB2) — is **0x7017DDAE inside the
?OPEN_FILE wrapper** (the SYSCALL 0300 routine): on open failure it
pushes `@[ac3+0xFFF2]` (forwarding its own message argument's pointer)
then `[ac3+0x12]` (the error-code local), `LCALL ?LIB_ERROR,2`. If
?LIB_ERROR ever returns, the wrapper does `WADC 0,0` (ac0 := -1,
deterministic — see WADC below) and stores it through its result
argument. This is the live path for QUEST_FAIL_OPEN triggers.

`?LIB_ERROR_CODE` has exactly one caller: game code 0x70175EE4
(`LCALL ,0`) inside LOGON's handler #2 (?CONNECT-failure path,
ON_ERROR catalog category D).

`?DEFAULT_ERROR_HANDLER` has no LCALL/LJSR callers; it is reached only
through the XCALL at 0x7017E3CD (and its address is stored only by
?LIB_ERROR — see the area analysis).

## Instruction semantics (pinned from emulator source, this session)

Everything below is from hw/EagleStack.cpp, hw/EagleCompute.cpp,
hw/EagleGeneral.cpp, hw/EagleSpecial.cpp, hw/EagleInstruction.cpp,
hw/Machine.cpp. Items already documented in I_ALLOC.md / O_ON.md are
not repeated (WSSVS image, LJSR, LDSP, lock primitives).

- **LCALL [T],n**: resolves T, pushes `(get_psr()<<16)|n` (bit15 of n
  clear), **then** sets ac3 = addr+4 and ovr = 0 — the pushed word
  carries the caller's pre-clear ovr. Consults native_registry on the
  resolved target.
- **XCALL a,b,[EA]**: word-form twin: index word at addr+1, argument
  word at addr+2, pushes `(psr<<16)|args`, ac3 = addr+3, ovr = 0,
  same registry consultation. At 0x7017E3CD the index word is
  ac2-relative displacement 0 (no indirection), so the target is the
  VALUE in ac2 — the handler address itself, not a further load. The
  argument word is 0.
- **WSAVS fs**: pushes FIVE wides — ac0, ac1, ac2, wfp, ac3|(c<<31) —
  then ac3 = wfp = wsp, wsp += fs*2, **ovk = 1** (psr is NOT pushed;
  WRTN recovers it from the caller's LCALL/XCALL word). Frame F (the
  new wfp): saved ac0 at [F-8], ac1 [F-6], ac2 [F-4], caller wfp
  [F-2], ret|c [F]. The caller's LCALL word sits at [F-10]; its LOW
  half (the argument count) is the NARROW at F-9, hence
  `XNLDA 0,[ac3+0x7FF7]` (15-bit displacement -9) reads argc. With
  one argument the ref slot is the wide [F-12]; with two, arg1 (last
  pushed) is [F-12] and arg2 is [F-14].
- **WRTN**: wsp = wfp; pops ret|c, wfp, ac2, ac1, ac0, then the
  caller's call word; ac3 := machine.wfp (the just-restored frame
  ptr); psr := word>>16; wsp -= 2*(word & 0x7FFF) — callee pops the
  arguments. c := bit31 of the ret wide.
- **X-addressing** (`eagle_x_resolve_indirect`): index word bit15 =
  indirect; displacement = 15-bit signed ((w<<17)>>17); II=2 →
  ac2-relative, II=3 → ac3-relative. Hence [ac3+0xFFF4] = @(F-12),
  [ac3+0xFFF2] = @(F-14), [ac3+0x7FF2] = F-14 (no @), [ac3+0x7FF7] =
  narrow F-9, [ac3+0x7FF8] = F-8 (the saved-ac0 slot),
  [ac2+0x8003] = @(ac2+3). Indirection chases bit31 of the fetched
  wide; all pointers involved here are segment-7 positive, one level.
- **XLEFB r,[acN+d]** (`eagle_x_byte_indexed`): byte address =
  ac[N]*2 + d, displacement in BYTES. Both copy operands use +0x2:
  skip the 2-byte length prefix of a PL/1 varying string.
- **WMOVR r**: pure logical `>>1`. No carry involvement (unlike the
  16-bit Nova MOVR).
- **WINC s,d / WADC s,d / WSUB s,d**: via add()/sub() with the
  verbatim carry formulas (I_ALLOC.md "Carry chains"). Consequences
  used here: `WSUB x,x` → 0 with **c=0**; `WADC r,r` → **-1 with c=1
  ALWAYS** (add() has no carry-in in this emulator: d = ~r + r =
  0xFFFFFFFF, carry formula bit31 of masked sum = 1). `WINC` on any
  16-bit-sign-extended length leaves c=0 (including len = -1:
  masked sum 0x100000000, bit31 = 0).
- **Skips** WSEQ/WSNE/WSGE/WSGT s,d: when s==d the comparison is
  against ZERO (src vs 0), so `WSEQ 0,0` = "skip if ac0==0". No
  flags. **WSGTI d,imm**: signed >, immediate sign-extended, 2-word
  instruction (skip target +3).
- **WBTO s,d (s≠d)**: word address = eagle_resolve_indirect(ac[s]) +
  (ac[d]>>4), mask 0x8000>>(ac[d]&0xF), OR into the NARROW word.
  `WBTO 2,0` with ac2=B, ac0=0 → sets 0x8000 in the narrow at B.
- **WCMV**: ac0 = dst count, ac1 = src count, ac2/ac3 = dst/src BYTE
  pointers; copies until dst count exhausts, drawing spaces once src
  exhausts; positive counts walk forward (direction() = -1 → count
  decrements, pointer increments). Ends ac0 = 0, ac1 = src leftover,
  ac2/ac3 = end pointers, **c = (src leftover != 0)**.
- **LDAFP n**: ac[n] = machine.wfp.
- **psr** = ovk<<15 | ovr<<14 | ires<<13 | ixct<<12 | ffp<<11 | sr.
  Body psr after WSAVS + first LCALL: `(entry_psr | 0x8000) & ~0x4000`
  (= i_alloc.cpp's body_psr). Nothing in these bodies sets ovr on
  sane values (the only add()s are WINC on a length and WADC r,r,
  whose overflow term is identically 0).
- **wide_push**: wsp += 2 THEN write — a frame pointer F equals the
  address of the LAST pushed wide.

## The condition area — resolving the "+0x8" question

`rt::t_area` (frozen) returns **wsb - 0x29**. Every T?AREA caller in
the binary — and ALL TWELVE are inside this trio — immediately does
`WMOV 0,2 ; XLEF 2,[ac2+0x8]`, so the record actually used is based at

    B = t_area(machine) + 8 = wsb - 0x21.

Offsets 0..7 from the t_area value (wsb-0x29 .. wsb-0x22) are read or
written by NOTHING in either binary (checked all LDASB-relative
encodings 0x7FD6..0x7FDF): reserved/dead header space. The signal
machinery proper addresses the task area DIRECTLY off wsb
([wsb-0x40] chain slot, [wsb-0x3E..-0x3A] O.SET register saves,
[wsb-0x2A] O?SIGNAL's optional 4th argument — O_ON.md), so B's fields
interleave with, but never collide with, those:

| Field | = wsb− | Width | Meaning | Writers | Readers |
|---|---|---|---|---|---|
| B+0x0 | 0x21 | narrow, bit 0x8000 | signal-occurred latch | ?LIB_ERROR (WBTO, set only) | none found — write-only latch |
| B+0x1 | 0x20 | wide | last error code | ?LIB_ERROR | ?LIB_ERROR_CODE, ?DEFAULT_ERROR_HANDLER |
| B+0x3 | 0x1E | wide | message buffer ptr (heap, class 3) | ?LIB_ERROR | ?LIB_ERROR (free on next signal) |
| B+0x1E | 0x03 | wide | handler address | ?LIB_ERROR install ONLY (0x7017E351) | ?LIB_ERROR tail (XCALL target) |
| B+0x20 | 0x01 | wide | handler companion (ac1 at dispatch) | ?LIB_ERROR install ONLY (:= 0) | ?LIB_ERROR tail |

**[B+0x1E] is written by exactly one instruction in the whole system**
(the lazy install, value 0x7017E3D2), so its value is always 0 or
0x7017E3D2 — ON ERROR units do NOT replace it (they live in the
per-frame chain nodes, O_ON.md), and the XCALL target is always
?DEFAULT_ERROR_HANDLER. The prompt's open question is thereby closed;
the native gate still checks the invariant defensively.

**Message buffer layout** (PL/1 varying string): narrow[0] = length,
text bytes from byte offset 2. The I.ALLOC request
`((len+1)>>1)+1` = 1 + ceil(len/2) narrow words — exactly the
string's footprint; class 3 then rounds to even and adds the 4-word
header, minimum 8 (I_ALLOC.md). This closes the prompt's
WMOVR-size-calculation question.

## ?LIB_ERROR (0x7017E33A) — decoded flow

Let W = entry wsp (post-LCALL), F = W + 10, args at [F-12] (code ref)
and [F-14] (message ref, argc==2 only), argc = narrow [F-9].

Each of the ten T?AREA calls is the idiom `LCALL T?AREA ; WMOV 0,2 ;
XLEF 2,[ac2+0x8]` → ac0 = area, ac2 = B, ac3 = F (WRTN sets ac3 =
restored wfp), other registers and carry preserved across the call
(T?AREA's WRTN restores its saved image; its saved-ac0 slot is
patched to the area value).

1. **e343–e355 install**: H = [B+0x1E]; `WSEQ 0,0` skips the WBR when
   H==0 → `WLDAI 0x7017E3D2 ; XWSTA →[B+0x1E] ; WSUB 0,0 (c:=0) ;
   XWSTA →[B+0x20]` (companion := 0).
2. **e356–e35E latch**: `WSUB 0,0` (ac0=0, **c:=0** — from here carry
   is 0 on every path until WCMV), `WBTO 2,0` → narrow [B] |= 0x8000.
3. **e35F–e368 code**: ac0 = wide @[F-12]; [B+1] := code.
4. **e36A–e389 free** (runs on BOTH message and no-message paths):
   oldbuf = [B+3]; if nonzero: `LJSR I.FREEW` at 0x7017E37E with
   ac0 = oldbuf, ac1 = ENTRY ac1 (untouched so far), ac2 = B,
   ac3 = 0x7017E381, c = 0, wsp = F+4, wfp = F; then [B+3] := 0
   (via `WSUB 0,0`, c stays 0).
5. **e38B–e38F argc test**: `XNLDA 0,[F-9] ; WSGTI 0,1` — argc <= 1
   branches to the tail (step 7).
6. **e390–e3C1 message** (argc >= 2):
   - len = narrow @[F-14] (sign-extended); **XNSTA → narrow [F+2]**
     (the wide local's other half [F+3] is never written — stale).
   - request = ((len+1) >> 1 logical) + 1; carries stay 0 (WINC on a
     sign-extended narrow, twice).
   - `NLDAI 3,1 ; LJSR I.ALLOC` at 0x7017E399: ac0 = request,
     ac1 = 3, ac2 = B, ac3 = 0x7017E39C, c = 0, wsp = F+4, wfp = F.
     Result (word ptr, class 3) → wide [F+4], then → [B+3].
   - clamp: saved = narrow [F+2], fresh = narrow @[F-14] (RE-READ from
     memory — replicate the read order; the free/alloc above could in
     principle alias), n = (fresh >= saved) ? saved : fresh (WSGE
     skip + WMOV; signed; no flags).
   - `XNSTA 0,@[ac2+0x8003]` → narrow buffer[0] := n (through [B+3]).
   - WCMV: dst = byte 2*newbuf+2, src = byte 2*msgptr+2 (msgptr =
     the wide AT [F-14]), dst count = n, src count = fresh. n <=
     fresh always, so no space padding; **c := (fresh != n)** — this
     is c_x, the carry the rest of the routine propagates (0 when the
     two reads agree, the live case). `LDAFP 3` restores ac3 = F.
7. **e3C2–e3CD tail**: T?AREA (ac0 = area — the LAST write to ac0, so
   the handler receives ac0 = area on both paths); ac1 = [B+0x20],
   ac2 = [B+0x1E]; `XCALL 0,0,[ac2+0x0]` → push (psr_body<<16)|0,
   ac3 = 0x7017E3D0, ovr = 0, target = handler address.
   c at the XCALL = c_x (message path) or 0 (no-message path — set at
   step 2 and preserved through every intervening call).
8. **e3D0 WRTN** — executes only if the whole signal chain returns
   normally (handlers usually I.GOTO out; the path exists and is
   translated).

## ?DEFAULT_ERROR_HANDLER (0x7017E3D2) — decoded flow

Entered with ac0 = area, ac1 = companion (0), ac2 = H (its own
address), ac3 = 0x7017E3D0, c = c_x, wsp = F+6, psr = psr_body.

WSAVS 3 → F' = F+16 (image at F+8..F+16, ret wide 0x7017E3D0 |
c_x<<31). T?AREA idiom (frame at F+26..F+34 — see residue). Then:

- ac0 = [B+1] (the code just stored);
- `WADC 1,1` → ac1 = **-1, c = 1** (deterministic, see semantics) →
  wide [F'+2];
- `WSUB 2,2` → ac2 = 0, **c = 0** → wide [F'+4];
- code → wide [F'+6];
- `XPEF [F'+6] ; XPEF [F'+4] ; XPEF [F'+2]` — pushes at F+24, F+26,
  F+28 (arg refs: O?SIGNAL's arg1 = &(-1) = F'+2, arg2 = &0 = F'+4,
  arg3 = &code = F'+6 — arg N reads the slot at wsp-2N);
- `LCALL [0x7017EDED],3` — push (psr_body<<16)|3 at F+30,
  ac3 = 0x7017E3EF, ovr = 0.

**Boundary state at O?SIGNAL dispatch** (the trio's transfer point):
pc = 0x7017EDED, ac0 = code, ac1 = -1, ac2 = 0, ac3 = 0x7017E3EF,
c = 0, wsp = F+30, wfp = F' = F+16, psr = psr_body (ovk=1, ovr=0).

e3EF WRTN — the return path if O?SIGNAL returns.

## ?LIB_ERROR_CODE (0x7017DE25) — decoded flow

WSAVS 0 (F = W+10, wsp stays F). T?AREA idiom (LCALL at wsp = F:
call word at F+2, T?AREA frame at F+4..F+12, saved-ac0 slot F+4
patched to area). Then ac0 = [B+1], `XWSTA 0,[ac3+0x7FF8]` patches
the OWN saved-ac0 slot [F-8], WRTN → returns the last error code in
ac0, everything else restored to entry values. Zero arguments.

## Residue maps (final values; last-writer-wins)

### ?LIB_ERROR + ?DEFAULT_ERROR_HANDLER to the O?SIGNAL boundary

All offsets relative to F = entry wsp + 10. `psr_body =
(entry_psr | 0x8000) & ~0x4000`. c_x as derived above. Intermediate
writers (ten T?AREA frames at F+6..F+16 with the patch at F+8; inner
heap-call WSSVS images at F+6..F+16 and residue F+18..F+34) are all
overwritten by the final writers listed — EXCEPT [F+36], see note.

| Addr | Final value | Final writer |
|---|---|---|
| [F-8]  | entry ac0 | ?LIB_ERROR WSAVS |
| [F-6]  | entry ac1 | WSAVS |
| [F-4]  | entry ac2 | WSAVS |
| [F-2]  | entry (caller) wfp | WSAVS |
| [F]    | entry ret 0x…(caller)+4 \| entry_c<<31 | WSAVS |
| [F+2]  | narrow: len (message path only; [F+3] stale) | XNSTA e392 |
| [F+4]  | alloc result (message path only) | XWSTA e39C |
| [F+6]  | (psr_body<<16) \| 0 | XCALL e3CD |
| [F+8]  | area (= B-8) | handler WSAVS (ac0) |
| [F+10] | 0 (companion) | handler WSAVS (ac1) |
| [F+12] | 0x7017E3D2 | handler WSAVS (ac2) |
| [F+14] | F | handler WSAVS (wfp) |
| [F+16] | 0x7017E3D0 \| c_x<<31 | handler WSAVS (ret) |
| [F+18] | 0xFFFFFFFF | handler local [F'+2] |
| [F+20] | 0 | handler local [F'+4] |
| [F+22] | code | handler local [F'+6] |
| [F+24] | F+22 | XPEF e3E5 |
| [F+26] | F+20 | XPEF e3E7 |
| [F+28] | F+18 | XPEF e3E9 |
| [F+30] | (psr_body<<16) \| 3 | LCALL e3EB |
| [F+32] | F+16 | handler-T?AREA frame (wfp slot) — survives |
| [F+34] | 0x7017E3D8 \| c_x<<31 | handler-T?AREA frame (ret slot) — survives |

Note [F+32]: the free/alloc calls also write this slot
(their [F_i+16] with F_i = F+16), value F+16 — identical to the
surviving T?AREA write; and [F+36] = block ptr (I.FREEW's water
scratch, free path only) survives UNWRITTEN by later code — it is
produced by emu_rt::i_freew itself, so calling the real function at
the exact emulated machine state reproduces it (and every other
heap-side effect: HEAP_BREAK/LOWMARK, sentinel, headers, machine.wsl)
without re-derivation. **This is the design principle of the whole
translation: inner heap calls are made by invoking the validated
emu_rt::i_freew / emu_rt::i_alloc with machine.{ac0..ac3, c, wsp,
wfp, ovk, ovr} staged to the exact values the emulated LJSR sites
have** (wfp = F and wsp = F+4 must be staged temporarily — the
clone's real wfp at dispatch time is still the caller's).

Area/heap writes: narrow [B] |= 0x8000; [B+1] = code; [B+3] = 0 then
(message path) newbuf; install pair [B+0x1E]/[B+0x20] when H was 0;
buffer narrow[0] = n and n text bytes from byte 2*newbuf+2; heap
effects via emu_rt.

### ?LIB_ERROR_CODE

| Addr | Final value | Writer |
|---|---|---|
| [F-8] | **code** (patched) | XWSTA 0x7017DE30 |
| [F-6..F] | entry ac1, ac2, wfp, ret\|c | WSAVS |
| [F+2] | (psr_body<<16) \| 0 | T?AREA LCALL word |
| [F+4] | area (patched saved-ac0) | T?AREA body |
| [F+6] | entry ac1 | T?AREA WSAVS |
| [F+8] | entry ac2 | T?AREA WSAVS |
| [F+10] | F | T?AREA WSAVS |
| [F+12] | 0x7017DE2B \| entry_c<<31 | T?AREA WSAVS |

Return: ac0 = code, ac1/ac2/c/psr = entry values, wsp = W-2 (0 args),
ac3 = restored wfp.

## Carry chain summary

- entry → e355: entry carry (install path clears it at e353 but both
  paths reach e35D `WSUB 0,0`);
- e35D → WCMV: 0 on every path (preserved across T?AREA, I.FREEW,
  I.ALLOC — each call restores its entry carry);
- WCMV: c_x = (fresh_len != clamped_n), 0 in the live case;
- XCALL/handler frames record c_x (ret wides at [F+16], [F+34]);
- inside the handler: WADC sets c=1 then WSUB 2,2 sets **c=0**, which
  is the boundary carry — deterministic regardless of c_x.

## Dispatch and pairing design (implemented; see lib_error.cpp)

- All three are LCALL_FRAME/XCALL-dispatch shaped:
  `uint32_t emu_rt::lib_error / lib_error_code /
  default_error_handler (hw::Machine&)`.
- **Nested-span rule** (first line of every wrapper): if
  `machine.rt_pending_return != 0` the dispatch fired inside another
  routine's emulated-fallback span (e.g. the e3CD XCALL reached while
  ?LIB_ERROR itself fell back); return the entry address WITHOUT
  re-arming, so the whole nest stays one emulated span on both
  engines. An unconditional re-arm here would end the clone's span at
  the inner return while the master's run-to-return continues — a
  guaranteed structural divergence. (The same latent hazard exists in
  i_alloc/i_lock's unconditional fallbacks but is unreachable there;
  noted in REPORT.md.)
- **O?SIGNAL composition**: the tail looks up 0x7017EDED in
  `machine.process->native_registry` — exactly what the emulated
  LCALL does. Absent (pre-integration): full fallback at entry,
  before any side effect. Present (Project 1 lands): the wrapper
  builds the exact boundary state above and calls the registered
  function as plain C++; a returned pc of 0x7017E3EF (normal return)
  triggers the two native WRTN replays; any other pc (handler
  dispatch, DEF?ON, or their own fallback continuation) is passed
  through — their fallback arms rt_pending_return itself and the
  swallow logic in run_steps keeps the pair symmetric.
- **Gates** (?LIB_ERROR, all pure reads, checked before any write):
  1. rt_pending_return == 0 (nested rule);
  2. [B+0x1E] ∈ {0, 0x7017E3D2} (defensive invariant);
  3. O?SIGNAL translated (`RTStubs::translated_bits`, NOT registry
     lookup — every entry has a stub registered) — else fallback
     (pre-integration state), or QUEST_LIBERROR_VALIDATE=N
     (validation-only: the N-th call proceeds and ends with
     native_transfer to 0x7017EDED — an RT-range transfer that is
     INTENTIONALLY pairing-divergent, used to harvest boundary pairs
     and captures);
  3a. `rt::signal_has_handler(machine, code)` — Project 1 review
     correction (binding): an unhandled signal runs to DEF?ON, an
     RT-RANGE terminal, and neither a native transfer there nor a
     mid-span fallback inside Project 1's wrapper pairs cleanly
     (span/terminal flag mismatch, or instruction-count skew). The
     no-handler case must therefore be predicted on PURE READS at
     THIS entry and fall back whole. The predicate is Project 1's
     chain-walk; a WEAK conservative default (false = always fall
     back) ships in lib_error.cpp until their strong definition
     replaces it at link time;
  4. stack headroom: wsl > 0 → entry wsp + 40 <= wsl (max native-path
     depth F+30; the emulated body would otherwise take the
     handle_overflow path, which only emulation reproduces);
  5. free-path gates on oldbuf (replicating emu_rt::i_freew's:
     leading < 0, lo/hi range both ends, trailing == leading, no
     pred/succ merge, block == break+4) — plus the shared heap gates
     (lock contended / waiters / defer > 0 / mode != 0 / owner);
  6. message-path gates against the POST-free heap image (break' =
     oldbuf + size - 4 when the free runs, wsl' = wsl + size):
     freeq == -1, request > 0, (wsl' - alloc_size) > F+22
     (their collision check with their F_i = F+16), shared gates;
  7. len >= 0 (a negative varying length would drive WCMV's
     negative-count reverse walk; statically impossible, gate anyway).
  Any failure → `RTStubs::entry_address("?LIB_ERROR")` with
  rt_pending_return armed (the standard single-span fallback).
- ?LIB_ERROR_CODE has no gates beyond the nested rule (leaf, no
  calls, no conditions). ?DEFAULT_ERROR_HANDLER (registered for its
  XCALL dispatch surface) gates on the nested rule + O?SIGNAL
  registration + headroom (entry wsp + 24).

## Open items closed

- WMOVR size calc: request = 1 + ceil(len/2) narrow words (varying
  string footprint). ✔
- "+0x8": record base B = wsb-0x21; t_area's first 8 words are dead
  header. ✔
- ~7017DDAF caller: 0x7017DDAE, ?OPEN_FILE wrapper, the sole two-arg
  site. ✔
- ON-unit replacement of [B+0x1E]: does not happen; single writer. ✔
