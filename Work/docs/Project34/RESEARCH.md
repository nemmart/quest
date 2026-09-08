# Project 34 — the readable layer: RESEARCH / PROTOTYPE (Python only)

This is a research session. The deliverable is a prototype renderer and
a census of what it can and cannot name — input to the design of the
"flat-graph world" (functions, types, the C++ translation). It changes
nothing the emulator, lower.py, IRExec or any battery reads. It touches
only `compiler/readable.py` and `docs/Project34/`. No git branch conflicts
with anyone: P31/P32/P33 own lower.py, IRExec, IR.md, string_sites.py,
the checker; you read their artifacts and write your own.

Hi Claude! Read docs/METHOD.md first. Context: docs/IR.md (ir 5 — the
grammar you consume), emulation/quest.ir2.book (the artifact of record;
verify its header shas against docs/Provenance.md), docs/Project29/
StringsDesign.md §9 (the C++ end state the rendering should approach),
emulation/quest.addrbook (per-routine frame layout: argc, locals, borrow
slots — the address book from P16–P20), Disassembled/quest.symbols
(routine and static names), docs/Project29/quest.strings, and the Sep 5
planning-session reading of PICK_X_Y (docs/Project34/PICK_X_Y_reading.md
— the target: what a human reconstructed from the IR by hand). TREE
VINTAGE: main after P31 — state the commit; if P32 has landed use its
book, and say so.

## Why

After P31 the IR reads as a faithful register-level program. A human
can already reconstruct a routine from it (PICK_X_Y took minutes), but
the data references are `M32[0x70000212]`, `wp(ac3, 4)`, `M16[wp(ac2,
11495)]`, and the register traffic (`ac0 = …; local = ac0`) hides the
expressions. We want to know, BEFORE designing the flat-graph world,
how much of the distance to readable code is mechanical naming and
folding — and what the hard residue is.

## The prototype — `compiler/readable.py`

A pass over the ir 5 book that produces `docs/Project34/readable/
<ROUTINE>.txt` for a chosen set of routines, applying these rewrites in
order, each SWITCHABLE and each REPORTING what it did and did not
resolve:

1. **Frame naming.** From quest.addrbook's per-routine layout: `wp(ac3,
   d)` / `[ac3+d]` → `local_<d>` (or a better name if the book has
   one); `R[ac3 − k]` / `[ac3 + 0xFFF…]` → `arg_<k>` (by-reference
   arguments; argc from the book); `ac3` itself is `fp` after WSAVS.
   Report every `ac3`-relative reference the layout does not cover.
2. **Static naming.** `M32[0x7000xxxx]`, `M16[…]`, byte pointers into
   the data segment → a name from quest.symbols when one exists; else a
   generated name `static_<addr>` with a USAGE census (how many routines
   read/write it, as word/halfword/byte/string, what it is compared
   with). The census is the deliverable here: it is the input to a later
   hand-naming pass.
3. **Record recognition.** `base + i·stride + K` with a `0 < i` guard →
   a 1-based array of records: origin `K + stride`, stride = record size,
   the field offset per site. Emit `<table>[i].field_<off>` and a table
   of (base, stride, fields seen) per table. PICK_X_Y's region table
   (stride 9, fields 11495/11496/11497) is the known case; find the
   others (the player record stride 686, the object record 20, the
   castle 23 are named in the Census).
4. **Literal folding.** `[@a:b, "text"]` is already readable; fold the
   ONE-USE varying local: `[@local, n varying] = "text"; rt_call
   ?WRITE_SCREEN([@local, n varying], chan)` → `rt_call
   ?WRITE_SCREEN("text", chan)` when the local is dead after the call
   (dead = not read before its next write in the routine — a
   conservative intra-routine check; report the cases it could not
   prove).
5. **Register-to-temp folding INSIDE a block.** Single-assignment
   register values consumed once within the block fold into their use:
   `ac0 = M32[x]; ac0 = mul(ac0, 9); local_4 = ac0` → `local_4 =
   mul(M32[x], 9)`. Registers live at the block's terminator (the strict
   surface) stay explicit. Report the fold count and the registers left
   live per block.
6. **Control-structure sketch** (optional, if time allows): mark
   backward gotos as `loop`, two-way gotos as `if`, `assert` as the
   bounds check it is; do not attempt structuring beyond marking.

Each rewrite is PROVABLY meaning-preserving on the IR (it renames or
folds; it never changes an address or a value). The output is a
rendering, not an artifact anything executes.

## Routines

At minimum: PICK_X_Y (compare against the hand reading), DIED (two
claim groups — see how the strings read), DISPLAY_INVENTORY, ATTACK,
LOGON, and the routine with the most static references (find it). Add
what the census makes interesting.

## Deliverables

- `compiler/readable.py` (runtime line; flag > 10 s).
- `docs/Project34/readable/*.txt` — the renderings, with the switches
  used in each file's header.
- `docs/Project34/Census.md`: per rewrite, what it resolved and what it
  could not (counts + examples); the static usage census; the record
  tables found; the one-use-local folds proven/unproven; registers
  left live per block (a histogram); and a written assessment: what the
  flat-graph world must decide first (functions with parameters? the
  static naming pass? structuring?), with evidence from the renderings.
- `docs/Project34/REPORT_worklog.md`.

## Boundaries — BINDING

- Nothing outside compiler/readable.py (moved Sep 8; was emulation/tools/) and docs/Project34/. No emulator,
  no lower.py, no IRExec, no IR.md, no artifacts the emulator reads, no
  battery, no runner task.
- Rewrites that cannot be shown meaning-preserving are not applied;
  they are listed as candidates with the reason.
- Nothing is hand-named in the output: a name comes from a book, a
  symbol table, or a generated scheme — the census is where humans
  name things later.
- Stop when the deliverables are committed on branch p34-readable and
  report with the counts and the assessment.
