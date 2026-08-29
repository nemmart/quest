# Where things stand

## ★ PARALLEL BATTERY TEMPLATE LANDED — task 034 (Aug 29 2026)

User ruling: no more 30+ minute serial batteries. tasks/034-parallel-
battery.sh is the TEMPLATE OF RECORD, superseding hold/031's serial
shape (leg INTERNALS unchanged): slot pool (JOBS=6 default — legs are
wall-clock dominated; hard core-pinning would starve the multi-threaded
emulator), long-pole play legs launch first, per-leg verdict/status
files collated in canonical order after wait (the serial FAILS counter
cannot cross subshells), driver timeout 480 for contention margin.
Per-leg isolation (own port 8791+, own scratch, own trace) was already
in place; the flock guard still prevents battery-level overlap — the
actual task-029 failure mode. Standing leg set = the 032 eleven (inj/
abort green under the F6 fix, drop=1 each) + the 033 all-emulated
isolation pair = 13 legs. Proof run (results/034-parallel-battery):
**13/13 GREEN, wall_clock=523s (~8.7 min)**, div=0 everywhere, strict
gate held, drop checks exact, coverage line matches the 032 pattern
(BEING_ATTACK cluster live at 2 hits, LOCK_FILE at 8) — no contention
flakiness. ~5-6x wall-clock reduction.

## ★ F6 CLOSED — terminal legs restored (Aug 29 2026, integrator session)

User ruling: option c, BOTH halves. (1) IRExec loader drops the IR
block whose span contains a mid-block QUEST_INJECT/QUEST_TERMINAL pc
(absent=emulated=symmetric; entry-armed pcs stay lowered — inj3
precedent; conservative floor-block drop, rt-range armed pcs ignored).
(2) Standing all-emulated inj/abort legs join the battery template as
the terminal-machinery isolation check — they answer "does raise/
dispatch/unwind/die still work" independent of the IR layer. Task 033
(runner): **4/4 GREEN**, strict gate — inj book FATAL (1 drop), abort
book WORLD-ABORT (1 drop), inj-emu FATAL (0 drops), abort-emu
WORLD-ABORT (0 drops), div=0 everywhere. The two task-032 reds are
superseded green; the P24 battery record is complete. Repo: fix +
task 033 on main. Remaining P24 open item: the consumer-site live
coverage ruling (KNIGHT_ATTACK/DIVX — census-sufficient vs manual
combat/store session).

## ★ P24 LANDED — THE WIDE-CARRY CORRECTION (Aug 29 2026)

`EagleInstruction::add()/sub()` now compute CARRY as the ALU carry-out
(bit 32, manual-backed) instead of the result sign bit (>>31). One fix
covers emulated master, emulated clone, IR clone (`#`-ops, P23 ruling)
and native runtime (frames.cpp's replicas are now thin forwards to the
helpers — replica drift eliminated as a class). WADC ruled by the user
(Aug 29): fixed with the rest, `WADC x,x` → ac=-1, **c=0**, no routing.
docs/Project24/CarryCensus.md is the census of record: **zero
wide-reached and zero ambiguous carry consumers anywhere** — every
reachable consumer is Nova/DIVX/CRYTO-fed (the four P23-era candidate
ADC.C sites read the adjacent ADD.O, not anything wide) — so the fix is
gameplay-unobservable and its correctness rests on the manual + the
census, not lockstep (METHOD §2 argument in the P24 report §6).
Native staged-carry residue re-derived atomically with the fix:
lib_error (O?SIGNAL boundary c=1; I.FREEW staging c=1; c_x default 1;
I.ALLOC staging stays 0, re-justified), o_signal/o_on catch-all 0→1
(the ee7B/ee37/edf8 WSUB producers), p_defon type==6 → c=0 (WADC
ruling), unsigned_to_char k==1/argc≤2 → 1, i_alloc unlock word drops
its 0x80000000 (pre-fix captures showing F017EA08 are historical).
Doc corrections per METHOD §11 (annotated): METHOD §5, WideCarry.md
(landing note; XWDO/LWDO added to the writer list, manual-confirmed),
Project1/2/3 DERIVATION headers, L2Contract normative rows, I_ALLOC
carry chains, UNSIGNED_TO_CHAR. Local gates: 4 legs (book fo/m/play +
stock fo), K=1 strict on the split pair, 0 div each, ?LIB_ERROR native
on both fo legs. Battery (repo task 032, 11 legs: book/stock/all-emulated ×
natural triggers, 031 hardened template): **9/11 GREEN** (incl. K=1 fo
381,818 pairs; 6.38M-pair book play; 10.73M-pair stock play; ?LIB_ERROR
native in seven legs; BEING_ATTACK ADC.C cluster executed live). The 2
RED legs (inj, abort) are **F6 — NOT carry**: QUEST_INJECT/
QUEST_TERMINAL arming at non-block-entry pcs cannot fire on the IR
clone (P23 latent; inj3's block-entry site passed; 031 ran the same
legs green pre-IR). **USER RULING OWED on F6** — options in the P24
report §7: (1) run inj/abort legs all-emulated, (2) IR-loader excludes
blocks containing armed pcs, (3) both. Branch p24-wide-carry (carries the P23-integrated tree as
its base — main still lacks P23; integrator to merge).

## ★ DETACHED-MASTER TRIPWIRE LANDED (Aug 29 2026, small interactive session)

A clone detach used to leave the master playing on with one easily
missed stderr line. Now (user design + ruling): the MASTER arriving at
the per-turn command dispatch — **START_TURN** entry, resolved from
symbols (`RTStubs::turn_loop_pc`; it dispatches both MOVE_PLAYER and
MOVE_IN_CAVE, so one anchor covers overworld and cave mode) — with its
clone ordinal detached hard-aborts the world (`abort_world`,
save=false: "it shouldn't happen", so the post-detach master isn't
trusted with the data files). Check lives in `Machine::run_steps`
beside the terminal-arrival check, master role only, once-per-turn
cadence. Graceful shutdowns can't false-fire (START_TURN's ONLY caller
is main-loop site 7015C5DC; DETACH-kind terminals run forward to exit;
?RETURN retires at the exit syscall) — this matters because a false
fire on a clean quit would suppress a legitimate save. Validated:
forced-detach leg (QUEST_TERMINAL=7016AA35 → detach at login → abort
at first START_TURN arrival; message: "Internal error: Master did not
terminate after clone detach") + clean K=1 book-IR leg
(login/creation/live-clone turn, 0 div, no fire). ESC-quit leg
attempted, inconclusive against turn cadence — structural argument
recorded; a future real ESC quit under lockstep should be noted in
TerminalDetach.md. Docs: TerminalDetach.md new section. Step-0 finding
worth keeping: today's only detach producers are verified terminal
pairs and retirement — one-sided clone failures already abort loudly —
so the tripwire is belt-and-braces for those plus any FUTURE detach
reason (which is exactly when it will pay).

## ★ P23 LANDED — GEN-6.1: THE IR (Aug 28–29 2026; reviewed + integrated Aug 29)

The clone executes the game as an intermediate representation. Built:
`c_src/tools/lower.py` (dis+blocks+pushmap+argmap → provenance-stamped
quest.ir; TOTAL — any inexpressible block is OMITTED, absent=emulated),
`c_src/hw/IRExec` (refuse-on-anything loader + block interpreter; clone
only, master always emulates), `c_src/tools/split_skips.py`. Grammar is
**rev 2** — consolidated normative spec now lives in **docs/IR.md**
(IRPhase1/IR2 are history). Shipped artifact `c_src/quest.ir2.book`:
17,983/18,009 blocks (99.86%) — 443 call / 165 ret / 1,039 arg-slot
stores / ~3.9k gotos (3,242 lowered WBRs) / ~31.1k embedded
instructions; the 26 omitted = the 7015BD6B exclusion + 25 @/bit-15
census. CFG rebuilt: ALL skips split (user ruling; 13,494 → 18,009
blocks; quest.blocks.split + identity quest.synclist.split are now the
operative lockstep pair). The strict continuation tripwire (decoder
word_length, no annotations) surfaced the **LNADI/LNSBI listing defect
in BOTH toolchains** — fixed, all listings regenerated, §14 diff-audit
exactly on-prediction (2 real instructions recovered). P22 obligations
ALL discharged: TEMPORARY insn-count term removed from the verdict
(trace-only now); **ovr joins the pair surface** (c retained →
carry-live-in stays covered); ENQT/DEQUE skip edges split; 7015BD6B is
its own block, exclusion-listed in lower.py AND refused by the loader.
`#`-ops call the SAME EagleInstruction helpers as emulation (user
ruling) — which is what let the **wide-carry emulator bug** (carry
computed as >>31, not ALU carry-out; masked from lockstep forever per
METHOD §2, caught by the manual) be PARKED as its own task:
docs/Project23/WideCarry.md + wide_carry_fix.patch, **NOT applied**
(native carry residue in Project1/2 translations must be re-derived
with it; WADC carry needs real evidence). Mode discipline: quest.ir
declares `mode stock|book`; loader refuses book IR without
QUEST_ADDRESS_BOOK+QUEST_PUSH_MAP and recomputes provenance sha256s.
Gates (REPORT §9, all K=1 strict, 0 div): pilot, whole-game pre-split,
split, rev-2 book/stock/site= legs (~2.2k IR blocks live each) — plus
an independent reviewer spot-check Aug 29 (fresh build, K=1 book leg,
boot→login→creation→turns: 2,184 IR blocks, div=0). Report:
docs/Project23/REPORT.md (reviewer notes appended §10).

**NEXT: wide-carry re-verification first (parked task, WideCarry.md —
REDO the carry-live-in ∩ wide-producer census, record it, then land
the patch + re-derive translation residue). Then the §8 queue: B-form
byte-EA extraction (unlocks 96 call sites), WPSH multi-wide (25),
`save`, @/bit-15 fix+regen+diff-audit (25 blocks). P24 = t-places
(borrows pilot per user ruling — no borrow/restore ops; `end if`
conditional exits at the P24 boundary; pre-P24 census owed: crossings
inside borrow-bracket interiors).**

## ★ P22 LANDED — GEN-6.0: THE BLOCK-SYNC CHECKER (Aug 28 2026)

The sync model is re-denominated: rendezvous every K listed basic-block
entries (QUEST_SYNC_K, default 50; K=1 debug) instead of 500-insn
batches — the first sync-model change since Gen 1, and the enabling
move for translation (P23+): the heartbeat no longer counts
instructions a translated clone won't execute. Sync identity =
(block entry pc, per-client block ordinal), counted at
arrival-transitions before every break decision; the sync list is an
explicit validated input (identity list shipped: all 13,495
quest.blocks starts; novelty and gate-delisting refused at load);
100M-insn runaway guard THROWS. Surface: + block ordinal (STRICT, no
span exemption), + FP state always (Q3), insn-count delta retained
TEMPORARILY — **P23 must remove it**. Gates/crossings/terminals/M4
untouched. Battery task 030 GREEN under a STRICT gate (pairs floors +
pinned endpoints): 8 legs, ~7.2M pairs, div=0, 0 ordinal mismatches,
max gap never > K — incl. a 6.86M-pair full play session and a 325K-pair
K=1 leg (max gap exactly 1). Task 029's GREEN is VOID (two concurrent
runner loops on the box truncated each other; the strict-gate shape is
now the battery template — hardened script parked in tasks/hold/031).
Carry live-in census for P23: 163/13,495 blocks (REPORT §8) — carry
stays in the surface. Q4 side-finding: quest.blocks predated the P1
WLDAI fix (78 lines, structure unaffected); user regenerates upstream.
Loader CRLF hardening LANDED (ruling: Windows Java emits CRLF, readers
tolerate it): chomp + grammar-exact token refusal — a CRLF blocks file
had silently collapsed 1,865 gates to 9. Report: docs/Project22/REPORT.md.

**NEXT: P23 — Gen-6.1, 1:1 lowering (docs/Project22/IRDesign.md context;
obligations: remove the insn-count term; carry-live-in list; CFG gaps:
ENQT/DEQUE skip edges + 7015BD6B interior LJSR).**

## ★ P20 LANDED — M4 DE-STACKIFICATION COMPLETE (Aug 23 2026)

The FINAL M4b tranche: all 23 WPSH/WPOP frame-borrow brackets redirected
off-stack into a 23-slot reserved block at [74000000, 7400005C); code
frames shifted to 0x7400005C (derived 4·N, nothing hardcoded). Frames
(M4a) + args (M4b A–D) + borrows (P20) are ALL off the stack — the game
program is flat and live-analyzable, still executing the original
instructions, checker green. Battery task 028 GREEN: div=0 on all five
legs, 0 i2/probes/aborts, and the AC round-trip verified BY VALUE (every
fired WPSH's stored value == its WPOP's load, offset off/off+2 exact,
every bracket closed; 101/101/1/146 round-trips per leg). Baseline vs
task 026 exact (argwr deltas == borrow stores; wWSAVS/wWRTN unchanged).
Mechanism was REUSE per the prompt: WPSH side is the stock P18 wides=1
hook; the ONE new hook is the decorated WPOP load (EagleStack) +
Mapper::note_arg_pop (−2 mirror). Tooling: ArgWindows borrow pass
(23/23 PROVEN single-block on the same targets set as arg windows, 0
flagged, cross-checked vs the coordinator scan); book builder reserves
the block; loader gets the `borrow_slots` line, a relative-16-grid check
(JUDGMENT CALL flagged in the report: 4·23 isn't a multiple of 16, grid
made relative to frames_base; borrow_slots=0 recovers the old check),
and the third `borrow` validation arm; all six pushmaps rebased +0x5C
with full 1352-line transcription revalidation; quest.pushmap.M4 =
ABCD + borrows (1313/566/46). Coverage: 3/23 pairs driver-reachable
(16, 19, 20), rest decorated+proven, usual backlog. WMSP/STASP groups
stay by design (M4cNotes: dissolve at translation). Report:
docs/Project20/REPORT.md.

**NEXT: M5 — live range analysis on the flat program (M5Notes.md);
ON-handler control flow (O.ON / I.GOTO dynamic dispatch) is the known
loose end to bring under the same discipline.**

## ★ P19 LANDED — M4b COMPLETE: tranches C & D (Aug 23 2026)

All 566 arg-bearing game→game call sites decorated (535 P18 + 26 XCALL
+ 5 RETURN_MESSAGE; 188 zero-arg need nothing). Batteries GREEN: task
026 (7-leg regression on quest.pushmap.ABCD, div=0, P18-baseline-equal)
+ task 027 (targeted coverage, div=0). Two completions of the existing
design, no mapper/ruling changes: (1) the marker hook replicated into
`case XCALL` (code-identical word; write-mode WSAVS opcode-agnostic;
live at 9 LIST_PLAYERS.3 sites, static link intact); (2) tranche D =
map entries only — all 5 sites are LCALLs on the P18-B WPSH hook.
Noreturn verified live: QUEST_FAIL_SSHPT (new TEMPORARY os knob; the
?SOPEN failure routes via ?LIB_ERROR→?FATAL instead — also exercised,
clean) → RETURN_MESSAGE,6 @7015BE74 write-mode, body clean, RETIRED at
?RETURN, wWSAVS−wWRTN=+1. Coverage: C 9/26, D 1/5 live; rest
driver-unreachable (BARGAIN/BOAT/CAST/KILL_PLAYER/OP_EDIT; LOGON/
UPDATE_USER_DATA_FILE/LOCK_FILE incl. the pass-by-ref site — structural
argument + verified pointer-arg analog in docs/Project19/REPORT.md §4).
Report: docs/Project19/REPORT.md; CheckerHistory Gen-4/5 addendum.

**NEXT: M4c — in-body stack residue: MSP dyn allocs (WMSP/STASP
sprintf-style groups, see M4cNotes.md), WPSH/WPOP brackets.**

## ★ P18 LANDED — M4b widened to ALL flat-LCALL sites, tranches A+B (Aug 23 2026)

535 decorated caller sites live (515 flat single-word + 20 WPSH
multi-slot to TERRAIN/TERRITORY), batteries GREEN (tasks 024, 025):
div=0 all five legs, 0 i2/probes/m4b/mapper aborts, write-mode
WSAVS==WRTN, 86k redirected arg writes in play, WPSH windows verified by
VALUES + offset arithmetic (TERRAIN off 2→10 across the WPSH, closing at
2·argc=18). Two emulator completions of the existing design, no
mapper/ruling changes: (1) caller_write hook replicated to XPEFB/LPEFB —
P16 hooked only XPEF/LPEF, which was the root cause of the task-021
driven divergence (captured in task 023: GET_INPUT@701760C4, shadow
+2·argc, argwr=0); (2) the WPSH multi-slot hook (AC[XX]→base slot
ascending, note_arg_write(m,wides), fail-loud wides/group cross-check) +
loader 3-field grammar with per-wide slot validation. Report:
docs/Project18/REPORT.md (§7–11); CheckerHistory Gen-4/5 addendum.

**NEXT (next project): tranche C — 26 XCALL/nested sites (static-link
interaction); tranche D — 5 RETURN_MESSAGE sites (pass-by-ref pointer
args, [[noreturn]]). Then M4c (in-body stack residue: MSP dyn allocs,
WPSH/WPOP brackets).**

## M4b progress (Aug 22 2026)
- P16: M4b mechanism PROVEN on one site (DIST,4 @ 70166E1C) — args to
  area, marker tombstoned+written, flag consumed at WSAVS. Stopped at a
  mid-window checkpoint condition (Boundary 2).
- P17: mid-window condition SOLVED — `stack_offset` field in LiveRecord
  (checkpoint compares shadow+offset; −2·argc consumed at write-mode
  WSAVS per the ratified timing amendment). QUEST added to the book as
  the base copy-mode record (records_ never empty). Task 020 GREEN:
  div=0 all five legs, 33 mid-window pairs passing, boot clean.
- Book now 102 live (QUEST folded in from the M5 nocall set).
- NEXT: widen M4b to N sites from quest.argmap; then WPSH multi-slot
  arg pushes; then M4c (in-body stack residue).


## ★ P17 LANDED — M4b first slice COMPLETE: stack_offset + QUEST base record (Aug 22 2026)

The P16 mid-window finding is FIXED per the M4bNotes ruling, with one
Stage-0 amendment (user-ratified): the −2·argc consumption fires at the
WRITE-MODE WSAVS, not the decorated LCALL — the post-LCALL/pre-WSAVS
boundary is a valid compare point and the args are still elided there
(battery pair at 70166E1C off=8 passing proves the amendment right).
Mechanics: `LiveRecord.stack_offset` (+2 per redirected push wide on the
caller's record; consumed at write-mode WSAVS; empty → mapper_abort);
checkpoint compare = `shadow_wsp + checkpoint_offset` (empty → 0 =
closed form). QUEST decorated as the BASE RECORD — just uncommented in
the book, ordinary copy-mode migration from the loader entry, boots as
record #1, `records_` never empty. Book now 102 live.

Task-020 battery GREEN: **div=0 on all five legs** (fo/m/inj/abort/play;
inj at normal driver speed, rest 10x), 0 i2, 0 probes, 0 m4b/mapper
aborts; 594–891 complete write-mode DIST calls per site-reaching leg
(P16 died at call 85); 33 mid-window pairs passing at off ∈ {0,2,4,6,8}
incl. the exact P16 70166E19/off=6 case; copy mode coexisting;
quest_base=1 everywhere. Comparison-term change noted in CheckerHistory
(no Gen-5 restructuring). QUEST folded out of the M5 nocall set (now 28)
— see Project14/M4A_ROLLCALL.md. Report: docs/Project17/REPORT.md.

**NEXT (later projects, binding P17 scope stops here):** widen M4b to N
sites from quest.argmap; the WPSH multi-slot arg case; then M4c residue
(MSP dyn allocs, WPSH/WPOP brackets). Known fail-loud residual (noted,
ruled acceptable): a mid-window async signal with a same-frame O.ON
handler would strand a nonzero offset → loud divergence; one-line
zero-on-unwind if ever observed.

## P16 (M4b first slice) — superseded by P17 above (Aug 22 2026)

One site converted (DIST,4 @ 70166E1C, caller map + write-mode WSAVS +
mode-aware record/fixup, both Stage-0 reconciliations ratified). The
MECHANISM IS PROVEN: 84 clean write-mode calls in the m leg, shadow/W−2/
+2·argc arithmetic exact, flag rules clean, copy mode coexists, master
stock. But the battery (task 018) hit the boundary-2 stop condition:
client batches are a 500-instruction QUANTUM, so compare pairs land
mid-window — master's wsp is 2k ahead of shadow after k partial pushes →
div=1 in every site-reaching leg (M4bNotes issue 1(a) is real). Stopped
per boundary 2; NO mapper/design edits. Two candidate rulings written up
neutrally in docs/Project16/REPORT.md §5: C1 open-window shadow
accounting (Gen 5) vs C2 quantum alignment (batch never breaks inside a
window). Task 019 captures the raw dump. Resume: rule C1/C2 → implement
→ re-run task-018 battery (expect div=0) → land.

## ★ M4a CLOSED (Aug 22 2026)

M4a is DONE. All 101 callable game routines migrate to 0x74000000 areas;
both mapper findings fixed (A: s>=W; B: wsl-heap_break fence latch).
Validated on 44 routines exercised in live play + the full scripted
battery, 0 divergences across 1.3M redirect events, including live
signal dispatch with area frames live. 57 routines are migrated but
not-yet-individually-exercised — coverage backlog, low risk, swept
opportunistically. NEXT: M4b (caller-side arg redirect; census done,
100% convertible; call-marker-stays-on-stack ruling; no mapper change).

All 101 callable game routines migrate to 0x74000000 areas; both mapper
findings closed (Finding A: `s>=W` stack leg; Finding B: I2 as wsl−heap_break
fence latch + live-wsl domain bound, clearance clause removed). Full battery
GREEN on the 101-live book — fo/m/inj/abort/play: 0 divergences, 0 I2 aborts,
0 probes (task 016). Remaining before M4a is *called* done: live-play breadth
coverage (menu/combat/death dyn routines the scripted drivers miss), the
roll-call, CheckerHistory Gen-4 append, Mapper.md I2 wording. Then M4b.

---
# (prior notes below)
 — tarball hand-off (reverting to normal-session flow)

The git/runner pipeline is set aside; back to Work.tgz per session. The
repo (github.com/nemmart/quest) still holds full history if wanted, but
the authoritative tree is THIS tarball.

## Milestone M4a — nearly complete

- **Mechanism DONE and proven** (Projects 12–14): WSAVS/WRTN frame
  redirect into fixed 0x74000000 areas; the Mapper (hw/Mapper.{hpp,cpp},
  first-principles rewrite — spec in docs/Mapper.md); Checker Gen 4/5.
- **Live routines: the book is at 101** (all callable game routines;
  29 nocall excluded → M5). See Work/c_src/quest.addrbook.
  - Validated under lockstep through batch 2 (45 live) + B1 nested
    family (ATTACK). The jump to 101 (adds dyn/push routines + the
    never-landed pure "batch 3") has passed a BUILD + BOOK-LOAD smoke
    (101 routines, 40 pages) but the SANITY BATTERY on the 101 book was
    not yet run when we reverted.
- **Finding A: RULED + FIXED + VERIFIED (Aug 22).** The ruling
  (docs/Project14/FINDING_A_MAPPER_FIX.md) called it a Mapper stack-leg
  bug; the fix landed as `>=` in the ToMaster stack leg (s == W is the
  record's own anchor — wsp position value → master's wsp; the
  shadow_wsp threshold, now unified) plus fixpoint-scoped I4/I6 at the
  new merge point master_wfp+2*frame (Q2-style, inverse→area/data;
  ToClone unchanged, no new record fields). NOTE: the ruling doc's
  mechanism narrative had the geometry inverted (stack grows UP; the
  crashes were s == W, not a descent) — corrected companion:
  **docs/Project14/REPORT_FINDING_A_FIX.md** (read together). Verified
  in-container on the FULL 101 book: m ×2 green (0 div, anchors exact,
  I.STOP, DISPLAY_SCREEN through WRTN both GTOD variants), inj green
  (?FATAL 7017F036), abort green (both-engines banner), play green for
  39 routines. Mapper.md updated (Q2 merge-point extension; §3b
  stack-leg-ties subsumed).
  - **Owed on the fix:** menu dyn routines (DISPLAY_MAGIC, DISPLAY_CAVE,
    DIED, DROP, LIST_PLAYERS) unreached — the play tour stalls at
    "Waiting for your turn" in-container (turn pacing > stock 160s
    drain). Patient driver variant handed to the user for a server-side
    run: `DRV=docs/Project14/drive_patient.py bash docs/Project14/run.sh <tag>
    play` (same steps, prompt-aware wait; repo driver untouched).
- **Finding B (I2, fail-open path): STILL OPEN, unchanged.** Re-verified
  post-fix byte-identical (latched 7001715A → 7001714C, same pc): the
  handler moves wsl (−14) while a game record is live; NOT per-routine —
  commenting cannot fix it. Needs its ruling (REPORT_B2 §3 candidates)
  before Stage 3. FINDING_B_INVESTIGATION.md exists in Project14.
  - CheckerHistory Gen-4 append still DEFERRED — the corrected line can
    now be written ("dyn/push symmetric AND bijective under the >= leg";
    plus the owed B1 stride-masking sentence) once fo is ruled, or
    earlier if the user wants the partial append.
  - m/fo drivers: docs/Project13/drive.py (modes m, failopen, play).
  - inj is TIMING-SENSITIVE: run at normal driver speed, not sped-up
    (QUEST_INJECT=7016A896:-1:0x2006 → ?FATAL 7017F036; drive mode
    `play`, not `m` — FIND_OBJECT is only reached in play).
  - fo needs QUEST_FAIL_OPEN=USER_DATA_FILE in the env (run.sh arg).
- **Then:** rule Finding B → fo green → Stage-3 live play session for
  breadth coverage (combat, store, bargain, cave, castle) → roll-call →
  CheckerHistory Gen-4 append.
- **Owed:** two B1 combat captures (spell-kill sibling XWLDA;
  child-body inject) — combat-gated, do them in the play session.

## Designs of record (do not casually edit)
docs/Mapper.md, docs/M4aDesign.md (§1–§11), docs/M4bNotes.md,
docs/M5Notes.md, docs/Project14/{REPORT.md, REPORT_B1.md, PROMPT_B2.md}.

## Ahead (planned, not started)
- M4b: redirect the caller-side arg pushes into callee areas (push_map +
  decorated calls). Census done (Project 15 / quest.argmap): 100% of 754
  call sites cleanly convertible. Watch the zero-arg mapping condition
  (Mapper §3b): when zero-arg calls leave nothing on the stack, the
  mapper switches from address-order to record-order for LIFO identity.
- M4c: the last in-body stack use (MSP dynamic allocs, WPSH/WPOP).
- M5: static analysis + translation to source; handler dispatch goes
  static once frames are in areas (M5Notes).
