# Project 30 — the C++ string library (ships dark)

Session Sep 6 2026, solo implementation; plan gate and landing reviewed
by the user. TREE VINTAGE: main @ 00f641c (P28 merged at 309585d, ir 4;
uploaded Work/Disassembled byte-identical to the repo; Provenance
post-P28 prefixes verified: quest.dis 5c1db5fb…, ir2.book 294f81d3…,
ir2.stock 1cf12f33…, blocks.split 1d3baaf6…, synclist.p27 af1be42f…,
pushmap.M4 b8953659…, addrbook e6fde2c2…). Branch: p30-string-library.
Gate: task 043 (k1fo + play-st vs 042, both self-tests as
preconditions) — queued; local smoke in §5. Design of record:
docs/Project29/StringsDesign.md §2.4, §3, §6; Mapper.md §1.3.

## 1. Outcome

The three pieces every string project after this one calls, built and
unit-tested, linked into the emulator with nothing calling them:

- **`hw/strings/EagleString.{hpp,cpp}`** — pieces as spans `(bp, len)`
  over emulated memory (literal / fixed / varying / substr / char);
  `copy` (= WCMV), `assign_fixed`, `assign_varying`, `append_char`
  (= WSTB), `block_move` (= WBLM), `pad_equal` (reference); the §3
  residues `residues_after_copy` (pure), `residues_after_compare` (≡
  `compare`, the WCMP read loop), `residues_after_blm` (pure). Every
  loop is a transcription of the EagleSpecial.cpp arm it cites; every
  byte goes through `Memory` with the per-byte segment check against
  `get_segment(site)`.
- **The Mapper's arena form** (`hw/Mapper.{hpp,cpp}`) — codec rows
  0x75/0xEA/0xF5 (I3 `static_assert`ed against all six existing
  prefixes, plus a segment-fits-one-prefix assert); `ArenaRow (block,
  arena_addr, capacity, length, wfp, master_addr)`; `configure_arena`
  (static layout, I1-style disjointness incl. closed ends, in-segment),
  `arena_bind` (block entry), `arena_set_length` (append; capacity
  overflow aborts), `arena_unmap_frame` (frame exit), `arena_row`
  (binary search, closed-end containment). `equivalent()` translates a
  clone arena value through the MAPPED row containing it and reports
  MISMATCH on an unmapped row or no row; `clone_location()` on any
  arena-prefixed address ABORTS; `frame_precedes()` on an arena address
  aborts. The three-call surface is unchanged. No master→clone lookup
  exists.
- **`hw/strings/ClaimDelta.{hpp,cpp}`** — the per-frame Δ accumulator
  (`claim +2·ac`, `release old−new`, `frame_exit`, `check`), keyed by
  wfp. Not wired.

**Emulator behaviour: unchanged.** The only touched emulator code path
is `Mapper::{equivalent,clone_location,frame_precedes}`, and for every
value that occurs today (no arena address exists until P33) the verdict
is what it was: an arena-prefixed value used to decode to `None` and
mismatch with a probe; it now decodes and mismatches on "no mapped row".
`decode()` gaining three cases is the whole diff on live paths.

## 2. Rulings taken (user, Sep 6, plan gate)

1. Operations that touch memory take `site` — the G-3 check is the
   instruction's (`get_segment(pc)` vs `get_byte_segment(ptr)`), not
   pointer-vs-pointer. Residue functions for copy/blm stay pure.
2. `compare()` ≡ `residues_after_compare()`: the stop point depends on
   the bytes, so §3's "pure" applies to copy and blm only.
3. Codec rows 0x75/0xEA/0xF5 with the I3 `static_assert`; dark while
   no row is bound.
4. Gate = the 038/039 pair (k1fo book K=1 + play-st stock K=50) vs 042;
   the prompt's "K=1 stock" was loose.
5. Mirror, not hoist: EagleSpecial.cpp untouched; equivalence is proved
   by the self-test, not by shared code.

Also confirmed: pieces as spans (never materialised — WCMV's overlap
semantics are preserved for free; `p@b` will be a span over the arena
in P33); closed-end containment on arena rows; `arena_bind` asserting
`wfp == owner_->wfp`.

## 3. Semantics, cited

| library | mirrors | residues (§3) |
|---|---|---|
| `copy(m, site, dst, n, src)` | EagleSpecial.cpp:42–74 (WCMV) | ac0=0; ac1=len−sgn(len)·t; ac2=dst+n; ac3=src+sgn(len)·t; c=(ac1≠0); t=min(\|n\|,\|len\|) |
| `residues_after_compare(m, site, dst, n, src, len)` | :76–109 (WCMP) | ac0=str-2 count at stop; ac1=−1/0/1; ac2/ac3 ONE PAST the failing byte (B-4); c untouched |
| `block_move(m, site, dst, src, k)` | :20–40 (WBLM) | ac1=0; ac2=src+k; ac3=dst+k; ac0/c untouched; bit-31 throw (G-2) |
| `append_char(m, bp, x)` | EagleGeneral.cpp:119 (WSTB) | none (the WINC is its own statement) |
| `assign_varying(m, site, a, cap, src)` | XNSTA + WCMV (§2.4) | length word := min(len,cap) BEFORE the copy; copy of min(len,cap) |

`ovr` is never written (B-1). All three segment throws carry the
emulator's exact strings. Descending counts follow `direction()`
exactly (a strip of negative counts is in the test although Quest's
string sites are all forward).

Unit of `capacity` in an arena row: data BYTES; a row's word extent is
`1 + ceil(capacity/2)` (length word + data) and its closed right end is
`arena_addr + extent`. The arena segment is `[0x75000000, 0x75800000)`
— 8M words, sized so the byte form is exactly 0xEA (the full 16M-word
segment would give byte prefixes 0xEA and 0xEB, breaking the
one-prefix-per-form codec table).

## 4. The self-test (`tests/strings_selftest.cpp`, `run_strings_selftest.sh`)

18,370 cases, GREEN. Two `Machine`+`Memory` rigs (heap-allocated —
`Memory` carries a 2 MB permissions array inline) with identical random
contents: the real instruction runs on A via `EagleSpecial::execute`,
the library on B; then ac0–ac3, c, ovr and every byte of the 16 KB
scratch region are compared field by field, and exceptions must agree
by text.

- copy: 21×21 lengths (0..300) × 4 layouts (disjoint, dst=src+1 smear,
  dst=src−1, identical) — equal / shorter (blank pad) / longer
  (truncate); 56 descending-count cases; varying and substr pieces as
  sources; `assign_varying` (length word + count) for 21×21.
- compare: 21×21 lengths × (equal, mismatch at every k for short
  strings / every 13th for long, both directions of the perturbation);
  `pad_equal` agrees with `ac1 == 0` throughout; descending counts;
  unsigned order (0x80 > 0x7F).
- blm: 9 counts × 5 layouts incl. the `src = dst−2` fill (pattern
  asserted) and descending counts.
- negatives: G-3 throws for all three (source or destination in
  segment 6; and a descending copy that walks below byte 0xE0000000
  mid-run — both sides throw with the same three bytes already written
  and the oracle's registers unwritten); G-2 bit-31 for both pointers;
  G-4 zero count with garbage pointers does NOT throw; `copy` of a
  `chr()` piece refuses.
- arena form: unbound rows MISMATCH in all three forms and do not map
  through the 0 sentinel; bound rows MAPPED at base / interior / closed
  end in word, byte (low bit kept) and @ forms; one past the end
  MISMATCH; wrong master value MISMATCH with the verdict carrying the
  mapping; `arena_unmap_frame` unmaps only its wfp; rebind resets
  length; binary search hits base/end and misses gaps for all 19 rows;
  aborts: `clone_location` (3 forms), `frame_precedes`, bind of unknown
  row / wrong wfp / master 0 / master in arena, length overflow /
  negative / unknown row, layout with touching closed ends / below
  segment / end past segment; plain (non-arena) verdicts unchanged.
- Δ: sum, keyed by wfp, release to 0, frame_exit erases.

**Teeth.** The run script rebuilds the test with the library compiled
`-DP30_BROKEN_RESIDUE` (ac3 after a copy := src+n) and requires RED:
1,326 failures. Beyond that, eleven ad-hoc mutations were tried during
the session (pad byte, WCMP pointer advance, result sign, WBLM
read-all-then-write, WBLM ac2, copy carry inverted, copy segment check
removed, arena open right end, unmapped row mapping through 0, wfp
assert removed, clone_location refusal removed): ten went RED at once;
"unmapped row maps through 0" stayed GREEN because no test used a
master value equal to the bare offset — two cases were added and it now
goes RED. All eleven RED on the final test.

## 5. Gate

- `tests/run_helpers_selftest.sh`: GREEN (unchanged).
- `tests/run_strings_selftest.sh`: library GREEN, broken build RED.
- Local smoke (this container, 1 core): k1fo book K=1 — **0 div, end
  clean, pairs 329,763** (blk_equal 329,763, mismatch 0, max_gap 1,
  floor OK). Band across 037–042: 298,566–389,610 (timing-dependent
  idle heartbeats); in band.
- Task 043 (`tasks/043-p30-string-library.sh`): both self-tests as
  preconditions (the strings one requires "teeth confirmed"), then k1fo
  + play-st via `bin/task_source.sh`, JOBS=3, vs the 042 lines. Bar: 0
  div, same endpoints (clean / clean), pairs in band. Result:
  results/043-p30-string-library — **TASK 043 GREEN** (integrator, Sep 6):
  helpers self-test GREEN; strings self-test GREEN (18,370 cases) and
  the broken build RED (1,326) — teeth confirmed; k1fo book K=1 0 div,
  clean, pairs 298,566; play-st stock K=50 0 div, clean, pairs
  4,249,241. Both endpoints as 042. (First queueing failed ×3 on a
  missing `#include <cstddef>` — fixed in ee90247.)

## 6. Design-vs-reality, recorded

- §3's table says residues are "computed by the library from the
  statement's operands". True for copy and blm; the compare residue is
  the read loop (ruling 2). The header says so.
- StringsDesign §6.2 gives the row as `(arena_addr, length, wfp,
  master_addr)`; the implementation adds `block` (the p@b identity, for
  aborts and traces) and `capacity` (the static bound the overflow
  fault needs). Nothing else.
- The codec table in Mapper.hpp/§1.2 of Mapper.md gains a third column.
  Mapper.md is a design of record (planning-session edit); integrator
  note below.

No addition to EmulatorDivergences.md: no manual page was read against
the source this project; B-1, B-4, G-2, G-3, G-4 are reproduced as
listed.

## 7. Integrator notes

- Mapper.md §1.2 (codec table) and §3 (mutation kinds) should gain the
  arena column / the two arena events at the next planning edit; the
  Mapper.hpp header comment already carries both.
- `Makefile.full` (legacy, no Mapper.cpp either) was not touched.
- `tests/run_helpers_selftest.sh` still links `hw/*.o` only — nothing in
  it references the library, so it needs no change.
- Trace type `arena` (bind/unmap lines) exists for P33; nothing emits it
  today.

## 8. For P31–P33

- P31 calls `copy` / `assign_fixed` / `assign_varying` /
  `residues_after_compare` / `block_move` with the site pc it already
  has for the block. `EagleString::varying(memory, a)` reads the
  length word at execution time — the master's XNSTA precedes its WCMV,
  so an IR that keeps the store as its own statement must construct the
  piece AFTER it, or use `assign_varying` which does both.
- P33: `Mapper::configure_arena` wants the layout artifact as
  `{block, arena_addr, capacity_bytes}`; `arena_bind` wants the
  master's address for the group (the last claim's `wsp_before+2`, the
  §6.3 plan-gate choice) and the machine's current wfp; `arena_set_length`
  after each append; `arena_unmap_frame(wfp)` at WRTN and at the ON-pop
  restore point. `ClaimDelta` hooks at the 57 WMSP / 19 STASP pcs and
  the same two frame exits; `check()` at the pair compare.

## Erratum (P31, Sep 6 2026)

`EagleString::varying()` zero-extended the length word; the compiler
loads a length word with XNLDA, which SIGN-extends (EagleGeneral.cpp:
51–52), so 0xFFFF is the count −1 (a one-byte descending string).
DISPLAY_INVENTORY's compare at 7016816B reads a record field holding
0xFFFF on the login path and the master runs WCMP with ac1 = −1. Fixed
in P31 (one line, cited in the source); tests/strings_selftest.cpp gained
case 1c′ (raw 0xFFFF/0xFFFE/0x8000/0x7FFF/0 through `varying()` for a
copy and a compare), RED on the P30 line, GREEN after. See
docs/Project31/Census.md §10 (F-B1).
