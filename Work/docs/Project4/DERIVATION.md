# Project 4 — O?AREA, P?DEFON (DEF?ON satellites): Translation Derivation

Status: TRANSLATED, REGISTERED (DEF?ON: staged-unregistered),
BUILD-CLEAN, REGRESSION-VALIDATED. Covers the full cluster:
O?AREA + P?DEFON (the Aug 12 satellites pass) and R?SIGNAL + DEF?ON
(the same-day continuation, sections 5-6).
Neither routine is lockstep-exercisable natively yet (see "Validation
status") — both are DORMANT BY CONSTRUCTION until the DEF?ON lift moves
the detach point deeper. O?AREA's derivation is EMPIRICALLY CONFIRMED
against a live master capture. Session: Aug 2026, single session (the
Aug 12 satellites session; SessionPlan.md record).

Derived instruction-by-instruction from `Disassembled/quest-rt.dis`
(METHOD.md §1); semantics pinned from the emulator source (§5), never
from intent. Implementation: `runtime/o_area.{cpp,hpp}`,
`runtime/p_defon.{cpp,hpp}`; registered in `hw/RTStubs.cpp`
translation_table.

## 1. Entry facts

| Symbol | Address | Words | Called via | Frame | Ends |
|---|---|---|---|---|---|
| O?AREA | 0x7017FC39 | 8 | LCALL, 0 args | WSAVS 0x0000 | WRTN (normal) |
| P?DEFON | 0x7017FD7A | 38 | LCALL, 3 args | WSAVS 0x0007 | WRTN (both paths) |

Call-site census (quest-rt.dis; zero game callers for either):
- O?AREA: DEF?ON at 0x7017EF07, ?FATAL at 0x7017F03C. Both terminal-subtree.
- P?DEFON: DEF?ON at 0x7017EF22 (sole site), on the `[area+0x2] > 0` branch.
- C?INIT (called BY P?DEFON): body is `WSAVS 0x0000; WRTN` at 0x7017FDB9
  — a no-op with residue. (The NextSession grouping "C?INIT (init +
  P?DEFON)" lists C?INIT's CALLERS; a first reading of it inverted the
  edge — see REPORT §4.)

## 2. O?AREA

```
7017fc39 WSAVS 0x0000;             frame, no locals; ovk=1
7017fc3b LDASB 2;                  ac2 = wsb
7017fc3c XLEF 0,[ac2+0x7FC0];      ac0 = wsb - 0x40   (15-bit disp: 0x7FC0 = -0x40)
7017fc3e XWSTA 0,[ac3+0x7FF8];     saved-ac0 slot ([F-8]) = ac0
7017fc40 WRTN;
```

The T?AREA twin (0x7017ED93: identical shape, displacement 0x7FD7 =
-0x29) at a different fixed offset below the task stack base. Returns —
via the saved-ac0 slot-patch idiom, the ?UDIV32/T?AREA precedent — the
address `wsb - 0x40`: the per-task SIGNAL AREA the condition system
already reads and writes throughout:

| Word | Content | Established by |
|---|---|---|
| [area+0x0] | ON-frame chain head | o_signal.cpp select/walker loops (`[wsb-0x40]`) |
| [area+0x2] | recorded signal type | O.SET store `[wsb-0x3E]` |
| [area+0x4] | recorded key2 | O.SET store `[wsb-0x3C]` |
| [area+0x6] | recorded code | O.SET store `[wsb-0x3A]` |
| [area+0x8] | walker out2 slot | O.SET/walker `[wsb-0x38]` |
| [area+0xA] | walker out1 slot | `[wsb-0x36]` |

So O?AREA is the ACCESSOR for the area the M3 work has been addressing
as raw `wsb-0x40` offsets all along; `rt::o_area(machine)` is now the
named form. Distinct from `rt::t_area` (= wsb-0x29, the ?LIB_ERROR
message/handler area).

### Empirical confirmation (master capture, this session)

`QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_CAPTURE=7017FC39
QUEST_CAPTURE_DEST=70000200`, L→P, continue: the unhandled second
signal runs DEF?ON on the master post-detach, LCALLing O?AREA at ef07.
Master capture pair (run2/capture-QUEST1.txt, seq=36):

- ENTRY pc=7017FC39: ac0=FFFFFFFF ac1=0 ac2=7017EF05 ac3=7017EF0B
  wsp=7000122A wfp=70001214 c=0 ovk=1; frame wide [wsp]=80000000
  (psr 8000, argc 0 ✓).
- RETURN pc=7017EF0B: **ac0=7000104C** (= wsb-0x40 for this task's
  wsb=7000108C), ac1/ac2 restored to entry values, ac3=70001214
  (entry wfp), wsp=70001228 (entry-2: frame wide popped, 0 args) ✓.
- Frame image at RETURN, [E+2..E+11]:
  `7000104C | 00000000 | 7017EF05 | 70001214 | 7017EF0B(c=0)` — the
  exact RTBridge::emulate_frame layout with the ac0 slot PATCHED to
  the result. Bit-for-bit the translation's footprint.
- Entry ac0=FFFFFFFF is itself evidence: DEF?ON loads ac1=[area+0x2]
  before this call only on re-entry paths; here the RECORDED TYPE was
  -1 (ERROR class, the failed-open signal), consistent with DEF?ON then
  taking its ef27→ef41 R?SIGNAL branch and P?DEFON staying cold (§3.6).

This is a MASTER-side confirmation of the derivation, not the 0-diff
NATIVE-vs-RETURN protocol — the clone detaches at DEF?ON and never
reaches the wrapper. See "Validation status".

## 3. P?DEFON

### 3.1 Body

```
7017fd7a WSAVS 0x0007;             image [E+2..E+11], F=E+10, ac3=wfp=F,
                                   locals [F+2..F+15], wsp=E+24, ovk=1
7017fd7c XWLDA 0,@[ac3+0xFFF0];    ac0 = *(ptr at [F-0x10]) = *arg3 = type
7017fd7e WSEQI 0,2 (0x0002);       skip next if type == 2
7017fd80 WBR 8 (0x7017FD88);
  -- type == 2 --
7017fd81 XPEF @[ac3+0xFFF4];       push ptr at [F-0xC] = arg1 pointer
7017fd83 LCALL [0x7017FDB9],1;     C?INIT (= WSAVS 0; WRTN)
7017fd87 WRTN;
  -- type != 2 --
7017fd88 XWLDA 0,@[ac3+0xFFF0];    ac0 = type again
7017fd8a WSEQI 0,6 (0x0006);       skip next if type == 6
7017fd8c WBR 5 (0x7017FD91);
7017fd8d WADC 0,0;                 ac0 = -1, c = 1 (§3.4)
7017fd8e XWSTA 0,[ac3+0xC];        local [F+12] = -1
7017fd90 WBR 5 (0x7017FD95);
7017fd91 NLDAI 6 (0x0006),0;       ac0 = 6 (no flag effects)
7017fd93 XWSTA 0,[ac3+0xC];        local [F+12] = 6
7017fd95 XPEF @[ac3+0xFFF2];       push ptr at [F-0xE] = arg2 pointer
7017fd97 XPEF @[ac3+0xFFF4];       push ptr at [F-0xC] = arg1 pointer
7017fd99 XPEF [ac3+0xC];           push F+12 (direct EA, no @)
7017fd9b LCALL [0x7017EDED],3;     O?SIGNAL
7017fd9f WRTN;                     reached only if a handler RESUMES
```

### 3.2 Argument mapping

Displacements are signed (0xFFF0=-0x10, 0xFFF2=-0xE, 0xFFF4=-0xC).
Arg slots sit below entry wsp E: arg1 [E-2]=[F-0xC], arg2 [E-4]=[F-0xE],
arg3 [E-6]=[F-0x10] — RTBridge `arg_pointer(n) = read_wide(wsp-2n)`
numbering, cross-checked against the caller:

DEF?ON (type>0 branch) copies the recorded signal into its locals and
pushes, in order: `&local12(=type)`, `&local10(=code)`, `&localE(=key2)`
→ P?DEFON's **arg1→key2, arg2→code, arg3→type**.

P?DEFON's own O?SIGNAL pushes, in order: arg2-ptr, arg1-ptr, &local
→ O?SIGNAL's arg1=&local(new type), arg2=&key2, arg3=&code, i.e.
**O?SIGNAL(type=new_type, key2=key2, code=code)** — key2/code pass
through by reference; only the type is rewritten.

### 3.3 Semantics

Default handling of an UNHANDLED USER (positive-type) condition:

| Condition | Action |
|---|---|
| type == 2 | LCALL C?INIT(arg1) — no-op body — then normal return (a RESUME) |
| type == 6 | resignal O?SIGNAL(-1, key2, code) — escalate to system ERROR |
| else | resignal O?SIGNAL(6, key2, code) — recast as user condition 6 |

(PL/1 reading — hypothesis, not load-bearing: 2 ≈ FINISH-like cleanup
condition, 6 ≈ the user-visible ERROR condition; unknown user
conditions become condition 6, and unhandled condition 6 escalates to
the system ERROR type -1. The code follows the disassembly regardless.)

### 3.4 Instruction-semantics evidence (emulator source)

- `WADC s,d`: `ac[d] = add(machine, ~ac[s], ac[d])` (EagleCompute.cpp).
  For s==d==0: result = ac0 + ~ac0 = 0xFFFFFFFF = -1. `add`'s carry-out
  = ((dst&M)+(src&M))>>31 & 1 = 1; overflow contribution
  ((src^result) & ~(src^dst))>>31 = 0. So: **ac0=-1, c=1, ovr
  unchanged** — the c=1 matters for the WSAVS image O?SIGNAL lays
  (its saved-carry) on the type==6 path.
- `WSEQI/WSNEI d,imm`: signed compare of ac[d] against the
  SIGN-EXTENDED 16-bit immediate; no flag writes. Corollary recorded in
  REPORT §4: DEF?ON's `WSNEI 0,65535` at ef29 is a **type == -1** test
  (the disassembler prints the raw immediate; 0xFFFF sign-extends to
  -1), not a 65535 comparison.
- `NLDAI imm,d`: ac[d] = sign-extended immediate; no flags.
- `XPEF [..]` pushes the RESOLVED EA as a wide; with `@`, the EA
  resolution reads through the slot, so `XPEF @[argslot]` pushes the
  ARGUMENT POINTER (pass-by-reference pass-through).
- `WSAVS n` (EagleStack.cpp): overflow check `wsl>0 && wsp+10+2n>wsl` →
  handle_overflow; else five wide pushes (ac0,ac1,ac2,wfp,ac3|c<<31),
  ac3=wfp=wsp(=E+10), wsp+=2n, ovk=1.
- `LCALL` (EagleStack.cpp): pushes (psr<<16)|argc wide, ac3=return pc,
  **ovr=0**, shadow call_stack->call, then native-registry dispatch
  (with the central nested-span guard).

### 3.5 Footprint maps (both laid in emulated execution order)

type==2 (C?INIT) branch — normal return, wsp never moved natively
(native_return pops from CURRENT wsp — RTBridge.cpp — so all writes
are residue):

| Words | Content |
|---|---|
| [E+2..E+11] | own WSAVS image (entry ac0/ac1/ac2, wfp, ret\|c) — emulate_frame |
| [F+2..F+15] | locals — UNTOUCHED on this branch (no writes; WSAVS does not zero) |
| [E+26,27] | XPEF: arg1 pointer |
| [E+28,29] | LCALL frame wide: ((entry_psr\|0x8000)<<16)\|1 |
| [E+30..E+39] | C?INIT's WSAVS image: type, entry ac1, entry ac2, F, 0x7017FD87\|entry_c<<31 |

(ac0 in C?INIT's image is TYPE — the fd7c load is live in ac0 at the
LCALL; ac1/ac2/c untouched between entry and fd83.)

Resignal branch — hands off to native O?SIGNAL mid-flight:

| Words | Content |
|---|---|
| [E+2..E+11] | own WSAVS image |
| [E+22,23] (=[F+12]) | new_type (-1 or 6) |
| [E+26,27] | arg2 pointer |
| [E+28,29] | arg1 pointer |
| [E+30,31] | F+12 |
| [E+32,33] | LCALL frame wide: ((entry_psr\|0x8000)<<16)\|3 |

then machine state set exactly as the emulated LCALL leaves it for the
callee — wsp=E+32, wfp=F, ovk=1, ovr=0, ac0=new_type, ac1/ac2
untouched (entry values), ac3=0x7017FD9F, c = 1 (WADC path) or entry
carry (NLDAI path) — and the registered native O?SIGNAL is invoked
through the registry (`native_registry.lookup(0x7017EDED)(machine)`),
the ?DEFAULT_ERROR_HANDLER composition precedent (lib_error.cpp). It
lays its own footprint from wsp=E+32 and native_transfers to the
handler. O?SIGNAL's arg slots then read [E+30]=&new_type,
[E+28]=&key2, [E+26]=&code ✓ (§3.2).

Shadow call-stack skew: neither the inner LCALL's shadow push nor
C?INIT's is replicated — the signal_dispatch precedent (none of the
inner XCALL/LCALL images in o_signal.cpp push shadow frames). Debug
backtraces only; lockstep does not compare it.

### 3.6 Pre-decision gates (pure reads, before any store)

SharedProtocol's terminal-composition rule, in wrapper order:

1. `rt_pending_return != 0` → emulate without re-arming (nested-span
   guard). Today this covers EVERY live path: DEF?ON is terminal, the
   clone only reaches it inside a whole-chain fallback span.
2. argc != 3 → fallback (sole site passes 3; symmetric emulation).
3. Headroom: `wsl>0 && E+40 > wsl` → fallback. E+40 covers WSAVS's own
   trap point (E+24) and the deepest native write ([E+39], C?INIT
   image); marginal cases fall back into faithful emulation of the
   stack-fault vectoring rather than replicating handle_overflow
   (METHOD §13 — the tripwire stays load-bearing).
4. type != 2 only: sibling-fallback conditions pre-checked so the
   sibling's own fallbacks are unreachable mid-composition:
   o_signal_translated (translated_bits, not registry-lookup — the
   Project 2 correction), walker gate closed, and
   `rt::select_frames(new_type, *arg1)` found — the has-handler
   prediction with THIS resignal's type/key2, not the catch-all
   `rt::signal_has_handler` (which hardwires type=-1: correct for
   ?LIB_ERROR's raise, wrong for the new_type==6 resignal).

### 3.7 C?INIT

Not translated, not registered. Its no-op body is laid as residue
inside P?DEFON (§3.5); its other callers (init tables at fcf5..fd09,
LANG?*) are stub-dispatched as before. If C?INIT ever needs to go
native independently, it is a five-write emulate_frame wrapper.

## 4. Validation status

| Routine | Derived | Translated | Empirical | Lockstep |
|---|---|---|---|---|
| O?AREA | ✓ | ✓ registered | ✓ master ENTRY/RETURN pair, bit-level (§2) | dormant — unreachable natively until DEF?ON lift |
| P?DEFON | ✓ | ✓ registered | — (path cold: needs an unhandled POSITIVE-type condition; both current triggers record type -1) | dormant — same |

Regression evidence (this session, both runs 0 divergences):
1. CONVERSION trigger (M→n→abc): full native chain
   (?LIB_ERROR→O?SIGNAL(native)→I.GOTO(native) ret 7015FBAF), clean
   ESC shutdown with write-back. New wrappers untouched (correct: no
   DEF?ON on a handled signal).
2. QUEST_FAIL_OPEN=USER_DATA_FILE, L→P, continue: handled signal 1,
   then the unhandled second signal — ?LIB_ERROR(native-fallback:
   no-handler/terminal-bound) whole-chain fallback, terminal pair at
   DEF?ON, clone "Thread halt" at 1,172,198 instructions, master alone
   through ?FATAL (syscalls from 7017F05B) to the death dump.
   Registration of the two wrappers changed NOTHING in either run —
   the dormancy argument holds empirically.

The wrappers become live — and lockstep-validatable — exactly when the
DEF?ON lift moves the detach point deeper (Layering.md validation
hook). P?DEFON's native paths additionally need a synthetic
positive-type unhandled condition: that is the fault-injection
extension the parked M3 provocation criterion was waiting on. The two
parked questions remain one question; nothing here forecloses it.

## 5. R?SIGNAL / ?ERROR (0x7017EF54)

59 code words; the 226-word Layering extent includes the EF8E..F030
data block (the ?SNAP traceback strings). Entry: LCALL; sole static
site DEF?ON ef41 with argc=0; the ?ERROR alias is the same address —
the public raise entry, argc-polymorphic. R.SIGNAL (0x7017EF51,
WSAVS 0 + WBR into the walk): ZERO static callers anywhere — derived,
not implemented (the O.SIGNAL/R.SIGREC precedent).

### 5.1 Body

```
ef54 WSAVS 0x0000;             F=E+10, no locals
ef56 XNLDA 2,[ac3-9];          ac2 = word [E+1] = the frame wide's LOW
                               word = ARGC (narrow, sign-extended)
ef58 WSLE 2,2;  WBR ->ef60 if argc > 0
  -- argc <= 0: (code, type) from the signal area --
ef5a LDASB 2;                  ac2 = wsb
ef5b XWLDA 0,[ac2-0x3A];       ac0 = [area+0x6] = code
ef5d XWLDA 1,[ac2-0x3E];       ac1 = [area+0x2] = type
ef5f WBR ->ef69
  -- argc > 0: ?ERROR(code[,type]) --
ef60 XWLDA 0,@[ac3-0xC];       ac0 = *arg1 = code
ef62 NLDAI 65535,1;            ac1 = -1 (sign-extended default type)
ef64 WSBI 2,2;                 ac2 = argc - 2 (disassembler prints the
                               RESOLVED immediate: tinyImmediateRegister
                               renders nn+1, the actual subtrahend)
ef65 WSEQ 2,2;  WBR ->ef69 if argc != 2
ef67 XWLDA 1,@[ac3-0xE];       argc==2: ac1 = *arg2 = explicit type
  -- the walk --
ef69 WMOV 3,2;                 cursor = ac3 (starts at own frame F)
ef6a XWLDA 3,[ac2-0x2];        next = saved-wfp slot
ef6c WSNE 3,3;  (same-reg skip = compare vs 0)
ef6d WBR ->ef8d;               next == 0 -> ef8d WRTN (plain return)
ef6e WSGT 3,2;                 (signed) skip if next > cursor
ef6f WBR ->ef69;               descending -> loop (cursor = next)
  -- anomaly: saved-wfp jumps UPWARD --
ef70 XWLDA 3,[ac2+0x0];        ac3 = frame's ret|c wide
ef72 XWSTA 0,[ac2-0x4];        frame's saved-ac2 slot = code
ef74 XWSTA 3,[ac2-0x6];        saved-ac1 slot = original ret|c
ef76 XWSTA 1,[ac2-0x8];        saved-ac0 slot = type
ef78 WANDI 3,0x70000000;       segment base of the ret
ef7b XWLDA 3,[ac3+0x124];      ac3 = [0x70000124] — the restart vector
ef7d WSGT 3,3;  WBR ->ef89 if vector <= 0   -> LCALL ?FATAL (terminal)
ef7f XWLDA 3,[ac3+0x0];        ac3 = [vector] = restart pc
ef81 XWSTA 3,[ac2+0x0];        rewrite the frame's return pc
ef83 LDASB 3;
ef84 WSUB 0,0;
ef85 XWSTA 0,[ac3-0x40];       CLEAR the ON-chain head [wsb-0x40]
ef87 STAFP 2;                  wfp = the anomaly frame
ef88 WRTN;                     return THROUGH it -> restart pc, with
                               (type, ret|c, code) in restored ac0/1/2
```

### 5.2 The restart vector

`[0x70000124]` is 0 in the load image and installed at startup by
I.GINIT (ea8a: `XLEF 0,[0x7017EB63]; XWSTA 0,[0x124]`). The data pair
at 0x7017EB63 is `7017EE02 7017EC7B`: **word0 = R.SIGREC** — Project
1's "zero static callers" WSSVS entry has a DYNAMIC caller via this
vector (shared-doc correction, REPORT §4). Runtime value confirmed
live in the run3 capture DEST window: [70000124] = 7017EB63.
I.GOTO's error path references the same dispatch (frames.cpp note).
Neither anomaly outcome (restart or ?FATAL) has ever been observed.

### 5.3 Key semantics evidence

Same-register skips compare against ZERO (EagleCompute: `dst =
(XX!=YY) ? ac[YY] : 0`), all signed for WSLE/WSGT — so ef58 tests
argc<=0 and ef6e tests next>cursor signed. WSBI's disassembly operand
is the resolved subtrahend (Disassembler tinyImmediateRegister:
prints nn+1; EagleCompute subtracts XX+1 where XX=nn).

### 5.4 Translation

The plain-return path's entire footprint is the WSAVS image: the walk
and both (code,type) load paths are pure reads whose values do not
influence the walk, and every clobbered register/flag is restored by
WRTN. So the wrapper (runtime/r_signal.cpp) needs NO argc gating:
pure-walk first (cycle-guarded 1024, loud); anomaly -> fallback whole
(covers both the ?FATAL terminal and the unobserved restart,
symmetric, decided before the emulated body's first store at ef72);
plain -> emulate_frame + native_return.

### 5.5 Empirical confirmation (run3 master capture, seq=36)

QUEST_FAIL_OPEN death path, `QUEST_CAPTURE=7017EF54
QUEST_CAPTURE_DEST=70000124`. ENTRY: ac0=ac1=-1 (confirms the WSNEI
-1 reading of DEF?ON ef29 AND DEF?ON's ef27 path selection),
ac2=7000104C (the area), ac3=7017EF45, wsp=7000122A, argc=0 frame
wide. RETURN at pc=7017EF45: pure image
`FFFFFFFF|FFFFFFFF|7000104C|70001214|7017EF45(c=0)`, entry registers
restored, wsp=E-2 — the plain-return path, bit-for-bit the
translation's footprint. The game's own death traceback ends
"from fp=0" — the zero saved-wfp the walk terminates on. The death's
?FATAL came from DEF?ON ef4b, NOT R?SIGNAL ef89.

## 6. DEF?ON (0x7017EF05) — STAGED, NOT REGISTERED

42 words. Entered XCALL-style, argc 0, from O?SIGNAL's exhaustion
dispatch; entry ac2 = 0x7017EF05 (the handler-slot value — confirmed:
run2/run3 O?AREA ENTRY captures show ac2=7017EF05). Terminal_table
entry: registration is the DEF?ON-lift session's move, together with
the terminal-table change (def_on.hpp banner; REPORT §5).

### 6.1 Body

```
ef05 WSAVS 0x000A;             F=E+10, locals [F+2..F+21], wsp=E+30
ef07 LCALL O?AREA,0;           ac0 = area
ef0b XWSTA 0,[ac3+0xC];        local [F+12] = area
ef0d WMOV 0,2;  ef0e XWLDA 1,[ac2+0x2];   ac1 = type
ef10 WSGT 1,1;  WBR ->ef27 if type <= 0
  -- type > 0: copy (key2,code,type) to locals [F+14/16/18], push
     their EAs, LCALL P?DEFON,3 (ef22), WRTN (ef26) --
  -- type <= 0 --
ef27 XWLDA 0,[ac2+0x2];        ac0 = type
ef29 WSNEI 0,65535;            = type != -1 test (sign-extended!)
ef2b WBR ->ef41 if type == -1
  -- other type <= 0: WADC(-1,c=1); locals [F+14]=-1, [F+16]=key2,
     [F+18]=code; push; LCALL O?SIGNAL,3 (ef3d): resignal as ERROR --
ef41 LCALL R?SIGNAL,0;
ef45 XWLDA 2,[ac3+0xC];        ac2 = area (from the local)
ef47 XNLDA 0,[ac2+0x16];       ac0 = sign-extended narrow [area+0x16]
ef49 MOV.L# 0,0,SZC;           pure test: skip iff bit15 CLEAR
                               (NovaCompute: CC=0 folds current c into
                               bit16; L: local c = bit15; N=1 (#):
                               NEITHER register NOR machine.c written;
                               SZC skips on local c==0)
ef4a WRTN;                     bit15 SET -> RESUME
ef4b LCALL ?FATAL,0;           bit15 clear -> die (the observed path)
ef4f WRTN;
```

So `[area+0x16]` bit15 is the "resumable" flag; every observed run had
it clear.

### 6.2 Translation (runtime/def_on.cpp) — the composition pattern

- Inner LEAF calls are laid AS RESIDUE, never dispatched: a
  dispatched sibling's native_return would end the clone batch at a
  mid-DEF?ON pc the master never breaks at. O?AREA's patched image,
  R?SIGNAL's plain image, and (type==2) P?DEFON's C?INIT composite are
  all residue; the whole composite ends in ONE native_return at
  0x7017EE40. Footprint maps and the instant-AC values for each inner
  image are in the .cpp, laid in emulated execution order with the
  shadowing overwrites explicit.
- The TRANSFER-capable resignal (type>0, type!=2) dispatches native
  P?DEFON through the registry with the machine set to the exact
  post-ef22-LCALL state; P?DEFON's own gates are pre-run in DEF?ON's
  pure-read phase so the sibling cannot fall back mid-flight.
- Pure-read fallback-whole gates: argc!=0; headroom (E+78 covers the
  deepest composite write); type>0 resignal with gate open /
  untranslated sibling / no handler (select_frames(new_type,
  [area+4])); type==-1 with a walk anomaly or the resume flag CLEAR
  (the ?FATAL terminal — the ONLY observed shape, so today native
  DEF?ON always falls back on -1, preserving the terminal detach);
  and the never-observed type in [-5..-2] resignal shape.
- Native paths, when live post-lift: (a) type>0 through P?DEFON,
  (b) type==-1 + plain walk + resume flag SET — each ends at a
  boundary both engines share. Path (b) firing under lockstep is
  precisely the "DEF?ON RESUMES" event Layering.md's validation hook
  exists to witness.

### 6.3 Validation status (cluster, end of session)

| Routine | Derived | Translated | Registered | Empirical | Lockstep |
|---|---|---|---|---|---|
| O?AREA | ✓ | ✓ | ✓ | ✓ bit-level (run2) | dormant |
| P?DEFON | ✓ | ✓ | ✓ | — | dormant |
| R?SIGNAL | ✓ | ✓ | ✓ | ✓ plain path bit-level (run3) | dormant |
| DEF?ON | ✓ | ✓ | STAGED | entry convention + ef27-path + WSNEI(-1) confirmed (run2/3) | awaits the lift |

Regressions post-R?SIGNAL-registration (run4 CONVERSION, run5
fault-injection): 0 divergences each, rtcalls signatures unchanged,
clean detach — the dormancy argument holds for R?SIGNAL exactly as for
the satellites.
