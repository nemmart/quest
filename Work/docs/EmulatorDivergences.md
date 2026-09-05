# Emulator ↔ Hardware divergence register

One place for every known way the emulator (C++ `c_src/hw`, and the Java
twin in Tools/) differs from the DG MV/8000 as documented in the manual
or inferred from it. Lockstep cannot see any of these — both engines
share the same helpers — so this list is the only defence. Rules:

- **Emulator = law for the IR** (METHOD §5). The IR replicates the
  emulator's value; an entry here is never a reason to change the
  lowering.
- An entry is either FIXED (with the commit), OPEN-BENIGN (argued
  unobservable in Quest, with the argument), or OPEN-GUARD (the emulator
  is deliberately louder than the hardware). Nothing is "probably fine".
- When a game path is found that makes an OPEN-BENIGN entry observable,
  it becomes a bug: fix BOTH engines, add a self-test row, re-argue.
- Add here whenever a manual page is read against the source; cite the
  project that found it.

Sources consolidated: HWFindings_Sep5.md (planning session manual
review), Project26 REPORT, Project28 REPORT §7 addendum,
Project29/Census.md §10 (string family manual review),
CHANGE_FLOAT_SHADOW.md.

## 1. FIXED

| id | instruction / helper | hardware | emulator had | status |
|---|---|---|---|---|
| F-1 | WHLV | `ac/2` rounds toward 0 | `>>1` (floor) | fixed Sep 5, C++ 177acd2 + Java; self-test |
| F-2 | NADD/NSUB overflow | OVR = 16-bit ALU overflow (sign bit 15) | tested bit 16 (missed `0x7FFF+1`) | fixed Sep 5; self-test |
| F-3 | NADD/NSUB result | 32-bit sign-extended 16-bit result | raw 17-bit sum (wrong high half on overflow) | fixed Sep 5 |
| F-4 | NSUB carry | ALU carry of `dst + ~src + 1` | `(-src)&0xFFFF` shortcut (0 for src==0) | fixed Sep 5 |
| F-5 | NMUL result | sign-extended low 16 | zero-filled | fixed Sep 5 |
| F-6 | NMUL overflow | bit-15 range test; OVR sticky (`\|=`) | bit-16 test; `ovr = 1` assign | fixed Sep 5 |
| F-7 | WADD/WSUB carry (C++) | ALU carry of the 32-bit op | `>>31` of the sum/difference | fixed Aug 28 (C++); Java lagged until Sep 5 |
| F-8 | WMUL overflow (Java) | product fits 32 signed ⇔ `>>31` ∈ {0,−1} | `>>32` | fixed Sep 5 (Java only; C++ was right) |
| F-9 | Follow.java XJMP | no fall-through after an unconditional jump | tagged `pc+2` as a successor | fixed Sep 5 (Tools; targets/blocks unchanged) |

Java parity: the Java emulator missed the Aug 28 round entirely (F-7,
F-8). After Sep 5 the two are believed helper-identical for the
integer family; a full side-by-side of EagleInstruction.{java,cpp} has
NOT been done and is owed.

## 2. OPEN-BENIGN — argued unobservable in Quest

| id | instruction(s) | hardware | emulator | why benign | found |
|---|---|---|---|---|---|
| B-1 | **OVR model, systemic**: WMOV, WMOVR, WHLV, NLDAI, WCMV, WCMP, WBLM, WMSP, STASP, STAFP … every page says "Overflow: 0" | OVR ≈ "the last instruction overflowed"; non-arithmetic ops clear it | `ovr` is STICKY (`\|=`), cleared only at WSAVS/WSSVS entry, RT boundaries, WRTN's PSR restore (EagleStack.cpp:128/204/274/466) | Game frames are WSAVS (OVK=1, OVR=0 at entry); the `ovk && ovr` check after every instruction traps AT the setting instruction, so a stale 1 never survives to the next one. OVK=0 runtime frames can accumulate a stale OVR but WRTN restores the caller's saved PSR. No WLDPSR/WSTPSR in quest.dis. **Consequence**: any flag-deadness analysis (MathDesign §5) must use the hardware model, not the emulator's. | HWFindings §4; P29 Census §10 W1/P3/B2/M2/S2 |
| B-2 | Nova/ECLIPSE 16-bit ALU load forms (MOV/ADD/SUB/COM/NEG/ADC/INC, SS≠1) | bits 0–15 of acd **undefined** after a 16-bit op (ECLIPSE ch. 10) | zero-fills the high half (NovaCompute.cpp:65) | undefined by spec; any legal implementation | HWFindings §3 |
| B-3 | Nova SS=1 (rotate-left through carry) load forms | 16-bit result, high half undefined | stores a 17-bit value (bit 16 = old bit 15) — NovaCompute.cpp:50–51,65 | no L form in Quest is followed by a result-test skip; every loaded register (`MOV.L 2,1` ×19, `SUB.CL` ×4 → SEX) is dead in its high half | P28 REPORT §7 / HWFindings §7 |
| B-4 | WCMP after a mismatch | AC2/AC3 = address OF the failing byte; AC0 = bytes left including it | pointers advanced before the compare → one past; AC0 one less (EagleSpecial.cpp:90–101) | all 40 sites do `LDAFP 3; WSEQ/WSNE 1,1` — only ac1==0 is ever read; ac0 reloaded before use | P29 Census §10 P1/P2 |
| B-5 | `?RANDOM_NUMBER` (LCG in floating point: WFLAD/LFMMD/LFAMD/D.MOD) | 56-bit mantissa | C++ `double`, 53-bit | self-consistent: the game's random sequence differs from a real MV/8000 but identically on both engines. "Bit-exact" means bit-exact with the emulator. | CHANGE_FLOAT_SHADOW.md |
| B-6 | NADD family high half on OVERFLOW (pre-F-3) | — | — | superseded by F-3; listed so the argument (OVK=1 traps at the op) is not lost | HWFindings §1 |
| B-7 | WCMV / WCMP / WBLM / WMSP interruptibility | interruptible; PC decremented to restart from the updated ACs | atomic | single-task emulation; no interrupt can arrive mid-instruction | P29 Census §10 W4/P5/B4 |

## 3. OPEN-GUARD — emulator deliberately louder (or softer) than hardware

| id | instruction | hardware | emulator | note | found |
|---|---|---|---|---|---|
| G-1 | WMSP limit check | faults ONLY on fixed-point overflow of 2·ac (stack not modified, return block pushed, handler); no WSL/WSB comparison — limits belong to the push-class instructions | throws on `wsp > wsl` / `wsp < wsb` after the add (EagleStack.cpp:570–573); no overflow-of-shift test | inert in Quest (57 claims, all positive and small). Keep as a loud guard; do NOT cite as hardware semantics (StringsDesign question ii). | P29 Census §10 M1 |
| G-2 | WBLM with bit 31 set in AC2/AC3 | forces the indirect bit to 0, proceeds | throws "WBLM instruction with indirection!" | no site sets it | P29 §10 B1 |
| G-3 | WCMV / WBLM / WCMP crossing a segment | protection/ring semantics; a string may span segments | throws "crossing segments not allowed" mid-copy | no site crosses | P29 §10 W3/B3 |
| G-4 | WCMV/WCMP with count 0 and an invalid pointer | protection fault MAY occur (WCMV) / result code 4 in AC1 (WCMP) | no check when nothing is moved; throws on segment change otherwise | no site has a 0 count with a computed pointer | P29 §10 W2/P4 |
| G-5 | WMSP ruling-8 `zero_claim`; STASP zero-on-raise | (no hardware equivalent) | zeroes newly exposed stack words when `zero_claims` is on | instrumentation convention (P26 ruling 8), not semantics | P29 §10 M3 |
| G-6 | WDIV divisor 0 / INT_MIN÷−1 | OVR=1, acd unchanged, no trap unless OVK | same, via `EagleInstruction::div` | agrees — listed because the IR's pure `/s /u %s %u` additionally FAULT loud on a zero divisor (grammar ruling), which is an IR choice, not hardware | P26 |

## 4. Verified matches worth remembering (so they are not re-investigated)

WMOVR (logical `>>1`, no rotate); WDIV (dst untouched on fault; manual
Arguments paragraph has dividend/divisor swapped — a manual typo);
Nova no-load `#` forms (no ac, no carry write); NADD carry is SET (not
Nova-complemented); WCMV pad/direction/carry/residues; WCMP equality and
blank padding; WBLM sequential word order (the 6 overlapping fills
depend on it and the manual's restartability guarantees it); WMSP
`wsp += 2·ac`; STASP/STAFP stores; LLEFB displacement is a BYTE
displacement (manual example `LLEFB 2,DEST*2`).

## 5. Owed

- Full EagleInstruction.java ↔ .cpp helper diff (post Sep 5).
- Survey of OVR readers to close B-1 formally (expected: none).
- Manual pages not yet read against the source: the float family
  (FAS/FRDS/WFLAD/FMS/FRH/FEXP/FHLV/FDS/…), DIVX/WDIVS/WLOB, WSTB,
  XLEF/LLEF family, the skip family beyond P26's census.
