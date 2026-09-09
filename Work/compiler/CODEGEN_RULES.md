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
