# Project 27 — DERR cluster compression: 2,271 clusters → assert, 4,499 blocks delisted

Session Sep 5 2026, solo implementation; plan gate (Census.md) and
landing reviewed by the user. TREE VINTAGE: the Sep 5 Work.tgz
(Work__75_) + Disassembled.tgz, verified against docs/Provenance.md
(17/17 prefixes) and byte-identical to repo `main` @ 29a1c24.
Branch: p27-derr-clusters. Battery: task 040 (034 template + `derr` +
`derr-emu` legs, JOBS=3, via bin/task_source.sh) — queued; see §5 for the local
gates. Plan-gate record: docs/Project27/Census.md; artifact of record:
docs/Project27/assumed-foldable.txt.

## 1. Outcome

**DERR embeds 2,273 → 2** (the two LDSP-fed jump-table sinks 701604D4 /
7016D707 — P28). **Embeds 8,529 → 6,258** (book), 10,408 → 8,137
(stock). **2,271 clusters folded** to one `assert(cond, "DERR nn @pc")`
+ `goto [K] 0` each, in the guard block. **IR blocks 18,006 → 13,507;
shipped sync list 18,009 → 13,510** (`c_src/quest.synclist.p27`,
identity minus the 4,499 interior starts). gotos 13,628 → 11,400.
Grammar unchanged (ir 3 note in IR.md §4a / §9; `assert` — a P25
statement — is now emitted by lower.py). Local K=1 gates 3/3 green
(§5); the `derr` leg pairs the clone's assert and the master's DERR at
7015C48E (§5). New harness knob `QUEST_POKE`.

## 2. Rulings taken at the plan gate (user, Sep 5)

- **F1 = A**: guard block ends `assert(...); goto [K] 0`; the
  continuation K stays its own listed block; only the skip/DERR
  interiors are delisted (13,510). B (absorb K) rejected: buys nothing
  observable; straight-line block merging is the flat-graph world's job
  with the full goto graph.
- **F2 = F2-a**: accept the detach semantics now; **F2-b** (an assert-
  detach paired with a kind-2 terminal → `TERMINAL-ABORT … verified on
  both engines`) is a recorded follow-up (§7). The `derr` verdict is
  explicit about what it accepts: the clone's `IR ASSERT FAILED … DERR
  17 @pc` at the predicted pc AND a non-clean master (START_TURN never
  reached — the detached-master tripwire would fire). Two lines, not
  one.
- **F3 = QUEST_POKE**, with the INJECT hygiene: env-gated, one-shot,
  both roles at the same pc, armed line at launch, malformed spec
  refuses to launch, zero effect unset. Harness, not checker; the
  `derr` leg is its test.

## 3. Findings

- **Machine.cpp:306 arrival counting vs. the prompt's lowering** (F1,
  Census §2): absorbing K into the guard block would skew ordinals
  unless K were delisted too; every K is reachable only from its own
  cluster (2,271/2,271), so either design is sound — A chosen.
- **A folded DERR is no longer a verified terminal pair** (F2): the
  clone asserts inside the IR block and detaches (Lockstep.cpp:436);
  the master's DERR.TRP arrival then hits the detached early-out
  (Lockstep.cpp:151) — no TERMINAL-ABORT line, no abort_world.
  Documented in IR.md §4a as a checker consequence.
- **QUEST_INJECT cannot drive a value out of range** (F3): it
  synthesises an O?SIGNAL raise (RTStubs.cpp:500-540); no knob set a
  register. Hence QUEST_POKE.
- **DERR.TRP terminal: hypothesis checked and refuted (METHOD §10/§11).**
  The review read the master's backtrace frame `DERR.TRP+0x20` as the
  vector's landing pc and inferred the entry-keyed kind-2 terminal had
  been dead code since M3a. The bytes say otherwise: word 39 = 0x01DB →
  page-0 stub 700001DB `AED9 0017 EB40` = LJMP pc-relative → 700001DC +
  0x17EB40 = **7017ED1C, the DERR.TRP entry**; DERR.TRP opens with
  `LDSP 3,[pc+0x58]`, a jump table on the DERR code, and +0x20 is the
  code-17 arm holding the O.SERROR call (hence the frame). Behavioural
  proof: the same poke on an ALL-EMULATED pair (`derr-emu` leg, §5) →
  `TERMINAL-ABORT at 7017ED1C, verified on both engines`, WORLD-ABORT.
  The terminal is live; F2 is purely the IR-side ordering (clone
  assert-detach → compare_pair's detached early-out, Lockstep.cpp:151,
  returns before the terminal-pair branch at :281). F2-b stays as
  ruled: pair an assert-detach with a kind-2 terminal → TERMINAL-ABORT.
- **NEW checker finding from the control leg**: the ABORT readout's
  stack indexing is off by one word — it printed `top stack wides:
  C48E0000 00007015` for (number, faulting pc) that are `00000011
  7015C48E` (Lockstep.cpp:288-289 reads `w-1`/`w-3`; the DERR pushed
  address then code, wides sit at `w-2`/`w-4` under the wsp convention
  in force). Cosmetic (the abort itself is right), never exercised
  before today (no run had reached a DERR), not fixed here (checker;
  boundary 1) — bundled with F2-b.
- **The master's DERR path had never executed** (METHOD §3 evidence):
  DERR 17 → DERR.TRP+0x20 → O.SERROR+0xA → DEF?ON+0x46 → ?FATAL+0x1E2
  → **`Unimplemented system call 0351`** (octal; decimal 233, not in
  os/AOSVSSymbols.cpp). The master dies there — a non-clean, FATAL-
  class end (never reaching START_TURN), which is what the `derr`
  verdict wants. The ?FATAL subtree is permanently excluded (METHOD
  §13); recorded, not fixed. `SYSCALL 0351` is a candidate for the
  syscall worklist if a real DERR is ever to print its DG message.
- **Condition-system cross-check clean**: 26 O.ON handler addresses +
  22 I.GOTO resume targets + 0 code-address EF operands; none inside a
  cluster (Census §3). The unmodelled residue — an on-error entry into
  an interior pc the text scan cannot see — is stated in the artifact
  header and discharged by the goto-graph project.
- **Interior block executions drop out of the pair count** as
  predicted: k1fo 382,922 → 322,425 pairs, IR blocks executed 1,575 →
  1,350 (the interiors no longer exist on either side; blk_mismatch 0).
- **Emission is exactly on prediction**: 13,507 / 6,258 / 11,400 /
  2,271 / 13,510 — every number in Census §2/§7 (option A) hit.

## 4. Implementation

- **tools/derr_clusters.py** (Part 1, analysis only, 8.5 s): inverts
  quest.tags; grows each DERR's cluster through skip predecessors
  while the exit set stays ≤ 1; FOLDABLE iff one continuation, all
  interior predecessors inside, guard block not interior to another
  cluster, no interior gate (BlockSync gate derivation transcribed),
  no condition-system entry; census by shape; option-A/B accounting;
  IR-side goto-label check (`--ir`); writes assumed-foldable.txt with
  tags/dis/blocks sha256 and per-cluster evidence (skips, interior
  predecessor sets, the assert text). Constants render as lower.py's
  `0x%08X` so the artifact text is byte-comparable to the emission.
- **tools/lower.py**: `--assumed-foldable PATH --tags PATH` (refuses
  on tags/dis sha mismatch), `--synclist-in/--synclist-out`. Guard
  blocks are identified by terminator pc; interior starts are HELD
  BACK from emission and emitted only if their guard refuses
  (totality — never a half fold; 0 refusals in practice). At the guard
  skip: successors must all be cluster members or K; the condition is
  re-derived by running lower_one on each skip (`skip_test`, with the
  block-start-keyed CFG — the first cut looked successors up by skip
  pc and folded only the 203 guard-is-block-start clusters; caught by
  the 2,068 refusals, fixed) and must equal the artifact's text;
  emission `assert(cond, "DERR nn @pc") ; P27 fold of <skips>` then
  `goto [K] 0 ; continuation`. The shipped list is identity minus the
  interiors of the clusters ACTUALLY folded, with tags/blocks/artifact
  sha256 + IR path header (`#` lines, which BlockSync already skips).
  Without `--assumed-foldable` the emission is byte-identical to the
  P26 artifacts (verified before folding).
- **hw/RTStubs.{hpp,cpp}, os/OSProcess.{hpp,cpp}, hw/Machine.cpp**:
  `QUEST_POKE=<hexpc>:<ac 0-3>:<value>`; parsed at RTStubs::initialize
  with INJECT discipline (exactly three fields, hex pc ≠ 0, ac 0..3,
  strtol base-0 value; else "refusing to launch", exit 2); armed per
  QUEST process (`poke_armed`, never the server); fires in the dispatch
  loop on arrival at pc immediately before the INJECT check
  (Machine.cpp), prints `POKE firing at pc: acN old -> new`, one shot.
  The pc must be a block start for the clone to arrive there (IRExec
  dispatches whole blocks) — documented in RTStubs.hpp.
- **Artifacts**: c_src/quest.ir2.book, c_src/quest.ir2.stock
  regenerated (folded); **c_src/quest.synclist.p27** new;
  quest.synclist.split and quest.blocks.split UNCHANGED (identity list
  and block census stay the record of the program; the P27 list is the
  translation's).
- **docs**: IR.md §4 (sync-list validation note), §4a (DERR clusters,
  checker consequence, POKE), §9 note; Project27/{Census.md,
  assumed-foldable.txt, REPORT.md, REPORT_worklog.md}; CURRENT_STATE /
  NextSession; tasks/040-p27-derr-clusters.sh.

Deviation, stated (same as P26): the prompt asked for three landing
slices (single-skip / two-sided / rest) each behind K=1 gates. The fold
was emitted in one pass and gated whole — three K=1 legs on the full
artifact are strictly stronger evidence than slice gates, and class
bisection (drop a shape from the artifact) was the fallback for a red.
Not needed. The per-slice bisection record does not exist.

## 5. Validation (local, 1-core box, METHOD §15; ~9 min of legs)

All legs: book/stock as tagged, `QUEST_SYNC_LIST=quest.synclist.p27`,
K=1, fail-open driver unless noted.

| leg | cfg | result |
|---|---|---|
| derr (POKE 7015C48B:0:11) | book K=1 failopen | **0 div**, 270,174 pairs, blk_mismatch 0, max_gap 1; clone `IR ASSERT FAILED [block 7015C48B stmt 0]: assert(!(ac0 >s 0x0000000A) && (ac0 >s 0), "DERR 17 @7015C48E")` + `DETACHED at IR assert`; POKE fired on both roles (ac0 1→11); master DERR.TRP+0x20 → O.SERROR → DEF?ON → ?FATAL → `Unimplemented system call 0351`; START_TURN tripwire silent; last verified pair pc=7015C48B blk 269,858 on both |
| derr-emu (POKE 7015C48B:0:11, control) | emu K=1 failopen | **0 div**, 271,374 pairs; POKE fired on both roles; `TERMINAL-ABORT at 7017ED1C, verified on both engines`, WORLD-ABORT — the entry-keyed DERR.TRP terminal fires for a real DERR (§3) |
| k1fo | book K=1 failopen | **0 div**, 322,425 pairs, 1,350 IR blocks, end clean; `BlockSync: 13510/18009 block entries listed, 1865 gates` |
| k1st | stock K=1 failopen | **0 div**, 314,025 pairs, 1,371 IR blocks, end clean |
| k1play | book K=1 play | PARTIAL (the sandbox reaped the detached leg mid-turn, no verdict line): **0 div**, 6,940,528 pairs, blk_mismatch 0, max_gap 1, 1,961 IR blocks executed, no detach/assert. Full play legs (K=50, book+stock) are in task 040. |

Note on the `derr` master end: the master's exit through
`Unimplemented system call 0351` in ?FATAL is a never-executed runtime
path surfacing (METHOD §3 evidence). It is acceptable for the leg's
purpose — a non-clean end with the START_TURN tripwire silent — and is
NOT a statement about the fidelity of the death path; that path is the
excluded ?FATAL subtree (METHOD §13). With F2-b the folded clone would
share the master's TERMINAL-ABORT instead.

Negative tests: (i) an IR block whose goto names a delisted interior
(`goto [7015C48D] 0`) → `IRExec: REFUSE: goto target 7015C48D is not
a listed block start` (IRExec.cpp:493 against the SHIPPED list, as
Census §5 said); (ii) assumed-foldable with a wrong tags sha →
lower.py `Refuse: assumed-foldable tags sha256 … does not match`;
(iii) `QUEST_POKE=7015C48B:9:11` → `QUEST_POKE malformed … refusing to
launch`, 0 pairs run; (iv) unarmed knob: the k1fo/k1st legs above
(zero effect unset).

Battery: task 040 — 034 template's 13 legs + `derr` (book K=1
failopen, `QUEST_POKE=7015C48B:0:11`, want end=IR-ASSERT) + `derr-emu`
(all-emulated control, same poke, want WORLD-ABORT with the
TERMINAL-ABORT line at 7017ED1C) with the
P27 verdict lines (embeds_book 6258, derr_embeds_remaining 2,
clusters_folded 2271, synclist 13510 / delisted 4499, derr: poke
fired ×2 / clone assert at 7015C48E / master DERR.TRP frame ≥1 /
START_TURN tripwire 0, master_end line; derr-emu: TERMINAL-ABORT at
7017ED1C). Landing bar: 15/15 green, 0 div. Result: pending on the runner (results/040-p27-derr-clusters).

## 6. Corrections recorded (METHOD §10/§11)

- derr_clusters.py's first `hexc` rendered `0xA`; lower.py's renders
  `0x0000000A`. Aligned to lower.py before the artifact of record was
  cut (the cross-check would have refused every fold).
- lower.py `skip_test` first looked CFG successors up by skip pc
  (guards sit mid-block): 203 folds, 2,068 refusals. Refusals were
  loud and interiors stayed listed — the totality rule worked as
  designed; fixed by passing the guard's block start.
- Census.md predicted the `derr` end as "WORLD-ABORT" under F2-b or
  "master reaches DERR.TRP and dies its own way" under F2-a; the way
  is `Unimplemented system call 0351` in ?FATAL (§3).

## 7. Tempting adjacencies NOT taken (boundary 1) / follow-ups

- **F2-b** (checker): in compare_pair, order the kind-2 terminal check
  ahead of the detached early-out when the clone detached at an IR
  assert in the same batch, printing TERMINAL-ABORT with the assert
  text — restores "one final verified pair, then the world stops" for
  folded DERRs (the terminal itself is live, §3). Fix the (number,
  faulting pc) readout's word offset in the same change. User-ruled
  follow-up.
- Option B (absorb K, chains collapse to 1,578 merged blocks; 11,239)
  — the flat-graph world's block merge, later.
- The LDSP pair (P28: `goto [labels] idx-lo` with a range assert),
  LNDO, the 67 Nova loads, RT-call decoration, strings, stack writes.
- `SYSCALL 0351` on the ?FATAL path (terminal subtree; excluded).
- The goto-graph project discharging assumed-foldable.txt's
  on-error-entry assumption (artifact header names the rule).
- BlockSync/IRExec loader changes: none needed (the shipped-list
  validation already existed — IRExec.cpp:435/493/508).

## 8. TODO / next session

1. Read results/040 (14/14, 0 div, P27 verdict lines) → review +
   integrate p27-derr-clusters; update Provenance.md (ir2.book/stock,
   new synclist.p27; blocks.split/synclist.split unchanged).
2. F2-b ruling/implementation if wanted.
3. P28: LDSP jump tables (+ the two DERR sinks), LNDO, Nova loads.
