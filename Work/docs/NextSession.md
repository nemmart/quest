# Next session

## ★ RE-ENTRY BRIEF (written Sep 6 2026 — project asleep until next weekend)

**Integrator/reviewer SOP: docs/INTEGRATOR.md** (prompts, plan-gate
rulings, delivery mechanics, verification before merge, doc upkeep,
standing judgements). METHOD.md remains the law for implementation
sessions.

**Tree of record: `main`.** Verify against docs/Provenance.md (post-P33-B
table). Build: `cd Work/emulation && make`; self-tests:
`tests/run_helpers_selftest.sh`, `tests/run_strings_selftest.sh`,
`tests/run_strhooks_selftest.sh` (all GREEN with teeth RED). Launch
recipe: docs/Run.md ("Lockstep play session") with
`QUEST_BLOCKS=quest.blocks.split QUEST_SYNC_LIST=quest.synclist.p27
QUEST_IR=quest.ir2.book QUEST_ADDRESS_BOOK=quest.addrbook
QUEST_PUSH_MAP=quest.pushmap.M4` (book) or `QUEST_IR=quest.ir2.stock`
(stock); the string checker is `QUEST_STRINGS_CHECK=1` with
`QUEST_STRHOOKS=quest.strhooks QUEST_ARENA=quest.arena` (task 047b's
env is the reference). Batteries: `tasks/047b-p33b-arena-temps.sh` is
the current 16-leg template (JOBS=3, `bin/task_source.sh`); task files
go on MAIN.

**Where the IR stands (ir 6).** Embeds 557 book / 2,436 stock — from
27,600 on Aug 29. Nothing of the string family remains
(WCMV/WCMP/WBLM/WMSP/STASP 0). Left: call/frame machinery (LCALL 159,
WSAVS 130, LJSR 130, XCALL 37, WSAVR 1 — milestone 5b's edges), float
59, DIVX/WDIVS/WLOB 21, SYSCALL 10, WPSH 8, the 2 LDSP DERR sinks.
Statements: 1,822 string, 987 rt_call, 2,273 assert, 11,442 goto, 57
arena twins. Designs of record: docs/IR.md (ir 6), Project26/MathDesign,
Project29/StringsDesign (+ corrections log), Mapper.md §1.4, Project22/
BlockSyncDesign, EmulatorDivergences.md, Provenance.md.

**Done Sep 5–6** (each with a REPORT in its docs/ProjectNN/): P26
grammar package · P27 DERR-cluster folds (assumed-foldable.txt is the
debt 5b discharges) · P28 rt_call · P29 strings research + design · P30
string library · P31 located strings · P32 append chains · P33-A/B/C
string checker, arena twins, the shutdown fix · P34 readable-layer
prototype (compiler/readable.py (moved Sep 8; was emulation/tools/); PICK_X_Y renders as the hand reading).
Also: the manual-review hardware fixes (HWFindings_Sep5.md,
EmulatorDivergences.md), two Tools fixes (Follow: XJMP pc+2, LJSR I.GOTO
pc+3), the runner's staging leak (bin/task_source.sh).

**TODO — MUST BE FIXED FIRST next weekend: docs/Project33/FINDINGS_SUMMARY.md
§2 (the play driver; play verdicts requiring I.STOP; the "never executed"
verdict line). Until it is fixed, every play leg ends by SIGTERM and four
screens are never exercised by any battery — treat the play legs'
coverage as partial.**

**P41 LANDED Sep 9 2026** (docs/Project41/REPORT.md). The FIRE mutual
check, owed since P38, is discharged: **vacuous on the pair P39 named**
(FIRE.1 and FIRE.2 reach disjoint slots), then performed against the
family with zero disagreements. R41 audited and ENFORCED; the same R21c
defect found and fixed forty lines below P40's fix. Five bad witness PCs;
`check_frames()` tightened to check the citation resolves to the
instruction it quotes. 349/349 primary, 242/242 folded at landing.

> **RULING OWED — R41′.** `bit_base_reg` loads a record base by plain R7
> and its witnesses put it in **ac1** (70166278, 7016628B), which R41's
> wording says is never a candidate. The implementation is right on
> either reading; the rule TEXT overclaims. The proposed narrowing is in
> Project41/REPORT.md §"R41's stated SCOPE". CODEGEN_RULES.md is
> deliberately unedited — it is a design of record.

> **DIRECTION (Sep 9 2026).** P43's prompt names
> `docs/Project42/REPORT.md` and its production table as its foundation,
> but **P42 has not been built** — main carried only its prompt. The
> order is P42 first (the reduction-ordered emitter; behaviour-preserving,
> no new rules, no routines closed, still 349/349 and 242/242), then P43
> once a real P42 REPORT.md exists. P43's TREE VINTAGE line will be
> corrected then. Do NOT improvise a reduction-address scheme over the
> current emitter: it would be unstable under exactly the C edits the
> choice-file format promises to survive.

**First items next weekend, in order**
1. **Play driver + play verdicts** (FINDINGS_SUMMARY §2): the driver's
   post-auto-move keys never reach the command prompt, so every play
   leg since task 034 has ended by SIGTERM (`end=clean` was a silent
   kill) and HELP / OBSERVE / DISPLAY_MAGIC / LIST_PLAYERS are never
   exercised by a scripted leg. Fix the driver (validate the FULL play
   sequence, foreground), require `I.STOP` for play verdicts, add a
   standing "statements never executed by any leg" verdict line
   (CheckerHistory Gen 6.2 items). Re-run the 16-leg battery.
2. **Gen 6.2 checker item**: the deterministic form of the P33-C fix —
   defer a halt to the next pair boundary (today `Lockstep::halting`
   just guards compare_pair).
3. **Float + divides** (80 embeds): same-helpers `f*()` family over the
   FPACs + DIVX/WDIVS/WLOB; small; manual pages owed
   (EmulatorDivergences §5).
4. **Milestone 5b design**: calls as edges, on-error edges (ON-unit
   bodies LOGON.1/.2, ALLY_PLAYER.1 are separate WSAVS frames — P34),
   discharge Project27/assumed-foldable.txt, frames → functions.
5. **P35 DONE** (Sep 8): recompilation as decompilation WORKS — three
   routines (PICK_X_Y, UPDATE_SCREENS, REFRESH_SCREEN) translate to the
   IR **197/197 statement-for-statement, register-exact** (compiler/
   translate.py + ircmp.py, reproduced on main; C99+C++17 clean); DIED
   refuses at the first bit field — the subset's edge. The DG compiler
   model is compiler/CODEGEN_RULES.md (25 rules). **P36 PARTIAL — MERGED** (fc623f8): four of five constructs landed for
   DIED — bit references (R26–R29a), record-field strings + a port of
   string_sites.py's symbolic address renderer (R31, R32), game→game
   calls (R30), plus ruling 1 (explicit cvwn/sx16/trunc16; implicit
   narrowing refuses) and ruling B (succs_of call fall-through: DIED
   73/73 blocks reached). P35's three stay 197/197 primary / 128/128
   folded, selftest PASS, C99+C++17 clean, **pragma count 0**. NOT
   landed: arena twins and the frame relocation — see P37. Original
   scope: docs/Project36/PROMPT.md; extend the subset
   to DIED — bit fields, ac3-repurposing, record string fields, arena
   twins, game→game calls; carried-in rulings (explicit cvwn/sx16/
   trunc16 intrinsics, no silent narrowing; falsify R10 against DIED's
   13 ac3 sites; no world-coordinate offset — CODEGEN_RULES tail).
   Deliver a Work.tgz (P35's session crashed sending a partial zip).
0. **P38 DONE (Sep 9 2026, branch `p38-routines`)** — see
   docs/Project38/REPORT.md.  **Routines finished: 0; total still 4 of ~130.**
   R33/R34 IMPLEMENTED and validated (four routines held 349/349 primary,
   242/242 folded; REFRESH_SCREEN's five LDAFPs run through the new path).
   Ledger: **R3b VOIDED** — P35's disjoint temp pools was never evidence, its
   string temps are live at every scalar allocation; replaced by R3b' (pools
   overlap, but a dead string temp's words reach a scalar temp only from a
   LATER statement).  **R41** (base-register class {ac2,ac3}, three witnesses)
   amends R33's mechanism and DERIVES R34.  **R8d** (the frame survives a
   join, conf A).  FP_COST=2, lower bound established, upper bound unwitnessed
   with the experiment named.  FIRE.1 and HIT_ANY_CHAR both ABANDONED with
   reasons; nothing staged.  DIED not reached — **all four
   pre-registrations remain NO WITNESS and DIED_PREREGISTERED.md is still
   valid.**  crossings.py moved to compiler/.  Next project's blockers, in
   order: **the static link / uplevel access**, **the twins/arena**, **floats**.

6. **P40 DONE — MERGED** (Sep 9). **0 routines finished; total 4 of
   ~130.** QUEST.1 abandoned at 16/86 with the reason. What it bought:
   **four rules amended, every one of which had been fitted to a single
   routine** — R36′ (the hoist lifts the *invariant part* of the
   reference), R36a (before the whole loop header, not the
   initialisation), R21e′ (the limit is an ordinary live value, reloaded
   only if its register was taken), R21c′ (the loop-register class was
   an exclusion and could fall through to ac3, the frame register — a
   reachable bug). **D4 answered by evidence**: QUEST (the parent) has
   the same loop with the registers PERMUTED, so "avoid the register the
   limit will take" survives where "avoid ac1"/"prefer ac2" fail, with
   OWNS as a negative control. The general statement-tree hypothesis was
   MEASURED (163/242 blocks) and NOT confirmed — recorded, not fitted.
   The session's own read: *the model closes the parts that had two
   witnesses and fails the parts that had one.* One thing has no witness
   at all — the emission order of a statement's operands — and should be
   established on the book, not patched against the comparator.
   → **SECOND CHANGE OF DIRECTION (user, Sep 9, later):
   docs/REWRITE_PLAN.md is the design of record.** Generate DUMB, then
   rewrite. Every rule today is PREDICTIVE — the generator must emit the
   book's exact form first try, which forces a bottom-up generator to
   know what only a later pass can know (R36 is the proof: P42's ledger
   found the hoist fires NO production at all, in exactly the one
   routine R36 was read off). Instead: a deliberately stupid generator
   whose output is correct and approximately shaped; a small fixed set
   of independently-justified, semantics-preserving REWRITES (hoist,
   materialise, coalesce, temp reuse, spelling); and an oracle that says
   which rewrite fires where — a far better-defined question, because
   the legal set is enumerable by RUNNING the applicable rewrites. A
   diff becomes a search ("what sequence takes A to B?") and the answer
   IS the oracle input. Guards: a rewrite justified only by "the book
   has this shape" is refused (that is a diff patcher); the set is small
   and fixed; **the rewrite count per routine is the metric** — six
   means the generator is right, sixty means it is laundering a wrong
   generator. Consequence: everything clever in translate.py
   (hoist_invariant_subscripts, the temp pool, the register cost model)
   is to be REMOVED and re-expressed as rewrite specifications. P42
   re-scoped (table = naive generator; do not port cleverness), P43
   rewritten as the rewrite engine, P44 becomes censuses per rewrite
   rule (where does it fire, and where does it NOT fire when it could).
   → **A FIRST CHANGE OF DIRECTION (user, Sep 9, earlier)**: the diagnosis that the
   model "closes what had two witnesses and fails what had one" points
   at the translator's SHAPE. The DG compiler was a bottom-up parser
   driving a code-generation table (a template per production plus a
   register allocator); ours is a recursive walk with ~45 rules as
   scattered conditionals, so a rule has no home, a divergence has no
   address, and a rule can only be derived from the one or two routines
   that exercise it. Plan: **P42** (docs/Project42/PROMPT.md) —
   restructure the emit layer into a production table fired in
   reduction order, behaviour-preserving, no new rules; **P43** (PROMPT WRITTEN,
   docs/Project43/PROMPT.md) — choice files keyed on reductions, so a
   routine closes as soon as its C is right and register/slot decisions
   are SUPPLIED rather than predicted. **A choice must be LEGAL and the
   production checks it** (a register holding a live value, an occupied
   slot, a reorder across a dependence, a value outside a production's
   class = HARD ERROR and a finding); a choice selects, never creates —
   it may not change which statements exist; a single-option "choice"
   is pruned. Choice-file size becomes the headline metric: what the
   model does not yet explain; **P44** — per-production censuses over the
   whole book (200 instances of one production, not 2), turning choices
   back into rules. 
   → **P41 PROMPT WRITTEN** (docs/Project41/PROMPT.md): deliberately
   narrow — the FIRE mutual check as THE deliverable, the R41 exclusion
   audit, then MOVE_PLAYER.1 and RANDOM only if budget remains; plus a
   standing instruction that operand emission order is measured on the
   book, never patched per-diff.
   **Carried, oldest first: (a) the FIRE.1/FIRE.2 mutual check is STILL
   not performed — second project running; declarations.json's
   parent-frame layout remains fitting, and this should LEAD P41, not
   trail it. (b) R41's {ac2, ac3} needs the same audit R21c just got —
   a class implemented as an exclusion can be left silently.**
   Integrator note: the branch shipped a quest_rt.h whose comment block
   was unterminated — NOTHING translated on it; `--selftest` passes
   without invoking the translator, so it did not catch this. Fixed at
   merge; INTEGRATOR.md §5 now requires translating a routine end to end
   before merging.

7. **P39 DONE — MERGED** (Sep 9, verified: four routines 349/349,
   selftest PASS, gen_declarations clean). **The static link is BUILT
   and works** — `UPLINK/UP/UPARG` (ruling c), the link is an ordinary
   R41 base (276/12 census), uplevel reads/writes/subscripts and the
   parent's by-reference parameters all lower; R42–R45 landed (three at
   A). Your witness guard is enforced as a HARD ERROR in
   gen_declarations' check_frames(). Two rules the gate did not predict,
   from LIST_PLAYERS.3: R21d (indirect DO control variable, C) and
   **R21e** (a DO limit that is an expression is evaluated once into a
   temp) — settled by a PRE-REGISTERED test: prediction written first,
   all three parts came out, loop head then matched exactly.
   **0 routines finished; total still 4 of ~130.**
   **THE MATERIAL GAP: the FIRE mutual check was NOT performed.** FIRE's
   parent-frame layout in declarations.json rests on single witnesses
   per slot; until FIRE.1 and FIRE.2 agree independently it is fitting,
   not derivation. **Next session starts at FIRE.2's cvwn/divide
   refusal, then FIRE.1, and treats FIRE's layout as unconfirmed.**
   LIST_PLAYERS.3 abandoned (an R9/R10 temp-placement divergence
   unrelated to the link). **Revised bottleneck: it is no longer the
   static link, nor twins/floats — it is EXPRESSION-LEVEL gaps (R9/R10
   temp placement, the width model for divide), which block nested
   routines for reasons that have nothing to do with nesting.**
   "16 of 23" is an upper bound on a weaker basis than it looked (see
   METHOD §16 on marker-based sweeps).

7. **P38 DONE — MERGED** (Sep 9, verified: four routines 349/349, 0
   routines FINISHED, total still 4 of ~130). Delivered: R33/R34
   implemented (Regs models ac0–ac3; the frame is an ordinary register
   content; REFRESH_SCREEN's five LDAFPs now come through it) with two
   corrections the derivation lacked — **R8d** the frame survives a join
   (a join executes no LDAFP) and **FP_COST ≥ 2**, upper bound
   unwitnessed with the experiment named. **R41** the base-register
   class {ac2, ac3}: address/base loads take ac2 then ac3, three
   witnesses in three routines, and it DERIVES R34 instead of fitting
   it. **R3b VOIDED** — P35's disjoint temp pools was never evidence
   (REFRESH_SCREEN's string temps are live wherever a scalar temp is
   allocated, so the observation could not have come out otherwise);
   replaced by R3b′, reuse from a LATER statement on, with a witness on
   each side. FIRE.1 and HIT_ANY_CHAR abandoned with reasons (the
   latter at 8/10 rather than fit a cross-procedural register model on
   three sites of one callee — open finding §9.5). DIED untouched: the
   four pre-registrations stand as NO WITNESS and
   DIED_PREREGISTERED.md is still valid — the experiment is intact, not
   spent. **Blocking constructs, in order: static link / uplevel access
   (23 nested entries across 13 parents — **P39 PROMPT WRITTEN**,
   docs/Project39/PROMPT.md: build the construct, validate on
   DROP.1 / KILL_PLAYER.4 / LIST_PLAYERS.3 / FIRE.2, then return to
   FIRE.1), twins/arena, floats**; plus three single-witness items (COM.# against −1, WADC as
   −1, the open register question). Throughput is now bounded by the
   constructs, not the register model.

7. **P37 DONE — MERGED** (Sep 9, verified: 349/349 MATCH across four
   routines, 242/242 folded, selftest PASS, pragma count 0). OWNS
   completed 152/152; GET_INPUT and RETURN_MESSAGE staged with their
   constructs derived but deliberately not fitted; UNLOCK_FILE dropped
   as hand assembly. **Three results outlive the project**: R39
   (inadmissible evidence — hand assembly contaminates its own metadata
   AND its callees'; the direction test), R40 (PL/I multiple ENTRY —
   two pairs, and per-routine addrbook statement counts MIS-SIZE them,
   which invalidates figures in P34's census and P37's own sampling
   frame), and the frame verdict: THERE IS NO RELOCATION — the frame
   pointer is an ordinary R7-managed value, ac3 is ordinarily
   allocatable, so the pragma allowance is moot (count 0). All folded
   into METHOD §16 + CODEGEN_RULES. **P38 PROMPT WRITTEN** (docs/Project38/PROMPT.md) — *implement, then
   match*: the project stands at 4 matched routines of ~130, so P38's
   binding constraint is **stage nothing** (finish or abandon with a
   reason; "staged, not fitted" is not a permitted outcome). Order:
   implement R33/R34 FIRST
   (Regs must stop pinning ac3 — it unblocks the most, and FIRE.1 is
   the small validation case), then twins on INIT_OBJ_TBL, then DIED as
   a PRE-REGISTERED EXPERIMENT (docs/Project37/DIED_PREREGISTERED.md:
   four one-witness beliefs ride on it — R29a, R36, R7c′'s release
   point, the join self-move — with R7c′ named in advance as the most
   likely refutation; a contradiction is a finding, not a cue to adjust
   the rule). Also carried: three predicted arm-path defects left
   unfixed so tripping one is a confirmation; the BITS()-shaped
   argument as a live search target (check DIED's eight call sites);
   TRANSPORT_SUNDAR is an R40 pair and must be reconstructed as one
   function with two entry prologues; run compiler/crossings.py at the
   start of any session that adds routines. Original scope:
   docs/Project37/PROMPT.md — changed approach
   after P36: prove each construct on SEVERAL routines rather than
   driving one big one. Ordered set (censused): UNLOCK_FILE 38 (bits +
   ac2 spelling) → DISTANCE_TO_PLAYER/RETURN_MESSAGE/GET_INPUT 30/30/22
   (no new constructs — the generality check) → INIT_OBJ_TBL 173 (the
   smallest claim group, twins in isolation) → FIRE.1@7016A3BD 89 (the
   smallest frame relocation: 3 ac2=wfp in 89 stmts) → OWNS 152 (second
   bit witness) → one RANDOM routine (the honesty test) → DIED as
   capstone. Success = completed routines and promoted rules, not
   percentage-of-DIED. The two unbuilt constructs carried over — (a) the FRAME RELOCATION,
   which is bigger than P36's prompt described: from 70166376 to
   701663B3 the compiler does `ac2 = wfp` once and keeps a record base
   in ac3 for ~10 blocks, so all 22 frame refs spell `wp(ac2, 8)`;
   Regs models ac0–ac2 with ac3 pinned, so this touches every emit
   site — the user's `#pragma fp ac2` allowance is expected to earn its
   keep here (pragmas counted next to the match number, recorded as
   unexplained directives). (b) the arena twins/claim groups — sizing
   is DERIVABLE (ceil(bytes/4) over the CAT chain, +2 for the length
   word; every constant falls out of the literal lengths), needing only
   a `LEN(v)` header form for a VARYING parameter's length. Then the
   full 495-statement loop and R10's verdict (both its supporting site
   701660F8 and its counter-instance 70166110 must be translated to
   judge it; R10 stays at confidence C with the counter-instance
   recorded). (translator + four routines + an IR
   comparator with slot/temp/label equivalences; primary target the raw
   book, secondary the register-folded one). Design decisions of record
   (Sep 8): compiler/README.md
   (pycparser front end; `NAME$N` arity-in-name runtime calls with
   `const` transcribing read/write; one body + leading `int arg_count`
   for two-arity game routines; 1-based indexing folded in the
   expression generator, `ARRAY1()` objects natively; the C/C++ delta
   confined to the generated game/declarations.h, seeded from
   GAME_REFERENCE.md + the P34 record census). Pilot PICK_X_Y, then DIED.
6. Tools (user): Follow and StartStop should share one successor
   function; Java↔C++ helper diff (EmulatorDivergences §5).

**Operational reminders**: task files on MAIN; a task's three attempts
are consumed by kills — re-queue under a new name; inherited verdict
lines carry stale wants — read the legs, then fix the wants; self-test
link scripts must list hw/strings/*.o; lower.py refuses a stale
assumed-foldable.txt header whenever Disassembled changes (regenerate
with derr_clusters.py); the ?UNSIGNED_TO_CHAR convention is "writes a
varying at ac2, returns ACs unchanged" (RTConventions.md was right).

---

## History — Sep 5–6 per-project hand-off blocks (all merged; superseded by the brief above)

## [merged] P33-B (Sep 6 2026) — arena twins, the string family retired
- Branch p33b-arena-temps: hw/strings/Arena, StrHooks per-claim + claim
  insertions, Mapper claim-insertion layer, IRExec ir 6, tools/{arena,
  p33_census}.py, string_sites.py --p33, lower.py --strings-sites33, the ir 6
  artifacts + quest.arena + quest.strhooks, docs/Project33/{CensusB,REPORT-B,
  REPORT_worklog-B,p33.ledger,p33.tsv,censusB_raw.txt}. **Task 047** on
  main: check results/047-p33b-arena-temps before merging (bar: 16/16, 0 div
  on 15, embeds 0/0/0, clone claim=0 on the IR legs).
- Integrator: Mapper.md §1.4 (claim-insertion layer; master temp → bound
  twin, unique while outstanding), StringsDesign §6.3 (TOTAL outstanding
  claims), §2.1 ("ground: t@b.k; readable: p@b ≡ t@b.last"), §5.2 (twins,
  claim, release) — REPORT-B §6 has the text.
- OWED: the play driver (docs/Project13/drive.py, drive_patient.py): the
  post-auto-move keys O/D/L/H never reach the command prompt (046's HELP
  fail was the first observation; P33-B's play run confirms — only
  DISPLAY_SCREEN / DISPLAY_INVENTORY / INIT_OBJ_TBL groups execute). Read
  the session log at the O step and re-place the keys; then DIED (the `m`
  driver), DISPLAY_MAGIC, LIST_PLAYERS, OBSERVE, HELP, GET_QUEST, OP_EDIT,
  STORE become live gates. Then set quest.arena's `cap=` column from 047's
  max-claim lines (regenerate p33.tsv + ir 6; the provenance chain enforces
  it). §5.3's LOCK_FILE constants remain the P33 tail item.


## [merged] P32 (Sep 6 2026)
- Branch p32-append-chains: the P31 correction commit (17 wrong
  statements — docs/Project31/Census.md §11; p31 649/127; artifacts
  649 / 1,673 / 3,552) then P32 (944 sites, 0 refused; 1,593 string
  statements; embeds 729 book / 2,608 stock; sync list 13,510
  unchanged). Task 046 queued on main — the landing bar is in its
  header. Local K=1 gates per slice: docs/Project32/REPORT.md §3.
- Read docs/Project32/Census.md §1 before touching strings again:
  pairwise concatenation (chains are 1–2 pieces, never cross a block),
  `?UNSIGNED_TO_CHAR` returns nothing, tail splits carry the room in
  src_count, and the evaluator now refuses to render any tracked value
  reloaded across a call (the P31 defect class).
- Integration: regenerate artifacts + binaries together (Provenance.md:
  string_sites.py --p31 --p32, then lower.py --strings-sites32
  --strings-slice 6). lower.py refuses assumed-foldable.txt whose
  tags/blocks sha header is stale — regenerate it with derr_clusters.py
  whenever Disassembled changes (09f6593 needed that; content identical).
- Next: P33-B (docs/Project33/PROMPT-B.md) — the 19 `p@b` groups; its
  population (96 sites / 37 blocks) is the tail of p32.ledger.


## [merged] P31 (Sep 6 2026)
- Located strings (ir 5) on branch p31-located-strings: 652/776 sites
  lowered (528 `v = 'lit'`, 74 `v = t`, 7 `fixed = 'lit'`, 31 cmp, 12
  word fills); embeds 2,322 → 1,670 (book), 4,201 → 3,549 (stock); sync
  list unchanged (13,510). Local K=1 gates 0 div (slice 0/1/2/3 k1fo
  book+stock, k1play book 8.8M pairs). Task 044 queued (042's 15 legs +
  the P31 verdict lines). Records: docs/Project31/{Census,REPORT,
  REPORT_worklog}.md, p31.tsv (the emitter's artifact), strings.ledger;
  IR.md ir 5 §5.8; Provenance.md.
- Integration notes: ir 5 loader refuses ir 4 files — regenerate
  artifacts + binaries together (Provenance.md has both commands: the
  tsv from string_sites.py --p31, then lower.py --strings-slice 3).
  Library fix F-B1 (EagleString::varying sign-extends; erratum in
  docs/Project30/REPORT.md) → StringsDesign §2.4 wording; Census §9
  wording for §2.1/§2.3/§7.
- Follow-ups: P32 takes the 124 refusals' shapes (86 copy-outs, 15
  substr, 9 chain capacities, 6 t-place-form sites); P33 reads claim
  sizes from docs/Project31/census_raw.txt (T2-clean).


## [merged] P33-A (Sep 6 2026) — the string checker, dark
- Branch p33a-checker-strings: hw/strings/StrHooks.{hpp,cpp}, quest.strhooks +
  tools/strhooks.py, hook lines in EagleStack.cpp (WMSP/STASP/WRTN) and
  frames.cpp, Mapper arena_bind assert (S2), Lockstep (symmetric-Δ wsp term,
  F2-b via `terminal_abort_pending`, readout `w/w−2`, arena-row naming),
  RTStubs (`QUEST_POKE …:CLONE|:MASTER`, loader call, verdict lines),
  ClaimDelta range/predicate exits, Trace types `arena`/`strings`,
  tests/strhooks_selftest (+ teeth), tests/strings_selftest wfp case.
  **Task 045** (committed on main): 040's 15 legs with the checker ON +
  k1fo-off + forced; check results/045-p33a-checker-strings before merging.
- Integrator: Mapper.md §1.4 — annotate the bind's wfp rule (rows keyed on
  the master's wfp; the mapper checks "real-stack address"), and that
  frame exit is in master coordinates. StringsDesign §6.3 — the wsp
  equality is now the symmetric form (§6.3's line is its P33-B special
  case). P27 REPORT §3's `w−2/w−4` is corrected by P33-A REPORT §3.5.
- P33-B inherits: docs/Project33/REPORT.md §7.


## [merged] P30 (Sep 6 2026) — string library, dark
- Branch p30-string-library: hw/strings/{EagleString,ClaimDelta}.{hpp,cpp},
  the Mapper arena form (Mapper.{hpp,cpp}), tests/strings_selftest.cpp +
  run_strings_selftest.sh (GREEN; -DP30_BROKEN_RESIDUE build RED),
  Makefile. Task 043 (k1fo + play-st vs 042, both self-tests as
  preconditions) — check results/043-p30-string-library before merging.
- Integrator: Mapper.md §1.2 codec table and §3 mutation kinds owe the
  arena column / the two arena events (design-of-record edit; the
  Mapper.hpp header already says it). Row struct carries `block` and
  `capacity` beyond StringsDesign §6.2's four fields.
- P31 Phase B waits on this merge; docs/Project30/REPORT.md §8 lists the
  calls P31 and P33 make and the one ordering trap (`EagleString::varying`
  reads the length word at construction — build the piece after the
  master's XNSTA, or use `assign_varying`).


## [merged] P28 (Sep 5 2026)
- rt_call decoration is IMPLEMENTED on branch p28-rt-call (ir 4):
  987/987 runtime call sites, LNDO, the LDSP pair, the 67 Nova loads;
  embeds 6,258 → 2,322 (book) / 8,137 → 4,201 (stock); sync list
  unchanged (quest.synclist.p27). Local K=1 gates 9 legs green, 0 div.
  Battery: 041 15/15 legs green but RED marker (two verdict-line grep
  bugs, METHOD §10); 042 = same legs, lines fixed: **15/15 GREEN DONE,
  0 div, 902 s**, every P28 line on prediction (results/042-p28-rt-call).
  Integrated (merge on main). Records: docs/Project28/{Census,
  RTConventions,REPORT,REPORT_worklog}.md; IR.md ir 4 §3/§6/§9.
- Integration notes: ir 4 loader refuses ir 3 files — regenerate
  artifacts + binaries together (Provenance.md has the command; `--rt-
  slice 3 --leftovers` is the artifact of record). QUEST_SYNC_LIST stays
  quest.synclist.p27. docs/Project28/rt_call.ledger is the per-site
  record (task 041 checks it).
- Follow-ups: F2-b (P27) now has a second customer (the LDSP range
  assert); the t-place argument form is specified but unemitted (0
  sites); the next embed census by mnemonic is in CURRENT_STATE (WCMV
  1,637 dominates — string ops; then the 159 + 130 + 37 undecorated
  calls/LJSR/XCALL and 130 WSAVS frames).


## [historical] Queue (Sep 5, post-P27 merge)
- P28 rt_call — DONE, merged (042 15/15 GREEN).
- P29 strings — research DONE (Census.md) and design DONE:
  **docs/Project29/StringsDesign.md** is the design of record (Sep 5–6).
  Implementation is four projects: **P30 library — MERGED (043 GREEN)**;
  **P31 located strings — MERGED (Sep 6; 044: 15/15 legs green, one
  verdict-grep defect, see Project31/REPORT.md integrator note)**; **P33-A** (the checker half, dark:
  docs/Project33/PROMPT-A.md — hooks, ClaimDelta wiring, F2-b, the
  ABORT readout) may run in parallel with P31 Phase B (no shared
  files); then **P32 append chains (docs/Project32/PROMPT.md — after P31 merges)**,
  P33-B p@b executor half. **P34 readable-layer prototype**
  (docs/Project34/RESEARCH.md — research, Python only, no shared files)
  may run any time.
- Later: float + DIVX/WDIVS/WLOB (~80 embeds), F2-b checker item + ABORT
  readout off-by-one, retire borrow-slot allocation from the address
  book, the typing pass (flat-graph world). Frames stay embedded until
  the flat-graph world makes them function boundaries.


## [merged] P27 (Sep 5 2026)
- Roadmap item 2 (DERR cluster compression) is IMPLEMENTED on branch
  p27-derr-clusters: 2,271/2,273 DERR embeds gone (the 2 LDSP sinks →
  P28), embeds 8,529 → 6,258, shipped sync list quest.synclist.p27 =
  13,510 (identity minus 4,499 interiors). Rulings F1=A / F2-a / F3=
  QUEST_POKE (docs/Project27/Census.md §2, REPORT §2). Local K=1 gates
  3/3 green + the derr leg paired at 7015C48E; task 040 15/15 GREEN on
  the runner (0 div, 903 s; results/040-p27-derr-clusters) — every P27
  verdict line on prediction. Ready to integrate.
- Integration notes: task scripts must now point QUEST_SYNC_LIST at
  emulation/quest.synclist.p27 (the identity .split stays the record of
  blocks.split, untouched); Provenance.md gains ir2.book/stock (new)
  and synclist.p27; docs/Project27/assumed-foldable.txt is the
  artifact of record (tags sha 90659843…). lower.py without
  --assumed-foldable still reproduces the P26 artifacts byte-for-byte.
- Follow-ups recorded: F2-b (checker: assert-detach + kind-2 terminal
  → TERMINAL-ABORT); `SYSCALL 0351` on the DERR.TRP → ?FATAL path
  (excluded subtree; the master dies there today — fine for the
  verdict, but a real DERR never prints its DG message); option B
  (absorb K) belongs to the flat-graph block merge; the goto-graph
  project discharges assumed-foldable.txt.
- P28 next: LDSP jump tables → `goto [labels] idx-lo` with a range
  assert in front (folds the last 2 DERRs), LNDO (register now
  visible), the 67 Nova loads (HWFindings §3: high half undefined,
  match the emulator's zero-fill).


## [historical] Sep 5 2026 planning session — integrator notes
- **Branch hw-findings-sep5** (on top of p26-math-grammar) carries:
  (1) the seven emulator helper fixes from the manual review —
  docs/HWFindings_Sep5.md (WHLV rounds toward 0; narrow_add/sub/mul
  overflow bit 15, sign-extended results, ~src+1 carry, sticky ovr) +
  emulation/tests/helpers_selftest.cpp (red on the old code, green on the
  fix); task 038 k1fo + play-st GREEN, 0 div, clean endpoints;
  (2) the regenerated Disassembled/ after the user's Tools fixes —
  Follow.java XJMP pc+2 edge dropped (1,160 tag/blocks successor lists;
  targets + synclist UNCHANGED), OldDisassembler XCALL `ea,arg` (63
  lines) and LNDO/LWDO register field (1 line); build_address_book.py
  re_xcall updated (generated book byte-identical); blocks.split,
  ir2.book, ir2.stock regenerated (embeds 8,529 / 10,408 unchanged);
  task 039 (same two legs) queued — check results/039 before merging.
  quest.addrbook and quest.pushmap.M4 are NOT regenerated (hand-curated
  live set; dis-independent) — do not overwrite them.
- Rulings of record from the manual (HWFindings §3–4): ECLIPSE 16-bit
  ops leave the high half UNDEFINED → the 67 deferred Nova load forms
  may lower matching the emulator's zero-fill, spec says don't-care;
  the emulator's sticky OVR model is a documented, benign divergence
  from the hardware — do not "fix" it without re-reading §4.
- MERGED to main Sep 5 (p26-math-grammar + hw-findings-sep5; note the
  runner's results commits had already carried both trees onto main —
  fixed: bin/runner.sh resets the index, tasks use bin/task_source.sh).
  Task 039 (regenerated artifacts) GREEN.  Checksums: docs/Provenance.md.  Then P27 = DERR cluster compression
  — PROMPT DRAFTED: docs/Project27/PROMPT.md (seven skip shapes in
  scope; the LDSP pair → P28 with LNDO + Nova loads) (Python only; predecessor census from
  quest.tags now clean; `assumed-foldable` artifact; asserts carry the
  DERR code).  Small follow-ons: LNDO lowering (register now visible;
  mirror XNDO with pc+4), the 67 Nova loads.


## [merged] P26 (Sep 5 2026)
- Roadmap item 1 (the P26 grammar package) is IMPLEMENTED on branch
  p26-math-grammar: IR.md is now ir 3; embeds 27,600 → 8,529 (bar
  8,600); local K=1 gates 3/3 green; battery 037 13/13 GREEN on the
  runner (0 div everywhere; REPORT §5). Awaiting review + integration. Full record: docs/Project26/{Census,
  REPORT,REPORT_worklog}.md. Roadmap item 3 (de-embedding) is thereby
  largely delivered: everything left embedded is out of P26 scope by
  construction (list in REPORT §8.4) except the 67 Nova load forms
  (waiting on the user's manual check) and 1 LNDO (dis rendering gap).
- NEXT: (a) review + integrate the branch; (b) the user's manual findings
  (WMOVR/WHLV/WDIV/Nova high half) → REPORT §3; (c) roadmap item 2 =
  P27 DERR cluster compression (2,273 DERR embeds; the first customer
  of the translations-ship-their-sync-list contract); (d) the XJMP CFG
  edge and LNDO rendering fixes are the user's (disassembler/CFG tool);
  regenerate + diff-audit per METHOD §14 when they land.
- Integrator note: ir 3 is NOT a superset of ir 2 (`#` family gone,
  plain-goto dump form gone); binaries and artifacts move together.


## RE-ENTRY BRIEF (written Aug 29 2026 — HISTORICAL; superseded by the Sep 6 brief at the top)

**Where we are in one paragraph.** The 1986 PL/I game Quest runs under
a C++ emulator with a master/clone lockstep harness (METHOD.md is the
binding discipline; Run.md how to run). Gen-6: the clone can execute
the game as an IR (docs/IR.md = the law for the shipped rev-2
grammar). ALL 566 decorated game→game call sites lower (P25); byte
addressing landed (wp/bp/M8, 0xW:b literals); the wide-carry emulator
bug is fixed with residue re-derived (P24); the block-sync checker
compares regs+c+ovr+wsp+block-ordinal at every K rendezvous (P22/P24);
batteries run PARALLEL on the runner box in ~9 min (task 034 template,
13 legs). Latest state entries: CURRENT_STATE.md (newest on top).
Latest battery: 035, 13/13 GREEN. Everything is merged to main; the
user works from Work.tgz archives, the runner box works from the repo.

**THE ROADMAP (user, Aug 29 2026 — priority order):**

1. **Add the new IR features** — the P26 grammar package,
   design of record docs/Project26/MathDesign.md: t-places (all 23
   borrow brackets, uniform), the strict `goto [label list] tN`
   terminator (conditional exits + switches), `tf()`, mandatory
   `<s/<u` ordering comparisons (bare `==`/`!=`), strict eager
   `&&`/`||`/`!`, C bitwise `& | ^ ~`, ISA-exact `ash()`/`lsh()`,
   and the `add()`/`sub()` effectful family REPLACING `#+`/`#-`.
   One open ruling left for the plan gate (pure-tier shift spelling
   after flag conversion — MathDesign §6).
2. **Compress the DERR clusters into single blocks with asserts.**
   DERR sites fragment the CFG: each design-era check is a chain of
   skip tests, so one DERR guard = many tiny blocks. Ruling: fold
   each cluster into ONE basic block whose checks are `assert`
   statements (the P25 assert op's first production use — note this
   is a DIFFERENT purpose from the rejected jump-table asserts:
   here asserts REPLACE control flow, shrinking the block census).
   Mechanism note: coarser blocks change block-ordinal accounting —
   this is the first real customer of the P22 "translations ship
   their own sync list" contract.
3. **Push hard on de-embedding** — get `@addr` instructions OUT of
   the lowered IR. Current census: 27,600 embeds across 18,006
   blocks (was 31,116 pre-P25). The grammar from item 1 (temps,
   conditional exits, add/sub family, shifts) is what unlocks most
   of them; measure by the embed count dropping.
4. **String formatting / MSP into the IR.** The PL/I string
   formatting machinery and its WMSP dynamic stack allocations
   (see Layering.md M4c for the WMSP/stack-residue background)
   need a census and an IR design — currently these paths are
   embed-heavy. Scope/design is plan-gate homework for that
   session.

**THE ARC AFTER THE ROADMAP (user, Aug 29):** once the IR is good
(items 1–4 above), the project RETURNS TO GETTING EVERYTHING FLAT —
the flat-graph analysis world: classify every routine per the
treatment ladder (terminal / summarize / graphize / clone,
docs/TreatmentLadder.md), build the flat per-routine graphs with
summarized calls, and proceed toward decompiled source from there.
IR maturation first; flattening second.

**How to resume:** read CURRENT_STATE.md top-down until it's
familiar, then MathDesign.md, then spec the next project prompt in
the Project-prompt house style (docs/Project25/PROMPT.md is the
freshest example; require the tree-vintage statement). Batteries:
copy tasks/034-parallel-battery.sh. Integrator diffs any incoming
tree against the last integrated Work.tgz.

**Also on the shelf:** the routine treatment classification
(terminal / summarize / graphize / clone) from the Aug 23–24
flat-graph session — docs/TreatmentLadder.md (a RECONSTRUCTION; the
original M5FlatWorld.md never made it into the tree — if the user
still has that downloaded file, it replaces the reconstruction).

**Small open notes:** (a) P25's ir2 artifacts were regenerated
post-battery for the 0xW:b literal (pure notation; reviewer accepted
the K=1 re-gate + an independent smoke on the literal-form artifacts;
the next battery re-covers them). (b) UPDATE_SCREENS borrow blocks
are unreached by scripted drivers — census-carried (ByteEA.md §5).
(c) The disassembler byte-operand defect is DEFERRED with a standing
lower.py shim (DISASSEMBLER_BYTE_OPERANDS.md). (d) KNIGHT_ATTACK /
DIVX carry-consumer sites: observed opportunistically in ordinary
play, nothing gated (P24 ruling). (e) Aug 29, post-integration: the
user ran a proper manual play session on the P25 tree — CLEAN (user
report). Real-play datapoint on top of the scripted batteries;
whether it reached the UPDATE_SCREENS borrow blocks or the
KNIGHT_ATTACK cluster wasn't checked — coverage remains
census-carried.


## P25 LANDED — battery GREEN, reviewed + integrated (Aug 29 2026)
- Battery 035: attempt 1 12/13 (inj-emu endpoint-reach flake, user
  ruling, evidence in REPORT §5); attempt 3 **13/13 GREEN DONE**.
  Reviewed + merged to main Aug 29; reviewer notes REPORT §9.
  P26 DESIGN OF RECORD: docs/Project26/MathDesign.md (goto [list] tN
  terminator, tf(), mandatory <s/<u, strict &&/||/!, C bitwise
  & | ^ ~, ash/lsh ISA-exact, add()/sub() family superseding #+/#-;
  1 open item for the plan gate). NEXT WORK: the four-item roadmap in
  the RE-ENTRY BRIEF at the top of this file (item 1 = the P26
  grammar package incl. t-places — all 23 borrow brackets uniform).
- P25 (byte addressing + call ledger) is IMPLEMENTED on branch
  p25-byte-addressing: **566/566 decorated sites lowered** (the user
  reversed the borrow exclusion in-session — bracket as @addr
  instruction pairs, args as stores; the old 564 target and the
  "96 B-form" framing are superseded — 18 of those 96 were word-form
  parse gaps, see Project25/ByteEA.md §1). Grammar: wp/bp pointer
  builders (masking in the executor), M8 raw-index loads/stores, `*`;
  `<<` removed (was spec'd, never implemented). Local gates 3/3
  green, 0 div. Battery 035: attempt 1 12/13 (inj-emu red ruled flake), attempt 3
  **13/13 GREEN, DONE** — flake confirmed. NOTE: the ir2 artifacts on
  the branch were regenerated AFTER the green battery for the 0xW:b
  literal (pure notation, identical census, K=1 local re-gate green);
  the reviewer may want the next battery run on the literal-form
  artifacts or may accept the K=1 gate. Also on the branch
  post-battery: the assert(e[, "msg"]) statement (spec'd in IR.md §3;
  fire-tested: print + detach + 0 div + master continues). NEXT: (1) integrate the branch.
  (2) Disassembler byte-operand
  defect: fix DEFERRED by user ruling (the byteIndexed masking is
  buggy in multiple ways; too risky to touch) — the lower.py
  reconstruction shim is STANDING; docs/DISASSEMBLER_BYTE_OPERANDS.md
  is the record and the future-fix protocol. (3) P26 = t-places (the two @addr borrow bracket
  pairs get the same treatment as the other 21 — no special
  casing, user ruling Aug 29) +
  conditional exits.


## Battery template (Aug 29 2026)
- tasks/034-parallel-battery.sh is the battery template of record
  (parallel, JOBS=6, 13 legs incl. the emu isolation pair); hold/031's
  serial shape is superseded. Copy 034, not 031/032.


## P24 handoff (Aug 29 2026)
- P24 (wide-carry) is LANDED on branch p24-wide-carry; battery = task
  032, verdict RECORDED in Project24/REPORT.md §7: 9/11 GREEN; 2 RED =
  finding F6 (inject/terminal arming vs IR at non-block-entry pcs — a
  pre-existing P23 gap, no carry involvement, evidence in the report).
  **F6 RESOLVED (Aug 29, user ruling: both):** loader drops armed-pc
  blocks + standing all-emulated inj/abort legs; task 033 4/4 GREEN —
  the terminal legs are back in the battery. (b) RESOLVED (Aug 29,
  user): census suffices; KNIGHT_ATTACK/DIVX observed opportunistically
  in ordinary play — nothing gated. (was: whether the sites
  need live-pair demonstration (a manual combat/store play session) or
  the census classification suffices.
- Coverage caveat of record: the four ADC.C consumer sites are in
  BEING_ATTACK/KNIGHT_ATTACK (combat); scripted legs may not reach
  them. Census proves them fix-invariant regardless; live combat pairs
  need a manual play session if the user wants them demonstrated.
- Integrator: main lacks the P23-integrated tree; p24-wide-carry
  carries it as its base. Merge order: P23 integration then P24 (or
  the branch wholesale).
- NEXT per the P23 queue: P25 = t-places (borrows pilot), then B-form
  byte-EA extraction (96 call sites), WPSH multi-wide (25), `save`,
  @/bit-15 fix+regen+diff-audit (25 blocks). Pre-P25 census owed:
  crossings inside borrow-bracket interiors.

> **[Aug 29 2026 — P23 banner]** This re-entry narrative is from Aug 13
> and predates P14–P23. Current state now lives in CURRENT_STATE.md
> (newest entry on top). **P23 LANDED** (Gen-6.1: the IR — reviewed and
> integrated Aug 29; docs/Project23/REPORT.md, spec docs/IR.md; all P22
> obligations discharged). Next work, per REPORT §8: the wide-carry
> re-verification (parked task, docs/Project23/WideCarry.md — redo the
> carry-live-in census BEFORE landing the parked patch), then B-form
> byte-EA extraction (96 call sites), WPSH multi-wide (25), `save`,
> and the @/bit-15 listing fix+regen. **P24 = t-places** [historic — renumbered, then folded into roadmap item 1 at top] (borrows are
> the pilot per user ruling; `end if` conditional exits are the
> boundary). The narrative below remains valid history for
> Milestones 1–3.

Hi Claude!

Quest reconstruction project — a game written for the Data General
MV/8000 (Eclipse Eagle) under AOS/VS, circa 1988, in PL/1. Source lost;
binaries survive; the user's emulator runs the game; we work backwards
to reconstructed C++17, keeping running code faithful. The user has
been away for weeks and will have forgotten details — THIS DOCUMENT IS
THE RE-ENTRY PATH. Trust the docs over anyone's memory, including his.


## State of the world (end of the Aug 13 2026 sessions)

1. **Milestones 1–2 done.** Dual-emulation lockstep harness: master
   and clone clients + one server, verified pairs, shared pages
   mirrored/compared, syscalls mediated.
2. **Milestone 3 formally done.** The full condition chain — raise,
   search/dispatch, unwind, frames, registration, the DEF?ON cluster —
   runs NATIVE (bit-faithful) on the clone; validated on the natural
   triggers plus three injected shapes (QUEST_INJECT) at 0
   divergences. Terminal machinery complete: DETACH (I.STOP/?FATAL),
   ABORT (DERR.TRP, abort_world, save suppressed), RETIRE (?RETURN
   0310 at dispatch).
3. **M3b Phase 1 delivered and approved**: docs/Project6/L2Contract.md
   + NativeDesign.md + REPORT.md + REVIEW.md — the contract that lets
   a stack-free L2 replace the bit-faithful one under
   contract-fidelity.
4. **THE CROSSINGS-ONLY CHECKER IS LIVE (Aug 13, second session).**
   The lockstep checker was REPLACED — no flag, no modes (user
   ruling: one sync model, permanently). Sync surface: L1 fabric
   unchanged (heartbeat, syscall gate, L0/L1 entry and leaf pairs);
   L1↔L2 crossings pair in BOTH directions (entry pc via deferred
   dispatch, exit at the post-call/transfer target); interior L2→L2
   invisible BY RULE (layer map, not translation coverage); L3 door
   unchanged. Full design + Step-0 characterization + regression
   evidence: docs/CrossingsChecker.md. Generation lineage (M2
   entry-keyed → M3b crossings-only → anticipated M4 shadow
   accounting): docs/CheckerHistory.md. Recalibration gate: all
   seven regressions at 0 divergences against the unchanged
   bit-faithful L2; the two anomalies found reproduced bit-for-bit
   on the pre-change baseline binary (environmental, recorded in
   CrossingsChecker.md side findings).
5. **Superseded**: Project6.5/PROMPT.md and Project7/PROMPT.md (both
   drafts of the harness-prep project — done differently and without
   the flag in the Aug 13 session; banners added). H1 of
   Project6/REPORT.md is resolved; H6's rendezvous is built and
   waiting for its Phase-2 native tail handling.


## Read order

1. docs/METHOD.md — how to work here. Binding, unchanged.
2. docs/Layering.md — the strategic frame (start at "Why this
   exists").
3. docs/CheckerHistory.md + docs/CrossingsChecker.md — what
   "verified" means now, and how it got that way.
4. docs/Project6/L2Contract.md + NativeDesign.md + REPORT.md — the
   contract and the Phase-2 design + hazards.
5. docs/TerminalDetach.md, docs/SharedProtocol.md — the machinery.
6. docs/README.md — index of everything else.


## UPDATE — Aug 15 2026 planning session (supersedes "Next work" below)

M3 is COMPLETE (SessionPlan.md final entry). The flat-graph analyses
are DEFERRED; M4a comes first. Design of record: docs/M4aDesign.md.
Project 12 DONE and approved (READ_IN off the stack, 0 div; its
rulings folded into M4aDesign.md §8). Project 13 batches 1+2 LANDED (45 routines live at 0 div; M4aDesign §10). Project 14 Phase A LANDED and approved: hw/Mapper.{hpp,cpp} implements
docs/Mapper.md (now updated with the P14 rulings: three-call surface,
Q2 overlay, wsl latch, wave-scoped validity conditions); accreted
T/T_any/T_inv deleted; R-C book layout fix (wfp -2); QUEST_INJECT
fail-loud; regression matrix is BANDED for world-downstream counts
(?GTOD wall-clock — see Run.md). Next work: Project 14 PHASE B per
docs/Project14/PROMPT.md — batch 3 (33 remaining wave-one routines,
parents + callable children together), play-driver growth toward the
19 armed-but-unexercised routines, landing roll-call + CheckerHistory
Gen-4 append (include the stride-masking near-miss sentence).
Note: the Tools/ tarball is now named Disassembled/.


## Next work, in order (HISTORICAL — see UPDATE above)

1. **Phase 2: the stack-free L2 itself** — prompt WRITTEN:
   docs/Project8/PROMPT.md (Project 7 is a burned name; see its
   banner). Includes the rulings to settle with the user first
   (I.EPILOG mismatch, walker outputs, chain cap, Q1 veto check,
   landing stages) and the shadow→flip→stop-writing landing
   strategy. Spine:
   hazards H2–H7 in Project6/REPORT.md §6 (H1 is done). The
   rendezvous definition it must satisfy is implementation-
   independent (CrossingsChecker.md "What Phase 2 inherits"):
   dispatch at the same 20 entries pairs at the door before the new
   code runs; every traversal ends at the contractual L2→L1 exit
   state; the DISPATCH_RET re-entry pair already fires — Phase 2
   adds the native handling after it (H6). A/B validation regime:
   Contract §7 (register file never private; ABORT-INTENDED third
   result class; footprint captures retired for conforming
   implementations).
2. Then M4 de-stackification of L1 per Plan.md Step 2 (synthetic
   WSAVS copies to flat storage; the native condition system owns
   the wsp reset; checker premise = shadow stack accounting,
   CheckerHistory.md Generation 3 placeholder).


## Open questions (parked deliberately)

- Defensive-raise → abort: CLOSED, subsumed by DERR ruling 7; the
  contract marks those branches ABORT-INTENDED; wire to
  abort_world(save=false) when next touched.
- The L0? census rows resolve lazily on demand.
- The natural "Forced exit" stimulus remains unidentified (mid-session
  hard disconnect just waits for terminal reconnection).


## Environment gotchas (believe them; they cost hours once)

- Turn cadence on a slow 1-core container is ~49 s/turn. Waits under a
  turn look like hangs. Same-turn interactions (menus, login, the M
  prompt) respond instantly.
- Cheap signal trigger, no injection: `M` + direction (n/s/e/w) +
  `abc` at "For how many turns?" (CONVERSION, handled, full chain).
  Fault injection: `QUEST_FAIL_OPEN=USER_DATA_FILE` + `L`→`P`
  (handled signal 1; continue reaches the unhandled signal 2 →
  ?FATAL detach).
- ESC from the map quits cleanly (I.STOP detach + write-back). The
  historical plain L→P→ESC driver does NOT reach I.STOP on this
  container (first ESC leaves the list sub-menu / lands mid-turn;
  session ends at socket close) — reproduced on the pre-change
  baseline, so it is a driver artifact, not a checker bug. Use the
  M-trigger driver for ESC-detach regressions.
- Injected shape 1 dies at ?FATAL if the driver waits a full turn
  after P (an extra game turn runs before ESC; "invalid channel
  number" cascade) — also reproduced on baseline; Project 5's 15 s
  wait gets the documented clean continue.
- "Segment fault - block 0, page 1" at first login = server's
  IPC_TASK dying benignly (UNIMPLEMENTED.md §8). Every session.
  ~1,100+ "RESERVED ACCESS player[0]" stderr lines per session are
  standing noise. Ignore both.
- Backtraces → stdout, exceptions → stderr; use `stdbuf -o0 -e0`.
  Scratch-copy QUEST/ per run. Login: CL / Claude / quest / Y / any /
  F. `QUEST_TERMINAL=<hex-pc>[:ABORT]` = test terminal point.
- Capture tool: ALWAYS set QUEST_CAPTURE_DEST (Project3 REPORT sharp
  edge). QUEST_TERMINAL at the same pc as QUEST_CAPTURE suppresses the
  ENTRY snapshot (Project2 §4.4).


## Working agreements

Plan before code; explicit go-ahead; short replies over long
agreement. Opus-era rt/, emu_rt/, types/ are reference-only — the
disassembly wins, and for L2 the NATIVE SOURCE is the analysis
medium. Every translation validated under lockstep before the next.
Expect the docs to be wrong somewhere; that is the method — lockstep
turns conceptual mistakes into divergences.


## Setup

Extract tarballs: Work/ (emulation, docs, DG_Quest), QUEST/, Disassembled/,
Tools/. Build: `make` in Work/emulation (g++ ≥ 11, C++17, warning-free).
Run commands: docs/Run.md. Layering tripwires live in the emulator
source at EagleFloat.cpp (FP-throw ruling) and EagleStack.cpp
handle_overflow (wsp==wsl boot-gate — LOAD-BEARING: the game
deliberately overflows at startup; do not "simplify" it, we tried).
Previous prompt: NextSession.prev.md.
