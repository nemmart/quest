# Project 35 — Recompilation as decompilation: REPORT

Date: Tuesday, September 8, 2026.
TREE VINTAGE: main, commit 35859fc (the P35 prompt commit on the 625d968 layout).
Inputs (unmodified, sha256 prefix): emulation/quest.ir2.book e2f18f144c195da4,
emulation/quest.addrbook e6fde2c246630e0e (both as in docs/Provenance.md);
Disassembled/quest.mem read only to resolve literal addresses.
Plan gate: docs/Project35/PLAN.md (rulings R1–R5 in §9). Worklog: REPORT_worklog.md.

## 1. Result in one paragraph

A restricted C subset (`game/quest_rt.h`, `game/declarations.h`) and a
translator that models the DG PL/I code generator with 25 numbered rules
(`compiler/CODEGEN_RULES.md`) reproduce the IR of record **statement for
statement** for three game routines: **PICK_X_Y 64/64, UPDATE_SCREENS 72/72,
REFRESH_SCREEN 61/61 — 197/197 MATCH**, register-exact, with identity frame
slot bijections, on both the primary comparison and the register-folded
one.  The C sources compile natively in both C99 and C++17 with
`-Wall -Wextra -Werror`.  The fourth routine, DIED (495 statements), is the
staged stretch: its opening is written in the reconstructed shape and the
translator refuses at the routine's first statement — a bit-field test —
which is where the present subset ends.  The regularity the prompt asked
about therefore held completely for arithmetic, subscripts, DO loops,
IF forms, runtime calls with dummy arguments, string temporaries and
literal assignment; it has not yet been tested on bit fields, ac3
repurposing, record string fields, the arena twins, or game→game calls.

Success criteria from PLAN §7 (stated in advance): PICK_X_Y ≥ 90 % primary
/ ≥ 95 % folded, UPDATE_SCREENS ≥ 85 %, REFRESH_SCREEN ≥ 80 %, DIED "report
whatever it reaches".  All three thresholds are exceeded at 100 %.

## 2. Deliverables

| file | what | size |
|---|---|---|
| compiler/translate.py | C subset → ir 6 (pycparser front end; codegen model; lower.py spellings; Refuse discipline) | 1537 lines |
| compiler/ircmp.py | the comparator (seven equivalences, MATCH/RENAME/DIFF, `--folded`, `--selftest`) | 613 |
| compiler/gen_declarations.py | generates game/declarations.h (C + C++ views) and declarations.json | 156 |
| compiler/CODEGEN_RULES.md | the 25 rules with motivating instruction pairs and confidence | — |
| compiler/README.md, game/README.md | updated | — |
| game/quest_rt.h | the runtime header filled in (TMP, SUB, ABS, CAT, BIT, VARYING, ARRAY1, `$N` prototypes, string primitives, callees) | 111 |
| game/routines/{PICK_X_Y,UPDATE_SCREENS,REFRESH_SCREEN}.c | proven sources | 26 / 17 / 21 |
| game/routines/DIED.c | staged opening (refused at line 18 by design) | 23 |
| docs/Project35/results/*.ir, *.cmp.txt, *.cmp.folded.txt, DIED.refusal.txt | translator outputs and comparator reports | — |
| docs/Project35/PLAN.md §9 | the rulings as received | — |

Boundaries respected: only compiler/, game/ and docs/Project35/ written;
nothing in emulation/ or Disassembled/ modified.

Commands (from the tree root):

    python3 compiler/gen_declarations.py
    python3 compiler/translate.py game/routines/PICK_X_Y.c --routine PICK_X_Y -o out.ir
    python3 compiler/ircmp.py emulation/quest.ir2.book out.ir --routine PICK_X_Y [--folded]
    python3 compiler/ircmp.py --selftest

## 3. Method (what "MATCH" means here)

ircmp.py compares blocks by canonical position (DFS from the entry,
successors in terminator order; the entry block's WSAVS fall-through
included) and statements within a block by sequence alignment.  A
statement is **MATCH** when identical after equivalences 1, 3, 4, 5 with the
frame offsets *literally equal*; **RENAME** when identical only after the
per-routine slot bijection (equivalence 2); **DIFF** otherwise, classed
reg / slot / border / expr / const-spelling / missing / extra / unknown with
both texts printed.  Registers (equivalence 7) and static addresses (6) are
compared verbatim in the primary comparison; `--folded` applies readable.py's
in-block register folding (`fold_block`, imported, not copied) to both
sides first.  Two rulings extended the equivalences: the `@pc` of a DERR
message is provenance (`DERR nn`, R1), and a literal piece compares by its
byte length, because the book renders data-segment literals without text.

The self-test guards the comparator against fooling itself: on each of the
four routines, book-vs-book is 100 % MATCH; adding 100 to every frame
offset turns exactly the slot-bearing statements into RENAME with zero
DIFF; swapping one register yields exactly one `DIFF(reg)`; the folded
identity is 100 %.  The PICK_X_Y result was additionally checked by a
direct textual diff of the two files in program order (labels and sites
masked): identical.

The translator refuses, by name, every construct outside the subset (exit
code 2) — it never guesses.  Register choice is not a refusal but a rule
the comparator can falsify (ruling R3); that is how the rules were found:
translate, read the DIFF(reg)/DIFF(slot) list, state the belief that
explains the instruction pair, re-run all three routines.

## 4. The routines

### 4.1 PICK_X_Y @701761E7 (argc 2, frame 0x05; 73 instructions, 18 blocks, 64 statements) — 64/64 MATCH

```c
void PICK_X_Y(int16_t *x, int16_t *y)
{
    int32_t r;
retry:
    r = RANDOM_NUMBER$3(TMP(1), TMP(OBJ_PTR->region_count), &SD_PTR->seed);
    if (REGION[SUB(r, 100000)].x == 0) goto retry;
    if (REGION[r].type / 100 + 1 != 3) goto retry;
    *x = RANDOM_NUMBER$3(TMP(REGION[r].x - 20), TMP(REGION[r].x + 20), &SD_PTR->seed);
    *y = RANDOM_NUMBER$3(TMP(REGION[r].y - 20), TMP(REGION[r].y + 20), &SD_PTR->seed);
    if (*x <= 15349) goto retry;
    if (*x > 16300) goto retry;
    if (*y <= 15219) goto retry;
    if (*y <= 16350) return;
    goto retry;
}
```

Slot bijection: identity (w2→2, w4→4, w6→6, w8→8).  Folded: 41/41.
**Correction recorded** (METHOD §11): the type test *retries unless the
region's type is 3* (`goto [retry, cont] (ac0 == 3)`); the P34 hand reading
(PICK_X_Y_reading.md) had the polarity reversed.  The final `goto retry` is
the long-jump stub (R19) that the three preceding WBR-unreachable retries
branch through — the DFS order absorbs the layout difference.

### 4.2 UPDATE_SCREENS @7017D635 (argc 3, frame 0x05; 70 instructions, 18 blocks, 72 statements) — 72/72 MATCH

```c
void UPDATE_SCREENS(const int16_t *x, const int16_t *y, const int32_t *cell)
{
    int16_t i;
    int16_t n;
    n = SD_PTR->player_count;
    for (i = 1; i <= n; i++) {
        if (ABS(PLAYER[SUB(i, 10)].fm589 - *x) > 4) continue;
        if (ABS(PLAYER[SUB(i, 10)].fm588 - *y) > 5) continue;
        PLAYER[i].screen[SUB(*x - (PLAYER[i].fm589 - 5), 9)][SUB(*y - (PLAYER[i].fm588 - 6), 11)] = *cell;
    }
}
```

Slot bijection: identity (2, 4, 6, 8).  Folded: 44/44.  Four DERR folds,
the XNDO loop with its `ret`-folded exit path, the ABS diamonds, and the
2-D store with its partial-sum temp all reproduce.  The last statement was
the hardest: it needed R7b (the compiler keeps `*y` in ac1 because the same
statement reads it again, so `*x` is forced into ac2, which clobbers the
element address, which is then reloaded from the R10 temp).

### 4.3 REFRESH_SCREEN @70176A93 (argc mixed 0/1, frame 0x17; 96 instructions, 14 blocks, 61 statements) — 61/61 MATCH

```c
void REFRESH_SCREEN(int arg_count, const int16_t *flag)
{
    int16_t i;
    if (arg_count == 0) {
        WRITE_SCREEN$2(&OUT_CHAN, TMP(0x00010C00));
    } else if (*flag < 0) {
        WRITE_SCREEN$2(&OUT_CHAN, TMP(0x00010C00));
    }
    WRITE_SCREEN$5(&OUT_CHAN, "______________________", TMP(0), TMP(2), TMP(2048));
    WRITE_SCREEN$5(&OUT_CHAN, "----------------------", TMP(10), TMP(2), TMP(2048));
    for (i = 1; i <= 9; i++)
        WRITE_SCREEN$5(&OUT_CHAN, CAT(CAT("|", "                      "), "|"), &i, TMP(1), TMP(2048));
}
```

Slot bijection: identity over nine slots including the byte-addressed
string temps (w2, w4, w6, w18, w20, w34; b12, b13, b44).  Folded: 43/43.
The two border literals were read from the image (0x70000E07 = 22 `_`,
0x70000DDF = 22 `-`; the book shows them without text), and the translator's
content lookup resolved all three literals to the book's addresses.  The
two-arity `arg_count` (marker word at `wp(ac3, -9)`), the sign test on
`*flag`, the if/else shape, the WCMV residues and lazy `ac3 = wfp`, and the
concatenation temporaries all reproduce.

### 4.4 DIED @7016603D (argc 2, frame 0x07; 529 instructions, 73 blocks, 495 statements) — refused at statement 1

`game/routines/DIED.c` carries the routine's opening in the reconstructed
shape (the bit test, the two 32-blank record-field assignments).
`translate.py` stops at line 18:

    REFUSE game/routines/DIED.c:18:9: BIT(): PL/I bit-field references (bit
    address 16*word + n, the WSZB/WBTZ tests) are not in the subset (FuncCall)

Census of the 495 book statements against the subset (ircmp/translate would
need all of these): bit arithmetic 51 (the `lsh(add(mul(16, i*686), -9436), -4)`
bit addresses and the WSZB/WBTZ/WADC idioms), **ac3 repurposed** as an
address register 13, string statements into record fields 12, arena
twins/claim/release 20, `words()` (WBLM) 1, game→game `call` 4 + two
`LCALL …,0` / one `LJSR` audit lines (six game callees: UPDATE_SCREENS,
HIT_ANY_CHAR, REPOSITION, REFRESH_SCREEN$0, DISPLAY_SCREEN$1,
DISPLAY_INVENTORY$1), DERR folds 20, ?WRITE_SCREEN 2.  Nothing about the
three matched routines predicts how the compiler allocates registers once
ac3 leaves the frame pointer, or how the two claim groups are laid out;
that is the question P36 inherits, with a working translator to ask it.

## 5. Findings (what the compiler turned out to do)

Numbered as in CODEGEN_RULES.md; confidence A/B/C as defined there.

1. **Frame layout (R1–R3, A).**  Declared locals in declaration order, one
   wide slot each; scalar temporaries in the lowest free even slot, freed
   from their *last use* on — the same statement that reloads a CSE temp
   can reuse its slot for a dummy argument (PICK_X_Y 70176241).  The
   hypothesis of PLAN §3 survived all 21 slots of the three routines.
2. **String temporaries are a separate pool (R3b, B).**  A dead string
   temp of sufficient size is reused; otherwise the temp is placed at the
   frame's high-water mark; scalar temps never take string words.  This
   explains REFRESH_SCREEN's 4/18/20 (scalars) against 6/22/34 (strings)
   exactly, which a single pool cannot.
3. **Register choice is a cost model, not round-robin (R7, A):** unknown or
   cached-variable or address registers cost 0, a duplicate 1, a constant
   loaded *in this statement* 2, a value still needed in this statement 3;
   ties go to the lowest register.  Constants lose protection at the
   statement boundary but stay known (the WCMV residue `ac0 = 0` is reused
   for `TMP(0)`; R7a).  A cached variable the current statement reads again
   is protected from the statement's start (R7b) — this single rule
   resolved the last seven DIFFs of UPDATE_SCREENS.
4. **Register knowledge persists past one-word THENs and resets at joins
   (R8/R8a).**  `*x` survives the skip at 70176262→6A but everything is
   reloaded after `retry:`.  At a DO-loop head only the loop register is
   known, and it stays protected through the body's first statement (R21b).
5. **Two kinds of CSE temp (R9, R10).**  The scaled subscript is saved when
   a later statement references the element again (A) — except when the
   subscript came from the DO-loop register (R9a, B).  The element address
   is saved when two later references exist and is consumed by the *second*
   later reference, the first recomputing from the scaled temp (R10, **C**):
   both instances (PICK_X_Y 70176208/17/26, UPDATE_SCREENS 7017D675/7A/8A)
   fit, but two instances do not distinguish this from other readings, and
   the rule is stated so P36 can falsify it.
6. **Dummy-argument stores are immediate unless a slot was just consumed
   (R18, B)** — then the stores are deferred to after the last dummy is
   evaluated; address arguments are computed in push order (right to left).
7. **Branch chaining (R19, B).**  A `goto` beyond WBR range goes through a
   per-label XJMP stub at the routine's end, and the routine's own final
   `goto retry` *is* the stub — three WBRs share it.
8. **Statement shapes are fully regular:** `if (c) S` for a one-word S is
   skip-if-not-c over S (R13); longer bodies are skip-if-c over a `goto
   else` (R13b); 16-bit zero and sign tests are the Nova MOV#/MOV.L# forms
   (R14); the DO loop is the XNDO idiom with the exit branch folded to a
   `ret` when nothing follows (R21/R22) and no entry test for constant
   bounds (R21a); the more complex operand is evaluated first (R20); an
   indexed store computes the address, then the value (R23); `ac3 = wfp` is
   emitted lazily after a string statement (R24).
9. **Unresolved:** the PLAN's register tie-break worry (ac2 chosen over ac1
   at 7017D67A) turned out to be R7b, not a tie-break at all; no unresolved
   register decision remains in the three routines.  R10's consumption
   pattern (finding 5) is the one rule held at confidence C.
10. **Independent confirmations gained along the way:** the PICK_X_Y type
    polarity correction; the two border literals of REFRESH_SCREEN (their
    texts were recovered from the image and their addresses reproduced by
    content lookup); `RANDOM_NUMBER$3`'s argument order; the PROMPT's
    PICK_X_Y/REFRESH_SCREEN instruction-count transposition.

## 6. Honest assessment

- **What is proven.**  For each of the three routines, `translate(S)` equals
  the IR of record under the ruled equivalences, so S is *a* source whose
  compilation by the modelled compiler is the observed code.  It is not
  thereby *the* lost PL/I source: the C is one statement per PL/I
  statement, but the model cannot see a comment, a variable name, or which
  of two source spellings the compiler would treat identically (e.g. the
  tail of PICK_X_Y could be one `IF … | … THEN` or four IFs — both would
  have to yield the same skip chain for the model to be complete, and only
  the four-IF form was tried).
- **Sample size.**  197 statements from three small routines fixed 25 rules.
  Rules marked A recur across routines; B rest on one routine; R10 is C.
  Some rules were adjusted several times while the three routines were
  being matched (R10 three times, R7 twice); the final set matches all
  three simultaneously, which is the check that matters, but a fourth
  routine could still overturn a B or C rule.  That is what DIED is for,
  and it will need a second round of subset work first (§4.4).
- **Comparator equivalences.**  Two were extended beyond the PROMPT's
  wording (DERR pc; literal text by length); both are recorded, both are
  conservative (nothing the translator could know is forgiven), and the
  literal addresses were reproduced anyway.  The comparator's `border`
  and `unknown` classes are coarse; they did their job of pointing at the
  structural error but a finer classing would help on a 500-statement
  routine.
- **The translator is a model, not a compiler.**  It has no general
  register allocator, no general expression evaluator (only the forms the
  three routines use), no `&&`/`||`, no if-bodies of two statements, no
  bit fields, no ac3 repurposing.  Each of those refuses by name.  That is
  the intended shape — refuse, don't guess — but it means every new routine
  will start with refusals.
- **Not done:** DIED beyond its first statement; the native *runtime*
  behind quest_rt.h (compile-check only; `TMP`'s C++ proxy has the
  parameter's width only nominally); a strings-index tool for literal
  addresses (translate.py greps the image directly).

## 7. Recommendation for P36

1. Extend the subset for DIED in this order, each step measured with
   ircmp: (a) bit-field references (`BIT(word, n)` → the bit-address
   arithmetic and WSZB/WBTZ/WADC forms; 51 statements), (b) record string
   fields as string-statement destinations (12), (c) game→game `call`
   decoration from the addrbook (args/marker/ret), (d) the arena twins
   (`t@<canonical>.k`, claim/release) on the two WMSP groups, (e) ac3
   repurposed as an address register — the likeliest place for the
   register model to break, since it removes the frame pointer.
2. Test R10 (confidence C) on a routine with three or more later
   references to one element before relying on it.
3. Batch the census tools: translate every routine whose constructs are
   in the subset (the addrbook lists 400+), report the refusal reasons
   sorted by frequency — that is the cheapest map of where the regularity
   runs out.
4. Keep the discipline that produced the 100 %s: one C statement per PL/I
   statement, rules stated with their instruction pair before they are
   coded, and the comparator's self-test run after every rule change.

## 8. Runtimes

translate.py 0.04–0.06 s per routine; ircmp.py 0.8–0.9 s per comparison
(dominated by loading the 1.6 MB book); `--selftest` 1.0–2.3 s; native
compile checks < 1 s each.  Part 2 wall time: one session.
