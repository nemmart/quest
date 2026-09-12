# Project 53 — REPORT: THE COMPILER AGAINST ir 8

Worker session, Sep 12 2026, branch `p53-compiler`. Gate `q001-plan-gate.md`;
rulings `a001` (Q-1a, Q-2a), `a002` (Q-3a), `a003` (multi-routine unit),
`a004` (q004 accepted, q005a). Follow-ups `q002`–`q006`.

**Outcome — the prompt's three criteria.**

| criterion | required | actual |
|---|---|---|
| the seven compile to ir 8 and LOAD | all seven, or named findings | **7/7 load in their compilation units; 5/7 load ALONE** — the two that need a unit are exactly the two that make a game→game call (§2) |
| the differential corpus GREEN | clear P52's expected-red gate | **`LOWERC DIFFTEST: GREEN`**, gate removed, 4 classes, 8/8 mutations caught (§3) |
| byte types in the generator BEFORE the compiler | ordering | **yes** — class D existed and ran natively at Stage A; the compiler gained `u8` at Stage C (§4) |

Nothing here read `quest.ir2.book` or `docs/attic/`. Nothing executes: §2's bar
is *loads*, and the one thing that runs is UPDATE_SCREENS, which ran in P48.

---

## 1. The native view — fixed, and the count corrected

**P52 q002 §3's "ten of fifteen" does not reproduce. It is seven.** The table
was right; the count was not. `g++ -fsyntax-only -Igame` over
`game/routines/*.c` fails on FAKE_LAND_MASS, FAKE_OCEAN, GET_INPUT,
HIT_ANY_CHAR, INIT_SCREEN, RETURN_MESSAGE, UPDATE_SCREENS. (`gcc` in C mode
gives 2; dropping `-I` gives 15. Neither is ten.) Recorded because the number
had reached a PROMPT, and a number nobody can reproduce becomes folklore.

**Now 15/15.** `quest_rt.h` gained MIN/MAX across all three views, the five
missing game prototypes (P51 §2 item 11), `DATA()`, `RETURN$2` (Salvage F11:
register arguments, so values not addresses), and a `tmp_arg` that holds a
pointer with an explicit `operator void*()` — that last is what made `TMP(buf)`
unambiguous. `UPDATE_SCREENS.c` got its missing include; `RETURN_MESSAGE.c`'s
`text` became `const`.

**Was `quest_rt.h` enough? No — two files needed a line each**, and the report
should say so rather than let "a header fix" stand.

**The proof, in P52's style, because the point is that the two VIEWS have not
diverged rather than that one compiles:** 14/15 translator-view function bodies
are byte-identical before and after; the 15th differs only by the intended
`const`. The added file-scope declarations are exactly the nine intended, plus
`declarations.h`'s twenty for UPDATE_SCREENS alone — which is precisely the
missing include.

**F1 — the one-line include broke the compiler.** The prompt called it "P53's
first five minutes". The moment UPDATE_SCREENS pulled in `declarations.h`,
`collect_file_scope` tried to allocate a 0x76 cell for `PLAYER`, `REGION` and
`SD_PTR`, and refused. **That path had never been exercised by a real
routine** — only by generated programs, which include `quest_rt.h` alone, and
UPDATE_SCREENS was the one file P51 did not write. Fixed by skipping `extern`
declarations and bare struct type definitions, which is also the right answer:
`extern` storage lives in the game image, whose geometry the compiler already
reads from the JSON. Skipping it does not make unknown names acceptable — they
still refuse at the USE site, where the message names the field.

---

## 2. The seven — both numbers, alone first

`a003` made this a requirement and it is the finding, not a caveat: *"seven of
seven load" would be true and would conceal that two cannot load as units.*

| routine | ALONE | in its unit | why |
|---|---|---|---|
| UPDATE_SCREENS | **LOADS** | (same) | no call, no byte, no string |
| PICK_X_Y | **LOADS** | (same) | 3 valued `rt_call`s, 6 TMP dummies, `&SD_PTR->seed` |
| FAKE_OCEAN | **LOADS** | (same) | MIN/MAX |
| FAKE_LAND_MASS | **LOADS** | (same) | MIN/MAX + the LANDMASS table (§5) |
| GET_INPUT | **LOADS** | (same) | bytes, `?READ`×6, a pointer-valued TMP, `BITS` via X.CB |
| INIT_SCREEN | **refuses** | **LOADS** | calls FAKE_LAND_MASS, FAKE_OCEAN |
| HIT_ANY_CHAR | **refuses** | **LOADS** | calls GET_INPUT |

**5/7 alone, 7/7 in units.** The refusal names the callee and prints the
`--routine` line that fixes it. The two are exactly the two that make a
game→game call, because a naive `call` enters the callee's `b0` and the loader
requires that block to be in the same file (P54 a001 R1 — §7 F2).

Against the gate's predictions: **6 of 7 scored**. I predicted INIT_SCREEN and
HIT_ANY_CHAR at "low risk" and they refuse alone, for a rule that did not exist
when I wrote the gate; I predicted FAKE_LAND_MASS would refuse on the
`declarations.json` boundary and `a002` removed that. GET_INPUT, which I named
as the one I would bet against myself on, landed without drama once `*u8` was
spelled `*char`.

**A note on what "loads" bought.** At the moment I reported it, a naive
game→game `call` loaded and then segfaulted (§7 F3). P54's reopening fixed
that, and the two unit files now also RUN through the rig. That was not my bar
and I did not chase it — but "loads" and "runs" had come apart on exactly the
construct I was emitting, and the report would have been misleading without
saying so.

---

## 3. The differential corpus

`emulation/tests/run_lowerc_difftest.sh 25` — one command, ungated:

```
9 hand cases, 0 bad
100/100 AGREE over classes A/B/C/D
t-places emitted: 0     effectful ops emitted: 0
8/8 mutations caught
UPDATE_SCREENS 594/594, 594/594, 198/198, 0 failed; trap leg fires;
  sign-extension teeth still bite
quest.assumptions: 3 rows, 3 legs, GREEN (12 .ir scanned)
LOWERC DIFFTEST: GREEN
```

**The gate is removed, not bypassed.** Both causes P52 §4 item 5 named are
gone: the emitter is ir 8, and the six `SUB(` tokens in
`cases/{sub_traps,shortcircuit}.c` are renamed (reverse substitution reproduces
all four originals exactly).

### 3.1 The construct census — 58 constructs, no zeroes

The thinnest rows over the 100-program run, which is where the claim is
weakest and therefore what belongs in the report:

| construct | programs (of 100) |
|---|---|
| `stmt.assign_byte_array` | **7** |
| `narrow.byte_explicit` / `narrow.byte_implicit` | **8** / **8** |
| `leaf.byte_read` | **10** |
| `cast.unsigned_char` | 14 |
| `stmt.break` | 15 |
| `index.sub_unguarded` | 16 |
| `leaf.byte_var` | 18 |
| `index.sub_inrange` | 21 |
| `stmt.continue` | 21 |

Everything else is above 35. **Every thin row is a byte row**, which is
expected — bytes live in class D, a quarter of the corpus — but it is the
honest statement of what this project's new coverage actually is: *byte
constructs are tested at 7–18 programs, not at 100.* The hand case
`byte_index.c` exists because that density is thin, and it pins the boundaries
the generator reaches only by luck.

### 3.2 Mutations — eight, all caught

| mutation | caught (of 24) |
|---|---|
| `no_sign_extend` | 8 |
| `cmp_unsigned` | 7 |
| **`byte_as_word`** (P51 §2 item 5, re-injected) | **4** |
| `u16_unsigned`, **`bp_scaled`** | 2 |
| `eager_bool`, `shift_logical`, `abs_argtype` | 1 |

`no_sign_extend` **moved with the meaning**: in ir 7 it corrupted the read
(`zx16` for `sx16`); in ir 8 there is no read to corrupt — §5.10.4 takes width
and sign from the declaration — so it now declares `u16` where `i16` belongs.
Same bug one level down, same catch rate P48 recorded.

**`bp_scaled` is the one that justifies `a001` Q-2.** It scales a byte
subscript by the element width: the store and the load then agree with each
other while disagreeing with gcc. It is caught only because the rig can render
a byte — the ten lines granted in `lowerc_rig.cpp`. Under the fallback I
offered (bytes drained into word observables) it would have scored **green**.

---

## 4. Bytes

`char`/`unsigned char` map to a new `u8` kind lowering to the IR's `char <n>` —
n BYTES, ceil(n/2) words. GET_INPUT's `unsigned char buf[144]` is **72 words,
not 144**.

Two decisions worth recording:

- **A byte cell is an AGGREGATE.** §5.10.4 has no one-byte value vtype, so even
  a lone byte names a region and every access goes through `bp()`. One extra
  statement per byte scalar access. That is what naive costs, so I took it
  rather than ask for a new vtype.
- **`KINDS["u8"]` carries 0 words on purpose.** A byte is the only kind that is
  half a word, so a forgotten `words * nelem` yields 0 and refuses loudly
  rather than sizing a buffer at 144 words. `byte_words()` is the one place
  that knows the ceil(n/2) rule.

**Generator first, as required.** Class D existed and ran 60 programs through
gcc at Stage A, before `lower_c.py` knew what a byte was. Classes A/B/C keep
P48's exact four kinds — **measured, 120/120 programs byte-identical over 40
seeds** — which is what made "300/300 AGREE" after the migration a genuine
before/after on unchanged inputs rather than a fresh corpus that happens to
pass.

---

## 5. `quest.assumptions`, and a correction to its seed text

At `emulation/quest.assumptions` (a001 Q-1a), checked by
`compiler/check_assumptions.py` — three legs, each with `--break` teeth, wired
into the difftest script so it runs on every corpus run.

**The correction, and it is load-bearing: "sole writer" is true of DIRECT
stores only.** 0x70000210 and 0x70000212 are ALSO passed BY REFERENCE as
argument 2 of `?GET_SHARED_PAGE`, which writes them after the game's own store.
That is the *reason* for the assumption, not a hole in it — but **a checker
that counted only `LWSTA` sites would have been checking the wrong thing while
looking right.** Each row therefore carries both halves: one direct store at
the recorded pc, and exactly the recorded address-taken sites.

Two smaller corrections:

- The prompt's three writer pcs (7015BE43, 7015BEE3, 7015C2CD) are a few
  instructions early; the stores are at **7015be48, 7015bee6, 7015c2d6**.
- **0x70000212's game-side store is provable, not assumed**: it writes
  `LLEF 0,[0x70017C00]`, a constant effective address.
- **0x700007A0 is arithmetic on 0x70000210**, not an independent fact:
  `SD_PTR + 686·n − 642`, where 686 is PLAYER's stride and −642 is
  `declarations.json`'s own "min K seen −642" for that table. It is a cached
  PLAYER element pointer, and it inherits rather than adds.

---

## 6. Every soundness bug the tester found in MY compiler

*"A report with zero there is a report I will suspect the tester for."* Four,
plus one hole in my own checking machinery — which is the one I would want read.

**B1 — the tripwire had a hole in exactly the construct it was added for.**
The emit-time argument-KIND check (built under q005/a004) could not see a
CALLEE's declarations, because the callee is compiled AFTER the caller: at the
moment HIT_ANY_CHAR emits `GET_INPUT.a1 = bp(...)`, `GET_INPUT.a1` has no
declared type and the check silently passed. **Measured by injecting the bug:
`bp`→`wp` compiled clean and produced IR the loader then refused.** A tripwire
with a hole where it is trusted is worse than none. Fixed as a unit post-pass
over the finished text — the same shape P54's loader settled on, for the same
reason. Found by TESTING the tripwire rather than trusting it.

**B2 — `BITS` compared decoded BYTES against a string** (`ch not in "01"` over
a `bytes`), a TypeError on the first real use. Found by compiling GET_INPUT.

**B3 — a byte-pointer parameter was spelled `*u8`.** The IR's pointee forms are
`i16|u16|i32|u32|char|varying n|words n`; `char` is the byte one. GET_INPUT's
`unsigned char *ch` refused at load until it became `*char`.

**B4 — the multi-source refusal path crashed on its own error message**
(`os.path.basename` of a list), so a genuine refusal printed a traceback
instead. Found by B1's injected bug.

**Not found by the corpus, and worth saying:** B2–B4 were all found by
compiling the seven, not by the 100-program corpus, because the corpus has no
calls, no strings and no bits. **The corpus tests the expression and control
lowering; the seven test everything the corpus cannot reach.** Those are
different instruments and neither substitutes for the other.

---

## 7. What did not survive contact

**F1 — the native-view include broke the compiler** (§1).

**F2 — a rule from a parallel project invalidated two rows of my gate.** P54's
a001 R1 (`call <ENTRY>` requires `<ENTRY>.b0` in the same file) landed after my
gate. Measured, not read. `a003` granted the multi-routine unit; the integrator
noted this is "the parallel structure working, not failing — the alternative
was discovering it in P55."

**F3 — I misattributed a crash, and the reproducer caught me.** q003 blamed a
segfault on `rt_call`. Writing the reproducer `a003` asked for, it did not
reproduce; measuring properly showed **the naive game→game `call` crashes 40/40
with no diagnostic, and `rt_call` is clean 40/40**. Corrected in q004 before it
hardened into a defect record on a closed project. METHOD §10 covers a claim
from a code read; this was the adjacent error — **a claim from a single
unreproduced observation carrying the confidence of a measurement.** Shipping
the reproducer looked like bookkeeping and was the check.

**F4 — a check specified twice and commented twice did not exist** (q005). The
argument pointer-KIND check: IR.md §5.1 and §6 mandated it, `IRExec.cpp:887`
and `:1349` asserted it, and a `wp()` into a `*char` cell loaded clean. Kind
was enforced where a pointer is USED and unenforced where one is PASSED — and
P52's suite had a leg for the first and none for the second. Now built by P54.

**F5 — IR.md's own worked example did not load** (q006). §5.10.4 and §5.10.10
both spelled a varying destination as a bare cell name; `wp(cell, 0)` is
required. P52 §7 F3 is the cause — `check_piece` was reworked and the examples
were not updated with it. §5.10.10 is labelled "the self-test's shape", which
is exactly what makes it the thing a new reader copies; I copied it. Fixed, and
P54 added a leg that loads the spec's examples verbatim.

**F6 — F4 and F5 are the same shape, and it is worth naming.** A normative
document and its implementation disagreed, twice, in ways nothing detected
because **nothing ran the document**. The self-test ran the loader's rules; it
did not run the spec's illustrations of them. The leg P54 added is the general
fix.

**F7 — the valued-call split never fired as a refusal.** A call cannot sit
inside a larger expression (§5.10.6), and none of the seven nests one, so the
refusal is implemented and untested by anything but its own existence. Recorded
so the refusal list is honest about what was exercised.

**F8 — ir 8 expressed everything P51's C says.** P52 read all seven and found
nothing inexpressible; compiling them is the stronger test and agrees. The
costs are in spelling, not capability: `BITS("001")` is two calls and three
blocks for one C argument, and `TMP` widths are prose in RTConventions rather
than data — which is why they live in the compiler as a table that **refuses
rather than guesses** when a row is missing (ruling 7 applied to a table).

---

## 8. Sizing: estimate vs actual

| item | gate estimate | actual |
|---|---|---|
| `lower_c.py` | +180/−120 (B) +120/−30 (C) +260 (D) ≈ +560 | **+883 / −153** |
| `cgen.py` | +60/−15 | **+40 / −17** |
| `quest_rt.h` | +35/−6 | **+41 / −1** |
| `check_assumptions.py` + data | ~90 | **162 + 51** |
| `gen_declarations.py` (Q-3) | not estimated | **+29** |
| `lowerc_rig.cpp` / `run_lowerc_difftest.sh` | ~10 | **+16 / +43** |
| **total** | **≈ 900 ± 250** | **≈ 1,270** |

**Over the band, by about 40%.** What I under-counted: the multi-routine unit
(not in the estimate at all — it did not exist until `a003`), the argument-KIND
post-pass (q005, likewise), and the call machinery, which I sized as one line
per construct and which is closer to one *block pair* per construct once a call
is a terminator.

---

## 9. Files

Branch `p53-compiler`. Written: `compiler/{lower_c,gen_declarations,
check_assumptions}.py`, `compiler/difftest/{cgen.py,difftest cases}`,
`compiler/difftest/cases/byte_index.{c,obs}`, `game/quest_rt.h`,
`game/routines/{UPDATE_SCREENS,RETURN_MESSAGE,FAKE_LAND_MASS}.c`,
`game/declarations.{json,h}` (generated), `emulation/quest.assumptions`,
`emulation/tests/{lowerc_rig.cpp,run_lowerc_difftest.sh}` (the two granted),
`docs/Project53/*`.

Not touched: `quest.ir2.*`, `quest.addrbook`, `quest.arena`, `Disassembled/**`,
`docs/IR.md`, `docs/Salvage.md`, `docs/Project44/DESIGN.md`, `tasks/`, `bin/`,
the rest of `emulation/`. No artifact regenerated.

**To re-run everything:** `emulation/tests/run_lowerc_difftest.sh [seeds]`.
For the seven: `compiler/lower_c.py <sources> --routine A --entry A [...]`.

## 10. For whoever is next

1. **`stmt.assign_byte_array` at 7 programs of 100** is the thinnest real
   coverage in the census (§3.1). Bytes are a quarter of the corpus; if byte
   work continues, class D should be more than a quarter.
2. **The corpus has no calls, no strings, no bits.** Every bug in §6 except B1
   came from compiling the seven. A generator that emits calls is the next
   instrument, and the valued-call split (F7) is untested by anything else.
3. **FAKE_LAND_MASS's field names are still `claimed`** (P51 §1). Moving the
   geometry into `declarations.json` did not change that, and the JSON comment
   says so. CREATE_MAP's writer (701652xx/701653xx) would settle it.
4. **P48's rig builds one frame and an empty shadow call stack.** It now
   survives a naive call (P54's fix), but it was written for call-free programs
   and everything about executing calls through it is new ground.
