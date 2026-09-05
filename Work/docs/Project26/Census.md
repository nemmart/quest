# Project 26 — embed census + semantics table (census of record, Sep 5 2026)

> Plan-gate draft reviewed Sep 5; rulings R1–R10 taken (all recommendations
> accepted; R9 = ash/lsh only; bar ≤ 8,600) — see REPORT.md §2. Two
> corrections were recorded after the gate (METHOD §10/§11), marked
> **[CORRECTED]** in place below.

TREE VINTAGE: the Aug 29 integrated Work.tgz (Work__72_). quest.ir2.book
provenance verified against the re-uploaded Disassembled/: dis
1f9153c0…, argmap 39c42d4c…, blocks.split a5efa05f…, pushmap.M4 b8953659….
Census source: quest.ir2.book `@addr` lines (27,600). Semantics read from
c_src/hw (cited file:line) and diffed against DG_Quest/hw (Java).
Nothing in this doc is implemented; Parts 2–3 wait on the rulings in §4.

## 1. Census by bucket (27,600 embeds, every mnemonic accounted for)

| bucket | embeds | mnemonics |
|---|---|---|
| (a1) skips → §1 terminator + §2 comparisons | **6,460** | WSGT 2268, WSGTI 2027, WSEQI 615, WSNEI 415, WUGTI 279, WSEQ 202, WSLE 188, WSGE 185, WSNE 115, WSLEI 97, WSLT 46, WULEI 14, WUSGE 9 |
| (a4) effectful add/sub family | **3,883** | LWADD 1862, WNADI 826, WADC 646, WINC 207, WADI 177, XWADD 144, WNEG 16, XWSUB 2, XWADI 2, XWSBI 1 |
| (a3) word layer: bitwise | **388** | WCOM 201, WIOR 79, WAND 75, WXOR 18, WANDI 13, WIORI 2 |
| (a3) word layer: shifts | **125** | WLSI 103, WLSHI 2, WHLV 20 |
| (a7) t-places: 23 borrow brackets | **46** | WPSH ×23 + WPOP ×23 (the borrow pcs of quest.pushmap.M4; all 23 brackets sit inside one block each) |
| **(a) MathDesign as written** | **10,902** | |
| (b) Sep 5 additions | **4,544** | WMUL 2169, LDAFP 1778, CVWN 278, WMOVR 114, WDIV 98, LDASP 62, XWMUL 45 (LWMUL: 0) |
| (c1) XJMP → `goto` | 1,160 | XJMP (all 1,160 are `[pc+d] (0xFOLD)`, constant target) |
| (c2) Nova ALU family | 1,043 | MOV.L# 669, MOV.# 172, ADD.O# 81, COM.# 32, ADD 23, MOV.L 19, ADD.# 19, ADC 6, SUB.CL 4, ADD.S 4, ADD.O 4, ADC.C 4, SUB.ZR 2, MOV.R# 2, NEG.L# 1, MOV.S 1 |
| (c3) bit-in-memory ops | 624 | WSZB 361, WBTO 143, WBTZ 120 |
| (c4) narrow (16-bit Eagle) family | 619 | NADI 96, XNSUB 87, XNADD 83, NSBI 72, XNADI 69, NADDI 67, NSUB 29, NNEG 27, XNSBI 26, NMUL 22, NADD 21, LNADI 9, LNADD 5, LNSBI 3, LNSUB 2, XNMUL 1 |
| (c5) loop ops (need general t-places) | 207 | XNDO 176, XWDO 30, LNDO 1 |
| (c6) free one-liners | 40 | SEX 23, WXCH 7 (t-place), ANDI 4, ZEX 3, CRYTO 1, WSKBO 1, XNISZ 1 (ADDI: 0) |
| **(c) proposed, needs ruling** | **3,693** | |
| (d) DERR → P27 | 2,273 | |
| (d) RT-call decoration | 2,887 | XPEF 2126, LPEF 750, WPSH 11 (8 feed ?/rt calls; 3 are WPSH x,x + LDASP pass-by-reference temps at 70169B77/7C/7F) |
| (d) call machinery | 1,313 | LCALL 1146, LJSR 130, XCALL 37 |
| (d) string / WMSP / stack writes (item 4) | 1,765 | WCMV 1637, WMSP 57, WCMP 40, STASP 19, WBLM 12 |
| (d) frame / OS | 143 | WSAVS 130, SYSCALL 10, LDSP 2, WSAVR 1 |
| (d) float | 59 | FAS 10, FRDS 9, WFLAD 7, FMS 6, FRH 5, FEXP 5, FHLV 4, FDS 3, XFAMS 2, FMOV 2, WFFAD 2, XFLDS 1, FSS 1, FSGT 1, FCMP 1 |
| (d) misc multi-register / loop | 21 | DIVX 19 (two-register 16-bit divide, writes c), WDIVS 1 (64/32), WLOB 1 (loop) |
| **(d) out of scope** | **8,461** | |

Check: 10,902 + 4,544 + 3,693 + 8,461 = 27,600.

**Predicted post-landing embed count**: (a)+(b) alone → **12,154**
(−15,446). With every (c) item ruled in → **8,461**. With the
recommended (c) set (everything except the Nova load forms) →
**8,528** [CORRECTED from 8,504: 8,461 + 67], plus LNDO 1 (see §5:
rendering gap) = **8,529**, which is exactly the landed emission.
Landing bar (ruling R10): **≤ 8,600**.

Structural facts verified against quest.blocks.split: all 7,030
skip-class embeds are the LAST line of their block; every skip block has
exactly 2 successors listed ascending = [no-skip, skip] (6,822 shapes
checked, 0 exceptions) → `goto [fall, skip] test` with true=1=skip.
XNDO/XWDO/LNDO blocks list [fall, loop-target]. XJMP blocks list TWO
successors [target, next-pc] — the CFG tool treats XJMP as conditional;
the fall-through edge is spurious (XJMP never falls through). Recorded
as an observation, not acted on (lowering emits `goto target`; the
loader does not consult edges).

## 2. Semantics table (emulator source, file:line; Java diff noted)

Operand rendering: registerRegister prints XX,YY (source, dest);
tinyImmediateRegister prints imm(=nn+1),ac (WADI/WSBI/WLSI/NADI/NSBI
appear as `WADI 4,0`); immediates print `dec (0xHEX)` after the ac.
Each is re-verified against Decoder.cpp formats during implementation.

### 2a. Skips (all: EagleCompute.cpp; skip = block exit index 1)

| mnemonic | source | test (IR form) | notes |
|---|---|---|---|
| WSEQ x,y | :175 | `acX == acY`; if x==y: `acX == 0` | 32-bit; XX==YY compares against 0 (dst=0) |
| WSNE x,y | :180 | `acX != acY` / `acX != 0` | |
| WSLT x,y | :185 | `acX <s acY` / `acX <s 0` | signed (int32 casts) |
| WSLE x,y | :190 | `acX <=s acY` / `<=s 0` | signed |
| WSGT x,y | :195 | `acX >s acY` / `>s 0` | signed |
| WSGE x,y | :200 | `acX >=s acY` / `>=s 0` | signed |
| WUSGE x,y | :210 | `acX >=u acY` / `>=u 0` | unsigned (& 0xFFFFFFFF into int64) |
| WSEQI y,imm | :215 | `acY == sx16(imm)` | imm is a 16-bit word, sign-extended; emit the folded constant |
| WSNEI | :221 | `acY != sx16(imm)` | |
| WSLEI | :227 | `acY <=s sx16(imm)` | signed |
| WSGTI | :233 | `acY >s sx16(imm)` | signed |
| WUGTI y,imm32 | :362 | `acY >u imm32` | unsigned, 32-bit immediate |
| WULEI y,imm32 | :367 | `acY <=u imm32` | unsigned |
| WSZB x,y | :279 | `(lsh(M16[ind(acX) + lsh(acY, -4)], 0 - (15 - (acY & 15))) & 1) == 0`; x==y: base 0 | needs `ind()` (c3) — see §3 |
| WSKBO n | :252 | `(lsh(ac0, 0 - (31 - n)) & 1) == 1` | n from the opcode (bitPosition format) |
| XNISZ [e] | :447 | stmt `M16[e] = (M16[e] + 1) & 0xFFFF`, then `M16[e] == 0` | 1 embed; re-read is pure |

No skip writes a flag. Java (EagleCompute.java) identical for every row.

### 2b. Effectful family — each row names its shared helper

| mnemonic | source | helper | IR (statement root only) |
|---|---|---|---|
| LWADD y,[e] / XWADD | :467 / :461 | EagleInstruction::add | `acY = add(acY, M32[e])` |
| XWSUB y,[e] | :473 | sub | `acY = sub(acY, M32[e])` |
| WNADI y,imm | :316 | add | `acY = add(acY, sx16(imm))` (folded constant) |
| WADC x,y | :51 | add(~src, dst) | `acY = add(acY, ~acX)` — `~` is a pure arg |
| WINC x,y | :87 | add(1, src) | `acY = add(acX, 1)` |
| WADI k,y / WSBI k,y | :92 / :97 | add / sub | `acY = add(acY, k)` / `acY = sub(acY, k)` (k=1..4; WSBI already lowered as `#-`) |
| WNEG x,y | :29 | sub(src, 0) | `acY = sub(0, acX)` |
| XWADI k,[e] / XWSBI | :485 / :492 | add / sub | `M32[e] = add(M32[e], k)` / `sub` |
| WMUL x,y / XWMUL y,[e] | :57 / :499 | EagleInstruction::mul (EagleInstruction.cpp:48; `ovr |= 1` if the int64 product does not fit int32) | `acY = mul(acY, acX)` / `mul(acY, M32[e])` |
| WDIV x,y | :63–68 INLINE | **to be hoisted** → EagleInstruction::div | `acY = div(acY, acX)`; divisor 0 or (−1, 0x80000000) → `ovr = 1`, dst UNCHANGED, else int32 truncating quotient, no flag |
| CVWN y | :80–85 INLINE | **to be hoisted** → EagleInstruction::cvwn | `acY = cvwn(acY)`; result = sx16(low16); `ovr |= (y>>15 not in {0,−1})` |
| NADD/NSUB/NNEG/NADI/NSBI/NADDI/XNADD/XNSUB/LNADD/LNSUB | :121–155, :372–398 | narrow_add / narrow_sub (EagleInstruction.cpp:86/:97) | `acY = nadd(acY, x)` etc.; NNEG = `nsub(0, acX)`; results are SIGN-EXTENDED to 32 bits by the helper |
| XNADI/XNSBI/LNADI/LNSBI k,[e] | :412–438 | narrow_add/sub | `M16[e] = nadd(M16[e], k)` — the store truncates (§5 rule); NO `trunc16()` wrapper because effectful ops may not nest (ruling item R6) |
| NMUL / XNMUL | :136 / :400 | narrow_mul (:108) | `acY = nmul(acY, acX)`; NOTE the helper returns `dst & 0xFFFF` (ZERO-extended) and writes `ovr = 1` (assignment) — differs in shape from nadd/nsub (sign-extend, `|=`); byte-identical to Java (narrowMul); recorded as a same-source observation, not resolved |
| XNDO/XWDO/LNDO y,[e],disp | EagleGeneral.cpp:167/180/225 | narrow_add / add | `t1 = nadd(M16[e], 1); M16[e] = t1; t2 = t1 >s acII; acII = t1; goto [fall, target] t2` (XWDO: add/M32) — needs general t-places (c5) |

`c`/`ovr` also become assignable at root (MathDesign §4): CRYTO
(EagleGeneral.cpp:127) → `c = 1`.

### 2c. Pure (flag-free) forms

| mnemonic | source | IR |
|---|---|---|
| WCOM x,y | :25 | `acY = ~acX` |
| WAND/WIOR/WXOR x,y | :106/:111/:116 | `acY = acY & acX` / `\|` / `^` |
| WANDI/WIORI y,imm32 | :344/:350 | `acY = acY & 0x…` / `\|` |
| ANDI y,imm16 | :170 | `acY = acY & 0x0000…` (16-bit immediate, zero-extended as the emulator reads it) |
| WLSI k,y / WLSHI y,imm8 | :102 / :327 | `acY = lsh(acY, k)` / `lsh(acY, sx8(imm))` — logical_shift writes NO flag (EagleInstruction.cpp:74) |
| WMOVR y | :161 | `acY = lsh(acY, -1)` — emulator is a plain logical right shift, NO carry, NO rotate (Java `>>>1`, identical) |
| WHLV y | :157 | `acY = ash(acY, -1)` — emulator is `int32 >> 1` (floor). arithmetic_shift(−1) has a provably-zero ovr contribution (sign preserved), so root-effectful `ash` is byte-identical to the inline `>>1`; or pure per §6.2 ruling |
| SEX x,y / ZEX x,y | :75 / :70 | `acY = sx16(acX)` / `acY = zx16(acX)` |
| LDAFP a / LDASP a | EagleStack.cpp:527 / :512 | `acA = wfp` / `acA = wsp` (machine.wfp / machine.wsp, the clone's own registers — same value the instruction reads) |
| WXCH x,y | :33 | `t1 = acX; acX = acY; acY = t1` (t-place) |
| WBTZ x,y / WBTO x,y | :257 / :268 | `M16[ind(acX) + lsh(acY, -4)] = M16[…] & ~lsh(0x8000, 0 - (acY & 15))` / `\| lsh(…)`; x==y → base 0. Java identical (`>>>4`) |
| XJMP [pc+d] (0xT) | EagleGeneral.cpp:135 | `goto T` (1,160/1,160 constant, T a listed block start) |

### 2d. Nova ALU (NovaCompute.cpp:8–80) — decomposed by field

Fields: CC carry-in (0: c, 1: Z=0, 2: O=1, 3: C=complement), op on
16-bit (src|carry<<16), SS shift (L: rotate-left-through-carry, R:
right, S: swap), N=`#` no-load (ac AND c untouched, :63–66), skip by
(c, 16-bit result). With `#` set NOTHING is written — the instruction
is a pure test. Load forms write `ac[YY] = 16-bit result` with the
HIGH HALF ZEROED (:65) and `c`.

| shape | count | derived test (skip=1) | flags written |
|---|---|---|---|
| MOV.L# x,y,SNC | 509 | c = bit 15 of acX; **SNC = skip if c == 1** (NovaCompute.cpp:72 `if (c == 1)`), i.e. skip when the 16-bit value is negative | none |
| MOV.L# x,y,SZC | 160 | skip if c == 0 (:71) | none |

**[CORRECTED]** the draft had the SNC/SZC polarity inverted (SNC read as
"skip if no carry"). The emulator's KKK table (:68–77) is 2=SZC skip
when c==0, 3=SNC skip when c==1; the disassembler's skip_actions[] agrees.
lower.py derives every Nova test mechanically from that table (nova_test),
never from the hand-written rows here, so the emission was never wrong;
the prose was. Tests for `#` forms are emitted as a t-place holding the
17-bit ALU value plus the carry/zero predicates, exactly as
NovaCompute.cpp computes them.
| MOV.# x,y,SZR / SNR | 104 / 67 | `(acX & 0xFFFF) == 0` / `!= 0` | none |
| COM.# x,y,SZR / SNR | 26 / 6 | `(acX & 0xFFFF) == 0xFFFF` / `!=` | none |
| ADD.O# x,y,SBN | 50 | with s = (acX&0xFFFF)+(acY&0xFFFF)+0x10000: `((lsh(s,-16) & 1) == 1) && ((s & 0xFFFF) != 0)` | none |
| ADD.O# x,y,SEZ | 31 | `((lsh(s,-16) & 1) == 0) \|\| ((s & 0xFFFF) == 0)` | none |
| ADD.# x,y,SEZ | 19 | s = (acX&0xFFFF)+(acY&0xFFFF): same SEZ test | none |
| MOV.# x,y,SNC | 1 | skip if c == 1 — reads the machine carry (c readable) | none |
| MOV.R# x,y,SNC | 2 | c = bit 0 of acX: skip if `(acX & 1) == 1` | none |
| NEG.L# x,y,SNC | 1 | s = ((acX^0xFFFF)+1): skip if `(lsh(s,-15) & 1) == 1` | none |
| **no-load tests total** | **1,000** | pure compound tests; every one sits at block end | |
| ADD x,y; MOV.L x,y; SUB.CL; ADD.S; ADD.O SBN; ADC SKP; ADC.C SNC; SUB.ZR SNC; MOV.S | **67** | LOAD forms: write acY (high half zeroed) AND c | c |

**[CORRECTED]** the plan-gate draft summed the load forms as 43; the
true total is 67 (23+19+6+4+4+4+4+2+1). A §10-class slip caught when the
first emission came out 8,529 against a predicted 8,504. Predictions
below are restated with 67.

The 67 load forms are two-destination effectful ops (acY and c). They
do not fit `acY = op(...)` without either a hoisted `nova(...)` helper
that also writes c, or two statements (c first from old values, then
acY) — the latter is a formula in the IR. Recommend DEFER the 43.

Manual check owed (not resolved here): the emulator ZEROES the high 16
bits on Nova loads (:65) but SIGN-EXTENDS narrow_add/sub results and
ZERO-extends narrow_mul. Three conventions for "16-bit result into a
32-bit AC" in one emulator — a place the DG manual should adjudicate
before any of the load forms are lowered. Lockstep cannot see it
(METHOD §2). Both Java and C++ agree with each other on all three.

### 2e. Java-vs-C++ shift-helper diff (MathDesign §3 demand)

EagleInstruction.java:35–64 vs EagleInstruction.cpp:56–84 —
structurally identical, line for line: ash: amount>0 → `<<` if <32
else 0; amount<0 → `>>` if >−32 else `>>31`; `ovr |= (result^src)>>>31`
(C++: `uint32_t(...) >> 31`). lsh: `>>>` ↔ `uint32_t >>`; |amount|>=32
→ 0; 0 → passthrough. Only residual: C++ `int32 << n` on a negative src
is UB before C++20; g++ implements wrap, matching Java. NO FINDING.
mul, WDIV, CVWN, WMOVR, WHLV, ADDI also diffed: identical.

## 3. Grammar additions needed beyond MathDesign (for the spec draft)

- `ind(e)` primary: Machine::eagle_resolve_indirect(e) — follow bit 31
  from the VALUE e (no forced first deref; R[e] forces one). Needed
  only by (c3).
- `c`, `ovr` as readable primaries (MathDesign already makes them
  assignable). Needed by 1 Nova test, and natural for future carry
  consumers; assignment `c = 1` covers CRYTO.
- `wfp wsp wsb wsl` readable primaries; assignable at root in the
  grammar but P26 emits READS ONLY (prompt ruling).
- t-places `tN`: block-local, definite assignment checked by the loader
  (straight-line block → trivial), REFUSE read-before-write. Proposed:
  single assignment per block (refuse reassignment) so audits read
  top-down. May be the destination of an effectful op.
- `goto [L0, …, Lk] e` alongside plain `goto L` (kept as sugar for
  `goto [L] 0` — 3,243 existing emissions; ruling R1).

## 4. Rulings requested (plan gate)

R1. Keep plain `goto L`? (recommend YES — sugar for `goto [L] 0`.)
R2. (c1) XJMP → `goto`: 1,160 embeds, zero grammar. Recommend IN.
R3. (c2) Nova: the 1,000 no-load tests as pure compound tests per §2d
    (derived from NovaCompute.cpp, table in the doc) — recommend IN;
    the 43 load forms — recommend DEFER pending the manual check on the
    high-half convention (or rule a hoisted `nova()` helper family).
R4. (c3) bit-in-memory ops via `ind(e)` + dynamic lsh: 624 embeds, one
    new primary that calls an existing helper. Recommend IN.
R5. (c4) narrow family `nadd/nsub/nmul` (helpers exist): 619. Recommend
    IN. Sub-ruling R6 on the RMW store form.
R6. Effectful op storing to M16: `M16[e] = nadd(M16[e], k)` with no
    `trunc16()` wrapper (the store truncates per §5; wrapping would nest
    the effectful op). Recommend accept; spec says so explicitly.
R7. (c5) general-purpose t-places (beyond borrow brackets) for
    XNDO/XWDO/LNDO (207) and WXCH (7). Recommend IN — same machinery.
R8. (c6) SEX/ZEX/ANDI/CRYTO/WSKBO/XNISZ: recommend IN (40, all free).
R9. MathDesign §6.2 — pure-tier shift spelling. Options: (i) `ash/lsh`
    only, at every tier (integrator lean; ISA amount semantics, no C UB
    corners); (ii) C `<<`/`>>` with an s/u suffix on `>>`. Present, not
    decided. Note WLSI/WLSHI/WMOVR are flag-free in the emulator, so
    `lsh` as a pure primary is needed either way; `ash` appears only as
    WHLV (20) — root `ash()` with its zero ovr contribution, or pure
    `ash` under (i).
R10. Landing-bar embed target: ≤ 8,600 (recommended set) or ≤ 12,200
    ((a)+(b) only).

## 5. Design notes / observations (report, not resolved)

- LNDO (1 embed, 7015C0C7): the dis renders wideIndirectArgument as
  `LNDO [ea],arg` — the format is shared with LCALL and OMITS the II
  register field (Disassembler.java:165; the X-form XNDO prints it).
  The register cannot be read from the listing, so LNDO stays embedded
  (§14 flag; not worked around).
- XJMP blocks carry a spurious fall-through successor in
  quest.blocks.split (e.g. 7015C0B1 `XJMP … (0x7015C1A4)` lists
  `n 7015C1A4 7015C0B3`): the CFG tool treats XJMP as conditional.
  Recorded per the user's ruling (disassembler/CFG fix is the user's);
  lower.py emits `goto [target] 0` and only checks that the target is
  among the successors.
- Manual checks owed by the user (manual not in the upload): WMOVR
  (emulator: logical >>1, no carry, no rotate), WHLV (emulator: floor
  `>>1`; if the manual rounds toward zero that is an emulator finding),
  WDIV (emulator: ovr=1 + dst unchanged on 0 / INT_MIN÷−1), Nova/narrow
  high-half conventions (§2d).
- t-places for borrow brackets drop the bracket's memory write
  entirely. Book mode: the P20 slot (74000000+) is never read except by
  its own WPOP — invisible. Stock mode: the real-stack word below wsp is
  no longer written; wsp is restored inside the block so every
  rendezvous agrees; only dead-stack residue differs (METHOD §5 notes
  code that reads such residue exists — none is known to read below a
  borrow). Flagged; MathDesign §7 ruling stands.
- Pure `/s /u %s %u` have ZERO emission sites in this census (WDIV is
  effectful `div`); they land as grammar + executor faults only.
  Executor fault list: goto index ∉ [0,count); zero divisor in
  `/s /u %s %u`; INT_MIN `/s` −1 (host UB otherwise — must fault, not
  wrap, unless ruled otherwise); non-0/1 operand to `&& || !`; t-place
  read-before-write (loader).
- Refuse-at-parse list: bare `< <= > >=`; bare `/ %`; `#+ #- #* #/`;
  effectful op anywhere but statement root; `!` on a non-boolean
  expression is a runtime fault (value check), not a parse refusal;
  `and()/or()/xor()/com()` functional spellings; `<<`/`>>` if R9=(i).

## 6. Liveness plan (stated before any run)

Blocks adjacent to the P25-proven startup block 7015C2B2 (INIT_OBJ_TBL):
**7015C2A4** (WSGT skip → terminator), **7015C2A6** (WMUL ×2, WNADI,
WBTZ), **7015C2BB** (MOV.L# SNC) — predicted LIVE in k1fo. P25-live WPSH
blocks 7015C4FB / 701670D7 / 70171164 / 70172642 each carry LWADD ×2 +
WMUL — predicted LIVE (k1fo/k1play as in P25). Class-level prediction:
every (a1)/(a4)/(b)/(c1)/MOV.L# class executes in k1fo (they are in
essentially every block; expect thousands of first-execution blocks
containing them). Census-carried (expected unreachable by scripted
drivers, ByteEA §5 precedent): WUSGE 9, WULEI 14 (SQR31?3 float path),
WIORI 2, WLSHI 2, XWSUB/XWADI/XWSBI 5, WXCH 7, CRYTO 1, WSKBO 1, XNISZ
1, NEG.L#/MOV.S/SUB.ZR (float-adjacent SQR31 blocks), LNDO 1, the
UPDATE_SCREENS borrow blocks 7015F795/7016A3C7. Verification command:
IRExec first-execution lines ∩ blocks containing each newly lowered
mnemonic, per class, appended to the battery verdicts.

## 7. Implementation order (unchanged from PROMPT, each behind K=1)

(0) hoist WDIV/CVWN bodies into EagleInstruction::div/cvwn — stock K=1
gate, byte-identical; (i) `#+`/`#-` → add/sub respelling (0 census
change); (ii) terminator + comparisons + XJMP goto; (iii) word layer +
effectful family + narrow family; (iv) mul/div/stack reads/WMOVR/CVWN;
(v) t-places: 23 brackets, WXCH, loop ops; (vi) Nova tests; (vii)
bit-in-memory via ind(). Regenerate .book/.stock; synclist UNCHANGED.
