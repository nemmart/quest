# CODEGEN_RULES — the DG PL/I code generator as compiler/translate.py models it

Project 35, Sep 8 2026.  Tree vintage: main, commit 35859fc.

Every rule below is a **declared belief** about the Data General PL/I
compiler that produced Quest, written down so that compiler/ircmp.py can
falsify it (ruling R3: a register rule is falsified by `DIFF(reg)`, never
refused).  Each rule names the instruction pair in emulation/quest.ir2.book
that motivated it and a confidence:

- **A** — needed by two or more routines, no counter-example among the three
  matched routines (PICK_X_Y, UPDATE_SCREENS, REFRESH_SCREEN: 197 statements);
- **B** — needed by one routine, consistent with the others;
- **C** — fits one instance; the alternative readings are listed.

The rule numbers are the ones translate.py's comments cite.  "Statement"
means one PL/I statement; the C source is written one C statement per PL/I
statement, and a `for` header / `if` header is a statement of its own.

## 1. Frame layout (the hypothesis of PLAN.md §3, as requested by the user)

| # | rule | evidence | conf |
|---|------|----------|------|
| R1 | Declared locals take frame slots 2, 4, 6, … in declaration order. | PICK_X_Y `r` → `wp(ac3, 2)`; UPDATE_SCREENS `i`, `n` → 2, 4; REFRESH_SCREEN `i` → 2 | A |
| R2 | A 16-bit local takes a whole (2-word) slot like a 32-bit one. | UPDATE_SCREENS `i` (XNSTA at +2), `n` at +4 | A |
| R3 | Scalar temporaries (dummy arguments, CSE temps, partial sums) take the **lowest free even slot** above the locals; a temp is free from its last use on, including within the statement that last uses it. | PICK_X_Y: TMP(1)→4, TMP(n)→6; then `r*9`→4 (TMP dead); 70176241 reloads slot 4 and the same statement's TMPs take 4 and 6 (the CSE temp died at that reload). UPDATE_SCREENS 7017D68A: the partial sum takes slot 6 the moment `i*686` was consumed. | A |
| R3b | **String temporaries are a separate pool**: a string temp reuses a *dead string temp* of sufficient size, otherwise it is placed at the frame's high-water mark (a bump allocation); scalar temps never take words that ever belonged to a string temp. | REFRESH_SCREEN: varying(22) dummy → 6..17; scalar TMPs 4, 18, 20; loop body: temp1 (23 bytes) reuses 6..17; temp2 (24 bytes) → 22..33 (bump past 18, 20); the varying(24) dummy → 34..46 (13 words: temp1's 12 do not fit); the body's scalar TMPs → 4 and 18 (never 6). All nine slots identical. | B |
| R4 | Field displacements are the raw K of the census (`wp(ac2, 11495)`, `wp(ac2, -589)`); the 1-based origin is already folded into K, so `T[i].f` is `base + i*stride + K` with no origin arithmetic. | every element reference in the three routines | A |

## 2. Registers

| # | rule | evidence | conf |
|---|------|----------|------|
| R5 | A base pointer used for indexing is loaded into **ac2** (`LWLDA 2`); a scaled subscript reloaded from a temp goes into ac2 (`XWLDA 2`) and the base is added there (`LWADD 2`). When the subscript is already in a value register the address is computed **in place** and copied to ac2 with WMOV. | PICK_X_Y 701761E9 (`ac2 = M32[0x70000212]`), 70176217 (`ac2 = M32[slot4]; ac2 = add(ac2, base)`), 70176208 (`ac0 = mul(ac0, ac1); ac0 = add(ac0, base); ac2 = ac0`) | A |
| R6 | The element address in ac2 is **protected** (cost 3) while the statement has further references to the element; after the last one it is an ordinary address (cost 0) and a field load may land in ac2. **R6′**: it is *not* protected when a temp (R10) still holds it. | 70176208 `ac2 = sx16(M16[wp(ac2, 11495)])` (single reference: load into ac2) vs 70176226 (two references: loads go to ac0, ac1). UPDATE_SCREENS 7017D67A: `*x` takes ac2 although a reference is pending — slot 8 holds the address (R6′). | A / B |
| R7 | **Destination register = lowest protection cost**, ties to the lowest-numbered register. Costs: unknown / cached variable copy / address 0; duplicate of another register 1; constant loaded in this statement 2; value still needed in this statement 3. | 701761E9: `1`→ac0, `region_count`→ac1 (ac0 const); 70176208: field → ac2 (dup) over ac1 (const 9); 70176217: `100`→ac1 (ac0 live); 70176226/241: two TMP values → ac0, ac1; REFRESH_SCREEN 70176ADF: `10`→ac1 (ac0 = const 0 from WCMV), `2`→ac2, `0x800`→ac0 (all constants: lowest number) | A |
| R7a | A constant is protected only within the statement that loaded it; at the next statement it is still *known* (reusable) but cost 0. | 70176217 `ac1 = 100` overwrites the `9` loaded two statements earlier; 70176ABA/ADF reuse the WCMV residue `ac0 = 0` for `TMP(0)` | A |
| R7b | A register holding a **variable that the current statement will read again** is protected from the statement's start until that read. | UPDATE_SCREENS 7017D67A: `*y` stays in ac1 (loaded in the previous statement, read again for the column) so `*x` goes to ac2 | B |
| R8 | Register knowledge **resets at a C label** (a join). **R8a**: it persists past `if (c) S` when S is one word that writes no register (a WBR/XJMP/WRTN). **R8b**: "live" marks expire at the statement boundary (the value becomes a cached copy). | PICK_X_Y 70176262→6A (`ac0 = *x` reused after the skip), 701761E9 (everything reloaded after `retry:`) | A |
| R11 | Multiplication / division by a constant loads the constant into a register (NLDAI) chosen by R7 and multiplies in place (WMUL/WDIV). | `ac1 = 9; ac0 = mul(ac0, ac1)`; `ac0 = 686; ac1 = mul(ac1, ac0)` | A |
| R21b | The DO-loop register (R21) stays protected through the body's **first statement**. | REFRESH_SCREEN 70176B10: `ac1 = 0x7C` (ac0 = i); UPDATE_SCREENS 7017D64E: `ac0 = 686` (ac1 = i) | B |

## 3. Common subexpressions

| # | rule | evidence | conf |
|---|------|----------|------|
| R9 | The scaled subscript `i*stride` is **saved to a temp** at its first computation when a later statement references `T[i]` again (and i is not reassigned in between); later statements reload it (XWLDA 2) and add the base (LWADD). | PICK_X_Y slot 4 (`r*9`, saved 70176208, reloaded 70176217 and 70176241); UPDATE_SCREENS slot 6 (`i*686`, saved 7017D665, reloaded 7017D67A) | A |
| R9a | No CSE temps are created when the subscript value was the **DO-loop register** (the first body statement of a loop). | UPDATE_SCREENS 7017D64E computes `i*686` from ac1 and saves nothing, though two later statements reference `PLAYER[i]` | B |
| R10 | The **element address** is saved to a temp at the first reference (emitted after the statement's expression is evaluated, before its test/store) when at least two later references exist; it is **consumed by the second later reference** (the first later reference recomputes from the R9 temp); the temp dies at consumption. | PICK_X_Y: saved 70176208 (slot 6), 70176217 recomputes, 70176226 loads slot 6; UPDATE_SCREENS: saved 7017D675 (slot 8, after the ABS diamond), 7017D67A recomputes, 7017D68A loads slot 8 | **C** — fits both instances exactly; alternatives considered: "consumed by the next statement with ≥2 references" (fits PICK_X_Y only), "consumed whenever ac2 was clobbered" (fits UPDATE_SCREENS only). P36 should test it on a routine with three or more later references. |

## 4. Statement shapes

| # | rule | evidence | conf |
|---|------|----------|------|
| R12 | A two-arity routine's `int arg_count` is the frame marker word, read as `sx16(M16[wp(ac3, -9)])` and tested with the Nova SZR/SNR form (R14). | REFRESH_SCREEN 70176A93 | B |
| R13 | `if (c) S` with a one-word S (`goto`, `return`, `continue`, `x = -x`) is **skip-if-NOT-c over S**: `goto [S, cont] (!c)`. | PICK_X_Y 70176208 (`(t1 & 0xFFFF) != 0` over the WBR), 70176272 (`ac0 >s 16350` over WRTN); UPDATE_SCREENS 7017D65C | A |
| R13b | `if (c) {body} [else {alt}]` with a longer body is **skip-if-c over a `goto else`** block; the body ends with `goto after` when an else part follows; an else body's call continuation *is* the after block. | REFRESH_SCREEN 70176A93..ABA | B |
| R14 | A **16-bit value against 0** uses the Nova `MOV# r,r,SZR/SNR` form: `t1 = ((acN & 0xFFFF) \| lsh(c, 16)); goto … ((t1 & 0xFFFF) ==/!= 0)`. **R14b**: a 16-bit sign test (`< 0` / `>= 0`) is `MOV.L# r,r,SNC`: `((lsh(t1, -15) & 1) == 1)`. A 32-bit value against 0 is a wide skip spelled `0` (`(ac2 >=s 0)`). | 70176208; 70176A93; 70176AA8; 7017D658 | A |
| R15 | A comparison with a constant is a wide skip-with-immediate, the constant spelled `hexc` (`(ac0 >s 0x00003BF5)`). | PICK_X_Y 70176262..72 | A |
| R16 | `cvwn` is emitted when a 32-bit result is narrowed: after a division of a 16-bit operand, and when a 32-bit value is stored to a 16-bit target; a 16-bit value stored to a 16-bit target is `trunc16` only. | 70176217 (`div; cvwn; add 1`), 70176241 (`cvwn; M16[R[ac3 + -12]] = trunc16`), 7017D635 (`n` stored without cvwn) | A |
| R17 | `SUB(i, n)` is the P27 DERR fold: `assert(!(r >s hexc(n)) && (r >s 0), "DERR 17 @pc"); goto [K] 0` in the guard block; `>s` when n ≤ 0x7FFF, `>u` otherwise. | 70176201 (`>u 0x186A0`), 7017D64A (`>s 0xA`) | A |
| R18 | TMP (dummy) arguments are evaluated left to right and **stored immediately**, unless a TMP's slot reuses a temp this statement itself consumed — then all not-yet-stored TMPs are stored (in argument order) after the last TMP is evaluated. Address arguments are then computed in push order (right to left). | 701761E9 (immediate) vs 70176226/70176241 (deferred: slots 6 / 4 had just been consumed) | B |
| R19 | An unconditional `goto L` whose WBR displacement would not fit (\|d\| > 127 words, estimated) branches to a per-label **long-jump stub** placed at the routine's end; the routine's own final `goto L` *is* that stub. | PICK_X_Y 70176269/6C/71 → 70176275 (XJMP), while 70176216/25 reach `retry` directly (−45, −60 words) | B |
| R20 | **Operand order**: the more complex operand of a binary operator is evaluated first (a field reference counts 2, an arithmetic node 1 + its operands, a variable or constant 0); a 32-bit memory operand goes straight into XWADD/LWADD, a 16-bit one is loaded first. | 7017D64E (`P.fm589 - *x`: field first) vs 7017D67A (`*x - (P.fm589 - 5)`: parenthesis first); 7017D68A `add(ac2, M32[wp(ac3, 6)])` | B |
| R21 | `for (v = a; v <= lim; v++)` (PL/I `DO v = a TO lim`) with a 16-bit v: v = a via the loop register (R7 pick, the limit protected), `goto [exit, skip] (lr <=s lim)`, the exit branch `goto [after] 0` — **R22**: `ret` when nothing follows the loop — a `goto body` block over the increment block; the increment block (`continue` target) is XNDO: `lr = lim; t1 = nadd(M16[v], 1); M16[v] = t1; t2 = (t1 >s lr); lr = t1; goto [body, exit] t2`. **R21a**: constant bounds have no entry test; the init block jumps straight into the body. | UPDATE_SCREENS 7017D635..64A; REFRESH_SCREEN 70176B06..B10 | A |
| R23 | An **indexed store** `T[i].f[r][c] = e` computes the address first — each subscript checked (R17), scaled (a constant in a register; stride 2 as `add(r, r)`), added to the running partial sum (the R9 temp, then a temp of its own), the base added last, WMOV to ac2 — then the value, then the store. | UPDATE_SCREENS 7017D67A..7017D6A7 | B |
| R24 | After a string statement (WCMV) `ac3 = wfp` (LDAFP) is emitted **lazily**, just before the next statement that addresses the frame; WCMV leaves `ac0 = 0` (known, protected in that statement), ac1 clobbered, ac2 at the destination's end. | REFRESH_SCREEN 70176ABA (`ac3 = wfp` first) vs 70176ADF (`ac1 = 10` before it) | B |
| R25 | A CHAR expression argument is built in a fixed string temp (R3b) and passed as a VARYING dummy: a one-byte piece by `XLEFB 2` + WSTB (+ WINC if more follows; at the WCMV destination end when it follows a WCMV), a longer piece by WCMV; then `[@wp(ac3, s), n varying] = [@bp(ac3, t), n]`. | REFRESH_SCREEN 70176B10 | B |

## 5. Spellings (lower.py's, IR.md §5)

- every constant an instruction carries is `hexc` (`0x%08X`), including sign-extended WNADI immediates (`add(ac0, 0xFFFFFFEC)`); the exceptions are `add(x, 1)` (WINC), `sub(0, r)` (WNEG), `add(r, r)` (WADD r,r), and the `0` of a wide compare against zero;
- `M32[wp(ac2, K)]` / `sx16(M16[wp(ac2, K)])` for fields; `M16[R[ac3 + -(10+2n)]]` for by-reference arguments; `wp(ac3, d)` for frame slots, `bp(ac3, 2d)` for byte addresses;
- `rt_call ?NAME(args) site=<continuation − 4>`; the return value is in ac0 (RTConventions.md);
- a literal piece is `[@0xW:b, "text"]` with the address resolved by content in Disassembled/quest.mem (an equivalence-4 nicety; the three literals of REFRESH_SCREEN resolved to the book's addresses).

## 6. What the three routines did NOT exercise (for P36)

bit-field references (DIED's first statement), ac3 repurposed as an address
register (13 statements in DIED), string statements into record fields,
`words()` (WBLM), `cmp()`, arena twins/claim/release, game→game `call`
decoration, `&&`/`||` conditions, `if` bodies longer than one statement,
value-returning game routines.  translate.py refuses all of them by name.


## 7. Project 36 additions (Sep 9 2026, branch p36-died)

Confidence as above.  Every rule here was motivated by DIED @7016603D and
checked against the three P35 routines (which stay 197/197).

### 7.1 Conversions (ruling 1, now enforced)

| # | rule | evidence | conf |
|---|------|----------|------|
| R16b | A 32-bit value reaching a 16-bit destination is a translator REFUSAL unless the source wrote `cvwn(e)`.  `cvwn` is effectful (ovr on overflow); `sx16`/`trunc16` are pure.  The value `cvwn()` yields is 16-bit, so the store's type-driven narrow does not fire a second time. | PICK_X_Y 70176241 (`cvwn; M16[...] = trunc16`) — unchanged output, now explicit in the source | A |

### 7.2 Bit references (DIED: 25 bit operations, 5 WSZB / 5 WBTO / 15 WBTZ)

The bit address is `16 * <scaled subscript> + (16*K + n)` in a register;
the record base goes SEPARATELY into the instruction's indirect operand
(WSZB/WBTO/WBTZ take acS = base, acD = bit offset — EagleCompute.cpp
:261/:272/:283).  `n` is numbered from the MSB.  Fields found in DIED:
`PLAYER.fm591` bits 7,8,9,12,13,14,15; `PLAYER.fm590` bits 1..6,8,11;
`PLAYER.fm63` bits 0,1,2,5.

| # | rule | evidence | conf |
|---|------|----------|------|
| R26 | The bit offset is produced by a `*16` multiply after the stride multiply; the displacement `16*K + n` is folded into ONE WNADI. | 70166046 `NLDAI 686,1; WMUL 1,0; NLDAI 16,2; WMUL 0,2; WNADI 1,0xDB24` (= word −590, bit 4) | A |
| R26a | The `*16` multiply's DESTINATION is the scaled value's register (R11, multiply in place) unless the scaled value is still needed after the multiply — then it is the CONSTANT's register and the IR reads `mul(16, scaled)`. | 70166283 `WMUL 2,0` (dest ac0; P*686 dead) vs 70166046 `WMUL 0,2` (dest ac2; P*686 saved to slot 10 afterwards) and 70166296 `WMUL 0,2` (dest ac2; P*686 needed for the element address at the block's end, `ac1 = add(ac1, ac0)`) | B |
| R26b | When `16*scaled` does NOT recur and the multiply landed in the constant's register, the product is copied to an R7 pick (WMOV) before the displacement add. | 70166046 `WMOV 2,1` — absent at 70166296, where the value is saved to a temp instead | C — two instances; the alternative reading is "the WMOV frees ac2 for the base (R5)", which fits 70166046 but not 70166283 |
| R27 | `16*scaled` is a CSE temp of its OWN, distinct from the R9 scaled temp: one is `i*stride`, the other `16*i*stride`, and a routine can save both (slot 10 vs slot 8 in DIED). It is saved when a later statement takes another bit of the same element, and each later bit reloads it and adds its own displacement. | 70166296 `XWSTA 2,[ac3+8]` then nine `XWLDA 2,[ac3+8]; WNADI 2,<disp>; WBTZ 1,2` pairs at 701662A2.. | A |
| R28 | The record base of a bit reference is loaded by the R7 pick AVOIDING the offset register, and stays PROTECTED while the `16*scaled` temp is alive — one `LWLDA` serves the whole run. | 70166283 `LWLDA 1` (offset ac0), 70166296 `LWLDA 1` (offset ac2, ac0 live) serving nine WBTZs, 70166046 `LWLDA 2` (offset moved to ac1 first) | B |
| R29 | A bit reference is ALWAYS materialised to 0 / −1 — `WSUB v,v`; the WSZB skip; `WADC v,v` — and a condition on it is then the R14b sign test.  The compiler does not fold the skip into the branch.  PL/I `^` (C `!`) is `WCOM` on the materialised value. | 70166054..57 `WSUB 0,0; WSZB 2,1; WADC 0,0; MOV.L# 0,0,SNC`; 70166376 `ac2 = ~ac2` | A |
| R29a | A bit ASSIGNMENT is set-then-undo: WBTO the destination unconditionally, then the source value's sign test, then WBTZ on the zero arm. | 70166376..82 (`M16[..] \| lsh(..)`, `MOV.L#`, `M16[..] & ~lsh(..)`) | B |

### 7.3 Statement shapes

| # | rule | evidence | conf |
|---|------|----------|------|
| R13c | `if (c) goto L` normally lowers as R13 (skip-if-NOT-c over the one-word `WBR L`, R19 stubbing L when it is far).  When L is beyond WBR range AND the routine's end — where the R19 stub would sit — is ALSO beyond WBR range from the branch, no one-word form exists, so the compiler INVERTS: skip-if-c over `WBR cont`, with the XJMP to L following. | DIED 70166057 `MOV.L# 0,0,SNC; WBR 3 (0x7016605B); XJMP (0x701661AE)` — target +0x154, routine end +0x360.  FALSIFIED FIRST DRAFT: the trigger "target out of range" alone broke PICK_X_Y's three retries (64→68 statements, 13 DIFF), whose stub IS in range; the comparator caught it (ruling R3 working as intended). | B |

### 7.4 Located strings into record fields

A string statement's address operand is NOT a register: lower.py prints the
expression the master's registers unfold to, as computed by
`emulation/tools/string_sites.py`'s `Evaluator` and printed by its `ir_word`.
compiler/translate.py therefore carries a PORT of that model (`Sym`, `_mk`,
`s_add*`, `ir_const`, `ir_word` — string_sites.py:221–360, :1521–1580),
restricted to the kinds a translation can build.  It is a port, not a
re-derivation: a disagreement between the two is a FINDING.  Constants in that
spelling are decimal below 256 and hex above (`- 86` vs `- 0x271`) — NOT
translate.py's 8-digit `hexc`.

| # | rule | evidence | conf |
|---|------|----------|------|
| R31 | When the record base is ALREADY in ac2 — left there by a bit reference's R28 load — and the scaled subscript is in a temp, the compiler adds the TEMP TO THE BASE rather than reloading the temp and adding the base.  The two orders are observable, because `_mk` merges a linear form's terms in the order they were ADDED, which is the order the instructions ran. | 7016605B `XWADD 2,[ac3+0xA]` → `(M32[0x70000210] + (sx16(M16[0x70000216]) * 686) - 0x271)` vs 7016606F `WMUL 2,1; LWADD 1,[0x70000210]` → `((sx16(M16[0x70000216]) * 686) + M32[0x70000210] - 0x260)` | A |
| R32 | A located-string statement into a record field renders its destination from the symbolic element address with the field's raw K folded in; a VARYING destination absorbs the compiler's XNSTA length-word store when `dst_count == src_count` (IR.md §5.8, P32), and the translator REFUSES the varying form otherwise. | 7016605B / 7016606F (the two 32-blank assignments); 70166096 is the separate `M16[wp(ac2,-625)] = trunc16(32)` store, which is the length word written as its own PL/I statement (`.len` as a named subfield of `VARYING(n)`) | B |

### 7.5 Game→game calls

| # | rule | evidence | conf |
|---|------|----------|------|
| R30 | A game→game call with argc ≥ 1 stores its arguments to STATIC book-mode slots from the addrbook's `alloc`; argument n lives at `wfp − 10 − 2n`, so the slots run downward from argument 1 and the stores appear in ASCENDING address order — right to left in the source, which is R18's push order.  The call is `call <tgt> args=<n> marker=<alloc + 2*argc> site=<pc> ret=<pc+4>`.  An argc-0 call is not a pushmap site and stays an embedded `LCALL [<tgt>],0`. | 70166108..0C (UPDATE_SCREENS: `M32[0x74009B5C] = wp(ac3,6)` = arg 3, `…5E` = arg 2, `…60` = arg 1, marker 74009B62); 70166216 / 70166327 the two undecorated LCALLs; 701661AB the LJSR audit line | A |

## Carried-in for P36 (Sep 8 2026 session, from the PICK_X_Y coordinate discussion)

- **Conversions are explicit intrinsics.** `RANDOM_NUMBER` (and every
  routine returning in ac0) returns a 32-bit value. A 16-bit destination
  inserts a CHECKED convert — the CVWN instruction, `cvwn(e)` in the IR
  (sign-extend low 16, set OVR if it did not fit) — before the
  `trunc16` store; a 32-bit destination inserts nothing. The C subset
  makes this explicit (`*x = cvwn(RANDOM_NUMBER$3(...))`): no implicit
  32→16 narrowing in the source of record; `cvwn`/`sx16`/`trunc16` are
  header intrinsics mapping 1:1 to the IR ops. (P35 emitted the cvwn
  from the type-driven implicit cast; P36 makes it visible and refuses
  the silent form.)
- **No world-coordinate offset.** The game computes entirely in the raw
  0x3Bxx coordinate space (X 0x3B53–0x402F, Y 0x3B77–0x3FC0); there is no
  origin subtraction in any routine. PICK_X_Y's gates
  (`0x3BF5 < x <= 0x3FAC`, `0x3B77 < y <= 0x3FDE`) are an interior spawn
  box in that space (the x-gate trims the barrier rim; the y-high bound
  sits just past the data). The pixel coordinates in the world render
  were a viewing transform, not part of the program — do not add an
  offset to the header or the readable layer.


## 8. Project 37 additions (Sep 9 2026, branch p37-routines)

Routine 1 of the P37 set: **OWNS @70175CBF, 152/152 MATCH primary, 114/114
folded**, with the three P35 routines at 197/197 throughout and `--selftest`
PASS after every change.  Pragma count: **0**.

### 8.0 Amendment to P36 ruling 1 (conversions)

R16b refuses a 32-bit value reaching a 16-bit destination without an explicit
`cvwn()`.  It does NOT reach a **literal that already fits in 16 bits**: the
compiler loads such a literal with a sign-extended `NLDAI` and stores it with a
bare `trunc16`, emitting no `CVWN`, because there is nothing to check at run
time.  OWNS 70175CE7 (`return -32768` -> `ac0 = 0xFFFF8000 ; NLDAI 32768
(0x8000),0`) is the instance; the same shape is every DO-loop initialisation
(`M16[wp(ac3,2)] = trunc16(ac1)`).  The refusal stands for every non-literal.

### 8.1 Value-returning routines

| # | rule | evidence | conf |
|---|------|----------|------|
| R35 | A value-returning PL/I function stores its result into the SAVED-ac0 IMAGE of its own frame, so `WRTN` restores it into ac0 — which is exactly the addrbook's `slotpatch` flag.  A 32-bit result takes the whole image word at `wp(ac3, -8)`; a 16-bit result takes its low half at `wp(ac3, -7)`.  The translator refuses a value return from a routine the addrbook does not mark `slotpatch`. | OWNS 70175DA2 `XNSTA 2,[ac3+0x7FF9]` (16-bit); DISTANCE_TO_PLAYER 701687D9 `XWSTA 0,[ac3+0x7FF8]` (32-bit) | A — two routines, both widths |
| R35a | A value return is therefore NOT a one-word THEN (R13): it evaluates the value, stores it, and only then `WRTN`s, so `if (c) return e;` takes the R13b shape (skip-if-c over `goto else`).  A bare `return;` remains one word. | OWNS' seven `if (*item == K) return BIT(...)` arms, 70175CED..70175D8B | A |

### 8.2 Loops

| # | rule | evidence | conf |
|---|------|----------|------|
| R21c | The DO-loop register is picked from **ac0/ac1 only** — ac2 stays free for addressing — and it is picked BEFORE the initial value's own register.  When the two differ the constant is loaded into its own register, stored to the loop variable, and copied to the loop register; when they coincide no move appears. | OWNS 70175CC7 (`ac2 = 1; XNSTA 2,[ac3+2]; WMOV 2,1` — lr ac1 because ac0 is live and ac1 holds the stride constant); UPDATE_SCREENS 7017D635 and REFRESH_SCREEN 70176B06 both coincide and emit no WMOV | A — three routines |
| R36 | A subscript in the loop BODY that is invariant in the loop variable is evaluated at the loop head: its bound check (R17) and stride multiply BEFORE the loop's own initialisation, its R9 temp store AFTER it.  The body reloads the temp each iteration.  This is the compiler's first observed optimisation. | OWNS 70175CBF..CC7: the DERR 17 on `*p` sits in the entry block, `NLDAI 686 / WMUL` opens the init block, and `XWSTA 0,[ac3+4]` follows `WMOV 2,1` | **C** — ONE instance.  Neither P35 loop can witness it: UPDATE_SCREENS' body subscript IS the loop variable (R9a) and REFRESH_SCREEN's body references no table.  DIED has two XNDO loops and should settle it. |

### 8.3 Indexed field reads

| # | rule | evidence | conf |
|---|------|----------|------|
| R23r | The READ counterpart of R23: `T[i].f[j]` builds its address exactly as the store does — each subscript checked, scaled, added to the running sum (the R9 temp), the base added last, `WMOV` to ac2 — and then loads from `wp(ac2, K)`. | OWNS 70175CDB (five instructions: `XWADD 1,[ac3+4]; LWADD 1,[0x70000210]; WMOV 1,2; XNLDA 0,[ac2+0x7E7A]`) | B |
| R23a | A stride of **1** emits NO scaling at all (stride 2 is `add(r, r)`, any other stride is a constant in a register and `WMUL`). | OWNS' `fm390` inner dimension, stride 1 word | B |
| R37 | The constant ZERO is materialised by subtracting a register from itself (`WSUB r,r`), never by an immediate load. | OWNS 70175DA5 `ac0 = sub(ac0, ac0)` for `return 0`; FIRE.1 7016A3D2 and INIT_OBJ_TBL 7016DF68 `WSUB 1,1`; it is also the first half of R29's 0/-1 materialisation | A |

### 8.4 Register knowledge across a branch — one rule kept, one falsified

| # | rule | evidence | conf |
|---|------|----------|------|
| R8c | An `if (c) {body}` whose body RETURNS or GOTOes has only ONE edge into its continuation — the skip's own `goto` — so nothing joins there and register knowledge SURVIVES.  R8 (reset at a label) still governs a genuine join, where two paths meet. | OWNS 70175CED: `ac0 = *item` is loaded once and all seven chain tests compare ac0 directly; a reset reloaded it seven times (7 extra statements, caught as DIFF(extra)) | B |
| R7c′ | An R13b body INHERITS the skip's register state, and a register holding a variable that the if's CONTINUATION still reads is PINNED (cost 3) for the duration of the body's **subscript expression**; the pin is released once that expression's stride multiply is done, and the bit-address arithmetic that follows may take the register. | OWNS: six bodies avoid ac0 for `*p` (`XNLDA 1,@[ac3+0xFFF4]`) and for the stride constant (`NLDAI 686,2`), then TAKE ac0 for the constant 16 (`NLDAI 16,0`); the seventh — after which nothing reads `*item` — runs ac0/ac1/ac2 in order (70175D8E `XNLDA 0`).  All 8 instances fit. | **C** — the release point is fitted to one routine.  DIED's 25 bit operations are the test. |
| ~~R7c~~ | **FALSIFIED.**  First draft: "a variable's register is protected until its LAST REACHABLE use, anywhere" — the natural generalisation of R7b past the statement boundary.  It did not fix a single OWNS DIFF and it regressed **UPDATE_SCREENS from 72/72 to 47/72 (25 DIFF)**, because `*x` and `*y` are read by later SIBLING statements there and the compiler plainly does not protect them across a statement boundary.  R8b stands as written; only the narrow R7c′ survives.  (METHOD §11: recorded as the wrong turn it was — a rule that was predicted at the plan gate, built, and refuted by the comparator.) | UPDATE_SCREENS 7017D64E..7017D6A7 | — |

### 8.5 R27's trigger, narrowed

R27 said `16*scaled` is saved to a temp "when a later statement takes another
bit of the same element".  OWNS falsifies that as stated: its seven bit
references are all on `PLAYER(*p)`, yet NONE saves a temp — each recomputes
`*p * 686 * 16` from scratch.  The difference is **reachability**: DIED's nine
WBTZs are consecutive siblings in one block, so the first genuinely has eight
later uses; OWNS' seven sit in seven different if-arms and are mutually
exclusive, so no arm has a reachable later use at all.

> **R27 (amended):** a later use counts only if the flow from this statement
> can REACH it — two statements in different ARMS of an `if` are not later
> statements for one another.

Implemented as `Translator.reachable_later_uses`, over an arm-path recorded by
the pre-pass.  Both DIED's saving behaviour and OWNS' recomputing behaviour
follow from the one amended rule.

### 8.6 Miscellany

- **R7 (extended instance).**  The LEFT operand of a two-register comparison is
  a value the statement still needs (cost 3), so the right operand's load picks
  elsewhere: OWNS 70175CDB puts the field in ac0 and is then forced to take ac2
  for `*item` (`XNLDA 2,@[ac3+0xFFF2]`).  A constant right operand is a wide
  skip-with-immediate (R15) and cannot collide.
- **Subscripts** may now be a dereferenced by-reference parameter (`*p`), not
  only a variable; OWNS subscripts PLAYER with `*p` in all eight of its element
  references.  `subscript_sym` still REFUSES to give such a subscript a
  symbolic form, so a located-string statement through one is a refusal, not a
  guess.
- **New declarations** (gen_declarations.py): `PLAYER.fm591` (K=-591) and
  `PLAYER.fm390[10]` (K=-390, inner stride 1, 1-based, bound 10).
- **Fields exercised.**  OWNS' seven bit displacements decode to `fm590` bits
  0, 2, 3, 4, 5 and `fm591` bits 14, 15.  Six overlap the set DIED established;
  `fm590` bit 0 is new.
- **R29a gets NO second witness here.**  All seven of OWNS' bit operations are
  `WSZB` tests; the routine contains no `WBTO` and no `WBTZ`, so the bit
  ASSIGNMENT rule stays at confidence B on DIED alone.

### 8.7 From GET_INPUT (routine 2, STAGED — no match number)

| # | rule | evidence | conf |
|---|------|----------|------|
| R2a | An ARRAY local takes as many words as it needs, rounded up to a whole even slot (a `CHAR(n)` buffer takes `ceil(n/2)` words); R1/R2's one-wide-slot rule is the scalar case. | GET_INPUT: `char buf[144]` at slots 4..75 puts the first temp at 76, which is where the book puts it (`XLEF 2,[ac3+0x4C]`), and 76/78/80/82 exactly fill the `WSAVS 0x0029` frame | B |
| R38 | A PL/I **BIT literal** is NOT constant-folded.  It is rebuilt at every evaluation by `X.CB @7017E708`: ac2 = the destination's word address (a frame temp), ac0 = a byte pointer to the character form, ac1 = its length, then an embedded undecorated `LCALL [0x7017E708],0`. | GET_INPUT 7016AA41 (`"001"` at 0x7016A9B9, len 3) and 701703A6 (`"1"` at 0x7017024D, len 1); literal bytes read from Disassembled/quest.mem | A — two sites, both game-level |
| R35 | (third witness) 701703A6's caller builds `'1'B`, reads it back with `XNLDA 0,[ac3+0x16]` and stores it to `wp(ac3, -7)` — a 16-bit slotpatch value return. | 701703AA..AE | A |

**OPEN — the temp allocation order for a multi-dummy call.**  GET_INPUT gives
slot 76 to argument 6 (the BIT literal) and slot 78 to argument 2 (the buffer's
byte-pointer dummy), yet emits the slot-78 store FIRST and the X.CB call
second.  "Left to right" (R18) gets the allocation wrong; "call-materialised
arguments first" gets the emission wrong.  One instance cannot separate them,
so the translator REFUSES the BITS() argument rather than carry a fitted rule
into every other multi-dummy call in the game.  Details and the evidence:
docs/Project37/GetInputFinding.md.

### 8.8 Predicted defects — three sites that share R27's flaw

R27's amendment (§8.5) replaced a bare `j > i` "is there a later use?" test with
`reachable_later_uses`, because two statements in different arms of an `if` are
not later statements for one another.  **Three other rules still ask the same
question the same wrong way**, and are recorded here as PREDICTED defects so
that when one trips it is a prediction confirmed, not a discovery:

| site | rule | the test it still uses |
|---|---|---|
| `element_address`, R9 save | the scaled-subscript CSE temp | `any(j > i for j in self.uses.get(skey, []))` |
| `element_address`, R10 save | the element-address temp | `later = [j for j in self.uses.get(skey, []) if j > i]` |
| `Frame.alloc_temp` / `last_use_of`, R3 | "a temp is free from its last use on" | `max(uses)` over all uses, arms included |

None of the four routines matched so far can expose them: no matched routine
references the same table element from two arms of one `if`.  INIT_OBJ_TBL and
DIED both have branchy bodies with repeated element references, so at least one
of the three is expected to need the same amendment there.  **They are NOT
being changed speculatively** — the comparator should be the one to demand it
(ruling R3: a register or slot rule is falsified by a DIFF, never pre-empted).

### 8.9 From RETURN_MESSAGE (routine 3, STAGED — no match number)

RETURN_MESSAGE @70176FDD is the game's fatal-error exit; its tail is
`SYSCALL 0310` = ?RETURN (AOS/VS process exit, never returns).  Its optional
message is argument 3, a CHAR VARYING by reference, and when absent it
substitutes the literal at 0x70000CCD — which reads **"Unexpected error"**,
exactly the 16 bytes the default length constant `NLDAI 16` claims.  The string
and the constant were recovered independently and agree.

**OPEN — the two mixed-arity spellings, and why this one cannot be ruled.**
REFRESH_SCREEN reads a frame MARKER word at `wp(ac3, -9)` (R12);
RETURN_MESSAGE instead tests an argument SLOT for null (`M32[wp(ac3, -16)]`,
not `R[ac3 + -16]`).  A census of the addrbook shows these are the **only two
`mixed:` routines in the program**, so no third instance exists and any binary
discriminator fits: the question is unfalsifiable here.  Recorded as such;
no rule.

**FINDING — RETURN_MESSAGE's `mixed:3/6` is an artifact of hand-assembly.**
Four of its five call sites pass six arguments.  The one 3-argument site,
70169B82, is inside **LOCK_FILE**, the hand-written assembly routine identified
at the P37 plan gate, in the tail it shares with UNLOCK_FILE, building its
arguments on the stack by hand.  The arity flag therefore records an assembly
calling sequence, not a PL/I language feature.  This generalised into a ruling
of its own — see **R39** below and `docs/Project37/InadmissibleEvidence.md`.

**RECORDED, not ruled — the self-move at 70176FF5.**  The join of the message
diamond opens with `ac0 = ac0` (`WMOV 0,0`).  Both arms already leave the length
in ac0, so the compiler appears to materialise a joined value into an R7 pick
without checking whether source and destination coincide.  One instance; it
cannot be tested until the routine translates.


### 8.10 R39 — inadmissible evidence (an admissibility ruling, not a codegen rule)

| # | rule | evidence | conf |
|---|------|----------|------|
| R39 | Not every addrbook entry is compiler output.  An observation taken from, or about, a **hand-assembly** routine is INADMISSIBLE as evidence about the PL/I compiler, and the contamination TRAVELS: it reaches the addrbook metadata of routines that are themselves ordinary compiled code.  Before promoting any convention, arity, frame value or entry variant to a rule, check whether its only witness lies inside a hand-assembly range; if it does, the observation is void and the rule has no evidence.  Cite as *"void under R39."* | Hand-assembly span **70169B0F..70169D69** (LOCK_FILE + UNLOCK_FILE, one unit): they branch into each other's ranges, which no two PL/I procedures can do | A |

**Voided by the R39 sweep** (every call site inside the span was enumerated;
exactly one leaves it, so the callee contamination is bounded but real):

1. `RETURN_MESSAGE` `mixed:3/6` — the 3-arg caller is the assembly (§8.9).
2. **`WSAVR` is not a compiler entry variant.**  It occurs exactly TWICE in the
   whole addrbook, and both are LOCK_FILE and UNLOCK_FILE; all 100 other live
   entries are `WSAVS`.  Any model treating WSAVR as one of two compiler-emitted
   entry conventions is modelling something the compiler never emitted.  (Found
   by the sweep, not previously noticed.)
3. `LOCK_FILE frame 0x01 dyn,push` / `UNLOCK_FILE frame 0x00` — the assembly's
   own stack discipline; not evidence about R1/R2/R3 frame layout.

**Direction matters.**  Contamination flows OUTWARD from an assembly call site,
not inward.  LOCK_FILE's `argc 2` and UNLOCK_FILE's `argc 1` are ADMISSIBLE:
they were inferred from compiled call sites in `SIGNAL_TURN` (70177E7D,
70177EFD, 70177F0B).  What a compiled caller does when calling assembly is
still evidence about the compiler; what assembly does when calling anything is
not.  The test is *"is the hand-written side producing the behaviour I am about
to generalise?"*

**Detector for further cases:** control flow crossing an addrbook boundary — a
routine that branches outside its own [entry, next-entry) range, or is branched
into from outside.  That is what exposed this pair.  This sweep does not prove
there are no others.

*(A companion entry in `docs/METHOD.md` §16 is recommended at merge time; P37's
boundaries do not permit editing that file.)*

### 8.11 R40 — multiple ENTRY points into one procedure

| # | rule | evidence | conf |
|---|------|----------|------|
| R40 | A PL/I procedure with more than one `ENTRY` compiles to SEVERAL addrbook entries sharing one body.  Each entry has its own real `WSAVS <frame>` prologue with the SAME frame size and argc, initialises the same frame slots with DIFFERENT constants, and branches one-way into the shared body. | TRANSPORT_TERRAK 7017D48F (slot 8 := 2) / TRANSPORT_SUNDAR 7017D492 (slot 8 := 7), both `frame 0x0C argc 2`; CREATE_MAP 7016509F (8:=19, 9:=19, 12:=0x8000) / DISPLAY_MAP 701650AC (8:=10, 9:=19, 12:=0), both `frame 0x6AA argc 2` | A — two independent pairs |

**Do not confuse R40 with R39.**  Both show as cross-family control flow.  The
discriminators: R39's hand-assembly crossing is **MUTUAL** (each branches into
the other) and the two sides differ in frame, argc and entry variant; R40's is
**one-way from a prologue into the other entry's body**, with identical frame,
identical argc and a real WSAVS at each entry.

**The addrbook's statement counts mis-attribute an R40 pair**: the shared body
falls into whichever range contains it.  TRANSPORT_TERRAK counts 2 statements
and TRANSPORT_SUNDAR 138; CREATE_MAP counts 7 and DISPLAY_MAP 902.  Any census
that treats those as four independent routines is wrong, and any sampling frame
built on statement counts inherits the error.

### 8.12 The frame relocation — DERIVED, pragma NOT needed (routine 4)

Full derivation: `docs/Project37/FrameRelocation.md`.  **Verdict: no
`#pragma fp ac2`; pragma count stays 0.**  P36 finding 1 called this a
"register reassignment of the frame pointer"; reading FIRE.1 (the small case)
first shows the weaker and correct story — **there is no relocation at all.**

| # | rule | evidence | conf |
|---|------|----------|------|
| R33 | The frame pointer is not pinned.  It is an ordinary VALUE: `ac3` holds it by default and is otherwise ordinarily allocatable; when a statement needs a base and ac3 is the R7 pick, ac3 takes it and the frame becomes "not in a register".  The next frame reference emits `LDAFP` into an R7 pick and spells `wp(acN, d)`.  R24 governs *when*, R7 governs *where*. | FIRE.1 7016A3C7 (3 × `ac2 = wfp`, 3 × `ac3 = wfp`); DIED 70166376 (1 × ac2, 12 × ac3); INIT_OBJ_TBL (8 × ac3); DISTANCE_TO_PLAYER (1 × ac3) | B |
| R34 | A live value in ac3 that must survive an `LDAFP 3` is preserved across it by `WPSH 3,3` / `WPOP 3,3`. | FIRE.1 7016A3C7; INIT_OBJ_TBL 7016DF68 | B |

**DIED's long stretch is not a special case.**  Through its run of R29a bit
assignments ac3 holds the record base (R28 already says one `LWLDA` serves the
whole run), ac1 holds the bit offset and ac0 the bit value — so ac2 is the only
register left for the frame, and nothing in those ten blocks wants ac2 for
anything else.  The 22 `wp(ac2, 8)` references are an absence of register
pressure, not a policy.  FIRE.1 and DIED differ only in how soon ac2 is reused.

**Do not code "the LDAFP target is always ac2."**  It was ac2 in every observed
instance, but in each of them ac2 was also the only register R7 could have
picked, so the observation carries no independent information.

**NOT IMPLEMENTED.**  `Regs` still models ac0–ac2 with ac3 pinned; making ac3
allocatable and threading a "where does the frame live" state touches every
emit site.  P36's estimate of the SIZE of that change was right even though its
description of the phenomenon was not.  **FIRE.1 has no match number and none
should be quoted for it.**

## 9. Project 38 additions (Sep 9 2026, branch p38-routines)

R33/R34 are now IMPLEMENTED.  Implementing them required one correction to R8
and produced one amendment to R33 itself, on two independent witnesses.

### 9.1 R8d — the frame survives a join (a correction to R8)

| # | rule | evidence | conf |
|---|------|----------|------|
| R8d | R8's reset at a join clears cached values but **not the frame**.  Where the frame is, is not *knowledge* about a value — it is the record of which `LDAFP`s have been executed, and a join executes none.  ac3 therefore still holds the frame at the top of a block. | Every block of all four matched routines addresses `wp(ac3, d)` with no preceding `LDAFP` — 349 statements.  Measured the other way: when `reset()` clears ac3 the frame becomes cost 0, `pick()` takes it at once and all four regress (PICK_X_Y 62/64, UPDATE_SCREENS 44/72, REFRESH_SCREEN 59/61, OWNS 107/152). | **A** |

This is not an implementation detail.  R8 as written ("register knowledge
resets at a C label") is false of ac3, and the only reason P35–P37 never met
the falsification is that ac3 was pinned and so never passed through `reset()`.

**Corollary (NO WITNESS).** `restore()` (R8c, register knowledge on one edge)
restores the other three registers but leaves the frame where the emitter has
actually left it.  In all four matched routines the frame is in ac3 on both
sides of every edge, so nothing tests this; it is a modelling choice, recorded
as one.

### 9.2 FP_COST — the frame's protection cost, bounded below only

The frame competes for a register like any other value (R33), so it needs a
protection cost in R7's table.  `FP_COST = 2`.

- **Lower bound, established:** at 0 or 1 all four matched routines regress
  (figures above).  So `FP_COST >= 2`.
- **Upper bound, NOT witnessed:** 2 and 3 give byte-identical output for all
  four routines.  They differ in exactly one case — a statement in which
  ac0, ac1 and ac2 are **all** live (cost 3) at the moment a register is
  picked.  At cost 2 the frame is evicted and an `LDAFP` follows; at cost 3
  it is not and the value must go elsewhere.  No such statement occurs in the
  four.  **The experiment that would pin it** is any routine containing that
  statement; INIT_OBJ_TBL 7016DF68 is a candidate (its `WPSH 3,3` site has
  ac0, ac1 and ac2 all live) and R34 covers it, so a routine where the
  contested register is wanted for an ordinary value is still needed.

2 is taken as the weaker of the two admissible claims: 3 would additionally
assert that ac3 is never taken while any other register is live, which has no
witness.

### 9.3 R41 — the base-register class {ac2, ac3} (amends R5 and R33)

| # | rule | evidence | conf |
|---|------|----------|------|
| R41 | An **address or base load** does not use R7's pick over all four registers.  It allocates from a two-register class **{ac2, ac3}, ac2 preferred**; ac0 and ac1 are value registers and are not candidates however cheap they are.  When ac2 is occupied by a live address the base goes to ac3 — displacing the frame, which R33/R24 then re-materialise by `LDAFP`. | INIT_OBJ_TBL 7016DF68: `ac2 = M32[wp(ac3, -14)]` with ac2 free, and `ac3 = M32[wp(ac3, -14)]` — the **same argument load, same routine** — with ac2 live and ac0 free and cost 0.  FIRE.1 7016A3C7: three `ac3 = <address>` sites, each with ac2 holding a live address and ac1 or ac0 free at cost 0. | **B** |

**R33 amended.**  R33's phenomenon stands and is confirmed: the frame is not
pinned, it is displaced and re-materialised by `LDAFP` into a picked register.
R33's *mechanism* — "when a statement needs a base and ac3 is the R7 pick, ac3
takes it" — is **contradicted**.  At FIRE.1's `ac3 = M32[wp(ac3, -6)]` the costs
are ac0 = 3 (live), ac1 = 0, ac2 = 0, ac3 = 2: R7 picks ac1, the book picks ac3.
The discriminator in both routines is only ever **ac2's occupancy**, never
ac0's or ac1's.

**Why the four matched routines could not have witnessed this.**  In all 349 of
their statements ac2 is free at every base load, so the class never reaches its
second member and R41 and R5 give the same answer everywhere.  Their staying at
100 % is therefore consistent with R41 but is not evidence for it (METHOD §16:
consistent is not evidence) — the two witnesses above are.

**R34 is now derived, not fitted.**  P37 recorded R34 (`WPSH 3,3` / `WPOP 3,3`
around an `LDAFP 3`) as a separate B-confidence rule from two instances.  Under
R41 it is a consequence: at INIT_OBJ_TBL 7016DF68 ac0, ac1 and ac2 are all live
and ac3 holds a live base, so there is no register for the frame at all, and
saving ac3's occupant is the only way to get one.  R34 describes what the
allocator does when the base class and the value registers are simultaneously
full.

### 9.4 R3b VOIDED and replaced — the temp pools are not disjoint

**R3b's disjointness clause is falsified.**  P35 wrote: *"scalar temps never
take words that ever belonged to a string temp"*, on REFRESH_SCREEN's
4/18/20-vs-6/22/34 slot pattern.  That pattern was never evidence for it.
REFRESH_SCREEN's string temps are **live** at every point where a scalar temp
is allocated, so slot 6 was unavailable under a disjoint reading and under an
overlapping one alike — the observation could not have come out the other way
(METHOD §16).  The belief has been in the ledger since P35 and was carried
through P36 and P37 unchallenged.

| # | rule | evidence | conf |
|---|------|----------|------|
| R3b′ | The scalar and string temp pools **overlap**.  A scalar temp takes the lowest free even slot above the locals, where a slot is free if no *live* temp of either kind holds it — but a dead **string** temp's words become available to a scalar temp only from a **later statement** on, not within the statement that killed it. | HIT_ANY_CHAR 7016DEAD: the packed CHAR VARYING takes `wp(ac3, 4)`, the slot the 30-byte prompt dummy held (4..19) until the `?WRITE_SCREEN` two statements earlier consumed it — disjointness predicts slot 20.  REFRESH_SCREEN 70176B10: the row temp does NOT take slot 6 although the CAT dummy there is already dead, because it died in that same statement — it takes 18; plain overlap predicts 6. | **B** |

The statement boundary is the discriminator and each routine is a witness for
one side of it.  Note the contrast with R3's clause for *scalar* temps, which
frees a slot "from its last use on, **including within the statement that last
uses it**": a string temp's words are released one statement later than a
scalar temp's.  Both P35 routines and HIT_ANY_CHAR are 100 % on slots under
R3b′ (identity bijections).

### 9.5 OPEN — the caller's register state after a game→game call

**No rule recorded.**  HIT_ANY_CHAR is complete and correct except for one
register choice, and the choice cannot be derived from the available evidence.

    7016DEAD   ac1 = 0x00020D0B      the book
               ac0 = 0x00020D0B      translate.py (R7: everything is cost 0
                                     after the call, ties to the lowest number)

Program-wide sweep — sites loading a packed immediate and storing it to a frame
slot in the block immediately after a decorated game→game `call`: **15 sites,
12 use ac0, 3 use ac1.  All three ac1 sites call GET_INPUT @7016AA35**
(701618A9, 7016DEAD, 7016F411); every ac0 site calls something else.

Two readings fit all 15 and nothing here separates them:

1. **The caller models the callee's exit registers.**  GET_INPUT leaves a live
   pointer in ac0 at both of its `ret` blocks (`ac0 = M32[wp(ac3, -12)]`, then
   `M8[ac0] = zx8(ac2)`), so ac0 is not free on return.  Quest is one PL/I
   compilation unit, so the compiler *could* know this.  But this is a large
   claim about the compiler resting on three call sites of a single callee.
2. **The byte-pointer argument.**  Those three sites are also the only ones in
   the fifteen that push a BYTE pointer (`XPEFB`, `bp(...)`) rather than a word
   pointer.  This has **no mechanism** — how an argument is spelled cannot
   change the caller's registers after the call — so §16 rules it out as a
   rule even though it fits perfectly.

**What would separate them:** a site calling a *different* callee that also
leaves a live ac0 (reading 1 predicts ac1 there, reading 2 predicts ac0), or an
ac1 site whose callee leaves ac0 dead.  Neither exists in the 15.  Until one
is found this stays an open finding; **HIT_ANY_CHAR is abandoned at 8/10 rather
than closed with a fitted cross-procedural rule.**

## 10. Project 40 additions (Sep 9 2026, branch p40-routines)

From **QUEST.1 @7015C5E1**, the expression-level diagnostic, with the parent
**QUEST @7015C337** read as evidence (not translated) and **OWNS** as a
negative control.  The four matched routines stay 349/349 primary and 242/242
folded and `--selftest` is PASS after every change below.  QUEST.1 itself is
**ABANDONED at 16/86** — see `docs/Project40/QUEST1_ABANDONED.md`; its loop
header matches the book instruction for instruction, its body does not.

QUEST.1 required no new construct and no new spelling.  Every rule here is an
AMENDMENT to a rule that was fitted to a single routine, and in each case the
amendment claims LESS and explains MORE.

### 10.1 Why these were invisible until now

R36 (the loop-invariant hoist) had one witness, OWNS, whose DO limit is a
CONSTANT.  R21e (the DO limit evaluated once into a temp) had one witness,
LIST_PLAYERS.3, which has no hoist.  **QUEST.1 is the first routine with both
in one loop**, and every rule below is a question that only a loop with both
can ask.  This is the general shape of the P40 finding: the model's
single-witness rules were not wrong so much as under-determined, and the
missing evidence was an ordinary routine, not a new construct.

| # | rule | evidence | conf |
|---|------|----------|------|
| R36′ | **R36 AMENDED — what is hoisted is the invariant PART of the reference, and how much is invariant falls out of the reference itself.** OWNS' `PLAYER(*p).fm390(i)` has an inner subscript varying with the loop variable, so only the outer scale lifts and the base add stays in the body; QUEST.1's `PLAYER(PLAYER_NUM).fm589` is invariant entire, so the whole ELEMENT ADDRESS lifts and the body reloads it into ac2 with no base add at all. | OWNS 70175CDB (`XWADD 1,[ac3+4]; LWADD 1,[0x70000210]`) vs QUEST.1 7015C5F9/7015C5FB (`LWADD 0,[0x70000210]` before `XWSTA 0,[ac3+0x6]`) and 7015C60E (`XWLDA 2,[ac3+0x6]; XNLDA 1,[ac2+0x7DB3]`); the parent QUEST 7015c347/7015c356/7015c36e is the same shape | **B** |
| R36a | **R36's placement, settled: "before the loop's own initialisation" means before the WHOLE loop header, the limit expression included.** OWNS' constant limit meant there was no limit evaluation to be ordered against. | QUEST.1 7015C5EA (stride multiply opens the init block, the limit load is four instructions later); QUEST 7015c344/7015c34d | **B** |
| R21e′ | **R21e AMENDED — the limit is an ORDINARY LIVE VALUE, reloaded for the entry test only if its register was taken in between.** P39's "and is reloaded from that temp" was fitted to LIST_PLAYERS.3, where the initial constant took ac0, the register the limit was in. QUEST.1 puts the constant in ac2 and ac1 carries the limit straight through the test. The weaker rule explains both witnesses with no special case. | LIST_PLAYERS.3 7016F563 (reload) vs QUEST.1 7015C5F0..7015C5FD (no reload) | **B** |
| R21c′ | **R21c ENFORCED and placed.** (a) The loop register comes from ac0/ac1 ONLY — stated in P37, but implemented as `pick(avoid=("ac2",))`, which fell through to ac3 and put the loop counter in the FRAME register once ac0 and ac1 were both live. QUEST.1 is the first loop to reach that state (the hoist holds one value register, the limit the other). (b) The copy into the loop register is emitted AT THE FIRST POINT THE LOOP REGISTER IS FREE: in OWNS lr is ac1, free once the control variable is stored, so the move lands there and the hoist store follows it; in QUEST.1 lr is ac0, still holding the hoisted address until its own store, so the move is deferred onto the body edge. (c) The entry test therefore compares the INITIAL VALUE's register, not the loop register — they coincide in every previously matched loop. | OWNS 70175CC7 (`WMOV 2,1` then `XWSTA 0,[ac3+4]`) vs QUEST.1 7015C5FD/7015C600 (`WSLE 2,1`, then `WMOV 2,0` in the skip block); QUEST 7015c358/7015c35a | **B** |

`Regs.pick` grows an `only=` parameter so that a rule stating a register CLASS
cannot silently fall out of it when every member is expensive.  R41's {ac2,
ac3} should be audited the same way; it was not reached in P40.

### 10.2 R7d — statement-wide allocation: CONFIRMED in the loop header, OPEN in general

At QUEST.1 7015C5EA the costs are ac0 = 3 (subscript, live), ac1 = 0, ac2 = 0,
ac3 = 2.  R7 ties to the lowest number and gives **ac1**.  The book loads
`NLDAI 686,2` — **ac2**.

| routine | site | live | limit | R7 says | book says |
|---|---|---|---|---|---|
| QUEST.1 | 7015C5EA | ac0 = subscript | expression | ac1 | **ac2** |
| QUEST | 7015c344 | ac1 = subscript | expression | ac0 | **ac2** |
| OWNS | 70175CC7 | ac0 = subscript | **constant** | ac1 | **ac1** |

> **R7d (scoped).** The stride constant of an R36 hoist is TRANSIENT — dead at
> the WMUL — while the limit that follows it in the same loop header needs a
> VALUE register.  The constant therefore takes ac2 and leaves the value
> register for the limit.

**QUEST is the discriminator and it could have failed.**  Its registers are
PERMUTED against QUEST.1's (subscript in ac1, limit in ac0), so "avoid ac1"
and "prefer ac2 whenever it is free" both predict the wrong answer there,
while "avoid the register the limit will take" comes out right in both.  OWNS
is the negative control: with a constant limit there is nothing to avoid and
plain R7 stands — which is why this was invisible until a routine had R36 and
R21e in one loop.  Implementing R7d in that scope made QUEST.1's entry block,
init block and skip block match the book instruction for instruction.

**The general form is NOT implemented and is NOT a rule.**  R7d scoped to the
loop header is a claim about one construct.  The general claim — that the
allocator works over a whole statement's expression tree and protects a
register a later operand will occupy — also fits QUEST.1 7015C621 (the stride
constant takes ac2, avoiding the ac1 that `i` is about to occupy) and
7015C650 (one operand, nothing to avoid, plain R7).  But that is a claim about
WHEN the allocator runs rather than what it prefers, it is the largest
structural claim anyone has made about this compiler, and QUEST.1's body is
where it would have to be tested.  Recorded as an open finding, deliberately
not fitted; see `docs/Project40/QUEST1_ABANDONED.md` §3.

### 10.3 Not a gap after all (METHOD §11)

The P40 plan gate reported that a CONSTANT bit assignment was unmodelled and
needed a new C spelling, citing a refusal at translate.py:1891.  **That was
wrong.**  Line 1891 types `BIT_PUT`'s third argument; it is not a general
refusal.  `BIT_SET` / `BIT_CLR` have been declared in `quest_rt.h` and
dispatched at translate.py:1016 since P36 and emit exactly the bare WBTO and
bare WBTZ QUEST.1 needs — they had simply never been exercised, because DIED,
the only previous bit-assignment witness, used the `BIT_PUT` diamond.  Both of
QUEST.1's bit statements matched on the first translation.  No rule, no
spelling, and the user ruling made on the strength of the wrong report was
withdrawn rather than implemented (the reasoning is recorded in quest_rt.h).
The claim came from reading the code instead of running it — METHOD §10.
