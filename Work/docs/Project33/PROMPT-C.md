# Project 33-C — chase the 047 play divergence (P33-B is otherwise done)

GOAL: find and fix the one divergence task 047 showed on the P33-B tree,
re-run the battery (047b), and hand P33-B to the integrator for merge.
Nothing else: no new lowering, no design change unless the cause forces
one (then STOP AND REPORT).

Hi Claude! Read docs/METHOD.md first. Start from branch
**p33b-arena-temps** (HEAD as pushed by P33-B; state its commit). Read
docs/Project33/REPORT-B.md + REPORT_worklog-B.md (what P33-B built),
docs/Project33/censusB_raw.txt and p33.ledger (the 19 groups, the 8
copy-outs), docs/Project29/StringsDesign.md §3–§6, docs/Run.md
"Capture" (QUEST_CAPTURE / QUEST_CAPTURE_DEST — the footprint-diff
tool you will use), and results/047-p33b-arena-temps/{play.divdump,
play-st.divdump, play.hooks, play.maxclaim} on main.

## The evidence (from 047)

- 14/16 legs OK; every P33-B verdict exact (ir 6; embeds 557/2,436;
  WCMV/WMSP/STASP 0/0/0; 57 twins; clone claim=0 everywhere; derr /
  derr-emu / forced as wanted; k1fo book and stock K=1 0 div).
- `play` (book K=50) and `play-st` (stock K=50) BOTH diverged at
  FIND_OBJECT+0xAA (7016A939), frames BEING_ATTACK → SIGNAL_TURN →
  START_TURN, ~2.0M / 3.5M pairs in. `blk_mismatch=1`, block ordinals 28
  apart (master 102343130 vs clone 102343102): a loop ran a different
  number of times on the two engines before the pairing broke.
  Differing registers: book ac1 700615B3 vs 70061574 (Δ 63), ac3
  700011BC vs 74003EFC; stock ac0 00003D8C vs 00003D6C (Δ 32 — looks
  like an X coordinate), ac1 70034562 vs 7003452C (Δ 54 — a pointer into
  the shared-data record). Outstanding claims 0/0; claim-free wsps
  equal (700011C8).
- 046's play leg (P32 tree, same driver) passed this point. So the
  cause is in P33-B's changes: the twin copy-outs into located targets,
  the twin appends' residues at tail cuts, the arena page mapping, or
  the Mapper claim-insertion layer (though claims are 0 at the site).

## Hypothesis to test first

One of the 8 **copy-outs from a twin into game state** wrote different
bytes (or a different length word) on the clone. INIT_OBJ_TBL's copy-out
into the object table (row 12 — the group with the intermediate
survivor; runs at every login; NOT searched by k1fo, so the short gates
cannot see it) is the prime suspect: a wrong object-table entry changes
how many entries FIND_OBJECT's search loop walks, which is exactly a
28-ordinal skew, and the registers at the site are position/record
values, not string residues.

## Method

1. **Localise by memory, not by pairs.** With the play driver, arm
   QUEST_CAPTURE at INIT_OBJ_TBL's entry with QUEST_CAPTURE_DEST at the
   object table (address from censusB / the P34 record tables:
   OBJ_PTR, stride 20 / stride 9), run master and clone, diff the
   RETURN vs NATIVE blocks. Do the same for the other 7 copy-out targets
   (list them from p33.ledger: the located destination of each
   `[@…, n varying] = …twin…` statement) at their routines' entries. The
   first target whose bytes differ is the site.
2. If no copy-out differs: capture the arena twins themselves at the
   consuming rt_call (the pushed varying image vs the master's stack
   temp — the memory oracle StringsDesign §6 lists as optional; a
   one-off check here) and the 8 copy-out sources.
3. If still nothing: bisect by slice — regenerate with `--strings-slice
   6` (P32 artifacts, no twins) under the P33-B binary and run play; if
   it passes, the twins are the cause; if it diverges, the P33-B binary
   (Mapper layer / arena mapping) is — then bisect the checker flag off.
4. Fix at the site. Expected shapes: a length word written from the
   wrong twin or with the wrong min; an append residue (ac2/ac3) at a
   tail cut that the copy-out's count reads; the `release`'s `ac1 :=
   wsp` masking a value the copy-out used; a capacity/stride overlap in
   quest.arena writing into a neighbouring twin.
5. Regression: add the failing shape to tests/strhooks_selftest or the
   strings self-test (teeth), K=1 book + stock gates, then **task 047b
   on main** (047's legs; the play legs are the proof). Landing bar:
   16/16, 0 div.

## Boundaries — BINDING

1. Files as P33-B's (branch p33b-arena-temps); no lower.py grammar
   change unless the fix requires one — then STOP AND REPORT.
2. The fix must be at the cause; no drops, no masks, no "the play leg is
   flaky." A divergence 2M pairs in with a 28-ordinal skew is a memory
   write, and the capture will name it.
3. If the memory oracle is what finds it, say so in the report — that is
   the argument for landing it as a standing K-gated check.
4. Deliverables: the fix + regression test, 047b GREEN, REPORT-C.md
   (the localisation trail: which capture differed, why), worklog,
   Provenance if artifacts change, and the integrator notes P33-B
   already owes (Mapper.md §1.4 / StringsDesign §6 for the two checker
   findings; "ground: t@b.k; readable: p@b ≡ t@b.last").
