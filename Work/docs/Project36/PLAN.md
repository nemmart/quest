# Project 36 — the Part 1 plan gate, as put and as ruled

Date: Sep 9 2026.  TREE VINTAGE: main d86f89a; branch p36-died.
Baseline at the gate (tree as handed over, before any change):
PICK_X_Y 64/64, UPDATE_SCREENS 72/72, REFRESH_SCREEN 61/61 = 197/197 MATCH,
primary and --folded, identity slot bijections.

## Rulings requested and received

- **A — accepted.**  `BIT / BIT_SET / BIT_CLR / BIT_PUT`, MSB-numbered, the
  record word as an lvalue.  The bitfield-struct alternative was rejected: the
  `16*offset + n` arithmetic is the thing being modelled, not an
  implementation detail to hide.
- **B — accepted, as a DEFECT FIX and not an eighth equivalence.**
  `ircmp.succs_of` returned no successors for a block ending in an embedded
  LCALL/XCALL/LJSR/XJSR, so the DFS stopped and 43 of DIED's 73 blocks fell
  back to ADDRESS order — emission order on the translator's side, a much
  weaker check.  Calls return, so the block falls through as WSAVS's does.
  DIED is now 73/73 reached; `--selftest` PASS on all four routines.
- **C — accepted.**  No eighth equivalence; `wp(ac2, d)` compares verbatim
  (the stricter bar), so the 22 relocated-frame statements get no bijection
  forgiveness.
- **D — accepted.**  DIED's signature corrected (varying by reference, second
  argument unused) and recorded as a P35 stub error under METHOD §11.
- **Stage order — accepted**, register relocation second rather than last
  (P35 §7 put it fifth): the ac2 stretch changes the frame-slot spelling of
  everything downstream, so getting it right early is cheaper than re-matching
  later.
- **Pragmas (user, mid-session).**  Where a rule cannot be derived, the source
  may assert it (`#pragma fp ac2`, `#pragma base ac3 REGION`) rather than the
  translator guessing.  Cost: a pragma is an input, not a derivation, and
  cannot be falsified by the comparator — so each is recorded in
  CODEGEN_RULES as an unexplained directive with its instruction evidence, and
  the COUNT is reported as a first-class number next to the match percentage.
  **P36 used none (count 0).**
- **Twin sizing (user, mid-session).**  If `CAT` alone cannot produce the
  claim arithmetic, design a named sizing intrinsic rather than fudging a
  constant, and bring it as a finding either way.  It turned out to be
  derivable — see REPORT §5 finding 2; the missing piece is `LEN()`, not a
  sizing form.

## Success criteria as stated in advance

DIED ≥ 90% MATCH+RENAME primary (target 100%), same folded; every DIFF
classified; `unknown` 0; the P35 three 197/197 throughout; `--selftest` green
after every rule change; native C99 + C++17 clean.

**Outcome: not met.**  Stages 2 and 4 are unbuilt, so DIED translates only as
a prefix and no match number is quoted (REPORT §1).  The criteria that were
met: the P35 three stayed 197/197 across every commit, `--selftest` PASS,
native compiles clean, pragma count 0.
