# Project 30 — the C++ string library (ships dark)

GOAL: build and unit-test the C++ pieces every string project after this
one calls — `EagleString` (values, appends, assignment with PL/I
pad/truncate, blank-padded equality, and the RESIDUES each replaced
instruction leaves in ac0–ac3/c), the Mapper's **arena form** (19 static
rows, clone→master only, `clone_location()` refusing), and the per-frame
Δ accumulator — without changing any emulator behaviour, any IR, or any
artifact the emulator reads. "Ships dark": after this project the
emulator links the library and nothing observable differs; one K=1
stock gate proves it. The value is that P31–P33 debug lowering, not
string semantics.

Hi Claude! Solo implementation session; the user reviews at the plan
gate and at the landing. Read docs/METHOD.md first. Design of record:
**docs/Project29/StringsDesign.md** — read all of it; §2.4 (byte
semantics), §3 (residues), §6 (the arena form and the Δ rule) are what
you implement; §1 is why. Also: docs/Project29/Census.md §10 (the
manual review of WCMV/WCMP/WBLM/WMSP/STASP), docs/Mapper.md (the
mapper's three-call surface you extend — do not add a fourth),
docs/EmulatorDivergences.md (B-4, G-1..G-3 are residue-class differences
you must REPRODUCE, not "fix"), emulation/tests/helpers_selftest.cpp + its
run script (the self-test style), docs/Provenance.md (verify your tree
FIRST). TREE VINTAGE: main after P28 (ir 4) — state the commit.

## Part 1 — plan gate (no code except reading)

1. **API draft**, as a header in the report: `hw/strings/EagleString.hpp`
   — construct from literal (address+len), located fixed, located
   varying, substr, char; `append`; `assign_fixed(dst_bp, n)`,
   `assign_varying(len_word_addr, cap)`; `pad_equal`; and ONE residue
   function per replaced instruction with the exact signature
   (`residues_after_copy(Machine&, dst_bp, n, src_bp, len)`,
   `residues_after_compare(...)`, `residues_after_blm(...)`), each
   documented against the §3 table and the EagleSpecial.cpp lines it
   mirrors. Byte access goes through `Memory` exactly as
   EagleSpecial does (segment checks included — G-3 must still throw).
2. **Mapper arena form**: how it registers with the existing module
   (Mapper.md §1: `equivalent`, `frame_precedes`, `clone_location` — the
   arena form participates in the first, is irrelevant to the second,
   and REFUSES in the third), the row struct `(arena_addr, capacity,
   length, wfp, master_addr)`, bind/unmap operations keyed as §6.3 says,
   the binary search, the invariants asserted on bind. State where the
   19 static rows come from (an artifact loaded at launch, P33's; for
   P30 a test fixture).
3. **Δ accumulator**: per-frame (keyed by wfp) outstanding-claim total
   with `claim(+2·ac)`, `release(old−new)`, `frame_exit()`; and the
   check `master_wsp − clone_wsp == Δ(frame)`. Pure bookkeeping; not
   wired into compare_pair in P30.
4. **Test plan**: which properties are brute-forced and over what
   operand space (lengths 0..300, counts equal/shorter/longer, blank
   padding, truncation; WCMP with equal/prefix/mismatch-at-k; WBLM copy
   and the self-overlapping fill; residues checked FIELD BY FIELD
   against running the real instruction through EagleSpecial on a
   scratch Machine+Memory). Include the negative tests: segment
   crossing throws, capacity overflow faults, `clone_location` on an
   arena address refuses, bind invariants.
5. **Gate plan**: the emulator links the library; a K=1 stock leg is
   byte-identical in pairs/endpoints to 042's k1fo line.

STOP AND REPORT at the plan gate.

## Part 2 — implementation

- `hw/strings/` (new directory): `EagleString.{hpp,cpp}`, the arena
  form (inside the Mapper module's file set, not a parallel table),
  `ClaimDelta.{hpp,cpp}`; Makefile wiring.
- `tests/strings_selftest.cpp` + `tests/run_strings_selftest.sh`: red
  on a deliberately broken residue (prove it has teeth, as
  helpers_selftest did), green on the library.
- No IR, no lower.py, no IRExec, no emulator behaviour change. The
  emulator's WCMV/WCMP/WBLM stay exactly as they are — the library
  MIRRORS them; if you find they should share code, hoist into
  `EagleInstruction`-style helpers ONLY with a K=1 stock gate proving
  byte-identity (the P26 `div` hoist precedent), and say so.

## Part 3 — validation

- Self-test green (and red on the broken build).
- K=1 stock leg on the runner (task 043 via bin/task_source.sh, JOBS=3,
  two legs like 038/039 is enough: k1fo + play-st vs the 042 baseline).
  Bar: 0 div, same endpoints, pair counts in the usual band.

## Boundaries — BINDING

1. Scope = the library + tests + the dark link. Nothing else.
2. Semantics from EagleSpecial.cpp / EagleStack.cpp and Census §10,
   cited per function. Residue-class divergences (B-4, G-1..G-3) are
   reproduced, not corrected.
3. Do not add a master→clone lookup to the arena form. Ever.
4. Design-vs-reality: STOP AND REPORT — a residue the §3 table gets
   wrong against the instruction, a Mapper surface that cannot host the
   form without a fourth call, a self-test that will not go red.
5. Deliverables: the library, the tests, task 043 result, REPORT.md +
   worklog, CURRENT_STATE/NextSession (or an explicit integrator note),
   TREE VINTAGE, and any addition to EmulatorDivergences.md.
