# Hardware findings — Sep 5 2026 manual review

Planning session, user + integrator, reading the DG MV/Family
instruction pages the P26 report flagged (WMOVR, WHLV, WDIV, the Nova
and narrow 16-bit families) plus ECLIPSE 16-Bit Programming ch. 10.
Emulator = law for the IR (METHOD §5); a manual disagreement is an
emulator finding.  Seven fixes below; branch hw-findings-sep5; helper
self-test tests/helpers_selftest.cpp (red on the old code, 4.26M
disagreements; green on the fix); task 038 (two legs vs the 037
baseline).

## 1. Fixes (C++ here; the Java emulator carries the same seven, applied
##    by the user in the Tools tree)

| helper / op | manual says | emulator had | fix |
|---|---|---|---|
| WHLV (EagleCompute.cpp) | `ac/2`, **rounds toward 0** | `>>1` (floor: −3→−2) | `/2` |
| narrow_add, narrow_sub | OVR = 16-bit ALU overflow | tested **bit 16** of the sign-extended sum; the 16-bit sign is bit 15 — `0x7FFF+1` not flagged | `>>15` |
| narrow_add, narrow_sub | result **sign-extended to 32 bits** | raw 17-bit sum (wrong high half exactly when overflowing) | `(result<<16)>>16` |
| narrow_sub | CARRY = ALU carry of `dst + ~src + 1` | `(dst&0xFFFF)+((-src)&0xFFFF)` — 0 for src==0, hardware 1 | `+ (~src&0xFFFF) + 1` |
| narrow_mul | result sign-extended low 16 | **zero-filled** (`dst&0xFFFF`) — wrong for every negative product | `(dst<<16)>>16` |
| narrow_mul | OVR iff product ∉ [−32768, 32767] | bit-16 test (`0x8000` not flagged) | `>>15` |
| narrow_mul | OVR sticky like the rest | `ovr = 1` (assign) | `ovr \|= 1` |

Java-only (the Aug 28 C++ corrections never reached the Java): `add`
carry `>>>32` of the unsigned sum (was `>>31`), `sub` carry via
`~src+1` (was `>>31` of the difference — wrong for large input ranges,
not just an edge), `mul` overflow `>>31` (was `>>32`, misses
`0x80000000`).  The Java tree is the user's; after these the two
emulators should be re-diffed helper by helper — the C++ has been under
lockstep pressure, the Java has not.

## 2. Verified matches (no change)

- **WMOVR**: "shift right one bit, zero into bit 0" (DG bit 0 = MSB) =
  logical `>>1`, carry unchanged.  No rotate.  Emulator correct.
- **WDIV**: `acd/acs`; divisor 0 or out-of-range quotient (only
  INT_MIN/−1) → OVR=1, acd unchanged; else truncating signed divide.
  Emulator correct (P26 hoisted it verbatim into `EagleInstruction::div`).
  The page's Arguments paragraph has dividend/divisor swapped — a
  manual typo; the Function line and the example agree with the code.
- **Nova ALU no-load (`#`) forms**: "do not load result; restore
  initial CARRY" — NovaCompute.cpp:63 guards both writes.  Correct; P26
  lowers them as pure tests (ruling R3).
- **NADD carry/overflow formulas** otherwise as the manual (carry SET,
  not Nova-complemented).

## 3. Rulings from the manual (spec, not code)

- **16-bit result high half (ECLIPSE ops)** — ch. 10: an ECLIPSE
  instruction "alters bits 16–31 and leaves bits 0–15 **undefined**,
  unless otherwise noted."  So for MOV/ADD/SUB/COM/NEG/ADC/INC load
  forms the high half is a don't-care by spec.  The emulator zero-fills
  (NovaCompute.cpp:65); when the 67 deferred Nova load forms lower,
  match the emulator and mark the high half undefined in IR.md — not a
  contract.  The N-prefixed Eagle narrow ops (NADD/NSUB/NMUL) are 32-bit
  processor instructions with an explicit rule: sign-extended (§1).
- **Three-conventions finding (P26 REPORT §3) resolved**: Nova = undefined
  (zero-fill ok), narrow = sign-extend (fixed).

## 4. The OVR model — documented, deliberately NOT changed

Every manual page lists "Overflow: 0" for instructions that cannot
overflow (WMOV, WMOVR, WHLV, NLDAI …): hardware OVR is effectively
"the last instruction overflowed".  The emulator's OVR is **sticky**
(`|=`; cleared at WSAVS/WSSVS, RT boundaries, WRTN's PSR restore).

Why this is unobservable for Quest, and safe to leave:
1. Game frames are all WSAVS (OVK=1, OVR=0 at entry).  The `ovk &&
   ovr` check after every instruction traps AT the instruction that set
   OVR, so a stale 1 never survives to the next instruction.
2. RT code in WSAVR frames (OVK=0) may accumulate a stale OVR, but WRTN
   restores the caller's saved PSR (OVR=0 at call time).
3. No WLDPSR/WSTPSR anywhere in quest.dis; nothing reads the PSR.

Consequence for the parked flag-deadness analysis (MathDesign §5): use
the HARDWARE model — OVR is dead after every non-arithmetic instruction
— not the emulator's, or the analysis will call OVR live everywhere.

## 5. Risk of the fixes to the game

Lockstep cannot see any of this — IRExec calls the same helpers.  What
CAN change is game behaviour: (a) a negative NMUL product now carries
the correct high half into any wide consumer; (b) a 16-bit overflow the
emulator silently wrapped now sets OVR and, under OVK=1, TRAPS — which
is what the hardware did.  A new fault or a moved endpoint in task 038
vs 037 is therefore hardware truth surfacing, not a regression to
revert; investigate the site.

Also noted while here: `EagleInstruction::cvwn` already tests bit 15
(correct) — the same shape the narrow ops now use.

## 6. Also fixed today (Tools, user)

Follow.java tagged XJMP with a `pc+2` fall-through edge (shared code
path with XPSHJ).  Fixed; regenerated quest.tags / quest.blocks — 1,160
tag lines lose the edge, targets/blocks/synclist unchanged (every XJMP
sits behind a skip whose target is exactly pc+2, so the phantom edge
never created a block, only over-counted predecessors).  P27's
predecessor census now sees the clean graph.
