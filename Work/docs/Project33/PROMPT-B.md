# Project 33-B — `p@b`: the 19 WMSP claim groups on the executor side

GOAL: lower the 19 dynamic-length concatenation groups (57 WMSP claims,
19 STASP releases, their pieces and copy-outs — the last WCMVs that are
not P32 scratch-chain pieces) as StringsDesign §5.2 says: `p@b = ""`,
appends, `[@v, n varying] = p@b` / `rt_call ?WRITE_SCREEN(p@b, …)`,
`release`. The clone keeps each `p@b` at its fixed arena address; the
master is unchanged; the checker half (P33-A: hooks, ClaimDelta, the
Mapper arena form wired into compare_pair, F2-b) is already on main and
green with no clone-side strings. After this project the string family
is gone from the IR: embeds ≈700 (post-P32) → ≈600, and the remaining
WCMV count must be 0 or a listed refusal.

Hi Claude! Solo implementation session; the user reviews at the plan
gate and at the landing. Read docs/METHOD.md first. Design of record:
**docs/Project29/StringsDesign.md** — §2.1 (`p@b`), §2.3 (the statements),
§3 residues, §4 timing/scope, §5.2, §6 in full (the Mapper arena form,
the two events, the symmetric Δ, the STASP rule, `release` =
`assert(wsp == ac1)`, the known edge). The checker you land on:
**docs/Project33/REPORT.md** (P33-A: quest.strhooks, the hooks, the
event queue the clone drains at run_steps top, `QUEST_STRINGS_CHECK`,
the provisional 4 KiB arena layout you replace), docs/Mapper.md §1.4.
The ground: docs/Project31/Census.md §1.5 (the 19 groups re-checked),
docs/Project32/Census.md (P32's disjoint population; its `p@b` list is
your site list), tools/string_sites.py, docs/IR.md ir 5 (+P32's
additions), docs/Project30/REPORT.md (the library: pieces as spans; the
arena image is emulated memory so `p@b` is a span too), docs/Provenance.md
(verify FIRST). TREE VINTAGE: main 2085231 (P32 and P33-A merged) — state the
commit. Note: P32's census counts 96 `p@b`-block WCMV sites in 37
blocks — reconcile with the 19 groups / 57 claims (the extra blocks are
the tail cuts: min-skip and post-call blocks carrying copy-out pieces).

## Part 1 — plan gate

1. **Site list**: the 19 blocks (by address), each with: its WMSP pcs
   and claim-size expressions, the pieces in order (as IR pieces:
   literals, located reads incl. the CALLRESULT `[@fp+k, ac0]` from the
   previous block, substr), the consumer (11 `rt_call ?WRITE_SCREEN`, 8
   copy-outs `[@v, n varying] = p@b`), the STASP pc, and the tail cuts
   (the min-skip blocks, the return block) that the residues must
   survive. Reproduce §1.5's 19/19 facts.
2. **Arena layout**: capacity per `p@b` from the operands' declared
   lengths (§2.1) — the sum of the pieces' maxima plus the length word,
   rounded to words; a table with the derivation per block; the
   artifact `quest.arena` (or the columns of quest.strhooks — P33-A
   shipped provisional 4 KiB there; say which file wins) with a
   provenance header; the loader refuses a capacity the census cannot
   bound.
3. **Grammar** (ir 5 → ir 6? state it): `p@<block>` as a piece and as an
   assignment target; `p@b = ""`; `p@b = p@b + piece`; `[@v, n varying]
   = p@b`; `rt_call ?X(p@b, …)` pushing the arena address; `release`;
   loader rules: `p@b` only in block `b`, `p@b = ""` first, no read
   before assignment, `release` only where the strhooks table has a
   STASP.
4. **Residues** (§3): every append sets ac0–ac3/c as the WCMV into the
   master's temp would — with ac2/ac3 as ARENA addresses (the Mapper
   translates them at compare); the copy-out and the rt_call are P28/
   P31 statements; `release` leaves the registers alone. State per
   group which registers are live across each tail cut and confirm the
   Mapper row is mapped there (bound at the block's first WMSP by the
   master, drained before the clone's block runs).
5. **The intermediates**: `temp1 = a‖b; temp2 = temp1‖c` on the master
   are successive appends on the clone; confirm for all 19 that no
   intermediate temp's pointer survives to a rendezvous (§2.1's claim).
6. **`release`**: the `XWLDA 1,[slot]; WSBI 2,1; STASP 1` tail lowers as
   the two loads/arith (already ir 4) and `release` (`assert(wsp ==
   ac1)`); the frame slot the master wrote at `LDASP; XWSTA` is written
   by the clone too (same lowered statement), so the assert holds.
7. **Liveness + battery**: which groups the standing legs execute (DIED
   is driver-reached; HELP, DISPLAY_INVENTORY, LIST_PLAYERS?); task
   047 = 046's 15 legs + P33-A's forced-mismatch leg, flag ON — and
   carry two fixes 046 showed (Project32/REPORT.md integrator note):
   the `derr` leg's want is WORLD-ABORT via TERMINAL-ABORT since P33-A's
   F2-b (and count the assert line anchored, `^IR ASSERT FAILED`), and
   the play driver's HELP step (H,1,0) must be placed where the game
   accepts it — appended after L/ESC it did not reach HELP; validate
   the FULL play sequence locally, not a standalone login→H; verdict
   lines: groups lowered, WCMV remaining, arena rows bound/unmapped
   counts, max live Δ_master with Δ_clone = 0, `release` asserts
   passed, memory-oracle result if enabled. Landing bar: 15/15 (+1), 0
   div, WCMV embeds 0 (or listed), Δ_clone 0 everywhere.

STOP AND REPORT at the plan gate.

## Part 2 — implementation

Emitter (`--strings-slice 7`), executor (`p@b` as an arena span; `= ""`
sets length 0 and reports `arena_set_length`; appends via the library's
`copy` into the arena with the residues; `release`), the layout
artifact and loader, IR.md, artifacts + Provenance; K=1 book + stock
gates with `QUEST_STRINGS_CHECK=1`; a deliberately wrong capacity must
fault at load; task 047 **committed on MAIN**.

## Boundaries — BINDING

1. Scope = the 19 groups + `release` + the arena layout. NOT the
   checker (P33-A owns it; a needed change there is STOP AND REPORT),
   NOT P32's pieces.
2. The strict surface is untouched; arena pointers compare through the
   Mapper; Δ_clone must be 0 at every rendezvous with all 19 lowered —
   a nonzero Δ_clone means a group still claims on the clone, which is
   a refusal, not a tolerance.
3. Refuse-don't-guess; a group that cannot be closed stays embedded and
   is listed (and then Δ_clone ≠ 0 there is EXPECTED — say so per group).
4. Design-vs-reality: STOP AND REPORT — a capacity the census cannot
   bound; an intermediate pointer surviving to a rendezvous; a `release`
   assert that fails; the §6.5 loop edge firing.
5. Deliverables: the arena artifact + tool, docs/Project33/{CensusB,
   REPORT-B,REPORT_worklog-B}.md, emitter + executor + IR.md, artifacts
   + Provenance, task 047 result, CURRENT_STATE/NextSession (the string
   family retired: state the final embed census), TREE VINTAGE.
