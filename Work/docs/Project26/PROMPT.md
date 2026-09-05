# Project 26 — the math & control grammar package: retire the easy Eagle instructions

GOAL (user ruling, Sep 5): land the P26 grammar of record
(docs/Project26/MathDesign.md — terminator, booleans, word layer,
effectful family, t-places) PLUS the misc math ops ruled in on Sep 5
(mul/div, `/s /u %s %u`, stack-register reads), and use it to lower
the "easy" embedded Eagle instructions. Success is measured by the
embed census dropping: 27,600 embeds today (quest.ir2.book); the Sep 5
census puts ~12k in MathDesign's existing scope and ~4k more in the
misc-math additions. Expected landing: embeds well under 15k, exact
target set at the plan gate.

Hi Claude! Solo implementation session; the user reviews at the plan
gate and at the landing. Read docs/METHOD.md first, as always. Context
of record: **docs/IR.md** (the normative spec — this project amends
it), **docs/Project26/MathDesign.md** (the design input — every item
there is a user ruling unless marked open; this prompt's §"Sep 5
additions" extends it), Project25/{PROMPT,REPORT,ByteEA}.md (the
freshest house-style example and the class-cap census discipline),
Project22/BlockSyncDesign.md (the sync-list contract — this project
does NOT delist anything; that is P27). TREE VINTAGE: the Aug 29
integrated Work.tgz (P25 on main, battery 035 GREEN); state the
vintage in the report either way.

## Scope of record (settled Sep 5 — not plan-gate questions)

From MathDesign as written:
- §1 `goto [label list] tN` terminator, strict index, false=0/true=1.
- §2 `tf()`, comparisons with mandatory `<s <=s >s >=s <u <=u >u >=u`
  (bare ordering REFUSES at parse), unsuffixed `==`/`!=`, strict
  eager `&& || !`.
- §3 `& | ^ ~`; `ash()`/`lsh()` ISA-exact with signed amount.
- §4 effectful family at statement root only: `add sub adc inc neg`
  etc. per the census, each citing its EagleInstruction helper;
  **`#+`/`#-` are RETIRED** (every current emission converts).
- §7 t-places: all 23 borrow brackets, uniform; conditional exits on
  the §1 terminator.

Sep 5 additions (user rulings, this session):
- **`mul(a, b)` and `div(a, b)` join the §4 family.** Both set ovr in
  the emulator: `EagleInstruction::mul` (EagleInstruction.cpp:48,
  `ovr |= 1` when the 64-bit product does not fit 32) and WDIV's
  inline body (EagleCompute.cpp:63 — divisor 0 or -1/0x80000000 sets
  `ovr=1` and LEAVES acd UNCHANGED, otherwise signed truncating
  divide, no flag). Same-helpers principle: HOIST WDIV's body into an
  `EagleInstruction::div` helper first, so emulation and executor
  share one implementation (a refactor with a K=1 gate of its own).
  Covers WMUL 2,169 / XWMUL 45 / LWMUL / WDIV 98. IR.md §5's reserved
  `#*` mention is retired with the `#` family.
- **Pure, flag-free division: `/s /u %s %u`**, signedness suffix
  mandatory (the §2 convention), bare `/` and `%` REFUSE at parse.
  Zero divisor = LOUD FAULT in the executor, never a silent value.
  `*` stays as today (pure, wrapping, no flags).
- **Stack registers as first-class registers**: `wfp wsp wsb wsl`
  readable in any expression and assignable at statement root, like
  the ACs — BUT P26 EMITS READS ONLY. LDAFP (`ac[AA]=wfp`,
  EagleStack.cpp:527) → `acN = wfp`; LDASP → `acN = wsp`. No stack
  writes in P26: STAFP has zero occurrences in the game; the 19
  STASP embeds are the WMSP dynamic-allocation pairs (roadmap item
  4) and stay embedded; wsp/wsl writes touch the load-bearing
  overflow gate and wait for the RT-call decoration project. Covers
  LDAFP 1,778 / LDASP 62.
- **WMOVR** (EagleCompute.cpp:161: plain logical shift right by 1,
  no flags in the emulator — read it, do not assume the ISA rotate)
  → `acN = lsh(acN, -1)` or equivalent pure form. 114 embeds.
- **CVWN** (EagleCompute.cpp:80: sign-extend low 16, `ovr |=` if the
  wide didn't fit) → §4 effectful `cvwn()`. 278 embeds.

## The work

### Part 1 — census + open rulings (plan gate, before any code)

1. **Embed census by mnemonic**, from quest.ir2.book, bucketed into:
   (a) lowered by MathDesign as written, (b) lowered by the Sep 5
   additions, (c) proposed additions needing a ruling (below), (d)
   out of scope with the reason (DERR → P27; WCMV/WMSP/STASP → item
   4; XPEF/LPEF/WPSH/WPOP → RT-call decoration — 2,907 of 2,933 feed
   `?` calls; LCALL/LJSR/XCALL → call machinery). State the
   predicted post-landing embed count.
2. **Semantics table, ByteEA-style**: one row per mnemonic being
   lowered, with the emulator source citation (file:line), the flag
   effects, and the IR form. Signedness per skip/comparison site is
   read from the source per mnemonic (MathDesign §7), never guessed.
   Any mismatch between the emulator and the DG manual is a FINDING,
   reported, not resolved by the session (METHOD §5). Include the
   Java-vs-C++ shift-helper diff MathDesign §3 demands.
3. **Proposed additions for ruling** (present with counts and
   evidence; recommend in/out):
   - The **Nova ALU family** — `MOV.x`/`ADD.x`/`SUB.x`/`COM.x`/
     `NEG.x`/`ADC.x`/`INC.x` with the `ssccnkkk` shift/carry/no-load/
     skip fields (~1,100 embeds; MOV.L# alone 669). These are 16-bit
     ops with skips riding on the §1 terminator. Either a `nova_*`
     effectful sub-family or a ruled deferral; size the ruling.
   - `SEX`/`ZEX` (23 + ?): expressible today as `sx16()` /
     `& 0xFFFF`; confirm and include if free.
   - Anything else the census surfaces as one-line pure or
     one-helper effectful.
4. **MathDesign §6.2 ruling collected**: pure-tier shift spelling
   (integrator lean: ash/lsh only, no C `<<`/`>>`). Present, don't
   decide.
5. **Grammar spec draft** for IR.md: the full production list for
   §1–§4 + the additions, the refuse-at-parse list (bare ordering
   comparisons, bare `/ %`, nested effectful ops, `#`-ops), and the
   executor fault list (goto index out of range, zero divisor,
   non-0/1 boolean operand).
6. **Liveness plan**: which newly lowered mnemonics the standing
   battery legs demonstrably execute (IRExec first-execution lines),
   stated in advance; which cannot be reached by scripted drivers
   (census-carried, ByteEA §5 precedent).

STOP AND REPORT at the plan gate. Parts 2–3 proceed only on the
user's rulings.

### Part 2 — implementation

- **EagleInstruction**: hoist `div` (and any other inline flag body
  the census finds, e.g. CVWN) into shared helpers; emulation
  behaviour byte-identical (K=1 stock gate before anything else
  changes).
- **lower.py**: emit the ruled grammar. Order of landing, each slice
  behind its own K=1 gate: (i) `#+`/`#-` → `add`/`sub` conversion of
  today's emissions (no census change — pure respelling, a 0-div
  regression); (ii) §1 terminator + §2 comparisons → skips lowered,
  conditional exits; (iii) §3/§4 word ops; (iv) mul/div, `/s` etc.,
  stack-register reads, WMOVR, CVWN; (v) t-places / 23 borrow
  brackets; (vi) any ruled-in Nova family. TOTALITY unchanged —
  anything still inexpressible stays `@addr` with a censused reason.
- **IRExec**: evaluate the new forms; loader validation in the
  refuse-on-anything style (the P23 pushmap-parser lesson); the
  stack registers join the register file the executor exposes.
- **docs/IR.md**: the spec update (version-history entry, spec-wins —
  the update is itself a landing deliverable). MathDesign.md gets a
  banner pointing at the spec once landed.
- Regenerate quest.ir2.book/.stock; the synclist is UNCHANGED (no
  delisting — P27's job).

### Part 3 — validation

- Local gates per METHOD §15: K=1 book + stock legs after every slice
  above; final K=1 book/play legs with the first-execution evidence
  for the part-1.6 predictions.
- **Runner: UP** as of Sep 5 (host "godspeed", 4 cores / 15 GB, g++
  11.4, Python 3.10, NO Java; probe = results/036-runner-probe). Task
  037 = COPY tasks/034-parallel-battery.sh (the template of record,
  13 legs; not 031/032) pointing at the new artifacts, **JOBS=3** for
  the 4-core box, with an embed-count line and a new-mnemonic
  coverage line appended to the verdicts. Landing bar: 13/13 green,
  strict gate, 0 div, embed count at or below the part-1 prediction.
  Tools/ (Java) is NOT in this session's upload and cannot run on the
  runner — the project is Python/C++ only; if something appears to
  need Tools, that is a STOP-and-report, not a reason to reimplement
  a Java tool in Python.
- A red battery is STOP-and-report, never a solo iteration loop.

## Boundaries — BINDING

1. **Scope = the grammar package + the Sep 5 misc-math additions,
   nothing else.** No DERR folding, no synclist delisting, no
   checker changes, no stack WRITES, no RT-call decoration, no
   string/WMSP work, no flag-deadness analysis (MathDesign §5 is
   parked). Tempting adjacencies go in the report.
2. **Part 1 before any code.** The Sep 5 additions are ruled; the
   Nova family, SEX/ZEX, and §6.2 are the plan-gate rulings.
3. **Semantics from the emulator source** (file:line cited per
   mnemonic), then the DG manual where it speaks — never from the ISA
   name or apparent intent. WMOVR and WDIV are the anticipated
   places where the source and the manual may differ; a difference is
   a finding.
4. **Same-helpers principle (P23 ruling)**: the executor calls the
   SAME EagleInstruction helpers as emulation for every effectful op.
   No formulas in the IR or in IRExec.
5. **Strictness is not negotiable**: refuse-at-parse and loud faults
   as listed; no coercion, clamping, or defaulted signedness anywhere.
6. **Design-vs-reality: STOP AND REPORT** — a lowered site that
   diverges, a mnemonic whose source semantics contradict MathDesign,
   a borrow bracket that does not fit the uniform t-place treatment.
   Do not tune around evidence.
7. **Implementation bugs: fix and record** (METHOD §11).
8. Deliverables: census + semantics table (docs/Project26/Census.md),
   implementation + regenerated artifacts, IR.md update, task 037
   (pending or green, reported honestly), REPORT.md + worklog in house
   form, CURRENT_STATE/NextSession updates or an explicit integrator
   handoff note, and the TREE VINTAGE statement.
