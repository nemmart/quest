# compiler/ — from the IR toward source, and from source back to the IR

Tools that read the verified IR (`emulation/quest.ir2.*`) and the census
artifacts, and produce human-facing or source-facing output. Nothing in
here is read by the emulator or the batteries.

- `readable.py` (P34) — the readable-layer prototype: frame/static naming,
  record recognition (1-based origin rule), literal folding, in-block
  register folding, control marks. Renders `docs/Project34/readable/*`.
- (P35, planned) `translate.py` — the C-subset → IR translator. Design
  decisions of record (Sep 8 2026, user + integrator):
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
