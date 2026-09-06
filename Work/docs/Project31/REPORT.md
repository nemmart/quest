# Project 31 — located strings (ir 5): 652 sites lowered; embeds 2,322 → 1,670

Session Sep 6 2026, solo implementation; plan gate (Census.md) and
landing reviewed by the user. TREE VINTAGE: branch p31-located-strings
rebased onto main @ 64f0cc3 (P30 merged; artifacts verified against
docs/Provenance.md). Battery: task 044 (042's 15 legs + the P31 verdict
lines, via bin/task_source.sh) — queued at push; local gates in §5.
Plan-gate record: docs/Project31/Census.md (§1 findings, §2 site list,
§3 grammar, §4 residues, §5 diamonds, §6 liveness, §7/§9 rulings, §10
Phase B findings); artifact: p31.tsv (+ p31.ledger for readers);
emitter record: strings.ledger.

## 1. Outcome

**652 of 776 candidate sites lowered** (528 `v = 'lit'`, 6 `fixed =
'lit'`, 48 constant-length copy-outs `v = [@buf, n]`, 10 min-shape
`v = t`, 15 fixed pad-fills, 2 `fixed = varying`, 31 compares, 12 word
fills) to the ir 5 statements `[@a, n varying] = piece`, `[@a, n] =
piece`, `ac1 = cmp(a, b)`, `words(@d, k) = words(@s, k)`, in both modes,
executed on the P30 library with the StringsDesign §3 residues. **Embeds
2,322 → 1,670 (book), 4,201 → 3,549 (stock)** — the plan-gate
prediction exactly. Blocks 13,507, gotos 11,442, asserts 2,273, rt_call
987 (unchanged); sync list unchanged (quest.synclist.p27, 13,510 — the
40 min diamonds stay listed by ruling). 124 sites refused with their
reason in the artifact (86 P32 copy-outs, 15 P32 substr, 9 P32 chain
capacities, 6 P33 temps, 6 needing the t-place form, 1 residue, 1 call
result). `--strings-slice 0` reproduces the P28 artifacts byte for byte
except the version line.

## 2. Rulings taken (user, Sep 6)

Pre-gate: F7 → finding (verified, stronger form); min diamonds kept
(no delist, statement in the join, synclist delta 0, diamonds
censused); WCMP → `ac1 = cmp(piece, piece)` root statement; literal
verification lazy at first execution through the normal read path.
Gate: O1 `[@a, varying]` capacity-less READ form; O2 the 12 fills as
`words()` + comment (`fill()` is the readable layer's rewrite); O3 no
`substr` in P31; O4 register-form operands are legal (`[@ac2, 13]`,
`words(@ac3, 90) = words(@ac2, 90)`); O5 32 K as a refuse threshold on
constants — and, after F-B1, NO runtime length-range fault (the master
does not fault). Bar 1,670 / 3,549 accepted. Landing: keep the library
fix + self-test 1c′, erratum in P30's REPORT.

## 3. Findings

- **F7 verified**: `SYSCALL` = `XJSR @[6]` → stub 7017FDEC ends `LDAFP 3`
  on both return paths, so ac3 = wfp after every syscall (stronger than
  "preserved"). 0 census lines changed. (Census §1.1)
- **Two P29 tool defects** (Census §1.2): byte-EA windows dropped the
  base register's producers (744 windows grow); WUSGT/WUSGE/WULEI are
  skips, not register writers (51 classifications change; the WMSP claim
  sizes for P33 are now clean). StringsDesign §1.3–1.5 re-checked on the
  regenerated census: all hold (19/19, 19/19, 57/57); count correction
  11 ?WRITE_SCREEN groups + 8 copy-outs (not 12 + 7).
- **All 12 WBLMs are fills** (dst − src = 1 or 2 words), none a record
  copy (Census §1.3).
- **F-B1 (library)**: `EagleString::varying()` zero-extended the length
  word; XNLDA sign-extends (EagleGeneral.cpp:51–52). Live at
  DISPLAY_INVENTORY 7016816B (a 0xFFFF field on the login path: the
  master compares a one-byte descending string). Fixed with citation;
  self-test 1c′ RED on P30's line, GREEN after; erratum in
  docs/Project30/REPORT.md. (Census §10)
- Indirect EAs must render as `R[...]` (7 sites through by-reference
  pointers) — the census modelled `@[ac3-12]` as a plain load; fixed.
- Design wording proposals for the integrator (Census §9): §2.1 the
  capacity-less read + register addresses; §2.3 `words()` for WBLM and
  the `cmp` statement; §7 table; §2.4 "length words are read
  sign-extended".

## 4. Implementation

- **tools/string_sites.py** (`--p31 <ledger> --p31-tsv <tsv>`): F7 arm;
  the T1/T2 fixes; per-site renderer (`ir_word`/`ir_bp`/`piece_of`),
  fold analysis (contiguous pure run + absorbed length store, LDAFP
  kept), leaf checks (register last-writer rule; memory stores in the
  window chain; fold consistency), register-form fallback, min-diamond
  finder + predecessor census, categorised refusals, ledger + tsv (with
  dis/blocks/mem sha256 header, fold pcs, IR text).
- **tools/lower.py** (ir 5): `--strings-sites p31.tsv --strings-slice
  {0..3} --strings-census`. The tsv is consumed as an artifact:
  provenance checked, every fold pc re-validated against the dis (in the
  block, before the site, a foldable mnemonic, nothing but LDAFP inside
  the run) — a mismatch dies; the only soft refusal is the slice gate.
  Folded instructions echo after `<-`; `strings` provenance header line
  when slice > 0; `ir 5` header.
- **hw/IRExec.cpp/.hpp** (ir 5): `Piece`/`StrOp`/`Stmt::STRING`; parser
  for the three statement kinds and four piece forms with the loader
  checks of IR.md §5.8; executor materialises registers, calls
  `assign_fixed` / `assign_varying` / `compare` / `block_move` (site =
  block start for the per-byte segment check), re-reads; lazy literal
  verification; first-execution coverage line per statement; refuses
  `ir 4`.
- **hw/strings/EagleString.cpp**: `varying()` sign-extends (F-B1);
  **tests/strings_selftest.cpp** case 1c′.
- **docs/IR.md** → ir 5 (§1, §2, §3, §5.6, §5.8 new, §9). Provenance.md
  post-P31 table + regen commands. Artifacts quest.ir2.book 281fd501…,
  quest.ir2.stock 1122b985…; docs/Project31/strings.ledger.
- **tasks/044-p31-located-strings.sh**: 042's 15 legs (source
  p31-located-strings) + verdict lines: embeds 1670/3549; string
  statements 652/652 by kind (534/31/12); strings.ledger == p31.tsv
  EMIT set (652/0 vs 652/124); synclist 13,510; literal mismatches 0;
  string-statement coverage from the IRExec lines with assign_varying,
  cmp and words REQUIRED live in play/play-st. Every expected value was
  produced by running its exact command against the artifacts of record
  (METHOD §10).

## 5. Validation (local, this 1-core box; K=1 strict, QUEST_SYNC_K=1)

| leg | slice | cfg | pairs | IR blocks live | div | string statements live |
|---|---|---|---:|---:|---:|---|
| k1fo | 0 | stock | 297,847 | 1,363 | 0 | 0 (regression) |
| k1fo | 1 (lit) | book / stock | 322,111 / 315,630 | 1,363 / 1,362 | 0 / 0 | 30 / 30 |
| k1fo | 2 | book | 314,272 | 1,353 | 0 | 39 |
| k1fo | 3 | book / stock | 307,254 / 325,054 | 1,352 / 1,346 | 0 / 0 | 51 / 49 |
| k1play | 3 | book | 8,833,425 | 1,943 | 0 (clean) | 58: assign_varying 43, cmp 10, words 4, assign_fixed 1 |

The first slice-3 leg diverged at 7016816B (F-B1) — a design-vs-reality
stop resolved in the library, not in IRExec; after the fix every leg is
0 div. Strings self-test: GREEN (18,410 cases), broken build RED (teeth).
Negative posture: an `ir 4` file is refused; the loader refuses a
varying lvalue without capacity, a non-`ac1` cmp lvalue, `words()` with
different counts, a literal without its address (each exercised by hand
during development).

## 6. Tempting adjacencies NOT taken (boundary 1)

The copy-outs with a chain-total count (86 — P32's `append()` tracking),
`substr` pieces, the t-place form for the 6 COMPUTED-OPERAND /
OPERAND-DEAD sites, `fill()` and `==` as IR statements (readable layer),
option-B absorption of the 40 min diamonds (flat-graph world), P33's
temps, any checker change.

## 7. TODO / next session

- Integrate after 044: merge p31-located-strings; Provenance is written
  for the branch's artifacts; StringsDesign wording per Census §9 +
  "sign-extended length words" (§2.4) for the integrator.
- P32 (append chains): the artifact discipline transfers — extend
  `--p31` into the append population; the 86 copy-outs need the chain's
  appended length (from the residues), the 15 substr sites the byte
  offset form, 6 sites the t-place form.
- P33: claim sizes from docs/Project31/census_raw.txt (T2-clean).
