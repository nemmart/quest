# Project 26 — the math & control grammar (ir 3): embeds 27,600 → 8,529

Session Sep 5 2026, solo implementation, plan gate + landing review.
TREE VINTAGE: the Aug 29 integrated Work.tgz (Work__72_; quest.ir2.book
provenance dis 1f9153c0…, blocks.split a5efa05f…, pushmap.M4 b8953659…,
argmap 39c42d4c… — all re-verified against the re-uploaded
Disassembled/). Repo main differed from the upload by ONE later
NextSession.md note (the user's Aug 29 manual-play datapoint); the
branch is based on main so that note is kept. Branch:
p26-math-grammar; battery: task 037 (034 template, JOBS=3) — 13/13
GREEN (§5). Ready for the planning session's review + integration.

## 1. Outcome

**Embeds 27,600 → 8,529** (−19,071; bar was ≤ 8,600, ruling R10).
Statements 30,013 → 40,382 expr + 13,628 goto; 18,006 blocks; skips
unchanged (the 3 standing exclusions); synclist UNCHANGED (no
delisting — P27). The grammar of record is now **docs/IR.md ir 3**:
`goto [labels] e` terminator (strict index, false=0/true=1), strict 0/1
booleans with mandatory-suffix ordering comparisons, eager `&& || !`,
the word layer (`& | ^ ~`, `lsh`), pure `/s /u %s %u` with loud faults,
the statement-root effectful family `add sub mul div cvwn ash nadd nsub
nmul` DEFINED as the shared EagleInstruction helpers (div/cvwn hoisted
from EagleCompute's inline bodies first, behind its own stock K=1
gate), `#+ #- #* #/` retired (refused), `c`/`ovr` readable + root-
assignable, stack-register reads (`wfp wsp wsb wsl`; writes refused),
`ind(e)`, block-local single-assignment t-places (all 23 borrow
brackets converted, uniformly), and the ruled-in census additions
(direct XJMP, Nova no-load tests, bit-in-memory ops, the narrow
family, XNDO/XWDO, SEX/ZEX/ANDI/CRYTO/WSKBO/XNISZ/WXCH). Local gates
3/3 green at K=1 strict, 0 divergences; 16/16 negative loader tests
refuse with the offending token. Census + semantics table:
Project26/Census.md.

## 2. Rulings taken at the plan gate (user, Sep 5)

R1 plain `goto L` kept as parser sugar, dump form `goto [L] 0`;
R2 direct XJMP → `goto` (indirect stays embedded; 0 indirect exist);
R3 Nova no-load `#` forms IN on the emulator's authority, the 67 LOAD
forms DEFERRED pending the user's manual check of the high-half
convention; R4 `ind(e)` IN with the spec obligation to define ind/R in
terms of the same helper (IR.md §5 does); R5 narrow family IN; R6
`M16[e] = nadd(M16[e], k)` unwrapped — the store truncates; R7 general
t-places IN; R8 the free one-liners IN; R9 ash/lsh only, no C shifts
at any tier; R10 landing bar ≤ 8,600. Standing notes: emulator is the
law — a manual disagreement is an emulator finding, not a lowering
change; the XJMP spurious edge is recorded, not worked around (the
user fixes the CFG tool).

## 3. Findings (details: Census.md §2/§5)

- **Nova load count**: the plan-gate draft said 43; the true figure is
  67 — caught when the first emission landed at 8,529 vs a predicted
  8,504 (§10 class; Census.md corrected in place).
- **Nova SNC/SZC prose inverted** in the draft table (SNC = skip when
  the carry bit is ONE, NovaCompute.cpp:72). lower.py derives every
  Nova test mechanically from the emulator's KKK/SS/CC tables, so the
  emission was never wrong; the prose was (§11 class, corrected).
- **LNDO rendering gap** (1 embed, 7015C0C7): the dis renders
  wideIndirectArgument as `LNDO [ea],arg`, the format shared with LCALL
  — the II register is dropped (Disassembler.java:165; XNDO's
  wordIndirectArgument prints it). Register unreadable from the
  listing → stays embedded (§14 flag; the user owns the disassembler).
- **XJMP spurious fall-through edge** in quest.blocks.split: e.g.
  7015C0B1 `XJMP [pc+0xF2] (0x7015C1A4)` lists `n 7015C1A4 7015C0B3`
  (all 1,160). The CFG tool treats XJMP as conditional. Recorded per
  ruling; lower.py checks only that the fold target is a successor.
- **Three 16-bit result conventions in one emulator**: Nova loads zero
  the high half (NovaCompute.cpp:65), narrow_add/sub sign-extend,
  narrow_mul zero-extends and ASSIGNS `ovr = 1` (EagleInstruction.cpp:
  108–116). Java and C++ agree with each other on all three. The Nova
  load forms are deferred until the manual speaks; the narrow ops are
  lowered as the emulator does them (emulator = law).
- **Java-vs-C++ shift helpers (MathDesign §3 demand)**: identical, line
  for line; the only residual is C++ `int32 << n` on a negative source
  (UB before C++20, wrap under g++ = Java). NO FINDING.
- **Manual checks owed by the user** (manual not in the upload): WMOVR
  (emulator: logical `>>1`, no carry, no rotate), WHLV (emulator:
  floor `>>1`), WDIV (ovr=1 + dst unchanged on 0 / INT_MIN÷−1), the
  Nova/narrow high-half question. All lowered per the emulator.
- **Borrow t-places drop the bracket's memory write** (design note,
  not a defect): in book mode the P20 slot is read only by its own
  WPOP; in stock mode the real-stack word below wsp is no longer
  written — wsp is restored inside the block, every rendezvous
  agrees, only dead-stack residue differs (METHOD §5 knows code that
  reads such residue; none is known to read below a borrow).
  Uniform treatment per MathDesign §7; flagged for the reviewer.

## 4. Implementation

- hw/EagleInstruction.{hpp,cpp}: `div` and `cvwn` hoisted verbatim
  from EagleCompute WDIV/CVWN (the emulator ASSIGNS `ovr=1` there —
  equivalent to `|=` on a 0/1 flag; `div` returns dst unchanged on the
  fault shapes); the whole family made static for IRExec. Slice 0 gate:
  stock K=1 failopen, 0 div, 372,391 pairs, BEFORE any grammar change.
  Honest caveat: lockstep cannot adjudicate a shared helper (METHOD
  §2) — the hoist's correctness rests on the line-by-line move.
- tools/lower.py: lower_one returns (statements, terminator); a
  BlockCtx allocates t-places, maps borrow slots to t's (pc→slot from
  the borrow map), and verifies every skip's successors against the
  CFG ([no-skip, skip] ascending, no-skip == dis-adjacent pc — refuse
  otherwise; the census had 6,822/6,822 conforming) and every
  XNDO/XWDO target against pc+1+arg. The Nova test generator
  (nova_test) is a transcription of NovaCompute.cpp's CC/op/SS/KKK
  decomposition into a t-place holding the 17-bit ALU value plus the
  emulator's carry/zero predicates. Immediate renderings per
  Disassembler format (tinyImmediateRegister prints imm(1..4),ac;
  registerWordImmediate/registerWideImmediate print `dec (0xHEX)`).
  Canonical `goto [L] 0` for fall-through/WBR/XJMP; header `ir 3`.
- hw/IRExec.{hpp,cpp}: new Parser — flat class-homogeneous chains
  (word / one comparison / boolean; mixed → refuse), prefix `~`/`!`,
  primaries t/c/ovr/wfp/wsp/wsb/wsl/ind/tf/lsh, refusals for bare
  ordering, bare `/ %`, `#`, `<< >>`, functional and/or/xor/com,
  nested effectful ops, stack writes, `goto [L] k≠0`; loader-static
  t-place definite assignment + single write; `goto [..] e` label
  tables validated against the sync list; Stmt gains eff/rhs2/labels.
  Executor: strict-index goto fault, `/s /u %s %u` faults (zero,
  INT_MIN/−1), 0/1 boolean faults, flag-assignment 0/1 fault; the
  effectful dispatch calls EagleInstruction::{add,sub,mul,div,cvwn,
  arithmetic_shift,narrow_add,narrow_sub,narrow_mul} with
  f(a,b) == helper(machine, src=b, dst=a); ovk/ovr check after every
  effectful statement as before.
- Artifacts regenerated (book + stock): 18,006 blocks, 40,382 expr,
  8,529 / 10,408 instr, 13,628 goto, 566 call, 165 ret.
- docs: IR.md → ir 3 (§1/§2/§3/§4/§5/§6/§8/§9; ind/R relationship per
  R4; R6 wording); Project26/Census.md (census of record, two
  corrections marked); MathDesign.md landed banner; lower.py/IRExec
  headers.

Deviation, stated: the prompt asked for one K=1 gate per landing slice
(i–vii). The emitter was written as one pass and gated whole (three
K=1 legs); class-level bisection (disable a lower_one branch) was the
fallback if red. It was not needed, and the whole-artifact gates are
strictly stronger evidence than any slice gate — but the per-slice
bisection record the prompt wanted does not exist.

## 5. Validation (local, METHOD §15, this 1-core box; ~35 min)

| leg | cfg | driver | result |
|---|---|---|---|
| base-k1fo (pre-change baseline) | book K=1 | failopen | 0 div, 384,752 pairs, 1,576 IR blocks |
| s0-k1st (slice 0 only) | stock K=1 | failopen | 0 div, 372,391 pairs |
| p26a-k1fo | book K=1 | failopen | **0 div**, 358,235 pairs, 1,584 IR blocks |
| p26a-k1st | stock K=1 | failopen | **0 div**, 373,347 pairs, 1,575 IR blocks |
| p26a-k1play | book K=1 | play | **0 div**, 10,192,023 pairs, 2,276 IR blocks |

Liveness (predicted in Census.md §6 before the runs): 7015C2A4 (WSGT
skip), 7015C2A6 (WMUL×2 WNADI WBTZ), 7015C2BB (MOV.L# SNC),
7015C4FB / 701670D7 (LWADD×2 + WMUL) — ALL LIVE in k1fo. Class
coverage (blocks live / blocks carrying, three legs combined, by the
IRExec first-execution lines ∩ lowered-line audit comments —
tools: /home/claude/p26cov.py, shipped as docs/Project26/p26cov.py):
**66/77 newly-lowered classes executed**, e.g. WSGT 223/2268, WSGTI
232/2027, WMUL 186/1805, LWADD 154/1528, XJMP 78/1160, LDAFP 89/1053,
MOV.L# 83/669, WSZB 74/361, XNDO 31/176, WBTO 21/140, WBTZ 19/106,
WDIV 7/87, CVWN 12/216, WXCH 3/7, CRYTO 1/1; borrow brackets: 4 of the
14 bracket blocks executed as t-places (UPDATE_SCREENS' two remain
driver-unreachable, as in P25). Census-carried (not reached by any
scripted leg; all ≤ 25 embeds, semantics in Census.md §2): ADD.# LNADI
LNSUB NNEG WIORI WSKBO XNISZ XNMUL XWADI XWSBI XWSUB.

Negative loader tests (16 malformed one-block files, each refused at
launch with the offending token; 3 valid forms load): bare `<`, bare
`/`, bare `%`, `#+`, nested effectful (two shapes), `<<`, mixed-class
chain, chained comparison, t read-before-write, t double write, `wsp =`,
unlisted goto label, `goto [L] 1`, functional `and()`, an `ir 2`
header. Runtime faults (goto index, zero divisor, non-0/1 boolean) are
code-reviewed only — no game path reaches them by construction.

Battery: task 037 (034 template, JOBS=3, ports 8831–8843; launch
coordinated with the user) RAN on the runner — **13/13 GREEN, DONE,
847 s wall clock** (results/037-p26-math-grammar/verdicts.txt). Every
leg div=0, blk_mismatch=0, gaps_over_k=0, endpoints as wanted; the
strict K=1 leg k1fo 389,610 pairs max_gap=1; play (book) 1,870,514
pairs, play-st (stock) 3,593,858 pairs. `embeds_book=8529 (bar <=
8600)`. Coverage across the battery: 66/77 newly-lowered classes live
in 2,431 executed IR blocks (WSGT 330/2268, WSGTI 314/2027, WMUL
274/1805, LWADD 234/1528, XJMP 126/1160, MOV.L# 133/669, WSZB 106/361,
XNDO 43/176, WDIV 11/87, CVWN 24/216, borrow-bracket blocks 4/14…);
the same 11 census-carried classes unreached as locally. Regression
lines unchanged from 035 (armed-pc drops 1/1/0/0; carry-consumer-site
pattern identical). Landing bar MET.

## 6. Corrections recorded (METHOD §10/§11)

- Nova load-form count 43 → 67 (arithmetic; caught by the emission).
- Nova SNC/SZC polarity in the draft prose (emission unaffected).

## 7. Tempting adjacencies NOT taken (boundary 1)

DERR folding, synclist delisting, checker changes, stack WRITES (the 19
STASP / WMSP pairs / the 3 pass-by-reference WPSH+LDASP temps at
70169B77), RT-call decoration (XPEF/LPEF 2,876 + 8 rt-call WPSH), the
Nova LOAD forms (67 — would need a manual ruling or a hoisted `nova()`
helper writing acY+c), LNDO (rendering gap), DIVX/WDIVS (two-register
divides), the XJMP CFG-edge fix, the disassembler fixes, flag-deadness
analysis (MathDesign §5), a pure arithmetic-shift primary (unneeded).

## 8. TODO / next session

1. Review + integrate p26-math-grammar (037 green, §5).
2. User's manual checks (WMOVR, WHLV, WDIV, Nova/narrow high half) →
   findings recorded here; only an emulator change would follow.
3. Nova load forms (67) once the high-half convention is settled.
4. P27 = DERR cluster compression (roadmap item 2); the remaining
   embeds are 8,461 out-of-scope by construction (DERR 2,273, RT-call
   2,887, calls 1,313, string/WMSP/stack 1,765, frames/OS/float/misc
   223) + 67 Nova loads + 1 LNDO.
