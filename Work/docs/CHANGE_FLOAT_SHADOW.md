# CHANGE: Float load raw-bit shadow (FLOAT_SHADOW)

Apply-ready change record for a session that does **not** have this in
its tree. One file, four lines of behaviour plus a helper and a comment
block.

- **File:** `c_src/hw/EagleFloat.cpp`
- **Date:** Aug 2026
- **Fixes:** `Exception: Floating point underflow` on the `?LIB_ERROR` /
  `?FATAL` path
- **Risk:** low, but it changes a hot instruction — see Validation

## Check whether you already have it

```
grep -c shadow_from_single c_src/hw/EagleFloat.cpp     # 3 if applied, 0 if not
grep -n "quads\[YY\]=quad;" c_src/hw/EagleFloat.cpp    # 3 lines if applied, 1 if not
```

The single pre-existing `quads[YY]=quad;` is in the `FSCAL` block near
line 97 — that one is not part of this change. If you get 3 and 3, stop:
it is already applied, and re-applying will not compile cleanly.

## The bug

`fpac[]` is a C++ `double`, so the eclipse representation is
reconstructed on store. Two ways that loses:

1. an eclipse **wide** float carries a 56-bit mantissa against
   `double`'s 53;
2. a bit pattern that is not a valid float at all can reconstruct to an
   out-of-range exponent, and `validate_exponent` throws.

The `quads[]` shadow exists precisely to carry exact bits past the
`double` representation — `XFSTD`/`XFSTS` check `quads[YY]` before
falling back to conversion, and `FMOV` copies it — but **no load ever
set it**. Every load did `quads[YY]=0`, so a pure load/store round trip
went through conversion and could not be bit-exact.

`C?TRIM` (0x7017FB81) uses `XFLDD`/`XFSTD` as a four-word block move for
a string descriptor — a pointer and a length, written as integers a few
instructions earlier:

```
7017fb98 XNSTA 0,[ac3+0xE]      ; integer into a local
7017fb9a XWSTA 2,[ac3+0x16]
...
7017fba2 XFLDD 0,[ac3+0xE]      ; "load a double"
7017fba4 XFSTD 0,@[ac3+0xFFFC]  ; "store a double"
7017fba6 XFLDD 0,[ac3+0x12]
7017fbaa XFSTD 0,[ac2+0x4]
```

Reinterpreted as an eclipse double those bits underflow, so the
emulator threw inside `?FATAL` — killing the process while it was trying
to report an unrelated error.

**Why the fix is right and not a workaround:** a pure load/store round
trip must be lossless on real hardware, or this idiom could not work on
the DG either. The game's own reliance on it is the evidence.

## The change

Populate the shadow on load. The invariant the rest of the file already
implies then holds: raw bits survive until arithmetic touches the
accumulator, and every arithmetic case already clears `quads[YY]`.

### 1. Helper, immediately after `namespace hw {`

```cpp
static inline int64_t shadow_from_single(int32_t src) {
  return static_cast<int64_t>(static_cast<uint64_t>(
           static_cast<uint32_t>(src)) << 32);
}
```

Single-precision loads put the value in the HIGH half, matching how
`XFSTS`/`LFSTS` read it back (`quads[YY] >> 32`).

### 2. Four load cases

| Case | Old | New |
|---|---|---|
| `XFLDS` | `machine.quads[YY]=0;` | `machine.quads[YY]=shadow_from_single(src);` |
| `LFLDS` | `machine.quads[YY]=0;` | `machine.quads[YY]=shadow_from_single(src);` |
| `XFLDD` | `machine.quads[YY]=0;` | `machine.quads[YY]=quad;` |
| `LFLDD` | `machine.quads[YY]=0;` | `machine.quads[YY]=quad;` |

Leave every other `quads[YY]=0` alone — those are arithmetic cases
(`WFLAD`, `WFFAD`, `FRDS`, `FHLV`, `FINT`, `FRH`, `FEXP`, the add/sub/
mul/div family) and clearing is correct there.

`quads[YY]==0` doubles as the "no shadow" sentinel. A genuinely zero
value converts to zero by either path, so the collision is harmless.

## Blast radius (measured, not assumed)

Wide float load/store in the **entire program** — 9 sites, all in the
runtime, none in game code:

| Routine | Instructions | Nature |
|---|---|---|
| `C?TRIM` | `XFLDD` x2, `XFSTD` x2 | integer bits — block move |
| `?RANDOM_NUMBER` | `LFLDD`, `XFSTD` x2 | genuine doubles (LCG) |
| `?UMUL32` | `LFSTD` | 64-bit product |

Game code contains exactly **one** FP memory reference (a single
`XFLDS`); all its float arithmetic works on FPACs directly, from
`SQR31?3` and friends.

`?RANDOM_NUMBER` is unaffected in the right way: `LFLDD 1,[const]` then
`XFSTD 1,[ac3+6]` with no arithmetic between is a copy, and preserving
bits reproduces the source exactly. Where arithmetic does intervene it
clears the shadow and the old conversion path runs.

## Validation performed

1. **Regression, normal play** — lockstep, full login + character
   creation + 12 `L`->`P` iterations: **zero divergences**, clean
   shutdown, only the three known-benign exceptions (`Segment fault -
   block 0, page 1 not loaded` from QUEST_SERVER startup, `INTWT
   interrupted` / `EXIT!` at shutdown). RT coverage unchanged at
   1234/1050 words.
2. **The failing scenario** — `QUEST_FAIL_OPEN=USER_DATA_FILE`, `L` ->
   `P`:
   - before: died at `?FATAL+0x16B`, `Floating point underflow` in `C?TRIM`
   - after: reaches `?FATAL+0x1E2`, 119 words deeper
   - **the underflow is gone** (`grep -c "Floating point underflow"` = 0)

## What it exposed next

`?FATAL` now advances and stops on `Exception: Unimplemented system call
0251`, at 0x7017F3F9 inside `?FATAL`. It sets up a 256-byte buffer
(`NLDAI 256` -> `[ac3+0x31]`, address at `[ac3+0x2E]`) with a
skip-return on error — very likely a name or text lookup for the
diagnostic report. Never reached before, hence never implemented.

That is the next wall on the way to a real PL/1 traceback. It is
**not** a regression from this change.

## Related note worth keeping

An eclipse wide float carries a 56-bit mantissa; a C++ `double` carries
53. `?RANDOM_NUMBER` is a linear congruential generator computed in
floating point (`WFLAD` seed -> `LFMMD` multiply -> `LFAMD` add ->
`D.MOD`), so its low bits depend on 56-bit rounding the emulator does in
53. The emulator's random sequence therefore probably differs from real
1986 hardware — self-consistently, so lockstep can never see it.

Consequence for the upcoming `?RANDOM_NUMBER` translation: "bit-exact"
will mean bit-exact **with the emulator**, not with the DG. Write that
down before someone assumes otherwise.

## Unified diff

```diff
--- a/c_src/hw/EagleFloat.cpp
+++ b/c_src/hw/EagleFloat.cpp
@@ namespace hw {
+static inline int64_t shadow_from_single(int32_t src) {
+  return static_cast<int64_t>(static_cast<uint64_t>(
+           static_cast<uint32_t>(src)) << 32);
+}
+
 void EagleFloat::setup(...)

@@ case XFLDS:
     machine.fpac[YY]=eclipse_float_to_double(machine, src);
-    machine.quads[YY]=0;
+    machine.quads[YY]=shadow_from_single(src);

@@ case LFLDS:
     machine.fpac[YY]=eclipse_float_to_double(machine, src);
-    machine.quads[YY]=0;
+    machine.quads[YY]=shadow_from_single(src);

@@ case XFLDD:
     machine.fpac[YY]=eclipse_wide_float_to_double(machine, quad);
-    machine.quads[YY]=0;
+    machine.quads[YY]=quad;

@@ case LFLDD:
     machine.fpac[YY]=eclipse_wide_float_to_double(machine, quad);
-    machine.quads[YY]=0;
+    machine.quads[YY]=quad;
```

(The full comment block belongs above `shadow_from_single`; it is in the
tree copy of `EagleFloat.cpp` and reproduced in the "The bug" section
above.)

## After applying

```
make                                     # expect warning-free
grep -c shadow_from_single hw/EagleFloat.cpp   # 3
```

Then a lockstep session and confirm zero divergences before moving on.
