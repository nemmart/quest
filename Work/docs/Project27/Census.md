# Project 27 — DERR cluster census (Part 1, plan gate)

Session Sep 5 2026, solo. PART 1 ONLY: analysis, no lower.py change,
no artifact the emulator reads was touched. Python only; nothing
needed the memory image.

TREE VINTAGE: the Sep 5 Work.tgz (Work__75_) + Disassembled.tgz;
verified against docs/Provenance.md (all 17 sha256 prefixes match;
quest.tags 90659843…, quest.dis 5c1db5fb…, blocks.split 1d3baaf6…).
The upload is byte-identical to repo `main` @ 29a1c24 (diff -rq: only
the new tool differs). Runner logs used for liveness: results/039
(k1fo, play-st) and results/037 (13 legs).

Tool: `tools/derr_clusters.py` (run from Work/c_src/tools with
`--dis ../../../Disassembled/quest.dis --tags ../../../Disassembled/quest.tags
--blocks ../quest.blocks.split --synclist ../quest.synclist.split
--ir ../quest.ir2.book --out ../../docs/Project27/assumed-foldable.txt`).
**Runtime 8.5 s wall clock** on this 1-core box (under the 10 s flag;
the cost is the per-cluster set rebuild in `grow`, trivially
memoisable if it ever matters).

## 1. Census — reproduces the prompt's table exactly

2,273 DERR embeds (2,198 `DERR 17`, 75 `DERR 16`); 2,273 clusters;
**2,271 FOLDABLE, 2 UNFOLDABLE**.

| count | skip chain (control order) | arms (fall/skip) | DERR | folded condition shape |
|------:|---------------|------------------|-----:|------------------------|
| 1,917 | WSGTI / WSGT  | G: S/D, S: D/K   | 17   | `!(acY >s imm16) && (acY >s 0)` |
|   279 | WUGTI / WSGT  | G: S/D, S: D/K   | 17   | `!(acY >u imm32) && (acY >s 0)` |
|    14 | WSGTI / WSGT  | G: S/D, S: D/K   | 16   | as above |
|     9 | WSLT / WSLE   | G: S/D, S: D/K   | 16   | `!(acX <s acY) && (acX <=s acY')` |
|     9 | WSLT / WUSGE  | G: S/D, S: D/K   | 16   | `!(acX <s 0) && (acX >=u acY)` |
|    30 | WSLE          | G: D/K           | 16   | `(acX <=s acY)` |
|    13 | WULEI         | G: D/K           | 16   | `(acY <=u imm32)` |
|     2 | LDSP (no skip)| —                | 17   | UNFOLDABLE (below) |

G = guard skip, S = second skip, D = DERR, K = continuation. Every
one of the 2,271 has the topology the prompt drew (first skip's skip
arm lands ON the DERR, second skip's skip arm lands past it; single
skip falls into the DERR). Every skip's fall-through is the
dis-adjacent pc (0 exceptions) and its CFG successors are `[fall,
skip]` ascending (lower.py's existing check).

**Unfoldable (2)** — both `no skip predecessor (pred LDSP)`,
`continuations: 0`: DERR 701604D4 in BARGAIN (pred 701604D1 `LDSP
1,[…]`) and DERR 7016D707 in HELP (pred 7016D704 `LDSP 0,[…]`). The
jump-table otherwise exits; P28 per the prompt. No other unfoldable
shape exists; no cluster overlaps another; no interior pc is a
BlockSync gate; no interior pc is a condition-system entry (§3).

Structural facts the fold design depends on (all mechanical, from
quest.tags inverted):

- **Every continuation K is entered ONLY from inside its cluster**
  (2,271/2,271; K's predecessor set = {S} or {G}). K is never a gate,
  never a condition-system entry.
- **203 guards are themselves block starts** (one-instruction guard
  block, e.g. a DO-loop body start).
- **693 clusters are chained**: K is the block start of the next
  cluster's guard block (nested range checks on consecutive
  subscripts). Chain roots 1,578; depth histogram 1:1116, 2:315,
  3:110, 4:19, 5:7, 6:6, 7:3, 11:1, 16:1.
- Interior block starts (strictly inside): 4,499 (2 per two-skip
  cluster ×2,228; 1 per single-skip ×43). All are in the synclist.
- The three standing exclusions (7015BD6B, 70169B0F, 70169B44) touch
  no cluster.

## 2. DESIGN FINDINGS — stop-and-report items for the ruling

### F1. The prompt's lowering and its sync-list accounting disagree with Machine.cpp:306

The clone ticks its block ordinal on ARRIVAL at a listed pc in the
dispatch loop (`Machine.cpp:306 if(rt_sync && BlockSync::listed(pc))
block_ordinal++`), i.e. when an IR block EXITS to a listed pc. The
prompt says the guard block "continues lowering the continuation's
instructions INTO THE SAME IR BLOCK … the block's terminator is the
continuation's terminator" AND "synclist entries down by the interior
count" (K stays listed). Both cannot hold: if the guard's IR block
runs K's instructions, the clone never arrives at K while the master
(original bytes) does → ordinal skew → blk_mismatch at the next
rendezvous. Two consistent designs:

- **A — assert + `goto [K] 0`.** Guard block = its instructions, the
  assert, `goto [K] 0`. K stays a listed IR block. Delist interior
  only (4,499). blocks/synclist 18,009 → 13,510; IR blocks 18,006 →
  13,507; gotos 13,628 → 11,400 (−4,499 skip gotos, +2,271 plain);
  DERR embeds 2,273 → 2; +2,271 asserts. Smallest lowering change;
  matches the prompt's numbers, not its lowering text.
- **B — absorb K, delist K too.** As the prompt's lowering text;
  delist interior ∪ K = 6,770 (sound: K's predecessors are all
  inside, §1). blocks/synclist 18,009 → 11,239; IR blocks → 11,236;
  gotos 13,628 → 9,129; DERR embeds → 2; +2,271 asserts. Chains
  collapse into one merged block per root (1,578 merged blocks). More
  compression, more lowering surface (cross-block t-place scope,
  BlockCtx per merged block, header/provenance for the merged block's
  source range).

Recommendation: **A** for this project — it is the fold the prompt's
success criteria and BlockSyncDesign rule 2 describe ("a DERR sink
merged into its guard block"), it keeps K as a rendezvous (finer
detection, rule 1), and the K-absorption of B is a general
straight-line block merge that deserves its own project with its own
census (every K→successor edge is then unobserved). The
assumed-foldable artifact carries K's predecessor evidence either
way. **Needs the user's ruling.**

### F2. After the fold the DERR is no longer a VERIFIED terminal pair

Today: master and clone both execute DERR → both arrive at DERR.TRP
(terminal kind 2) → `compare_pair` forms the final pair and prints
`TERMINAL-ABORT at … verified on both engines (top stack wides: number,
faulting pc)` → `abort_world` (Lockstep.cpp:281-296). After the fold
the clone never reaches DERR.TRP: the assert fails inside the IR
block → `Lockstep::assert_detach` (Lockstep.cpp:436-456: clone halted,
"master continues unverified") → the master's terminal batch hits the
detached early-out (Lockstep.cpp:151) → NO TERMINAL-ABORT line, no
abort_world; the master runs DERR.TRP forward on its own (the RT's
DG error exit).

So the prompt's expected `derr` leg end — "WORLD-ABORT on the master
and IR ASSERT FAILED on the clone" — is not what the current code
does; literally it will be "clone DETACHED at IR assert `DERR 17
@pc`" + the master exiting through DERR.TRP unverified. Options:

- **F2-a (no checker change):** accept detach semantics; the leg's bar
  = clone `IR ASSERT FAILED … "DERR 17 @7015C48E"` and the master
  arriving at DERR.TRP with top stack wides (17, 7015C48E) — the same
  pc, read from the master's own stderr/backtrace — with 0 div before
  it. The verified-pair property for DERR is traded for the fold
  (honest statement in IR.md/CheckerHistory).
- **F2-b (small checker addition, boundary 1 → ruling):** in
  `compare_pair`'s terminal path, a master terminal of kind 2 whose
  clone detached at an assert in the same ordinal/batch prints the
  TERMINAL-ABORT line with the assert text and aborts the world as
  today. Restores "one final verified pair, then the world stops".

Recommendation: F2-a for the landing (it is what the prompt's
boundary 1 allows), with F2-b recorded as the follow-up. **Needs the
user's ruling.**

### F3. QUEST_INJECT cannot push a value out of range

`RTStubs.cpp:435` `QUEST_INJECT=site:type:code[:RESUME]` synthesises
an **O?SIGNAL raise** at the site (`inject_fire`, RTStubs.cpp:500-540;
fires in Machine.cpp:374). It sets no register and cannot make a
DERR fire. No other knob does (`QUEST_TERMINAL`, `QUEST_BAD_TOKEN`,
`QUEST_IR_DEBUG_BLOCK`, … — full getenv inventory checked). P25's
assert fire test was a hand-authored `assert(0)` block — clone-only,
not a paired DERR.

Proposal: a one-shot **`QUEST_POKE=<pc>:<ac>:<value>`** test knob,
both roles, applied in the dispatch loop at arrival at `pc` at the
same point `QUEST_INJECT` fires (Machine.cpp:374), refusing to launch
on a malformed spec (the P14 near-miss rule). ~25 lines C++ in
hw/Machine.cpp + hw/RTStubs.cpp; not the checker, not Java. The
poke pc must be a block start (the clone dispatches whole IR blocks,
so a mid-block pc would never be "arrived at" on the clone) — which
is why the leg picks one of the 203 guard-is-block-start clusters
(§6). **Needs the user's ruling** (C++ test knob or a natural
in-game trigger, of which none is known on a driver path).

## 3. Condition-system cross-check (partial, cheap — not a proof)

Derived from quest.dis text alone (`derr_clusters.condition_entries`):

- **26 O.ON handler addresses**: for each of the 26 `LJSR [0x7017ED9B]
  # O.ON` sites, the nearest preceding `XLEF 2,[pc+…] (0xADDR)`
  within 6 instructions (O_ON.md resolution 2: ac2 = handler
  address). All 26 found (0 warnings).
- **22 distinct I.GOTO resume targets** from the 26 `LJSR [0x7017EC7C]
  # I.GOTO` sites, same XLEF-2 rule (ON_ERROR_CATALOG: handlers #5/#6
  and others share exits). All 26 sites resolved.
- **0 code-address XLEF/LLEF/XPEF/LPEF operands**: every EF-family
  operand whose folded target lies in the game range (4,164
  PEF lines) points at a data line, not an instruction (e.g. 7015BD7B
  `CASTLE_D…` strings); none names an instruction pc.
- LDSP dispatch tables: already in quest.tags (the two 24/38-entry
  `n` lines), hence in the predecessor map — the two LDSP-fed DERRs
  are exactly the unfoldable pair.
- `DEF?ON`/`P?DEFON`/`SWAT.REX` entries: **not called from game code**
  (0 sites in quest.dis; they are RT-internal). The catalog #26
  resume 0x7017A520 and the SWAT.REX+0x23/+0x3E entries are RT
  addresses, outside every cluster by range.

Result: **48 entries, 0 are cluster members** (guard, interior or K).

What remains UNMODELLED (boundary 4): a condition-system dispatch
landing on an interior pc via a path this text scan cannot see — an
I.GOTO whose target is computed, or an RT-internal resumption into
game code other than the O.ON/I.GOTO shapes above. quest.dis shows
no computed I.GOTO target (all 26 are `XLEF 2,[pc+…]`). The
goto-graph project discharges this list.

## 4. Skip semantics (emulator source; P26 Census §2a rows cited)

All in hw/EagleCompute.cpp; skip = successor index 1; no skip writes a
flag. Signedness is the source's cast, never the name.

| mnemonic | source | predicate evaluated (skip when true) | ir 3 form |
|---|---|---|---|
| WSGT x,y  | :193-196 | `(int32)acX > (int32)dst`, dst = acY, or 0 if x==y | `acX >s acY` / `acX >s 0` |
| WSLE x,y  | :188-191 | `(int32)acX <= (int32)dst` | `acX <=s acY` / `<=s 0` |
| WSLT x,y  | :183-186 | `(int32)acX < (int32)dst` | `acX <s acY` / `<s 0` |
| WUSGE x,y | :208-211 | `(acX & 0xFFFFFFFF) >= (dst & 0xFFFFFFFF)` (int64) | `acX >=u acY` / `>=u 0` |
| WSGTI y,imm | :231-235 | `(int32)acY > sx16(imm)` | `acY >s <folded sx16 const>` |
| WUGTI y,imm32 | :360-363 | `(acY & 0xFFFFFFFF) > imm32` | `acY >u <const>` |
| WULEI y,imm32 | :365-368 | `(acY & 0xFFFFFFFF) <= imm32` | `acY <=u <const>` |

Not present in any cluster: WSEQ/WSNE/WSGE/WSEQI/WSNEI/WSLEI/WUSGT
(the tool's tables carry them anyway, mirroring lower.py 444-447).
P26 Census §2a lists the same rows (WSLT :185, WSLE :190, WSGT :195,
WUSGE :210, WSGTI :233, WUGTI :362, WULEI :367 — the one-line offset
is the `case` label vs the compare line; same bodies). Java
EagleCompute identical per P26.

**Folded condition = transcription of the skips** (tool header rule):
`cond(K)=true, cond(D)=false, cond(skip t; fall f, skip s) = t ?
cond(s) : cond(f)`, rendered `(t) && cond(s)` when cond(f) is false
and `!(t) && cond(f)` when cond(s) is false, with `x && true → x`.
Every cluster is one of the two resulting shapes (`!(t1) && (t2)` or
`(t)`); the `||` case never arises. The reviewer reads the skips back
out: the negated term is the first skip (its skip arm is the DERR),
the positive term is the second (its skip arm is K). The message is
`"DERR <code> @<derr pc>"`.

## 5. Sync-list plan

- **Option A**: shipped list = identity (18,009) minus the 4,499
  interior starts = **13,510**. **Option B**: minus 6,770 = 11,239.
- Loader validation is against the SHIPPED list, confirmed:
  `BlockSync::load_from_env` reads QUEST_SYNC_LIST; `IRExec.cpp:435`
  refuses an IR block whose start is not listed; `IRExec.cpp:493,508`
  refuse any `goto` label that is not `BlockSync::listed` — so a goto
  into a delisted pc refuses at load. `BlockSync.cpp:171` refuses
  delisting a gate; no interior pc (or K) is a gate.
- IR side (book and stock): 8,998 goto labels name a delisted pc;
  **0** are emitted by a block outside the owning cluster (chain
  links, S→K where K is the next guard block, counted as inside).
  Condition (b) holds on the IR side as on the tags side.
- `quest.synclist.p27` written by a small `tools/ship_synclist.py`
  from assumed-foldable.txt with a provenance header (tags / blocks /
  assumed-foldable sha256); the loader header check is unchanged (the
  synclist has no sha header today — the P27 file adds one as `#`
  comment lines, which the parser already skips).

## 6. Battery plan

034 template (13 legs, JOBS=3) as task 040 with the `bin/task_source.sh`
staging fix, PLUS the `derr` leg:

- **Site**: cluster guard **7015C48B** in QUEST (main routine): `XNDO
  0,107,[ac3+0x3]` DO-loop body start, `WSGTI 0,10; WSGT 0,0; DERR 17;
  NLDAI 686,1` — guard IS the block start; the block executes in
  every k1fo/fo/play leg (results/039 k1fo first-execution line
  present). Folded: `assert(!(ac0 >s 0xA) && (ac0 >s 0), "DERR 17
  @7015C48E")`.
- **Knob**: `QUEST_POKE=7015C48B:0:11` (ac0 := 11 on arrival, both
  roles, one shot; F3 ruling permitting). 11 fails the first term; 0
  would fail the second — 11 is the choice (tests the WSGTI arm, the
  one the master takes to the DERR).
- **Config**: book, K=1, fail-open driver (login-fast, ≈5 min).
- **Expected end** (F2-a): master — DERR 17 at 7015C48E → DERR.TRP,
  stack top wides (0x11, 0x7015C48E), then the RT's DG exit; clone —
  `IR ASSERT FAILED [7015C48B, …]: assert(… "DERR 17 @7015C48E")`
  + `DETACHED at IR assert`; 0 div before; verdict greps both pcs
  equal. Under F2-b the end is `TERMINAL-ABORT at DERR.TRP … 00000011
  7015C48E`. A master abort with no clone assert (or vice versa) is a
  design finding — STOP.
- **Poke-unarmed control**: the k1fo leg itself (the knob exists but
  is off) proves the knob is inert.
- Fallback sites if 7015C48B misbehaves: 701786D6 START_TURN
  (`ac0 in 1..10`), 70177E8F SIGNAL_TURN (`ac1 in 1..10`), 7016E10F
  INIT_SCREEN — all guard-is-block-start and live in k1fo.

## 7. Predicted verdict lines (Option A, F2-a)

`derr_embeds_remaining=2 clusters_folded=2271 unfoldable=2
synclist_delisted=4499 blocks=13510 ir_blocks=13507 derr_leg_pc=7015C48E
(master DERR.TRP top wides == clone assert pc)`. Option B:
`synclist_delisted=6770 blocks=11239 ir_blocks=11236`.

## 8. Part 2 implementation sketch (for the ruling; not started)

lower.py: `--assumed-foldable PATH` (refuse on tags sha mismatch);
parse into guard→cluster; in the guard block's lowering, when the
terminator pc is a folded guard, emit `assert(cond, "DERR n @pc")`
then (A) `goto [K] 0` / (B) continue with K's statements; skip
emission of interior blocks (and K under B); TOTALITY — any refusal
inside a cluster refuses the whole cluster (interior blocks then
emitted as today and NOT delisted; ship_synclist consumes lower.py's
actual fold list, not the plan). IRExec: no grammar change. Slices
(i) 43 single-skip, (ii) 2,228 two-sided, each behind K=1 book+stock
gates; `derr` leg locally first.
