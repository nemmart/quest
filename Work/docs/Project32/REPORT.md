# Project 32 — append chains (ir 5, slices 4–6)

Session Sep 6 2026, solo; plan gate and landing reviewed by the user.
Branch `p32-append-chains` (on main c88c0ff: P31/P33-A/P34 merged at
6c3c28c, the Follow.java LJSR-I.GOTO fix 09f6593, the
?UNSIGNED_TO_CHAR design correction 08afd77/c88c0ff). Design of record:
docs/Project29/StringsDesign.md §2–§5.1; ground: docs/Project32/Census.md
(Phase A, the plan-gate document — read it first for the findings).
Gate: task 046 (044's 15 legs + verdict lines), queued on main.

## 1. Outcome

- **P31 correction, landed first as its own commit** (docs/Project31/
  Census.md §11): string_sites.py's evaluator kept frame slots across
  calls that wrote them; 17 P31 statements carried a pre-call constant
  where `?UNSIGNED_TO_CHAR` had written the digit count. Fixed (passed
  slots dropped at the call; a value reloaded across any call is never
  rendered — memory is read instead); p31 649 EMIT / 127 REFUSE (3
  refuse, 14 re-rendered as the located read of the call-written string);
  artifacts 649 statements, embeds 1,673 / 3,552; K=1 book + stock gates
  green (0 div, 308,923 / 299,387 pairs).
- **P32 census** (`string_sites.py --p32`): 944 sites, every one
  expressible (448 expression form, 415 mixed, 81 full register form);
  211 scratch buffers, 446 sequences, the longest 2 pieces — the compiler
  concatenates pairwise, and no sequence crosses a block (fact of record);
  335 copy-out totals verified against the appended pieces, 0 mismatch.
  `?UNSIGNED_TO_CHAR` returns nothing (design error, corrected on main);
  tail splits are bounded concatenation (`src_count` = remaining room).
- **Emitter** (`lower.py --strings-sites32 docs/Project32/p32.tsv
  --strings-slice 4..6`): the P32 rows join the P31 rows; slice 3
  reproduces the P31 artifacts byte for byte; slices 4/5/6 add 677/134/
  133 statements. Every fold pc re-validated against the dis (0 dies);
  the strings ledger lists 1,593 emitted / 0 refused and byte-matches
  p31.tsv ∪ p32.tsv.
- **Executor** (hw/IRExec): `<n>` is any pure expr (`Piece::n_expr`,
  evaluated in the statement's context before the library call;
  `check_treads` covers it); the `strings32` provenance line. No library
  change — every P32 statement is `copy`/`compare` with the master's own
  four operands.
- **Artifacts of record**: quest.ir2.book **729 embeds**, quest.ir2.stock
  **2,608 embeds**, **1,593 string statements** (857 literal assignments,
  40 cmp, 12 words), 987 rt_call, 2,273 assert, 11,442 goto; sync list
  13,510 unchanged. Exactly the accepted bar.
- **Drivers**: `play` (Project13/drive.py and Project14/drive_patient.py)
  grew `H`, `1⏎`, `0⏎` so HELP's two-stage chain (7016D5E3..7016D602) is
  a named check (user ruling O1).
- **Docs**: IR.md §5.8 + version history (ir 5, P32 amendment — no
  bump); docs/Project32/{Census, REPORT, REPORT_worklog}.md, p32.ledger,
  p32.tsv; Provenance; CURRENT_STATE / NextSession.

## 2. Rulings taken (user, Sep 6)

1. Correction (a)/(b)/(c) of the evaluator; P31 regenerated at 649/127
   with the 14 LIST_PLAYERS sites emitted as the located read.
2. No readability floor: emit the ground form — expression where it is
   one, `[@ac2, ac0] = [@ac3, ac1]` where that is the truth; readability
   is P34's job.
3. `<n> := pure expr`, negative bp displacements as `-0x…`; no `len()`
   (the prompt's claim was an error).
4. Pairwise concatenation as a fact of record.
5. O1 HELP driver step yes; O2 conditional-writer chains emitted as is;
   O3 the 3 ex-P31 copy-outs stay in P32-COPYOUT; O4 slices as listed.

## 3. Gates (local, 1 core, sequential; task 046 is the record)

| leg | cfg | slice | div | pairs | blk mismatch | end | string first-execs |
|---|---|---|---:|---:|---:|---|---:|
| k1fo | book K=1 | 3 (correction) | 0 | 308,923 | 0 | clean | 49 |
| k1fo-st | stock K=1 | 3 (correction) | 0 | 299,387 | 0 | clean | 52 |
| s4-k1fo | book K=1 | 4 | 0 | 309,780 | 0 | clean | 84 |
| s5-k1fo | book K=1 | 5 | 0 | 323,555 | 0 | clean | 95 |
| s6-k1fo | book K=1 | 6 | 0 | 296,585 | 0 | clean | 112 |
| s6-k1fo-st | stock K=1 | 6 | 0 | 298,650 | 0 | clean | 107 |
| help | book K=50, HELP driver (`H`, `1⏎`, `0⏎`) | 6 | 0 | — | — | I.STOP | 3 in block 7016D5E3 (first piece, continuation, copy-out); the Terrain screen in the session log |

0 literal mismatches in every leg. Pair counts in the 037–044 band
(298k–390k; s6 book 296,585 is 0.7% under the band's low end — idle
heartbeats, timing-dependent on this 1-core box).

## 4. What changed where

| file | change |
|---|---|
| tools/string_sites.py | evaluator provenance (`Src`, passed-slot drop, stale rule, register-tagged `unk`, pre-call snapshot); `--p32`/`--p32-tsv` renderer + chain census |
| tools/lower.py | `--strings-sites32`, 14-column rows with slice/form, slices 4..6, `strings32` header line |
| hw/IRExec.{hpp,cpp} | `Piece::n_expr`; count evaluated at execution; `strings32` provenance; `check_treads` on counts |
| docs/IR.md | §5.8 `<n>` widening, P32 semantics paragraph, emitter slices; version history |
| docs/Project13/drive.py, docs/Project14/drive_patient.py | HELP step in `play` |
| docs/Project31/{Census §11, p31.*, sites.txt, census_raw.txt, strings.ledger} | the correction |
| docs/Project27/assumed-foldable.txt | header regenerated for 09f6593 (content identical) |
| docs/Project32/* | Census, REPORT, worklog, p32.ledger, p32.tsv |
| emulation/quest.ir2.{book,stock} | slice 6 artifacts |
| tasks/046-p32-append-chains.sh | the gate (on main) |

## 5. Findings and corrections (METHOD §11) — details in Census.md §1

- The P31 evaluator defect (§1.1) and its 17 wrong statements — the
  battery could not see them (blocks not executed); the census now
  refuses to render any value reloaded across a call.
- `?UNSIGNED_TO_CHAR` does not return in ac0; "CALLRESULT" counts are the
  caller's running total kept live across the call (§1.2).
- Pairwise concatenation; 0 copy-out mismatches (§1.3, §1.4).
- Tail splits: `src_count` carries the remaining room (§1.5) — also the
  proof that P31's `min(13, 30)` literal sites are exact.
- Two small tool corrections (Merged provenance, signed bp displacements).

## 6. What P33-B inherits

- p32.ledger's tail lists P33-B's 96 temp sites in 37 blocks (the 19
  claim-group blocks plus the tail joins and copy-outs).
- The chain census machinery (`build_chains`, WSTB accounting, the
  preserved-register substitution, join matching) applies unchanged to
  the `p@b` groups; their pieces are the same `[@ac2, n] = piece` shape
  with the cursor in the arena.
- The evaluator's `precall` snapshot and `Src` provenance are available
  for any renderer.


## Integrator note — task 046 (Sep 6)

results/046-p32-append-chains: 13/15 legs OK, 0 div on all 15; every
P32 verdict value exact (1,593/1,593 statements; 857/40/12; embeds 729 /
2,608; ledger == p31 ∪ p32 by pc set and text; slices 4/5/6 live; 0
literal mismatches; sync list 13,510). The four FAILs are outside the
IR: (1)(2) `derr` want=IR-ASSERT and `clone_assert=2` — the task was
cloned from 044 before P33-A's F2-b landed; WORLD-ABORT via
TERMINAL-ABORT is now the correct end (045b) and the grep counts the
quoted assert text; (3) HELP block 7016D5E3 not live in play/play-st —
the H,1,0 steps appended after L/ESC do not reach HELP in the full play
sequence (validated only standalone); (4) `inj-emu` end=I.STOP —
all-emulated, no IR; the injection at 7016A896 was not reached in that
run (inj, same driver, book, reached it). Merged on the leg evidence;
the driver placement and the F2-b expectations are P33-B's task 047 to
carry.
