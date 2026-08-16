# Project 1 — O.SEARCH → O.SET cluster: Translation Derivation

Status: DERIVATION IN PROGRESS. Method: METHOD.md; merge rules:
SharedProtocol.md; writer-side chain documentation this must agree
with: O_ON.md. Quality bar: I_ALLOC.md.

All instruction semantics below are pinned from the emulator source
(hw/EagleStack.cpp, hw/EagleCompute.cpp, hw/EagleGeneral.cpp,
hw/EagleInstruction.cpp, hw/Machine.cpp), not from apparent intent.
Raw opcode words were dumped with `java -cp Tools HexDump QUEST QUEST
7017eddd 7017ef05` because the disassembler omits register fields on
some mnemonics (WLDAI prints no register; the finding in §3 depends on
that field).

## 1. Extent table — verified, WITH CORRECTIONS

| Addr | Symbol | Words | Callers (complete static inventory) |
|---|---|---|---|
| 0x7017EDDD | O.SEARCH | 10 | 1: LCALL from 0x7017ECDB (I.FFALT) — dead in emulation |
| 0x7017EDE7 | O.SIGNAL | 6 | **0 callers anywhere** (game + RT scan for `[0x7017EDE7]`) |
| 0x7017EDED | O?SIGNAL | **21** (EDED–EE01) | 3: 0x7017E3EB (?LIB_ERROR — LIVE), 0x7017EF3D (DEF?ON — beyond terminal frontier), 0x7017FD9B (routine ending at I.START-1, terminal family) |
| **0x7017EE02** | **R.SIGREC** | **5** | **0 callers anywhere** — entry missing from the PROMPT extent table (its 5 words were silently inside the "O?SIGNAL 21 words" row; the symbol IS in the disassembly banner) |
| 0x7017EE07 | O.SUNDER | 8 | 1: 0x7017ECE7 (I.FFALT) — dead |
| 0x7017EE0F | O.SOVERF | 8 | 0 callers |
| 0x7017EE17 | O.SZEROD | 8 | 0 callers |
| 0x7017EE1F | O.SFIXED | 8 | 0 callers |
| 0x7017EE27 | O.SSUBSC | 6 | 0 callers (matches M3Plan's exclusion note) |
| 0x7017EE2D | O.SCONVE | 6 | 1: 0x7017E71E (X.CB) — LIVE |
| 0x7017EE33 | O.SERROR | 35 (EE33–EE55; the SHARED BODY, see §4) | 9 LCALLs, all RT: E86D, E95C, E965, E96C, EC2C, ECC7, FDCA + 2 more in the grep set — heap-corruption paths; none observed live |
| 0x7017EE56 | O.SET | 175 (EE56–EF04; interiors EE62/EE7A/EE9D) | 1 external: 0x7017EBCC (I.SFALT, dead) + the internal XCALL at EE38 (the LIVE route) |

Boundary corrections vs the PROMPT (for REPORT.md "Shared-doc
corrections"):

- **R.SIGREC (0x7017EE02, 5 words) exists and is unlisted.** O?SIGNAL
  ends at EE01, not EE06. R.SIGREC: `WMOV 1,3; WSUB 1,1; WSSVS 0;
  WBR →EE38`. Zero static callers; presumably a PL/1 library entry
  Quest never links to. Derived (trivially) below; NOT translated
  (nothing can reach it; registering a symbol with no callers adds
  risk for zero validation).
- O.SIGNAL (EDE7) likewise has zero static callers (the PROMPT lists
  no note for it). Same treatment.

Hidden-code hazard check (METHOD §4): every word in EDDD–EF04 decodes
as an instruction in the linear dump; no WBR-2-skipped holes, no XCT
(the two known XCT sites E9F6/ECF4 are outside), no opcode-shaped
constants except the WLDAI immediates, which are load-time constants
read from code space at execution (see §3 — one of them is
load-bearing). quest-rt.addrs already classifies 7017ED8F–7017EF50 as
code (O_ON.md).

## 2. Instruction semantics (from emulator source)

Only what this cluster uses and previous docs haven't pinned
(O_ON.md pinned WSSVS/WSSVR/LDASB/LDATS/STATS/ISZTS/XPSHJ/WPOPJ/
WPSH/WPOP/WXCH/skip-return; I_ALLOC.md pinned WBLM/carry formulas).

- **Same-register skip instructions compare against literal zero.**
  `WSEQ/WSNE/WSLE/WSGT x,x`: `dst=(XX!=YY)?ac[YY]:0` in
  EagleCompute.cpp — so `WSGT 0,0` means "skip if ac0>0 (signed)",
  `WSEQ 2,2` means "skip if ac2==0". Every guard in this cluster uses
  the idiom. (The disassembly's rendering `WSGT 2,2` reads as a
  never-true comparison unless this is known.)
- **WSAVS fs** (vs WSSVS): pushes FIVE wides — ac0, ac1, ac2, wfp,
  ac3|c<<31 — NO psr wide (the caller's LCALL/XCALL already pushed
  (psr<<16)|argc). ac3=wfp=wsp after pushes; wsp+=fs*2; ovk=1; ovr
  NOT touched by WSAVS itself (LCALL/XCALL cleared it at the call).
  Saved slots from new ac3: ret|c [0], wfp [-2], ac2 [-4], ac1 [-6],
  ac0 [-8], caller frame word [-10], arg-N pointer [-10-2N].
- **WRTN**: wsp=wfp; pop ret|c, wfp, ac2, ac1, ac0, frame word;
  psr=frame>>16; wsp-=2*(frame&0x7FFF); ac3=restored wfp;
  c=ret>>31; jump ret&0x7FFFFFFF. Works identically over WSAVS and
  WSSVS frames (WSSVS's pushed psr wide has argc=0 in the low half).
- **XCALL a,argw,[tgt]**: resolves target; if argw bit15 clear pushes
  (psr<<16)|argw else argw&0x7FFF; ac3=pc+3; **ovr=0**; consults
  native_registry at the resolved address (so an emulated XCALL into
  a registered entry dispatches native). `XCALL 0,0,[ac2+0x0]` with
  target word 0x0000, ii=2 resolves to **ac2 itself** (displacement
  0, no indirect bit): call the address held in ac2.
- **LCALL**: as XCALL with wide target and ac3=pc+4.
- **XJSR**: ac3=pc+2; also consults native_registry.
- **Effective addresses**: index displacement is 15-bit sign-extended
  (`(w<<17)>>17`); bit15 of the operand word = one level of
  indirection. Hence `[ac3+0x7FF7]` = ac3-0x9 and `@[ac3+0xFFEE]` =
  indirect through [ac3-0x12].
- **wide_push**: wsp+=2 FIRST, then write at [wsp, wsp+1] (big-endian
  wide, high word at the lower address). wsp points AT the topmost
  wide. LDATS/STATS/ISZTS read/write/increment the wide at [wsp].
- **NLDAI imm,ac**: 16-bit immediate sign-extended (0xFFFC → -4).
- **WLDAI**: 32-bit immediate load; the register is in the yy field
  of the opcode word, which the disassembler does NOT print. Decoded
  from raw words: C689=ac0, D689=ac2 (pattern 110yy11010001001, yy
  at bits p3p4 MSB-first).
- **WSGTI ac,imm**: skip if ac > sign-extended 16-bit imm.
- **WSBI n,ac**: ac -= (n_field+1) via sub() — sets carry per the
  source formula `((dst&0xFFFFFFFF)-(src&0xFFFFFFFF))>>31 & 1`.
- **WADC x,y**: ac[y] = add(~ac[x], ac[y]) — for x==y this yields
  0xFFFFFFFF (-1) and sets carry per the add formula
  `((dst&0xFFFFFFFF)+(src&0xFFFFFFFF))>>31 & 1` = 1.
- **WSUB x,x**: zeroes the register AND sets c=0 (METHOD §5's
  documented trap), ovr|=0 (unchanged).
- **XNLDA**: narrow load, sign-extends the 16-bit word.
- **INC.# s,d,SZR** (Nova): d gets s+1 with no-load (#), skip if the
  16-bit... — in EagleCompute's Nova path INC computes src+1; with #
  the result is not stored; SZR skips if the (32-bit wrapped) result
  is zero. `INC.# 0,0,SZR` = "skip if ac0 == -1". (Verified against
  NovaCompute.cpp: carry/skip evaluated on the would-be result.)
- **MOV.L# 0,0,SZC** (in ERROR_PROCESSING.md's EOF idiom, not this
  cluster) — listed for completeness only.

psr composition (Machine.cpp): `(ovk<<15)|(ovr<<14)|(ires<<13)|
(ixct<<12)|(ffp<<11)|sr`. Pushed frame words embed the psr at push
time; the derivations below track ovk/ovr per push site.

## 3. THE CENTRAL FINDING — the I?LINEID region is statically dead

The walker at EE9D opens:

```
7017ee9d WSSVR 0x0005          ; 5 wides of (never-written) locals
7017ee9f WLDAI ac0, 0x00000000 ; raw words C689 0000 0000 — ac0 = 0
7017eea2 WSGT 0,0              ; skip if ac0 > 0  — never (ac0 == 0)
7017eea3 WBR 71 → 0x7017EEEA   ; ALWAYS taken
```

The register field of the WLDAI is ac0 (C689; the dis prints only the
immediate), and same-register WSGT compares against literal 0 (§2).
So the branch to EEEA is unconditional **given the immediate 0 at
words 0x7017EEA0–EEA1 in the program image** (verified in the .PR via
HexDump). Everything from EEA4 through EEE9 — the [wsb-0x40] cache at
EEA4–EEA7, the frame-identity walk at EEAC–EEC3, the XPEF/LCALL
**I?LINEID** call at EEC4–EED2, and the two result paths EED3–EEE9 —
is unreachable in this binary. It decodes as coherent PL/1
line-number/scope machinery (consistent with the I?LINEID →
?FIND_SCOPE naming), plausibly a compile-time-disabled diagnostic
mode: the immediate is the enable flag.

Consequences:

- This EXPLAINS the empirical I?LINEID ×0 across both live signal
  shapes (M3Plan): not "empirically untaken", but statically
  unreachable.
- **Correction to the locked-decision framing** (PROMPT "Facts
  established", M3Plan "O.SET's I?LINEID branch = terminal"): the
  guard is not the mid-walk `[wsb-0x40]` comparison (that comparison
  exists, at EEB2, but is itself dead); the guard is the entry test
  of the wide at 0x7017EEA0. The locked decision's substance is
  PRESERVED and gets cheaper: the native walker reads the wide at
  0x7017EEA0 exactly as the WLDAI does (the emulator re-reads code
  space on every execution, so a patched immediate would change
  emulated behavior — the native must match); if it is >0, the
  translation falls back to emulation from entry BEFORE ANY STORE
  (trivially satisfied — the gate is the first thing the walker
  does), arming rt_pending_return, with a loud gate-reason log. The
  I?LINEID subtree stays untranslated. If the fallback ever fires,
  that is the signal the 908-word subtree has become live.
- The "EE9D walker is PURE READS until 0x7017EEFF" fact survives and
  strengthens: on the ONLY reachable path there are no writes at all
  before the two task-area stores at EF00/EF02 (the dead region
  would have written frame locals and pushed I?LINEID arguments).

## 4. Control-flow truth (the extent table is symbol accounting only)

All signaling entries funnel into a SHARED BODY that lives inside
O.SERROR's symbol extent:

```
entry            sets                        joins at
O.SIGNAL   WSSVS; ac2=0x11601 (WLDAI)        EE38
O?SIGNAL   WSAVS; ac0/1/2 = *args1-3;
           [wsb-0x2A] = *arg4 or 0           EE38
R.SIGREC   ac3=ac1; ac1=0; WSSVS             EE38   (dead)
O.SUNDER   WSAVS; ac0=-4; ac2=0x11616        EE37
O.SOVERF   WSAVS; ac0=-3; ac2=0x11607        EE37
O.SZEROD   WSAVS; ac0=-5; ac2=0x11608        EE37
O.SFIXED   WSAVS; ac0=-2; ac2=0x11606        EE37
O.SSUBSC   WSAVS; ac2=0x11612                EE35
O.SCONVE   WSAVS; ac2=0x11611                EE35
O.SERROR   WSAVS                             EE35
```

Register roles at EE38: **ac0 = condition type** (negative = built-in:
-1 ERROR, -2 FIXEDOVERFLOW, -3 OVERFLOW, -4 UNDERFLOW, -5 ZERODIVIDE;
O?SIGNAL passes the caller's value, which may be >0 for user
conditions), **ac1 = key2** (0 from all fixed entries; O?SIGNAL passes
*arg2), **ac2 = condition/message code** (0x1160x/0x1161x from fixed
entries; O?SIGNAL passes *arg3). The WLDAI target register is ac2
(D689) — NOT ac1 as a naive reading of "load code then WSUB 1,1"
would survive; EE35 (`NLDAI -1 → ac0`) and EE37 (`WSUB 1,1` → ac1=0,
c=0) execute AFTER the WLDAI, so the code in ac2 is live into EE38.

Shared body (all word addresses):

```
EE38  XCALL 0,0,[O.SET]        ; argw=0: push (psr<<16)|0; ac3=EE3B; ovr=0
EE3B  XPSHJ [EE62]             ; push wide EE3D; jump select loop
EE3D  XCALL 0,0,[ac2+0x0]      ; DISPATCH: call the address in ac2
                               ; (handler, or DEF?ON on exhaustion);
                               ; push (psr<<16)|0; ac3=EE40; ovr=0
--- handler-returned tail (runs only if the dispatched code WRTNs) ---
EE40  LDASB 2
EE41  XWLDA 2,[wsb-0x2A]       ; the 4th-arg flag O?SIGNAL stored
EE43  WSEQ 2,2                 ; skip if flag == 0
EE44  WRTN                     ; flag != 0: resumable signal — return
EE45  WSGT 0,0                 ; skip if handler's ac0 > 0
EE46  WBR →EE48
EE47  WRTN                     ; ac0 > 0: treat as handled — return
EE48  WLDAI ac2, 0x00011618    ; ERROR code
EE4B  INC.# 0,0,SZR            ; skip if ac0 == -1
EE4C  WBR →EE35                ; type != -1: re-signal as ERROR
EE4D  LDASB 3
EE4E  XWLDA 0,[wsb-0x3A]       ; the code O.SET saved (entry ac2)
EE50  WSEQ 0,2                 ; skip if code == 0x11618
EE51  WBR →EE35                ; not yet ERROR/0x11618: re-signal
EE52  LCALL [I.STOP],0         ; already ERROR-signaled: stop
```

The re-signal loop (EE4C/EE51 → EE35) prevents infinite recursion:
an unhandled/mishandled signal escalates once to ERROR (type -1, code
0x11618), and if THAT comes back it terminates via I.STOP.

### EE62 select loop (entered by XPSHJ; "returns" by WPOPJ)

Stack on entry: [... ret-wide]. Registers: ac0=type, ac1=key2,
ac2=code (code is dead here), ac3=EE3D-or-EDE1.

```
EE62  WPSH 1,1                 ; back up key2 (the helper clobbers ac1)
EE63  LDASB 2
EE64  XWLDA 2,[wsb-0x40]       ; innermost condition frame (I.PROLOG's)
EE66  WSGT 2,2                 ; skip if frame > 0
EE67  WBR →EE6F                ; chain exhausted
EE68  XJSR [EE7A]              ; chain_search(frame, ac0, ac1)
EE6A  WBR →EE75                ; ret+0: FOUND (helper's ac1 = node)
EE6B  LDATS 1                  ; ret+1: restore key2 from TOS (no pop)
EE6C  XWLDA 2,[ac2+0x8]        ; enclosing-frame link
EE6E  WBR →EE66
EE6F  WPOP 3,3                 ; NOT FOUND: pop key2 backup into ac3
EE70  LLEF 2,[EF05]            ; ac2 = DEF?ON entry address
EE73  WSUB 1,1                 ; ac1 = 0 (and c=0)
EE74  WPOPJ                    ; jump to pushed ret (EE3D / EDE1)
EE75  WPOP 3,3                 ; FOUND: pop key2 backup into ac3
EE76  WXCH 1,2                 ; ac1 = frame, ac2 = node
EE77  XWLDA 2,[ac2+0x6]        ; ac2 = node[+6] = handler address
EE79  WPOPJ
```

Exit contract (matches O_ON.md's signal-path sketch, now exact):
found → ac1 = the FRAME that registered the handler (I.GOTO's unwind
argument), ac2 = handler address, ac3 = stale key2 backup (a pop
clobber), c = whatever the last helper run left (see helper carry
note below); not found → ac1=0, c=0, ac2=DEF?ON, ac3 = stale key2.
The WPSH/LDATS dance exists because the EE7A helper both zeroes ac1
in its preamble (catch-all) and overwrites its saved-ac1 slot with
the result — the backup on TOS is the only surviving copy of key2
across iterations.

### EE7A helper — IDENTICAL to O_ON.md resolution 6

Verified word-for-word against the raw dump; rt::chain_search
(runtime/o_on.hpp) is an exact functional port and is REUSED, not
re-derived (same-tree validated code, not a parallel project's).
Residue per invocation (from write_helper_residue in o_on.cpp): its
six-wide WSSVR image at [xjsr_wsp+2 .. +13], the scratch wide at
[+14], saved-ac1 slot patched with the result, ret wide +1 on
not-found. Preamble carry: `WSUB 1,1` when type<=0 sets c=0 BEFORE
the frame save, so the pushed ret|c wide carries c=0 for catch-all
searches (the O_ON.md single-bit lesson); for type>0 the entry carry
rides through.

### O.SET (EE56) and the EE9D walker

```
EE56  WSAVS 0
EE58  XJSR [EE9D]
EE5A  LDASB 3
EE5B  XWSTA 0,[wsb-0x3E]       ; entry ac0 = type
EE5D  XWSTA 1,[wsb-0x3C]       ; entry ac1 = key2
EE5F  XWSTA 2,[wsb-0x3A]       ; entry ac2 = code
EE61  WRTN
```

(The stores see ENTRY values because the walker's WRTN restored
them.) Consumers of [wsb-0x3E/-0x3A]: the shared body's EE4E and
R?SIGNAL/?ERROR at 7017EF5B/5D (beyond the terminal frontier);
[wsb-0x3C] has no reader anywhere. Live walker (post-§3 gate):

```
EE9D  WSSVR 5                  ; locals never written (residue)
EE9F  WLDAI ac0, [0x7017EEA0]=0
EEA2  WSGT 0,0 / EEA3 WBR →EEEA   ; the §3 gate
EEEA  LDASB 3
EEEB  XWLDA 3,[wsb-0x40]       ; frame = innermost condition frame
EEED  WSGT 3,3                 ; skip if frame > 0
EEEE  WBR →EEFA                ; none → defaults
EEEF  XWLDA 2,[ac3+0x4]        ; p = frame[+4]
EEF1  WSGT 2,2                 ; skip if p > 0
EEF2  WBR →EEF7                ; no p → next frame
EEF3  XNLDA 1,[ac2+0x0]        ; w = sign-extended word at [p]
EEF5  WSLE 1,1                 ; skip if w <= 0
EEF6  WBR →EEFD                ; w > 0 → capture
EEF7  XWLDA 3,[ac3+0x8]        ; frame = frame[+8] (enclosing link)
EEF9  WBR →EEED
EEFA  WSUB 2,2                 ; ac2 = 0 (c=0)
EEFB  WSUB 1,1                 ; ac1 = 0 (c=0)
EEFC  WBR →EEFF
EEFD  XWLDA 1,[ac3+0x6]        ; ac1 = frame[+6]
EEFF  LDASB 3
EF00  XWSTA 1,[wsb-0x36]       ; result 1
EF02  XWSTA 2,[wsb-0x38]       ; result 2 (p, or 0)
EF04  WRTN
```

Walk semantics: find the innermost frame whose frame[+4] points at a
positive word; record (frame[+6], frame[+4]) to [wsb-0x36]/[wsb-0x38],
else zeros. frame[+4]/[+6] are I.PROLOG-initialized slots adjacent to
the O_ON.md-documented [+8] (enclosing link) and [+0xA] (chain-head
pointer) — plausibly a condition-prefix / ONCODE cell, but the
translation does not need to know: it replicates the loads and
stores. **Static scan finds NO reader of [wsb-0x38] or [wsb-0x36]
anywhere in game or RT code** — the stores are dead but are
replicated regardless (bit-identical footprint; a computed-address
reader would be invisible to the scan).

Carry subtlety: on the found/capture path the walker executes no
carry-writing instruction after entry, and WRTN restores entry carry
anyway; on the empty/default path WSUB clears c but WRTN's restore
makes it invisible to the caller. Register-visible effect of the
whole walker: NONE (WRTN restores everything); memory effect: its
WSSVR image + the two task-area stores.

### O.SEARCH (EDDD) — dead, derived for completeness

```
EDDD  WSAVS 0
EDDF  XPSHJ [EE62]             ; push EDE1; run the select loop
EDE1  LDAFP 3                  ; ac3 = own frame (WPOP clobbered ac3)
EDE2  XWSTA 1,[ac3-6]          ; saved-ac1 slot := frame-or-0
EDE4  XWSTA 2,[ac3-4]          ; saved-ac2 slot := handler-or-DEF?ON
EDE6  WRTN
```

Returns the select-loop result to its caller (I.FFALT) via patched
register slots. Uses the caller's ac0/ac1 as search keys — I.FFALT
sets ac0=-4 (UNDERFLOW) before the call. Untranslatable-validation
class: no live path can exercise it (I.FFALT is a hardware fault
vector the emulator replaces with a C++ throw). Derivation recorded;
translation decision in §6.

## 5. Frame/residue images per live entry (word-for-word)

Notation: E = entry wsp at native dispatch (frame word already pushed
by LCALL/XCALL: wide at [E] = (caller_psr<<16)|argc). "fw(x)" = a
frame word (psr<<16)|x at the machine state of that push.

### O?SIGNAL (LCALL, argc=3 or 4), through handler dispatch

WSAVS 0: wides at [E+2]=ac0e, [E+4]=ac1e, [E+6]=ac2e, [E+8]=wfp_e,
[E+10]=ret|c_e; F := E+10 (frame ptr); ovk=1. Body loads args (reads
only), stores [wsb-0x2A]. Then the shared-body sequence stacks:

- [E+12] = fw(0) with ovk=1, ovr=0-at-XCALL... — the XCALL at EE38
  pushes (psr<<16)|0 where psr has ovk=1 (WSAVS), ovr as left by the
  body (no ovr writers since LCALL cleared it → 0).
- O.SET's WSAVS image at [E+14..E+22] (its F' = E+22), pushed values:
  ac0/1/2 = type/key2/code, wfp=F, ret=EE3B|c (c unchanged since
  entry... c at this point: O?SIGNAL's body runs WSUB 0,0 on the
  argc<=3 path (c=0) or not (entry c) — TRACKED per path).
- Walker WSSVR image at [E+24..E+34] (psr first, with ovk=1 from
  O.SET's WSAVS; ovr=0), locals [E+36..E+44] untouched, wsp=E+44.
  Walker returns (residue stays), O.SET stores 3 task-area wides,
  returns (residue stays), machine back to wsp=E+12... WRTN of
  O.SET: wsp=F'=E+22 → pops → wsp = E+22-10-2*0 = E+12. ✓
- XPSHJ: [E+14] (OVERWRITING the O.SET image base) = wide EE3D.
- Select loop: [E+16] = key2 backup. Helper invocations at
  wsp=E+16: WSSVR image [E+18..E+28], scratch [E+30]. (Each
  iteration overwrites the same addresses — LAST iteration's image
  survives; iteration count = frames walked until found/exhausted.)
- Exit: WPOP (wsp=E+14), WPOPJ (wsp=E+12), XCALL [ac2]:
  [E+14] = fw(0) again (psr now: ovk=1 — unchanged since WSAVS; ovr
  0), ac3=EE40, ovr=0, TRANSFER with wsp=E+14.

Final register state at transfer: ac0=type, ac1=frame-or-0,
ac2=handler-or-DEF?ON, ac3=EE40, c = last helper's exit carry
(catch-all searches: 0 via the preamble WSUB; the not-found direct
path EE73 WSUB also 0; type>0 found path: entry carry). The three
live shapes all have type<=0 ⇒ **c=0 at transfer in every observed
case**; the translation computes it exactly anyway.

### O.SCONVE / shorthands (LCALL argc=0) and O.SERROR

Identical from EE38 onward with fixed ac0/ac2, ac1=0, and no
[wsb-0x2A] store — NOTE: the EE41 read of [wsb-0x2A] in the
handler-returned tail then sees a STALE value from some earlier
O?SIGNAL. Faithfully replicated (the tail is emulated anyway — §6).

### O.SET called directly (XCALL at EE38 emulated, or I.SFALT)

WSAVS image + walker residue + 3 stores; native_return. argc=0 both
ways (I.SFALT's LCALL pushes argw=0).

## 6. Translation design

Files: `runtime/o_signal.{hpp,cpp}` (one pair — the cluster shares
everything).

Layered exactly like o_on.cpp:

- `rt::signal_walker(machine, out)` — pure function of machine
  memory: reads the EEA0 gate wide, walks, returns the two result
  values + a `gate_open` flag. No writes.
- `rt::select_frames(machine, type, key2, out)` — walks [wsb-0x40]
  frames calling **rt::chain_search** (reused from o_on.hpp) per
  frame; returns found/frame/node/handler, iteration count, and the
  last search's residue inputs so the wrapper can lay the final
  helper image.
- `emu_rt::o_set` — bridge(LCALL_FRAME); if walker gate open →
  fallback: log gate reason, arm `rt_pending_return = ac[3]`, return
  `RTStubs::entry_address("O.SET")` (METHOD §12; the walker is
  read-free before the gate so fallback precedes any store). Else:
  emulate_frame(), walker WSSVR residue image, two task-area stores,
  three saved-value stores, native_return().
- `emu_rt::o_qsignal` — bridge; extract args ([ac3-9] argc through
  the frame image — via bridge accessors), store [wsb-0x2A]; then the
  SHARED C++ body `signal_dispatch(machine, bridge, type, key2,
  code, entry_shape)` which lays every §5 image (O.SET inlined as
  C++ — subtree rule, no emulation re-entry), runs the select loop,
  and ends with the XCALL-state replication + 
  `RTBridge::native_transfer(machine, target)` where target =
  handler (live) or DEF?ON (terminal machinery detaches on arrival).
  The handler-returned tail EE40–EE55 is NOT translated: the
  transfer's return address EE40 points into emulated code; if a
  handler ever WRTNs, both engines emulate the tail symmetrically
  (any re-signal re-enters via the registered O.SET / the I.STOP
  LCALL is terminal). This keeps the native side loop-free.
- `emu_rt::o_sconve`, `o_serror`, + the other four shorthands — thin
  wrappers setting the fixed type/code and calling signal_dispatch.
- O.SIGNAL / R.SIGREC / O.SEARCH: NOT REGISTERED (zero callers /
  dead vector paths; O.SEARCH additionally has no validation path —
  METHOD §9). Derivations above are complete should that change.

**CORRECTION (found by lockstep, first validation run):** the design
above originally ended the not-found path with
`native_transfer(DEF?ON)` per SharedProtocol's "terminal paths compose
with transfer" instruction. That diverged: DEF?ON is a TERMINAL entry
INSIDE the RT range, and terminal pairs compare instruction counts —
the count exemption requires native_span on BOTH sides
(hw/Lockstep.cpp compare_pair), while the master ends its run-to-return
at a terminal entry with terminal_reached and NOT native_span
(hw/Machine.cpp Exception 1). Master insns=87 vs clone insns=17,
identical registers — a pure pairing-structure divergence. Fix, kept
entirely inside this project's files: (1) the not-found case falls
back to emulation from entry (decided on pure reads, before any
store) so both engines emulate to DEF?ON with equal counts and both
terminal flags; (2) a nested-in-fallback guard on every entry (clone
rt_pending_return != 0 at native dispatch → return entry_address
WITHOUT re-arming), because the re-emulated O?SIGNAL body XCALLs
O.SET, which is also registered — a nested native run would skew the
terminal pair's count the same way. The transfer machinery remains in
use for the FOUND path, whose target is game code (range exit →
native_span both sides). SharedProtocol's DEF?ON guidance needs the
same correction — REPORT.md item.

Fallbacks (all arming rt_pending_return per METHOD §12, except the
nested guard which must NOT re-arm):
- o_set: the EEA0 gate.
- signal_dispatch: the EEA0 gate, and select-exhaustion (above).
- every entry: the nested-in-fallback guard (above).
- signal_dispatch: same gate (it embeds O.SET's work), checked FIRST
  — before the [wsb-0x2A] store, which precedes it in the emulated
  order... it does NOT: O?SIGNAL stores [wsb-0x2A] BEFORE the XCALL
  to O.SET. A gate-open fallback after that store would leave a
  clone-only store → asymmetric. Order in the native: read gate
  wide FIRST (pure read), fall back before touching anything, else
  proceed. The master emulating from entry performs the same store
  — symmetric. ✓
- Defensive: if the select loop iterates > 64 frames (cycle guard),
  abort loudly (a cycle would hang the emulated master identically;
  prefer the named failure — METHOD §8).

## 7. Agreement with O_ON.md (required check)

- Chain-node layout, [frame+0x8]/[frame+0xA] roles, helper contract,
  skip-return, backstop STATS semantics: AGREE (and the select-loop
  read side is now exact where O_ON.md's signal-path section was
  sketched: the "LDATS/next frame via [frame+0x8]" guess is
  confirmed, and the WPSH 1,1 purpose — key2 preservation across
  helper clobbers — is now explained).
- O_ON.md "O.SET ... saves ac0-2 to [wsb-0x3E,-0x3C,-0x3A]": AGREE.
- O_ON.md's REVERT-vs-wildcard tension (node[+2]==0 meaning):
  unaffected by this cluster (the search side is chain_search,
  already implemented and validated).
- New: [wsb-0x36]/[wsb-0x38] are the walker's outputs; no readers
  found (§4). [wsb-0x2A] is O?SIGNAL's 4th-arg cell, read at EE41.

## 8. Validation (executed; full evidence + exact commands in REPORT.md)

The `QUEST_FAIL_OPEN=USER_DATA_FILE` + L→P trigger turned out to
exercise BOTH dispatch outcomes in one session: signal 1 (failed open
inside LIST_PLAYERS) is caught by catalog handler #13 at 0x7016EC57
(found path, handler I.GOTOs to resume), and a follow-on signal 2
(?WRITE_SCREEN path via REFRESH_SCREEN) exhausts the chain and goes to
DEF?ON (terminal detach). Ground truth from a vanilla-lockstep rtcalls
trace; wsb = 0x7000108C from a T?AREA capture (RETURN ac0 + 0x29).

- Master expected-value captures (vanilla lockstep): O.SET
  ENTRY/RETURN pairs at 7017EE56 (both signals), dispatch-arrival
  ENTRY at 7016EC57 (found) and at 7017EF05 (DEF?ON). Every word of
  §5's residue maps confirmed, including the ISZTS'd helper ret
  0x7017EE6B on the not-found search and the un-incremented 0x7017EE6A
  on the found one.
- With translations registered (temporary local table entries,
  reverted after): the found-path transfer pairs as a native_span at
  pc=7016EC57; master ENTRY vs clone NATIVE diff = **0 differing
  words, all registers identical**. The no-handler signal falls back
  and detaches cleanly at DEF?ON, 0 divergences (after the §6
  correction; the pre-correction divergence report is the evidence for
  it).
- o_set in isolation (only O.SET registered; clone dispatches it
  natively at the emulated EE38 XCALL): master RETURN vs clone NATIVE
  = **0 differing words** on both signals; register deltas are the
  documented pre-native_return snapshot timing, and the pair gate
  verified post-return state by not diverging.
- Not exercised on this trigger: the shorthand entries (O.SCONVE
  needs the store-"ABC" CONVERSION trigger — real gameplay at ~49
  s/turn; O.SERROR needs a heap-corruption path). Their entire body is
  the validated signal_dispatch; only the 2-instruction fixed-constant
  prefixes are unexercised. Left for the integration pass / the
  M3Plan provocation criterion.

Per-routine status lives in REPORT.md.

## Reviewer addendum (Aug 13 2026, Project 6 correction c1)

§5's residue offsets are +2 words high from the XPSHJ line onward —
the prose forgets O.SET's WRTN popped the EE38 frame word. The
capture-validated o_signal.cpp offsets are authoritative (three
successive writes land at [E+12]; see its ORDER comment).
L2Contract.md §3.9 carries the corrected image.
