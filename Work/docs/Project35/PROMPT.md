# Project 35 — recompilation as decompilation: the C→IR translator, a few routines, and an IR comparator

GOAL: build `compiler/translate.py` — a translator from a restricted C
subset (with `game/quest_rt.h`) to our IR — write three or four game
routines in that C, and measure how closely `translate(S)` matches the
IR of record (`emulation/quest.ir2.book`, ir 6) using a comparator
that knows which differences are NOT differences (stack-slot and
temporary renames, block labels, provenance). The question this project
answers is: **can source that compiles to a textual match be written for
this program at all, and where does the DG compiler's regularity run
out?** A match makes the C a proven source with no soundness argument;
the residual diff, classified, is the finding. Nothing here executes:
no emulator change, no artifact change, no battery.

Hi Claude! Read docs/METHOD.md first. Design decisions of record for the
translator: **compiler/README.md** (Sep 8: pycparser front end; `NAME$N`
arity-in-name runtime calls with `const` transcribing read/write; one
body + leading `int arg_count` for two-arity game routines; 1-based
indexing folded in the expression generator; `ARRAY1()`; the C/C++ delta
confined to the generated declarations). Context: docs/IR.md (ir 6 — the
grammar you must produce, exactly), docs/Project34/{Census.md,
PICK_X_Y_reading.md, readable/*} (what the readable layer already
recovers, and the record tables), compiler/readable.py (reuse its IR
parser; its register-folding pass is the secondary target — see §3),
docs/GAME_REFERENCE.md (the world layout: SD_PTR/OBJ_PTR/CAS_PTR,
player record, arrays), emulation/quest.addrbook (per-routine frame
layouts: argc, locals), docs/Project28/RTConventions.md (which runtime
arguments are read vs written → the `const`s), docs/Project29/
StringsDesign.md §2 (the string statements the IR has — your C's
`assign_padded`/`pad_equal` map to them directly), game/README.md +
game/quest_rt.h (the scaffold). TREE VINTAGE: main after the Sep 8
layout rename (625d968 or later) — state the commit; verify
docs/Provenance.md.

## What "match" means — the comparator's equivalences (RULED for this project)

`compiler/ircmp.py A.ir B.ir [--folded]` compares two IR texts routine
by routine and reports per-statement MATCH / RENAME / DIFF, treating the
following as equal:

1. **Block labels**: blocks are named by pc in the book; the translator
   cannot know pcs. Compare the CFG structurally: canonical block order
   by DFS from the routine entry, `goto [labels] e` compared by target
   position, `site=` addresses ignored.
2. **Frame slots**: a consistent bijection per routine between the
   book's `wp(ac3, d)` / `[ac3+d]` offsets and the translator's — the DG
   compiler's slot assignment is not something the C says. Report the
   bijection; a slot mapped to two different offsets is a DIFF.
3. **Temporaries**: t-place names (`t0/t1`), arena twin names
   (`t@block.k` ↔ `t@<canonical>.k`).
4. **Literal addresses**: `[@0xW:b, "text"]` compares by text; the
   address is provenance.
5. **Provenance comments**: everything after `;` on a line is dropped.
6. **Static addresses** are NOT equivalences: `M32[0x70000212]` must
   match — the C declares statics at their addresses (declarations.h).
7. **Register choice** is NOT an equivalence in the primary comparison
   (the residues and the strict surface are real); `--folded` compares
   after applying readable.py's in-block register-folding pass to BOTH
   sides, which is the secondary, easier target.

Everything else — expression shape, operand order, the `min()` skip
diamond, the DERR assert text, push order, residue-setting statements
— must match textually.

## Part 1 — plan gate

1. **Routine set**: PICK_X_Y (96 instructions; P34 renders it as the
   hand reading), DIED (two claim groups; strings; a call), one small
   pure-arithmetic routine, and one routine with a two-arity call
   shape (the `arg_count` convention) — name them from the addrbook
   with instruction counts. Four routines, ~400 instructions total.
2. **C subset**: the constructs those four need, listed (declarations
   with `ARRAY1`/`VARYING`, `if`, loops as `while`/`goto`, calls with
   `$N`, `SUB()`, string assignment/append/compare via the header's
   functions, integer arithmetic incl. the flag-effect ops the IR keeps
   as `add()/sub()/mul()/div()`). Say how each maps to an ir 6
   production, and what the translator REFUSES (anything outside the
   subset — loudly).
3. **Frame layout rule**: how the translator assigns slots (in
   declaration order? the DG compiler's apparent rule from the
   addrbook?) — the comparator's bijection absorbs the difference, but
   the closer the rule, the smaller the report.
4. **Comparator design**: the parser (reuse readable.py's), the
   canonicalisations of §"What match means", the report format (per
   routine: statements total / MATCH / RENAME / DIFF, the slot
   bijection, the first N DIFFs with both texts), and a self-test (the
   book against itself = 100% MATCH; the book with a slot permutation =
   100% RENAME).
5. **Native check**: `gcc -std=c99 -Wall -Wextra -Werror -c` and
   `g++ -std=c++17 …` on each routine with quest_rt.h — a compile check
   only (no runtime exists yet); the `const` conventions must hold.
6. **Success criteria** stated in advance: primary target ≥ 90%
   MATCH+RENAME on PICK_X_Y; the residual DIFFs classified (register
   choice / slot order / block order / expression shape / unknown).

STOP AND REPORT at the plan gate.

## Part 2 — build

- `game/quest_rt.h` filled in for the subset; `game/declarations.h`
  GENERATED by a small tool (`compiler/gen_declarations.py`) from
  GAME_REFERENCE + the P34 record census for the tables the four
  routines touch, with a provenance header; `game/routines/*.c`.
- `compiler/translate.py`: pycparser → ir 6 text for one routine
  (`--routine NAME`), emitting blocks in the canonical order, with the
  frame-layout rule of Part 1.3 and the arithmetic/flag conventions of
  IR.md.
- `compiler/ircmp.py` + self-test.
- Iterate: translate, compare, fix the translator where the DIFF is a
  translator error, record where it is the DG compiler doing something
  the C cannot say without a rule (e.g. a specific slot reuse, a
  spilled register, an unexpected `WMOV`) — those are FINDINGS, each
  with the instruction pair.

## Part 3 — report

- Per routine: the C source, the match report, the slot bijection, the
  classified residual (primary and `--folded`).
- The FINDINGS list: every place the compiler's output could not be
  produced from the C without a rule we do not yet know, with a
  proposed rule where one is visible (e.g. "the compiler spills ac0
  across a call to slot X — rule: …").
- An honest assessment: is a 100% textual match reachable for whole
  routines, or is the register-folded target the realistic one? What
  should P36 be — more routines, the missing rules, or the native
  runtime?

## Boundaries — BINDING

1. Files: `compiler/`, `game/`, `docs/Project35/`. Nothing in
   `emulation/`, no artifact, no battery.
2. Part 1 before Part 2.
3. The translator produces ir 6 exactly per docs/IR.md; it never
   invents a production. The comparator's equivalences are the seven
   above — no others without a ruling (adding "register choice" to the
   primary set defeats the purpose).
4. Refuse-don't-guess in the translator; classify-don't-hide in the
   comparator: an unmatched statement is a DIFF with both texts shown.
5. Deliverables: translate.py, ircmp.py (+ self-test), gen_declarations.py,
   quest_rt.h, declarations.h, the four routines, docs/Project35/
   {REPORT,REPORT_worklog}.md with the match tables and findings, tool
   runtimes, TREE VINTAGE.
