# Project 22 — REPORT: Gen-6.0, the block-sync checker (re-sync only)

*Session of Aug 28 2026. Design of record: docs/Project22/BlockSyncDesign.md.
Branch p22-blocksync, merged to main. Battery: task 030 GREEN under the strict
gate (task 029 invalidated, §6).*

## 1. What landed

The sync MODEL was swapped, nothing else. Both engines still emulate
original instructions; rendezvous are now denominated in basic blocks:

- **hw/BlockSync.{hpp,cpp}** — the sync-list loader. Inputs under
  -lockstep (REQUIRED; refuses to run without them):
  `QUEST_BLOCKS=<quest.blocks>` (ground-truth CFG),
  `QUEST_SYNC_LIST=<file>` (the translation artifact),
  `QUEST_SYNC_K=<n>` (default 50; ruling b: env, not flag).
  Validation on load, all paths verified firing in-session: novelty
  refused (entry not a quest.blocks start), gate delisting refused
  (gates = post-call/post-jump/post-syscall block starts + game-range
  terminal starts; 1,865 computed), K range enforced. Identity list
  shipped: `Work/c_src/quest.synclist`, all 13,495 block starts.
- **Machine::block_ordinal** — cumulative count of arrival-transitions
  at listed game-range block entries, ticked in run_steps BEFORE every
  break decision so both roles count identically through spans,
  deferred dispatch, and native transfers. A batch's initial pc is not
  counted (counted at the previous break; syscall-return resumes enter
  through OS code both engines traverse identically).
- **The K-heartbeat** — batch ends at the K-th listed entry since the
  last rendezvous, checked after all gate checks (gates keep
  precedence). Every batch end is a rendezvous, so the counter is
  batch-local by construction.
- **The 500-insn batch is REPLACED** (one sync model): lockstep QUEST
  clients get a 100M-instruction RUNAWAY GUARD whose exhaustion THROWS
  (METHOD §8 — loud, not a parallel heartbeat). Server batches
  untouched; a non-QUEST program duplicated under -lockstep (generic
  capability, never used for Quest) keeps historical 500-insn pairing.
- **Compare surface** (Lockstep::compare_pair): block ordinal compared
  STRICTLY at every pair — no native-span exemption; translated code
  skips instructions, never listed entries. FP ACs (double bits), the
  quads raw-bit shadow, fplr, and fpr joined the surface per ruling Q3
  (always, not FP-blocks-only). Pair trace lines and divergence dumps
  carry blk=.
- **Retained TEMPORARILY**: the instruction-count delta compare at
  pairs (user ruling: free extra checking while both sides emulate;
  **P23 is OBLIGATED to remove it** — translated clones break it).

Untouched: syscall gate, L1↔L2 crossings pairing, L3 door, mirrored
shared pages + compare-on-read + page audit, terminal machinery
(DETACH/ABORT/RETIRE), mediation, mapper/M4 machinery, all wrappers.

## 2. Rulings collected (plan gate, Aug 28)

- **Q1**: K=50 default; K counts GAME blocks only (RT is crossed at
  gate events). Ruled as proposed.
- **Q3**: FP ACs + float status in the surface ALWAYS. Ruled as
  proposed.
- **(a)**: insn-count compare retained in 6.0, recorded as TEMPORARY,
  P23 obligated to remove.
- **(b)**: K via env (QUEST_SYNC_K).
- **Q4 stale-blocks finding (§3)**: user regenerates quest.blocks on
  his end; Disassembled/ untouched by this project.
- **(c) CRLF (post-battery)**: the user's toolchain is Windows Java —
  artifacts may arrive with CRLF line endings; every reader must
  tolerate them. Applied: BlockSync chomps trailing whitespace per
  line and refuses any terminator-line token that is neither a
  1-letter tag nor 8 hex digits (file:line). The incident: a CRLF
  quest.blocks silently loaded with 9 gates instead of 1,865 — starts
  parsed, but every terminator's LAST successor token grew a \r and
  was dropped. Verified post-fix: LF and CRLF both load 1,865 gates;
  corruption refuses loudly; CRLF live smoke 6,506 pairs div=0.
  (>>-based loaders — AddressBook, pushmap, ProbeSuppressions — are
  naturally CRLF-safe: \r is whitespace to istringstream.)
  quest.wpsh_wpop side-note: it is a P16 session artifact, not a tool
  output — no missing tool version; upstream regeneration should
  carry the file along (or a future session commits a generator).

## 3. Q4 precondition — evidence, and a finding

Toolchain rebuilt (OpenJDK 21); full regeneration from QUEST.PR:

- `quest.addrs`, `quest.targets`, `quest.tags`: **byte-identical** to
  the committed artifacts. M4 is clean — zero reachability or
  boundary drift, as expected (M4 redirects are emulator hooks;
  QUEST.PR was never modified).
- `quest.blocks`: differs in exactly **78 lines, every one the same
  class** — committed `WLDAI 0xNNNN` vs regenerated `WLDAI r,0xNNNN`.
  This is the omitted-register-field disassembler bug METHOD §14
  records as fixed by Project 1: **quest.dis was regenerated after the
  fix (78 fixed-form lines, 0 old-form); quest.blocks never was.**
  Raw-word check: 70168C04 renders `WLDAI 1,0x0000C350` under the
  fixed decoder. **All 13,495 block starts and all terminator lines
  are byte-identical** — the block structure, the only thing the sync
  list consumes, is unaffected. USER RULING: he regenerates the
  artifact upstream so future uploads carry everything; this project
  left Disassembled/ untouched. The BlockSync loader reads only block
  starts, terminators, and SYSCALL last-instructions, so the stale
  text is inert either way.

## 4. "Blocks end at gates — verify, don't assume"

Exhaustive check over all 13,495 blocks: all 1,686 LCALLs, 63 XCALLs,
130 resolved LJSRs, and 10 SYSCALLs in the game range terminate their
blocks (SYSCALLs with both skip successors). **One catalogued
exception**: SQR31?3's negative-input error path (block 7015BD6B)
contains an interior `LJSR @[...] # .LIERR` — the single `u` tag in
quest.tags, an unresolved indirect Follow deliberately treats as
fall-through. Never observed live; harmless for 6.0 sync (both
engines emulate it identically if it ever runs); a P23 translation
hazard for that one block.

Related CFG-completeness note for P23: Follow does not model
**ENQT/DEQUE skip edges** (e.g. 70169B23, LOCK_FILE's contended-lock
wait pad, is absent from the CFG entirely; the CRYTZ→DEQUE→CRYTO→
MOV.# lock sequence sits in one drawn block). Dynamically dead so far;
counting by listed-address arrival is unaffected; a block containing
an unmodeled skip cannot be naively translated.

## 5. Stage 0 / Stage 1 in-container evidence

Stage 0 (counting armed, OLD heartbeat live, ordinal in the compare):

| leg | pairs | blk equal | div | notes |
|---|---|---|---|---|
| login | 2,747 | 2,747 (all) | 0 | ~300K listed entries; native-span pairs show insns differing, blk equal — the designed invariant |
| m | 2,834 | 2,834 (all) | 0 | 4,572 redirects, 1,545 gcalls underneath; clean I.STOP detach |

Stage 1 (the swap):

| leg | K | pairs | blk equal | heartbeats | max gap | div |
|---|---|---|---|---|---|---|
| login | 1 | 286,035 | all | 285,623 (gap=1) | 1 | 0 |
| m | 50 | 6,041 | all | 5,562 | **exactly 50** | 0 |

K=1 gap histogram {0: 404, 1: 285,623}: 404 gate rendezvous between
entries, every listed entry paired, **no gap ever exceeds 1** — no
listed entry is skipped between rendezvous. Note K=50 averages ~230
instructions per heartbeat — FINER granularity than the old 500-insn
batch.

## 6. Tasks 029/030 — an invalid GREEN, and what it taught

Task 029 (the first battery) returned DONE + "P22 BATTERY GREEN" with
**hollow evidence**: fo pairs=0, five legs stuck at identical 120
pairs, no endpoints observed, verdict lines interleaved-duplicated.
Root cause (established from commit archaeology + the box): **two
runner loops were live on the runner machine**. Both executed 029
concurrently with no interlock: their leg sequences interleaved writes
into the shared results dir (the adjacent duplicate verdict lines),
their name-based pkills killed each other's emulators mid-leg (the
identical truncated legs), and the surviving loop finished the last
leg alone (one full 328K-pair k1fo) — its late output scooped by a
second results commit seconds after the first loop's DONE. Compounding
factors: 10x driver speed cannot compress the game's wall-clock turn
cadence, and the 029 gate had no pairs floor and no endpoint pin, so
truncation read as success. The 029 result stands in results/ as the
record; its GREEN is VOID.

Task 030 added: a flock overlap guard, per-leg QUEST_PORT (8791–8798)
with wait-for-port-free, drivers patched to honor QUEST_PORT, all legs
at normal speed with real post-driver grace, and a STRICT gate —
div=0, blk_mismatch=0, gaps_over_k=0, per-leg pairs floor, per-leg
endpoint pinned from the P14 §6 matrix. The flock did its job in the
two-loop world: loop-1 took the lock and ran the battery cleanly to
the §7 GREEN; loop-2 bounced off the lock twice (the FAILED markers in
results/030, which the later DONE supersedes). A hardened template
(setsid + lock-fd closed on emulator launch, port-scoped fuser kills)
is parked in tasks/hold/031 for future batteries. The duplicate runner
loop is the user's to remove on the box.

Standing repo note: bin/battery.sh references a nonexistent
Project14/drive.py; tasks have rolled their own legs since 026. Left
as-is per user ruling (record-only).

## 7. Recalibration gate — task 030 (GREEN, strict gate)

Runner result 688415a (08:19:29). Every leg: div=0, blk_mismatch=0,
gaps_over_k=0, pairs above floor, endpoint pinned. Master == clone by
construction throughout — the checker implementation verified against
itself under every regression shape.

| leg | K | pairs | blk mismatch | K-heartbeats | max gap | endpoint (want) |
|---|---|---|---|---|---|---|
| fo (FAIL_OPEN=USER_DATA_FILE) | 50 | 6,686 | 0 | 6,047 | 50 | clean (clean/I.STOP) |
| m | 50 | 6,263 | 0 | 5,784 | 50 | I.STOP (I.STOP) |
| inj (7016A896:-1:0x2006, play) | 50 | 6,449 | 0 | 5,985 | 50 | ?FATAL (FATAL) |
| abort (7016871D:ABORT) | 50 | 5,519 | 0 | 5,313 | 50 | WORLD-ABORT (WORLD-ABORT) |
| play (patient driver) | 50 | **6,862,784** | 0 | 6,861,616 | 50 | clean (clean/I.STOP) |
| inj2 (70176AA7:-1:0x2006, login) | 50 | 5,408 | 0 | 5,245 | 50 | ?FATAL (FATAL) |
| inj3 (70176AA7…:RESUME, m) | 50 | 6,434 | 0 | 5,953 | 50 | I.STOP (clean/I.STOP) |
| k1fo (FAIL_OPEN, K=1) | 1 | 325,170 | 0 | 324,565 | **1** | clean (clean/I.STOP) |

Replicated: task 031 (the hardened template) was picked up by a runner
in the window before it was parked and ran to an independent second
GREEN — same 8 legs, div=0, 0 ordinal mismatches, 6,925,242-pair play
leg — under setsid/lock-fd-closed launches and port-scoped kills
(results/031). Two independent full-battery greens total.

~7.2M pairs total per run, zero block-ordinal mismatches, no inter-pair
gap ever exceeded K on any leg, i2=0 and probes=0 everywhere (M4 mapper
quiet underneath), guard_throws=0. The 6.8M-pair play leg at K=50 is
the single strongest piece of evidence the new sync model has: a full
patient session with the heartbeat, gates, signals, and detach all
riding block-denominated rendezvous. The K=1 leg additionally proves
per-entry pairing end-to-end in the battery environment (max gap
exactly 1 across 325K pairs).

## 8. Q2 flag scan (stretch, report-only — informs P23's surface ruling)

Question: do any blocks read carry/skip state before writing it (carry
live-in)? Answer: **YES — 163 of 13,495 blocks.** Q2's "flags die in
the defining block" bet is FALSE; per the ruling's own rule, carry
stays in the cross-block comparison surface (it already is: `c` is
compared strictly at every pair).

Method (METHOD §5 — semantics from the emulator source, not intent):
carry read/write sets auto-extracted from every `case` body in
hw/Eagle*.cpp + the NovaCompute ALC path, then refined with the exact
ALC dataflow: prior carry P enters as bit 16 of the 17-bit datapath
(CC blank ORs P, CC 'C' complements it); low 16 result bits NEVER
depend on P; the shift class decides where P lands (no-shift/S →
c_new; L → result bit 0; R → result bit 15); `#` suppresses both AC
and carry writes, so a `#` form observably reads P only when its skip
tests a P-dependent value. This matters: the ubiquitous
`MOV.L# r,r,SNC` (sign-bit test) does NOT read prior carry — a naive
classifier flags 851 blocks; the exact one finds 163.

The 163: **132 frame saves** (WSAVS/WSSVS at routine entries capture
c into frame bit 31, restored at WRTN — call-transparent carry via
memory) + **31 ALC consumers** in three idioms:
1. `ADD.S 0,2` ×4 — SQR31?3's sqrt bit-loop carry chain (multiword
   arithmetic across its skip-split blocks);
2. `ADD 2,0` as block-first instruction ×16 across 10 routines
   (ATTACK, CAVE_ATTACK, DISPLAY_INVENTORY, FIRE, GET_QUEST, HELP ×3,
   MOVE_PLAYER ×6, SEIGE, START_TURN ×3, STORE) — carry-in mixes into
   carry-out;
3. `ADC.C r,r,SNC` / `SUB.CL r,r` chains in BEING_ATTACK and
   KNIGHT_ATTACK — carry-to-Boolean materialization split across
   one-instruction skip blocks.

Caveat: unmodeled ENQT/DEQUE skip edges (§4) could in principle
re-split a block such that a def-use pair becomes cross-block; both
known sites are bracketed by CRYTZ/CRYTO kills, so no additional
live-in arises. Full block list: the scan script and output are
reproducible from this report's tables (classifier in §8 method).

P23 consequence: carry stays in the block-exit surface; the 31 ALC
consumer blocks (and every routine-entry block) must treat carry as a
live input in any lowering.

## 9. Surprises / corrections

- The 029 false GREEN (§6) — the most important correction: a
  runner-side environmental failure mode (timeout remnant + weak gate)
  that read as success. The 030 gate shape (pairs floor + endpoint
  pin) should be the template for future batteries.
- The stale quest.blocks (§3) — a generated artifact can silently
  predate a tooling fix; Q4's regenerate-and-diff caught it exactly as
  designed, just for a different reason than anticipated.
- MOV.L#/SNC does not read carry (§8) — the exact ALC dataflow
  mattered; the naive answer was wrong by 5x.

## 10. Deliverable index

- Code: hw/BlockSync.{hpp,cpp}, Machine.{hpp,cpp}, QueueEntry.{hpp,cpp},
  Lockstep.cpp, Launch.cpp, Makefile, quest.synclist (identity).
- CheckerHistory.md Generation 6 section (appended at landing).
- Run.md lockstep section updated (QUEST_BLOCKS/QUEST_SYNC_LIST/
  QUEST_SYNC_K, K=1 debug mode).
- Tasks 029 (void, kept as record), 030 (the gate, GREEN), 031
  (hardened template, parked in tasks/hold/).
- NextSession.md pointer to P23; CURRENT_STATE.md entry.
