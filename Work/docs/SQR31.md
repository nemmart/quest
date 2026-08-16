# SQR31?3 (0x7015BD20) — Translation Derivation

Status: DERIVATION COMPLETE, not yet implemented. Decoded from
`Disassembled/quest.blocks` + `quest.dis` this session; all instruction
semantics pinned from the emulator source (`hw/EagleFloat.cpp`,
`hw/EagleInstruction.cpp`, `hw/NovaCompute.cpp`, `hw/EagleCompute.cpp`).

Note on inputs: `Tools/quest-rt.dis` referenced by earlier docs is stale;
the current artifacts are in `Disassembled/`. SQR31?3 lives in the
**game** address range, so it appears in `quest.dis`/`quest.blocks`, not
in the RT disassembly.

## Entry facts

| Fact | Value |
|---|---|
| Address | 0x7015BD20 .. 0x7015BD72 (body ends 0x7015BD6E, error tail to 0x7015BD72) |
| Called via | **LCALL, argc = 0** — `LCALL [0x7015BD20],0` at all three sites |
| Frame | `WSAVS 0x0000` (no locals) / `WRTN` |
| Call sites | 0x7016872E, 0x701687D9 (DIST), 0x7017CF78 |
| Calls/session | 319K–482K (hottest routine in the game) |
| Input | FPAC0 (the value to root) |
| Output | FPAC0 |
| Checklist 0 (ENQH/ENQT/DEQUE) | **CLEAN** — none in the body; no callees on the normal paths |

**The FPAC-convention worry from NextSession.md does not materialize.**
It is a normal LCALL/WSAVS/WRTN routine with argc=0; the existing
RTBridge LCALL model applies unchanged. No SS-style bridge sibling is
needed. Only the *data* travels in FPAC0 instead of on the stack.

Because `WRTN` restores ac0-2 + carry from the WSAVS image, **every AC
manipulation in the body is invisible to the caller**. The routine's
entire observable output is floating-point state plus the WSAVS stack
residue.

## Constants (pc-relative, in the `RTSC` mem block at 0x7015BD1A)

Read as eclipse single floats (wide at each address):

| Address | Bits | Value |
|---|---|---|
| 0x7015BD1A | 0x411CEA01 | +1.8071298599243164 |
| 0x7015BD1C | 0xC1193C7F | −1.5772695541381836 |
| 0x7015BD1E | 0x40F44546 | +0.9541820287704468 |

The native port must **read these from emulated memory** (same
`read_wide` + `eclipse_float_to_double` path), not hardcode doubles.

## Algorithm (understood, for confidence — the port is instruction-level)

Split x = m · 16^e with m ∈ [1/16, 1) (mantissa) and e the unbiased
exponent, then:

1. Rational seed: `y0 = A + B/(m + C)` with the constants above.
2. Newton: `y1 = (y0 + m/y0)/2`, then `q = m/y1`; the final iterate is
   `(y1 + q)/2`, with the ÷2 folded into the exponent arithmetic.
3. Reattach 16^(e/2), correcting for odd e by a factor of 4.

Verified numerically: worst relative error of the final iterate over
m ∈ [1/16, 1) is **3.6e-8** — right at the 24-bit single-precision
limit, confirming the decode is correct.

## Instruction semantics pinned (do not re-derive)

**Nova-format ops** (`hw/NovaCompute.cpp`): the disassembler suffixes map
to the CC/SS fields as `.Z/.O/.C` = carry-in 0/1/complement and
`.L/.R/.S` = rotate-left-through-carry / rotate-right / **byte swap**.
`#` = no-load (test only). Skip field: printed `SNC` = KKK 3 = **skip if
carry == 1** (KKK 2 = SZC = skip if carry == 0). Getting this backwards
inverts the whole sign dispatch — it was my first misreading.

**Float ops** (`hw/EagleFloat.cpp`), printed as `OP XX,YY`:

- `FAS XX,YY`: fpac[YY] += fpac[XX]; quads[YY]=0; validate_exponent; fplr=result.
- `FSS XX,YY`: fpac[YY] -= fpac[XX]; same side effects.
- `FDS XX,YY`: fpac[YY] /= fpac[XX]; **throws "Division by zero" if fpac[XX]==0**.
- `FMOV XX,YY`: fpac[YY]=fpac[XX]; **quads[YY]=quads[XX]** (copies the shadow); fplr=result.
- `FHLV YY`: fpac[YY] *= 0.5; quads[YY]=0; validate_exponent; **does NOT set fplr**.
- `FRDS XX,YY`: fpac[YY] = eclipse_wide_round(fpac[XX]) — round to single; quads[YY]=0; fplr=result.
- `XFAMS YY,[addr]`: fpac[YY] += eclipse_float_to_double(read_wide(addr)); quads[YY]=0; fplr=result.
- `XFLDS YY,[addr]`: fpac[YY] = eclipse_float_to_double(read_wide(addr)); quads[YY]=0; fplr=result.
- `FRH YY`: ac0 = top 16 bits of double_to_eclipse_wide_float(fpac[YY]), zero-extended. **Reads fpac, ignores the quads shadow. No fpac/fplr change.**
- `FEXP YY`: replaces fpac[YY]'s 7-bit exponent field with `(ac0 & 0x7F00) >> 8`; quads[YY]=0; fplr=result.

`WULEI YY,imm32`: **unsigned** compare, skip if ac[YY] <= imm (3 words,
skip = +4). `ANDI YY,imm16`: ac[YY] &= imm over the **full 32-bit** ac.
`NLDAI`: sign-extended narrow immediate load.

## Register layout during the body

`FRH 0` at 0x7015BD22 puts the float's top 16 bits in ac0:
`[sign|exp7][mant_hi8]`. `MOV.S 0,2` byte-swaps it into ac2:

    ac2 = (mant_hi8 << 8) | (sign|exp_biased)

so the biased exponent sits in the **low** byte, with the mantissa's
high byte riding along above it. That layout is what makes the later
`SUB 0x40` + rotate-right compute e/2 while carrying the mantissa byte
harmlessly along.

## Decoded flow

### Sign dispatch (0x7015BD22..0x7015BD25, and the 0x7015BD68 tail)

    FRH 0; MOV.S 0,2; NEG.L# 0,0,SNC; WBR 0x7015BD68

`NEG.L#` computes −ac0 and tests **bit 15** of the result (SS=1 sets
carry from bit 15); `SNC` skips when that bit is 1. Result:

- **x > 0** (ac0 = 0x40xx..0x7Fxx) → −ac0 has bit15 = 1 → skip → main path.
- **x = 0** (ac0 = 0) and **x < 0** (ac0 = 0x8000..0xFFFF) → no skip → 0x7015BD68.

At 0x7015BD68: `FSS 0,0` (fpac0 := 0, quads[0]=0, fplr=0), then
`MOV.L# 0,0,SNC` tests bit 15 of ac0 itself:

- **x = 0** → bit15 = 0 → no skip → `WRTN`, returning fpac0 = 0.
  **fpac1 and fpac3 are NOT touched on this path.**
- **x < 0** → bit15 = 1 → skip → 0x7015BD6B: `SUB.ZR 1,1`,
  `WLDAI 0x00011628`, `LJSR @[0x700001BE]` (**.LIERR**) — a mid-body
  emulated call. See "Negative inputs" below.

### Main path (0x7015BD26..0x7015BD35) — seed and two Newton steps

    NLDAI 0x4000,0 ; FEXP 0          -> fpac0 = m  (exponent forced to 64)
    FMOV 0,1                          -> fpac1 = m
    XFAMS 1,[BD1E]                    -> fpac1 = m + C
    XFLDS 3,[BD1C]                    -> fpac3 = B
    FDS 1,3                           -> fpac3 = B/(m+C)
    XFAMS 3,[BD1A]                    -> fpac3 = y0 = A + B/(m+C)
    FMOV 0,1                          -> fpac1 = m
    FDS 3,1                           -> fpac1 = m/y0
    FAS 3,1                           -> fpac1 = y0 + m/y0
    FHLV 1                            -> fpac1 = y1
    FDS 1,0                           -> fpac0 = q = m/y1

`FEXP 0` with ac0 = 0x4000: the mask `(ac0 & 0x7F00) >> 8` yields 0x40,
i.e. biased exponent 64 → m ∈ [1/16, 1). ac2 still holds the original
exponent/mantissa word from `MOV.S`.

`FDS 1,3` cannot divide by zero: y0 ∈ [0.258, 1.001] over the whole
mantissa range (checked numerically).

### Path selection (0x7015BD36..0x7015BD3E, 0x7015BD50..0x7015BD51)

    NLDAI 0x40,0 ; WULEI 2,0x3FFF     -> skip if ac2 <= 0x3FFF

ac2 ≤ 0x3FFF ⟺ mant_hi8 ≤ 0x3F ⟺ **m < 0.25**.

- m < 0.25 → 0x7015BD3C (`FAS 1,0` → fpac0 = y1 + q = 2·√m), then
  `SUB.ZR 0,2,SNC`.
- m ≥ 0.25 → `WBR` → 0x7015BD50, then `SUB.ZR 0,2,SNC` **without**
  the `FAS 1,0` (fpac0 is still q).

`SUB.ZR 0,2,SNC` with ac0 = 0x40 does three things at once:

    ac2 := (ac2 - 0x40) >> 1        (rotate right; the mantissa byte rides along)
    carry := bit 0 of (ac2 - 0x40)  = LSB of the unbiased exponent e
    skip if carry == 1              = skip if e is ODD

The low byte of `ac2 - 0x40` is e (borrowing into the mantissa byte when
e < 0, which is harmless — the halving is uniform across the 16-bit
quantity). After the rotate the low byte is ⌊e/2⌋.

### The four exits

All four end with the same six-instruction tail:

    ADD.S 0,2      ; ac2 = byteswap(0x40 + ac2)   -> target biased exponent in the HIGH byte
    FRH 0          ; ac0 = top16 of fpac0
    ANDI 0,0x0F00  ; keep the low nibble of fpac0's exponent = its excess over 64
    ADD 2,0        ; ac0 = ac2 + ac0
    FEXP 0         ; install the exponent
    WRTN

The `ANDI`/`ADD` pair is a **normalization-carry correction**: fpac0 at
that point is in [0.5, 4), so its own eclipse exponent is 64 or 65, and
the correction adds 0 or 1 hex digit accordingly. That is what makes the
×4 odd-exponent scaling come out exact.

| Exit | Condition | Body before the tail | fpac0 entering the tail |
|---|---|---|---|
| 0x7015BD3F | m < 0.25, e **odd** | `FAS 0,0` | 4·√m ∈ [1,2) → exp 65, corr +1 |
| 0x7015BD47 | m < 0.25, e **even** | `FHLV 0` ; `FRDS 0,0` | √m ∈ [0.25,0.5) → exp 64, corr 0 |
| 0x7015BD52 | m ≥ 0.25, e **odd** | `FAS 0,0` ; `FAS 1,1` ; `FAS 1,0` | 4·√m ∈ [2,4) → exp 65, corr +1 |
| 0x7015BD5C | m ≥ 0.25, e **even** | `FHLV 0` ; `FRDS 0,0` ; `FHLV 1` ; **`FRDS 0,0`** ; `FAS 1,0` | √m ∈ [0.5,1) → exp 64, corr 0 |

**Original-code fossil at 0x7015BD5F.** The second `FRDS` on that path is
`FRDS 0,0` — it rounds fpac0 a *second* time and leaves the just-halved
fpac1 unrounded. Read against the symmetry of the other paths, the
intent was plainly `FRDS 1,1`. This is a **bug in the original 1986
routine**, not a disassembly artifact: the two `FHLV`/`FRDS` pairs were
meant to round both halves before the final add.

Consequences: (1) the port must replicate it exactly — `FRDS 0,0` twice
— or results diverge in the low mantissa bits for m ≥ 0.25 with even
exponent, which is a large share of all calls; (2) when the reconstructed
PL/1-equivalent C++ source is eventually written, this line should be
carried across with a comment rather than "fixed"; (3) it is a useful
provenance marker — hand-written assembly or a compiler quirk, and
evidence this routine was not generated from the same PL/1 source as the
game code.

The two rounding paths (`FRDS` on even exponents, none on odd) mean the
result is **not** a pure function of the mathematical value: bit-exact
replication requires following the path structure, not recomputing a
square root. `std::sqrt` is not an acceptable implementation.

## Residue map

**Stack**: `WSAVS 0x0000`, no locals, no stores anywhere in the body →
the footprint is exactly `RTBridge::emulate_frame()`'s WSAVS image. This
is the simplest residue surface of any translation so far.

**Registers**: ac0-2 + carry restored by WRTN → invisible. ac3 = wfp.

**Floating point** (the real output surface, and currently unchecked by
the pair comparator — see below):

| Path | fpac0 | fpac1 | fpac2 | fpac3 | fplr | quads |
|---|---|---|---|---|---|---|
| x > 0 | result | path-dependent (y1, 2·y1, or y1/2) | untouched | y0 | = fpac0 (last FEXP) | [0]=[1]=[3]=0, [2] untouched |
| x = 0 | 0.0 | **untouched** | untouched | **untouched** | 0.0 | [0]=0, rest untouched |

`fplr` is set by the final `FEXP`, so it equals the result on all normal
paths. `FHLV` does not touch it, which is why the last writer matters.

## Negative inputs — the one emulation re-entry

x < 0 reaches `LJSR @[0x700001BE]` (.LIERR) with error code 0x00011628 and
ac1 = 0. A native wrapper must not re-enter emulation mid-routine (the
subtree rule), so:

**Plan: test the sign at wrapper entry and fall back to emulation**
(`return entry_address(...)`) for x < 0, handling only x >= 0 natively.
Cheap, honest, and keeps the error path bit-identical by construction.

Caveat to respect: SessionPlan records that arity/entry fallbacks are
divergence-safe only while the fallen-back body's callees are
untranslated. .LIERR and its subtree are untranslated today, so this is
safe now; revisit if the condition system's error entries go native.

The sign test in the wrapper must use **the routine's own test**, not
`x < 0.0` on the double: the machine test is bit 15 of `FRH(fpac0)`,
i.e. of `double_to_eclipse_wide_float`'s top word. For IEEE doubles the
two agree on every value the game can produce, but deriving it from the
emulator's own conversion keeps the equivalence structural rather than
argued. Zero must route to the native path (it returns 0.0 without
touching fpac1/fpac3), not to the fallback.

## Implementation plan

1. **Extend the lockstep pair comparison to FP state** (see below) —
   this must land *first*, or SQR31?3's validation is vacuous.
2. `runtime/sqr31.{hpp,cpp}`: `rt::sqr31_1(...)` operating on the
   machine's FP state through the same `EagleFloat`/`EagleInstruction`
   helpers, structured as a literal transcription of the instruction
   sequence (one C++ statement per instruction, addresses in comments),
   plus `emu_rt::sqr31_3` as the RTBridge wrapper (argc=0, entry sign
   test + fallback, `emulate_frame`, `native_footprint`, `native_return`).
   Reusing the emulator's own conversion helpers makes bit-exactness
   structural rather than something to argue about.
3. Register in `hw/RTStubs.cpp` translation_table.
4. Captures: `QUEST_CAPTURE=7015BD20`, footprint diff to zero words
   (extend Capture to snapshot FP state — the stack region alone proves
   almost nothing here).
5. Scripted smoke session, then a user playtest. At 319K–482K calls per
   session, validation volume is not a concern; a single move that
   changes distance exercises it thousands of times.

## Pair-comparison gap (must fix before this translation)

`Lockstep::compare_pair` compares `ac[0..3]` and carry only. SQR31?3
returns in FPAC0 and clobbers FPAC1/FPAC3/fplr — **a wrong native result
would pass the pair check silently** and only surface later as a
divergence in whatever the game does with the distance (or not at all).

Proposed change:

- `compare_pair`: add `fpac[0..3]`, `fplr` (compared by **bit pattern**,
  not `==`, so −0.0/NaN can't compare equal), `quads[0..3]`, and `fpr`.
- `describe()`: print the FP state on divergence.
- Land it and run a regression session on the **existing** translations
  first. If a pre-existing benign FP divergence exists somewhere, better
  to find it now, with a known-good baseline, than to have it fire
  mid-SQR31-validation and be misattributed.

`Capture` should gain the same FP fields for the same reason.
