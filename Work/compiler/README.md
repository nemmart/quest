# compiler/ — from the IR toward source, and from source back to the IR

Tools that read the verified IR (`emulation/quest.ir2.*`) and the census
artifacts, and produce human-facing or source-facing output. Nothing in
here is read by the emulator or the batteries.

- `readable.py` (P34) — the readable-layer prototype: frame/static naming,
  record recognition (1-based origin rule), literal folding, in-block
  register folding, control marks. Renders `docs/Project34/readable/*`.
- `translate.py` (P35, built Sep 8 2026) — the C-subset → ir 6 translator.
  Design decisions of record (Sep 8 2026, user + integrator):
  - front end: **pycparser** (C99; accepts `$` in identifiers); the source
    stays in the C subset, `#include "game/quest_rt.h"`.
  - runtime calls are arity-in-name: `WRITE_SCREEN$2(...)`,
    `WRITE_SCREEN$5(...)`, `READ$4/6/7`, `WRITE$3/6`, ...; the translator
    emits N right-to-left pushes and `LCALL ?NAME,N` and REFUSES if
    `N != len(args)`. Natively each `$N` is an ordinary non-variadic
    function.
  - `const` on pointer parameters transcribes the read/write convention
    table (Project28/RTConventions + Project29/Census): a write through a
    const pointer is a compile error natively and a refusal in the
    translator. Callees that write (READ's buffer, UNSIGNED_TO_CHAR's
    varying at ac2, WRITE_SCREEN$5's row/col) take non-const pointers.
  - game routines with two arity shapes have ONE body with a leading
    `int arg_count` parameter (reads of it become the frame marker-word
    load `M16[fp+0x7FF7]`; never pushed) and `$N` call sites; natively
    `FOO$3(a,b,c){FOO(3,a,b,c,NULL);}` wrappers are generated.
  - 1-based indexing: the source writes `foo[i]` with PL/I's index; the
    translator's expression generator subtracts one and FOLDS
    (`(i−1)·9 + 11504` → `mul(i,9) + 11495`, the compiler's exact text);
    natively `ARRAY1(T,N)` is a C++ object whose `operator[]` does the
    `−1` with the bounds check. The C-vs-C++ delta is confined to the
    generated declarations file (`game/declarations.h`).
  - pilot: PICK_X_Y against the P34 register-folded book, then DIED;
    `translate(S) == IR` textually makes S a proven source; the same S
    compiles natively.

## P35 tools (Sep 8 2026) — see docs/Project35/REPORT.md

- `translate.py SRC.c --routine NAME [-o out.ir] [--mem Disassembled/quest.mem]`
  — pycparser front end (cpp with `-D__TRANSLATOR__`, stub headers in
  `fake_include/`), the code-generator model of **CODEGEN_RULES.md** (25
  numbered rules: frame slots + temp pools, register cost model, CSE temps,
  DO/IF/DERR/string shapes), lower.py's spellings.  Output: an ir 6 book-like
  file with synthetic block pcs in canonical order and a header line
  `routine NAME entry lo hi` for ircmp.  Unknown constructs REFUSE by name
  (exit 2).  0.05 s per routine.
- `ircmp.py A.ir B.ir --routine NAME [--folded] [-n N]` — the comparator:
  the seven ruled equivalences (labels/sites/DERR pc; a slot bijection;
  tN and `t@block.k`; literal addresses by length; comments; static addresses
  and registers verbatim), MATCH / RENAME / DIFF(reg, slot, border, expr,
  const-spelling, missing, extra, unknown), `--folded` runs readable.py's
  `fold_block` on both sides.  `--selftest` (identity, slot permutation, one
  register swap, folded identity on the four P35 routines; 2.3 s).
- `gen_declarations.py` → `game/declarations.h` (C and C++ views) and
  `game/declarations.json` (the same table for the translator).
- Results: PICK_X_Y 64/64, UPDATE_SCREENS 72/72, REFRESH_SCREEN 61/61
  statements MATCH (register-exact, identity slot bijections; primary and
  folded); DIED refused at its first statement (bit-field test).
