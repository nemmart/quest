# Project 36 — extend the translator to DIED: bit fields, ac3-repurposing, record strings, twins, game→game calls

GOAL: grow the C subset and the DG-compiler model (P35's
`compiler/translate.py` + `compiler/CODEGEN_RULES.md`) until **DIED
(0x7016603D, 495 statements) translates to its ir 6 statement-for-
statement**, register-exact, on both the primary and `--folded`
comparison — the same bar P35 hit on three routines. DIED is the
stretch routine chosen in P35 because it packs every construct the first
three lacked: PL/I bit-field tests, ac3 repurposed as an address
register, string statements into record fields, WMSP arena twins, and
decorated game→game calls. Reaching it falsifies (or promotes) the
compiler rules P35 left at low confidence, and produces the second
proven source in `game/routines/`.

Hi Claude! Read docs/METHOD.md first. This continues P35 — read its
whole output as the foundation: **compiler/CODEGEN_RULES.md** (the 25
numbered rules with confidence A/B/C — you extend this, and you FALSIFY
or promote the C-confidence ones), **compiler/translate.py** and
**compiler/ircmp.py** (the translator and the comparator — you extend
translate.py; ircmp.py's seven equivalences are FIXED unless a new
construct needs an eighth, which is a ruling), **docs/Project35/REPORT.md**
(the result and DIED's 495-statement census with the staged order),
game/routines/{PICK_X_Y,UPDATE_SCREENS,REFRESH_SCREEN}.c (the models to
imitate), game/quest_rt.h + game/declarations.h + compiler/
gen_declarations.py (the header and the generated world layout — you
add DIED's record fields and any new intrinsics). Also:
docs/IR.md (ir 6 — the target grammar), docs/Project29/StringsDesign.md
§2–§6 (the twin/claim statements: `t@block.k`, `claim`, `release`, the
located string statements DIED emits into record fields),
docs/Project28/RTConventions.md (the two rt_calls and the six game→game
`call` conventions), docs/GAME_REFERENCE.md + the P34 record census
(the player/object record layouts DIED writes). TREE VINTAGE: main after
P35 (state the commit); verify docs/Provenance.md; nothing here touches
emulation/ or Disassembled/ or runs a battery.

## Carried-in rulings from the Sep 8 session (apply from the start)

1. **Conversions are explicit intrinsics; no silent narrowing.** The C
   subset gains `cvwn(e)`, `sx16(e)`, `trunc16(e)` as header intrinsics
   that map 1:1 to the IR ops. A 32→16 assignment (`int16 = int32`) is
   a REFUSAL unless the source wrote the conversion — the P35 routines'
   implicit `*x = RANDOM_NUMBER$3(...)` narrowing is rewritten
   `*x = cvwn(RANDOM_NUMBER$3(...))`, and CODEGEN_RULES records that
   `RANDOM_NUMBER` returns 32-bit in ac0, a 16-bit destination inserts a
   checked CVWN, a 32-bit destination does not. Re-run the three P35
   routines after this change — they must STILL be 197/197 (the
   explicit `cvwn` produces the same IR the implicit cast did; if it
   emits a second cvwn, the type-driven auto-narrow must be suppressed
   when the value is already a `cvwn()` result).
2. **R10 (element-address temp consumed by the second later reference,
   confidence C) is to be falsified against DIED**, which has 13
   ac3-repurposing sites — enough instances to separate it from the
   competing readings. Promote to A, correct it, or replace it, and say
   which with the instruction evidence.
3. **No world-coordinate offset.** The game works entirely in the raw
   0x3Bxx space; there is no origin subtraction anywhere (the render's
   pixel coordinates were a viewing transform, not the program's). Do
   not add a coordinate offset to the header or the readable layer.

## The five new constructs (P35 REPORT's DIED census; staged)

Work them in this order; each is translated/compared/fixed on the
statements it governs before the next, so a partial DIED still leaves a
growing match percentage and a clear stopping point.

1. **Bit-field tests (51 statements)** — PL/I bit references (bit
   address `16*word + n`, the WSZB/WBTZ/WBTO family). Design the C form
   (a `BIT(word, n)` intrinsic? a `bitfield` struct member?) and its ir
   6 mapping; the emulator semantics are the truth (cite EagleCompute /
   the manual — a P36 finding if they differ). This is the construct
   that made DIED's first statement refuse, so it is first.
2. **ac3 repurposed as an address register (13 statements)** — the
   compiler drops `ac3 = fp` and uses ac3 as a record base mid-routine,
   restoring it with `LDAFP 3` later. The frame-naming and register
   rules assume ac3 = fp; extend them, and this is where R10 is decided.
3. **String statements into record fields (12)** — located-string
   assignments/appends whose destination is `[@record + K, n varying]`
   rather than a frame local; P30–P32's statements, new only in the
   address form. declarations.h gains the record's string fields.
4. **Arena twins / claim groups (20)** — DIED's two WMSP groups (STASP
   at 701661AA and 70166215): `t@block.k`, `claim`, `release` per
   StringsDesign §5.2/§6, with the comparator's twin equivalence (#3).
   The C form is the string-building the group performs; the translator
   emits the claim/release around it.
5. **Game→game decorated calls (6)** — `call <tgt> args=n marker=…
   site=… ret=…` with the book-mode arg-slot stores, to UPDATE_SCREENS,
   HIT_ANY_CHAR, REPOSITION, REFRESH_SCREEN, DISPLAY_SCREEN,
   DISPLAY_INVENTORY. The C is `NAME$N(&a, …)`; the arg slots come from
   quest.addrbook (static addresses, must match).

## Part 1 — plan gate

1. **Re-baseline**: the three P35 routines still 197/197 after the
   explicit-conversion change (ruling 1). State it.
2. **The five C forms**: for each construct, the exact C syntax, the ir
   6 production(s) it maps to, the CODEGEN_RULES additions (numbered,
   with confidence and the motivating instruction pair), and what the
   translator REFUSES. Bring the bit-field spelling and any new
   comparator equivalence to the gate as ruling requests.
3. **DIED's C**: the routine written in the extended subset (or as far
   as the plan is confident), with the frame/twin layout predicted.
4. **Falsification plan for R10** (ruling 2) and any other C-confidence
   rule DIED touches.
5. **Success criteria**: DIED ≥ 90% MATCH+RENAME primary (target 100%);
   every DIFF classified; the `unknown` count 0. Native C99+C++17 clean.

STOP AND REPORT at the plan gate.

## Part 2 — build

Extend quest_rt.h (the intrinsics, the bit-field form), gen_declarations
(DIED's record fields), translate.py (the five constructs, the rule
changes), DIED.c; iterate translate→compare→fix, recording every rule
change in CODEGEN_RULES.md with its evidence. A DIFF that is the
compiler doing something no rule predicts is a FINDING with the
instruction pair and a proposed rule.

## Part 3 — report

Per construct: the C, the match delta it bought, the rules added/
changed. The final DIED match table (primary and folded). The R10
verdict. The findings list (what still can't be produced from C without
a rule we don't have — the frontier for P37). An assessment: is the
subset now general enough to try a routine nobody hand-picked (a
random mid-size routine, as the real test of the model), and what P37
should be.

## Boundaries — BINDING

1. Files: compiler/, game/, docs/Project36/. Nothing in emulation/,
   Disassembled/, no artifact, no battery.
2. The translator emits ir 6 exactly (docs/IR.md); it never invents a
   production. ircmp.py's equivalences stay the seven of P35 unless a
   construct forces an eighth — that is a ruling, stated with why.
3. Refuse-don't-guess in the translator; classify-don't-hide in the
   comparator; every rule change carries its motivating instruction
   pair and a confidence.
4. The three P35 routines must remain 197/197 throughout (a regression
   there is STOP-and-report).
5. Deliverables: the extended translate.py / quest_rt.h / declarations /
   gen_declarations, DIED.c, CODEGEN_RULES.md (updated), docs/Project36/
   {REPORT,REPORT_worklog}.md with the match tables, the R10 verdict and
   the findings, tool runtimes, TREE VINTAGE. **Deliver a Work.tgz**
   (the P35 session crashed sending a 5-file zip; send the whole tree so
   nothing is lost), or push a branch if given the clone URL.
