# Project 30 — worklog

## Sep 6 2026 — plan gate

- Tree verified: uploaded Work/c_src byte-identical to repo main 00f641c
  (P28 merge 309585d + P29 docs); Provenance post-P28 prefixes match.
  Branch p30-string-library. Baseline build clean.
- Read METHOD, StringsDesign (all), Census §10, Mapper.md,
  EmulatorDivergences, helpers_selftest + run script, EagleSpecial.cpp,
  EagleStack.cpp WMSP/STASP, EagleGeneral WSTB, Mapper.{hpp,cpp}.
- Findings put to the user: (1) G-3 compares the INSTRUCTION's segment
  (pc>>28) to the byte pointer's (bp>>29) → operations need `site`;
  (2) compare residues cannot be pure; (3) 0x75 decodes to None today —
  codec rows needed, I3 holds; (4) 042's k1fo is cfg=book; (5) mirror
  vs hoist. All five ruled as proposed. API, arena form, Δ, test plan
  and gate approved. Go.

## Sep 6 2026 — implementation

- hw/strings/EagleString.{hpp,cpp}: pieces as spans; copy / assign_fixed
  / assign_varying / append_char / block_move / pad_equal; residues.
  Transcribed loop-for-loop from EagleSpecial.cpp; int32 pointer
  arithmetic kept as the emulator has it.
- hw/strings/ClaimDelta.{hpp,cpp}.
- Mapper.hpp: ARENA_BASE 0x75000000, ARENA_WORDS 0x800000 (so the byte
  form is exactly 0xEA — the full 16M-word segment spans 0xEA/0xEB),
  ArenaLayout/ArenaRow, configure_arena / arena_bind / arena_set_length
  / arena_unmap_frame / arena_row. Mapper.cpp: I3 static_asserts, the
  three decode cases, arena branch in equivalent() BEFORE A (stock mode
  has no records), clone_location refusal BEFORE the empty-records
  shortcut, frame_precedes assert, probe counts a mapped row as a
  mapped range, the event implementations with `arena` trace lines.
- Makefile: two SRCS lines. Build clean, no warnings.
- tests/strings_selftest.cpp + run_strings_selftest.sh. First run
  segfaulted silently: three Rigs on the stack overflow it (Memory has
  a 2 MB permissions array inline) — heap-allocated. Then GREEN first
  time (18,370 cases); the guard cases were strengthened to assert the
  exception text actually appeared (an agreeing no-throw would have
  passed `agree` on its own).
- Mutation pass (11 hand mutations of the library and the arena form):
  10/11 RED immediately; "unmapped row maps through the 0 sentinel"
  GREEN — added two cases with master == bare offset; now RED. Teeth
  build (-DP30_BROKEN_RESIDUE): RED, 1,326 failures.
- helpers selftest GREEN. Local k1fo book K=1: 0 div, end clean,
  pairs 329,763 (band 298k–390k).
- tasks/043-p30-string-library.sh: 039's two-leg shape, 042's sourcing
  (bin/task_source.sh) and sync list (quest.synclist.p27), both
  self-tests as preconditions, 042 baseline lines appended.
- Docs: REPORT.md, this worklog, CURRENT_STATE and NextSession entries.
