# Project 35 — Part 2 worklog (Sep 8 2026)

Chronological.  Each translator step lists the comparator's MATCH+RENAME
after it (primary comparison), so the effect of every rule is visible.
Commit of record: main 35859fc.  Nothing under emulation/ was written.

## Comparator first

1. `compiler/ircmp.py`: readable.py's parser imported; ir 6 twins
   `t@BLOCK.k`, `claim`, `release` pre-lexed to numeric stand-ins; the book's
   `-0xHHHH` negative bp displacements (IR.md §5.8) normalised to the 32-bit
   word for readable's lexer.  Canonical DFS order; slot bijection built in a
   first pass from structurally paired statements; classification in a
   second.  `--folded` builds a synthetic World/Routine for readable's
   `fold_block` (twin lines kept opaque as instruction barriers).
2. Self-test on the four routines: identity / +100 slot permutation / one
   register swap / folded identity — PASS (2.3 s).  Statement counts:
   PICK_X_Y 64, UPDATE_SCREENS 72, REFRESH_SCREEN 61, DIED 495.  Checked
   that the only blocks in quest.blocks.split absent from the book are the
   folded DERR interiors (all four routines fully lowered).

## Scaffold

3. `gen_declarations.py` → declarations.h/json (statics, direct fields,
   REGION stride 9 / PLAYER stride 686 with raw K).  `quest_rt.h` filled:
   TMP, SUB (a prototype under `__TRANSLATOR__` so cpp does not erase it — the
   first translation lost every DERR that way), VARYING, ARRAY1, `$N`
   prototypes, string primitives, callees.  `compiler/fake_include/` stubs
   for pycparser.  Native check: gcc -std=c99 and g++ -std=c++17, -Wall
   -Wextra -Werror, both views, from the start.

## PICK_X_Y (18 blocks, 64 statements)

4. First translation: 41 % (30 MATCH).  Diffs: a stray empty block after
   the entry; stale "live" marks leaking across statements; the address
   register unprotected while a second field reference was pending; the
   element-address save flushed a statement late; ac0 chosen where the
   book chose ac1 (a stored dummy constant kept its protection).
5. Fixes: empty blocks dropped; live → cached at the statement boundary
   (R8b); ac2 protected while references remain (R6); the R10 save moved
   to before the test; a stored constant stays a constant → **75.4 %**.
6. Constants protected only within their statement (R7a, from the
   REFRESH_SCREEN evidence) and the long-jump stub (R19, with estimated
   instruction lengths) → **96.9 %**.  Trailing empty block removed so the
   final `goto retry` is the stub → **100 % (64/64)**, folded 41/41.
   Verified by an independent textual diff (labels/sites masked): identical.

## UPDATE_SCREENS (18 blocks, 72 statements)

7. Raw book blocks read; the DO idiom (XNDO, `ret`-folded exit), ABS
   diamond, operand order, 2-D store, R9a (no CSE for the loop register)
   worked out.  `screen[9][11]` added to the PLAYER table (K −611, strides
   22/2); `ABS()` added to quest_rt.h.  Translator: for/continue/compound,
   ABS, operand-order rule (R20), 16-bit memory operands loaded before
   sub, statement numbering by node identity (the pre-pass had numbered
   nested statements with their header's index).
8. First structural pass → the entry block had to carry the first
   statements (only PICK_X_Y's WSAVS block is alone, because a label
   follows) → **86.3 %** (target 85 %).  Residual: the 2-D store statement.
   PICK_X_Y unchanged at 100 %.
9. Later (after REFRESH_SCREEN): R7b (a cached variable the statement will
   read again is protected from the statement's start), R10 restated by
   reference position (consumed at the second later reference), R6′ (an
   address still held by a temp is unprotected) → 90.4 %, then with
   protection applied at statement start → **100 % (72/72)**, folded 44/44.
   PICK_X_Y and REFRESH_SCREEN unchanged at 100 %.

## REFRESH_SCREEN (14 blocks, 61 statements)

10. Literal texts recovered from Disassembled/quest.mem (0x70000E07 = 22 `_`,
    0x70000DDF = 22 `-`; the book shows them without text).  Equivalence 4
    extended: a literal compares by byte length.  `CAT()` and `BIT()`
    declared; `TMP` made untyped (the dummy's width is the callee
    parameter's: 16-bit for WRITE_SCREEN$5, 32-bit for RANDOM_NUMBER$3);
    a C++ converting proxy for TMP so both views compile.
11. Translator: callee prototypes read for TMP widths; memory image lookup
    for literal addresses; separate string temp pool (R3b) derived from the
    slot pattern 4/18/20 vs 6/22/34; lazy `ac3 = wfp` (R24); marker word
    (R12); 16-bit sign test (R14b); if/else with multi-word bodies (R13b);
    CAT built with XLEFB/WSTB/WINC/WCMV (R25); constant-bound DO with the
    loop register picked by the cost model (R21a).
12. First run: an extra `goto` block where the else body's call
    continuation should itself be the after block → joined → **96.7 %**.
    Remaining two DIFF(reg): the loop register must stay protected through
    the body's first statement (R21b) → **100 % (61/61)**, folded 43/43.  All
    three literal addresses resolved to the book's.

## DIED (staged)

13. Census of the 495 statements: bit arithmetic 51, ac3 repurposed 13,
    string statements 12, twins/claims 20, WBLM 1, calls 4 (+3 audit
    lines), DERR 20.  `DIED.c` opening written (bit test, two blank-field
    assignments); `fm590` added to PLAYER; `UNUSED` marker added for the
    staged parameters.  translate.py refuses at line 18 with a named
    reason (BIT); both native views compile.

## Documents

14. `compiler/CODEGEN_RULES.md` (25 rules, evidence, confidence),
    compiler/README.md and game/README.md updated, PLAN.md §9 (rulings),
    REPORT.md, results/ (translator outputs, comparator reports, the DIED
    refusal).  Final checks: ircmp `--selftest` PASS; native compile of all
    four sources in both views; the three comparisons regenerated
    (64/64, 72/72, 61/61; folded 41/41, 44/44, 43/43).

## Things tried and dropped

- A single lowest-free-even pool for string temps (contradicted by 22/34).
- "R10 consumed by the next statement with ≥ 2 references" (fits PICK_X_Y,
  not UPDATE_SCREENS) and "whenever ac2 was clobbered" (the reverse).
- LIFO / LRU tie-breaking for registers: unnecessary once R7b was found;
  the lowest-numbered register explains every tie in the three routines.
- Flushing the element-address save right after the field load (a
  statement early in UPDATE_SCREENS; the save follows the whole condition).
