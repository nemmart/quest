# Project 27 — DERR cluster compression: skip-chains → assert, interiors delisted

GOAL (roadmap item 2; user rulings Sep 5): every DERR bounds-check
cluster in the game — 2,273 DERR embeds (2,198 `DERR 17`, 75 `DERR
16`) plus the skip blocks that guard them — becomes ONE `assert(cond,
"DERR nn @pc")` statement in its guard block, with the cluster's
interior blocks DELISTED from the sync list. This is the first customer
of the P22 contract (translations ship their own sync list; both
engines count only listed addresses — BlockSyncDesign.md rules 1–2,
which already name "a DERR sink merged into its guard block" as the
canonical coarsening). Success: DERR embeds → 0 for every folded
cluster, blocks and synclist entries down by the interior count, the
full battery green, and a `derr` leg proving the assert and the
master's DERR.TRP abort fire at the same pc.

Hi Claude! Solo implementation session; the user reviews at the plan
gate (end of Part 1) and at the landing. Read docs/METHOD.md first.
Context of record: docs/IR.md (ir 3 — `assert(e[, "msg"])` §… is the
statement you emit; the s/u comparison grammar and `&& || !` are what
the folded condition is written in), docs/Project22/BlockSyncDesign.md
(the delisting contract — read all of it), docs/HWFindings_Sep5.md §6
(the XJMP phantom edge is FIXED; quest.tags is clean), the P26 REPORT
+ Census (the lowering machinery you extend), Project25/PROMPT.md
(house style). TREE VINTAGE: the Sep 5 Work.tgz (branch hw-findings-
sep5 merged: ir 3, the helper fixes, regenerated Disassembled); state
it in the report.

## What a cluster is (census of Sep 5; the ruling that makes the fold sound)

Every DERR in quest.dis sits behind a chain of skips — eight shapes:

| count | skip chain           | DERR | family                          |
|------:|----------------------|-----:|---------------------------------|
| 1,917 | WSGTI / WSGT         | 17   | two-sided range check, signed   |
|   279 | WUGTI / WSGT         | 17   | two-sided range check, unsigned |
|    14 | WSGTI / WSGT         | 16   | two-sided range check, signed   |
|     9 | WSLT / WSLE          | 16   | two-skip                        |
|     9 | WSLT / WUSGE         | 16   | two-skip                        |
|    30 | WSLE                 | 16   | single skip                     |
|    13 | WULEI                | 16   | single skip                     |
|     2 | LDSP (no skip)       | 17   | jump-table out-of-range exit    |

The first seven shapes (2,271 DERRs) are ONE mechanism and are P27's
scope: a connected set of skip instructions and DERRs with exactly two
exits — the DERR sink and ONE continuation pc. Note the two-skip shape's
topology: the first skip's skip-target lands ON the DERR, the second
skip's skip-target lands past it:

```
7015c0cb WSGTI 0,10;      skip → 7015c0ce (DERR)
7015c0cd WSGT 0,0;        skip → 7015c0cf (continuation)
7015c0ce DERR 17;
7015c0cf NLDAI 686,1;     continuation
```

The fold computes the path condition that reaches the continuation and
emits `assert(<condition>, "DERR nn @<derr pc>")` where the first skip
stood; the skip and DERR pcs vanish from the IR block, and every
interior block start (strictly inside the cluster) is delisted.

The LDSP pair (701604D4, 7016D707) is OUT OF SCOPE: it is the
`select`-otherwise exit of a jump table, and folds when LDSP itself
lowers to `goto [labels] idx-lo` with a range assert in front — P28,
with LNDO and the 67 Nova load forms. Part 1's census must reproduce
this table (a difference is a finding).

Why the fold is faithful: DERR is TERMINAL by ruling (RTStubs.cpp
terminal_table: `DERR.TRP` kind 2 = ABORT via Lockstep::abort_world;
Layering.md ruling 7; ERROR_LIFT_SCOPE.md §1 — the trap jumps through
word 39 and never returns). So the master hitting DERR and the clone
failing the assert are the same event: one final verified pair, then
the world stops. Nothing downstream of a DERR is ever observed.

Why delisting needs care: after the fold the interior pcs are no
longer compared. If control could ENTER an interior pc from outside
the cluster (a jump landing on the continuation-side of a skip, or the
condition system dispatching into it), the master would run it
uncounted while the clone has no block there — lockstep would stay in
step and see nothing. The static graph (quest.tags, inverted) rules
out the first; the second (on-error paths) we cannot yet model — hence
the `assumed-foldable` artifact below (user design, Sep 5): fold on
the static evidence, RECORD the assumption per cluster, and let the
future goto-graph project discharge the list.

## The work

### Part 1 — analysis + `assumed-foldable` (plan gate; PYTHON ONLY)

All inputs are text (quest.dis, quest.tags, quest.blocks[.split],
quest.synclist.split, quest.symbols). No Java: per the Sep 5 ruling,
Java is for anything needing the memory image; this needs none. If
something appears to need the image, STOP and report.

1. `tools/derr_clusters.py`: invert quest.tags into a predecessor map
   (target → set of source pcs). Enumerate every DERR; grow its cluster
   backwards through the skips that reach it and forwards to the unique
   continuation. For each cluster record: guard pc, skip pcs (with
   mnemonic), DERR pc(s) + code, continuation pc, interior block
   starts, and for EVERY interior block start its static predecessor
   set. A cluster is FOLDABLE iff (a) it has exactly one continuation,
   (b) every interior block start's predecessors are all inside the
   cluster, (c) the guard block's entry is not itself interior to
   another cluster (chains of clusters fold one at a time, innermost
   first, or are reported). Anything else is UNFOLDABLE with the
   reason. Print the census: clusters by shape (skip-mnemonic
   sequence), foldable/unfoldable counts, interior blocks to be
   delisted, predicted blocks/synclist/embed counts after the fold.
2. **Condition-system cross-check** (partial, cheap, not a proof):
   collect every pc the game hands to the runtime as a handler or
   resumption target — the LPEF/XPEF-of-code-address before
   `O.SET`/`P_DEFON`/`DEF?ON`-class calls, the dispatch-table entries
   Follow already knows (LDSP tables are in quest.tags), the
   `SWAT.REX+0x23/+0x3E` style hand-listed entries — and confirm none
   is an interior pc. Report the set and how it was derived; a hit
   makes that cluster UNFOLDABLE.
3. **Skip semantics table**: one row per skip mnemonic in the clusters
   (WSLE, WSGT, WSGTI, WSGE, WSEQ, WSNE, WUGTI, …) with the emulator
   source citation (file:line), the exact predicate the emulator
   evaluates (signedness READ FROM THE SOURCE, per mnemonic — P26
   Census §2 already has most of these; cite, don't re-derive), and the
   ir 3 comparison it lowers to. The folded condition is the
   conjunction/disjunction along the continuation path; write it
   without simplification beyond what the grammar forces (no algebra —
   the reviewer must be able to read the skips back out of it).
4. **`docs/Project27/assumed-foldable.txt`** — the artifact of record:
   one line per folded cluster, `guard_pc derr_pc code continuation
   interior_pcs... | evidence`, plus a header with the tags sha256 and
   the rule used. This is the checklist the goto-graph project
   discharges later; it must be complete and mechanical.
5. **Sync-list plan**: the delisted list = identity synclist minus
   interior block starts; state the count. Confirm the loader's goto-
   label validation (P26) is against the SHIPPED list, so a goto into a
   delisted pc refuses at load — and confirm no folded cluster is the
   target of any goto (that is condition (b) again, but check the IR
   side too).
6. **Battery plan**: the 034 template (13 legs, JOBS=3) PLUS one new
   `derr` leg that FIRES a DERR — pick a cluster on a driver-reached
   path, arm QUEST_INJECT (RTStubs.cpp:435, `site:type:code`) to push
   the guarded value out of range, expected end = WORLD-ABORT on the
   master and `IR ASSERT FAILED … DERR nn @pc` on the clone at the
   same pc. Name the site and the injected value in the plan.

STOP AND REPORT at the plan gate: census, unfoldable list with
reasons, the cross-check derivation, the semantics table, the battery
plan, predicted numbers. Part 2 proceeds only on the user's go.

### Part 2 — the fold (lower.py) + the shipped sync list

- lower.py consumes `assumed-foldable.txt` (path by flag; refuse if
  its tags sha256 does not match the tags the blocks were built from).
  For a folded cluster the guard block's lowering emits the assert at
  the skip position, continues lowering the continuation's
  instructions INTO THE SAME IR BLOCK (the cluster's interior blocks no
  longer exist as IR blocks), and the block's terminator is the
  continuation's terminator. Everything else in lower.py is unchanged;
  TOTALITY as before (a cluster whose continuation block refuses to
  lower makes the whole cluster refuse — never a half-fold).
- lower.py (or a small `tools/ship_synclist.py`) writes
  `quest.synclist.p27` = identity minus delisted, with a provenance
  header (tags/blocks/assumed-foldable sha256s). blocks.split is NOT
  edited by hand; if the block census needs to reflect the fold, that
  is a regenerated artifact with its own header.
- IRExec: no grammar change expected. If the loader needs to know
  about delisted pcs (block-ordinal accounting, first-execution
  logging), that is a loader change with a K=1 gate of its own, and it
  is reported as such.
- IR.md: an ir 3 note (not a version bump unless the grammar moves):
  DERR clusters, the assert message convention, the shipped sync list,
  a pointer to assumed-foldable.
- Land in slices behind K=1 book + stock gates: (i) the shape-1 single-
  skip clusters, (ii) the two-sided range checks, (iii) anything else
  the census ruled foldable.

### Part 3 — validation

- Local K=1 book/stock/play legs after each slice; the `derr` leg
  locally first (it is the one that proves the fold does what we say).
- Task 040 = 034 template + the `derr` leg, JOBS=3, on the runner.
  BUT: replace the template's `git checkout origin/<branch> -- Work
  Disassembled` with `SRC=$(bin/task_source.sh <branch> 040-…)` and
  point W/DIS at $SRC — the old form staged the branch tree into the
  queue checkout and the runner's results commit carried it onto main
  (results: 037–039 did this; bin/task_source.sh header). Verify the
  tree you were handed against docs/Provenance.md before starting.
  (godspeed: 4 cores, no Java). Verdict lines appended: DERR embeds
  remaining, clusters folded / unfoldable, synclist entries delisted,
  and the `derr` leg's paired pc. Landing bar: 14/14 green, 0 div,
  every DERR embed in a foldable cluster gone, `derr` leg WORLD-ABORT
  with matching assert pc.
- A red battery is STOP-and-report. A `derr` leg where the master
  aborts and the clone does NOT assert (or vice versa) is a design
  finding, not something to tune.

## Boundaries — BINDING

1. **Scope = DERR clusters.** No other de-embedding (LNDO, the 67 Nova
   loads, RT-call decoration, strings) — those are P28. No checker
   changes. No Java.
2. **Part 1 before any code that touches lower.py.** `derr_clusters.py`
   is analysis and may be written in Part 1; it changes no artifact
   the emulator reads.
3. **Refuse, don't guess.** A cluster that does not match the two-exit
   pattern, has an outside predecessor, or trips the cross-check stays
   fragmented and is listed. No "probably fine".
4. **The assumption is recorded, not hidden.** Every fold appears in
   assumed-foldable.txt with its evidence. The report states plainly
   that on-error entry into an interior pc is unmodelled and what
   would discharge it.
5. **Semantics from the source**, cited per mnemonic; the folded
   condition is a transcription of the skips, not a simplification.
6. **Design-vs-reality: STOP AND REPORT.** The `derr` leg disagreeing,
   a cluster shape the census did not predict, a delisting that the
   loader or the checker rejects.
7. Deliverables: tools/derr_clusters.py, docs/Project27/{Census.md,
   assumed-foldable.txt}, the lower.py fold, quest.synclist.p27 (+ any
   regenerated artifacts with headers), IR.md note, task 040 result,
   REPORT.md + worklog, CURRENT_STATE/NextSession updates or an
   explicit integrator handoff, TREE VINTAGE statement, and a runtime
   line for derr_clusters.py (flag if > 10 s wall clock — the Sep 5
   Python-performance note).
