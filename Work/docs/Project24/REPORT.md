# Project 24 — the wide-carry correction. FINAL REPORT

Session: Aug 29 2026 (solo implementation; plan gate + WADC ruling by
the user mid-session). Prompt: docs/Project24/PROMPT.md. Census
deliverable: docs/Project24/CarryCensus.md. Branch: p24-wide-carry.
Battery: repo task 032 (see §7).

## 0. Where this leaves the system

`EagleInstruction::add()/sub()` now compute CARRY as the ALU carry-out
(bit 32), matching the DG manual ("CARRY set according to value of ALU
carry"), instead of the result's sign bit. Because the IR `#`-ops and
the (former) native replicas all route through the same two functions,
one fix covers emulated master, emulated clone, IR clone, and native
runtime at once. Native staged-carry residue is re-derived everywhere
it was hand-baked from the old behavior; the docs that recorded the old
values carry dated correction notes per METHOD §11. The census proves
the fix is unobservable to gameplay (no wide-produced carry is consumed
on any reachable path) — which is exactly why lockstep alone could
never have found or now adjudicate it (§6).

## 1. Part 1 — the census (plan-gated, delivered before any code)

docs/Project24/CarryCensus.md. Method: mechanical extraction of the
reader/writer sets from the emulator source (no hand lists), a
datapath-dependence model of the Nova ALC (the sole consumer class),
and a backward reaching-writer walk over the split CFG plus a
branch-aware scan of the rt listing. Headline: **zero wide-reached and
zero ambiguous consumers.** 47 game + 7 reachable rt consumers all
resolve to fix-invariant producers (ADD.O / DIVX / CRYTO / MOV.O/Z /
XNDO); the 3 remaining rt sites are dead (unreferenced ?URTB; a SWAT
trap handler behind an unimplemented Nova LEF — METHOD §3 evidence).
The user's four candidate `ADC.C x,x,SNC` sites read carry from the
adjacent `ADD.O x,x,SBN` — real readers, not wide-reached (finding F3,
a delta from the hypothesis, reported not reconciled).

Findings: **F1** XWDO/LWDO are wide carry writers omitted from
WideCarry.md's list; **F2** runtime/frames.cpp (and i_alloc.cpp)
contained verbatim replicas of the buggy formulas; **F4** ?URTB
branches on entry carry but is unreferenced; **F5** doc-correction
obligations (all discharged, §4).

## 2. The WADC ruling (user, Aug 29 2026)

"WADC 0,0 sets carry to 0. We end up with -1 in ac0, and 0 in c."
WADC is fixed with the rest — the patched `add()` produces exactly this
(carry of x + ~x = 0xFFFFFFFF has no bit 32) with no routing or special
case. WideCarry.md's conservative-routing default is superseded and
annotated. The census made this evidence-safe: no reachable consumer
sees WADC's carry, so the ruling is unobservable to gameplay either
way; p_defon.cpp and the Project2 derivation are re-derived to c=0.

## 3. Parts 2+3 — the atomic tranche (code)

- **hw/EagleInstruction.cpp**: docs/Project23/wide_carry_fix.patch
  applied cleanly. Sanity vector verified by execution (not memory,
  METHOD §10): 0xFFFFFFFF+1 → c=1; 5−3 → c=1; 3−5 → c=0; x−x → c=1;
  WADC x,x → (−1, c=0).
- **runtime/frames.cpp**: emu_add/emu_sub are now thin forwards to
  `hw::EagleInstruction::add/sub` (static, public — the same
  single-source reasoning as the P23 `#`-ops ruling), eliminating the
  replica-drift class outright. I.PROLOG's exit carries follow the
  helpers automatically (count 0 → c=0, count 1 → c=1; the pre-fix
  values were reversed).
- **runtime/i_alloc.cpp**: add_c formula corrected (its carry is
  discarded at every call site — hygiene); the alloc path's
  unlock-LCALL frame word drops its hardcoded 0x80000000 (WADC c=0);
  the freew path's c=0 keeps its value with the honest justification
  (genuine borrow).
- **runtime/lib_error.cpp**: O?SIGNAL-boundary carry 0→1 (WSUB 2,2);
  I.FREEW staging 0→1 (the e35D latch); c_x default 0→1 (no-message
  path); I.ALLOC staging stays 0 — re-verified from the listing (the
  second WINC at e396 is the last writer; its operand, a logical >>1,
  can never be 0xFFFFFFFF).
- **runtime/o_signal.cpp / o_on.cpp**: every catch-all 0 → 1 (the
  helper preamble WSUB 1,1 at ee7B, the shorthand WSUB 1,1 at ee37,
  the argc≤3 WSUB 0,0 at edf8 — each producer re-verified in the
  listing).
- **runtime/p_defon.cpp**: type==6 staging 1→0 per the WADC ruling
  (fd8D); the NLDAI path's entry-carry claim re-verified (no c-writer).
- **runtime/unsigned_to_char.cpp**: re-derived three-way — k==1/argc>2
  entry carry (unchanged); k==1/argc≤2 → 1 (the daA3 WSUB latch);
  k>1 → 0 (the daCB XNDO, narrow and fix-invariant, is the surviving
  iteration's last writer — the pre-fix "else 0" happened to be right
  for that leg for a different reason).
- **Audited, no change needed**: def_on.cpp (its one WADC path is
  fallback-gated as an unobserved shape; every entry_carry claim
  re-verified against the DEF?ON listing), i_lock, t_area, o_area,
  mv_error_handler, r_signal, fill_words, udiv32, i_alloc's freew
  value, native/check error-handler halves. Game code contains zero
  push-jump instructions, so the XPSHJ/WPOPJ carry-opacity subtlety
  (rt-internal) touches no verdict; documented in the census §4.

Build: clean, zero warnings.

## 4. Doc corrections (METHOD §11 — annotated, never silently rewritten)

METHOD §5 ("WSUB x,x clears carry" → dated correction note, lesson
retained); WideCarry.md (landing note + F1 delta + WADC ruling);
Project1/2/3 DERIVATION.md (dated correction headers with the value
mapping); Project6/L2Contract.md (normative §3 rows corrected in place
with "(P24: …)" tags — I.PROLOG exit carries, the c_h row, the joining
WSUB, the WADC/WSUB boundary pair, the "c=1 borrow" ruling text);
I_ALLOC.md "Carry chains" (WADC row + the 0xF017EA08 capture word
marked pre-fix); UNSIGNED_TO_CHAR.md (the three-way carry rule).

The Project 2/3 empirical captures in docs/captures/ show pre-fix
carry bits (e.g. F017EA08); they remain the record of the derivation
method — the battery re-proves the pairing under the fixed tree.

## 5. Local gates (METHOD §15: ~15 min total, K=1 strict, split pair)

| leg | cfg | trigger | result |
|---|---|---|---|
| fo_book | book IR | FAIL_OPEN=USER_DATA_FILE, L→P | **0 div**, 1,587 IR blocks; ?LIB_ERROR(native) → O?SIGNAL(native) ran the re-derived staged carries; I.PROLOG(native)×6 |
| m_book | book IR | M-trigger turn | **0 div**, 1,567 blocks |
| play_book | book IR | movement+menus | **0 div**, 2,258 blocks |
| stock_fo | stock IR | FAIL_OPEN | **0 div**, 1,575 blocks, ?LIB_ERROR(native) |

The LOCK_FILE consumer block 70169B56 (CRYTO-fed) executed in every
leg — a live carry consumer paired clean at K=1 under the fix.

## 6. Why the fixed behavior is RIGHT (the non-lockstep argument, METHOD §2)

Master==clone by construction even when both are wrong, so the
battery's zeros prove consistency, not correctness. Correctness rests
on: (a) the DG manual's WADD and WSUB pages — "CARRY set according to
value of ALU carry", bit 32 of the unsigned sum / of the complement-add
(user-supplied scans, P23 session; WideCarry.md); (b) the fixed
formulas' agreement with `narrow_sub`'s own (correct, >>16) convention,
i.e. one carry semantics across widths; (c) the WADC user ruling for
the one page the manual leaves ambiguous; and (d) the census, which
bounds the blast radius: no reachable consumer can observe the change,
so the correction cannot regress gameplay even in principle — the
1986-machine fidelity it buys is exactly the class lockstep can never
check.

## 7. Battery (runner box, task 032)

Shape: tasks/hold/031 hardened template (single loop, flock guard,
per-leg ports, pairs floors, pinned endpoints), extended per the P24
prompt: 11 legs = book IR × {fo, m, inj→?FATAL, abort→WORLD-ABORT,
patient play, inj3-RESUME, K=1 fo} + stock IR × {fo, play} +
all-emulated × {fo, m}, all on the split CFG + split synclist, with a
carry-consumer-site coverage report (all 28 census consumer blocks)
appended to the verdicts. Landing bar: 0 divergences everywhere,
blk_mismatch=0, gaps_over_k=0, endpoints pinned, coverage evidence
recorded. RESULT (results/032-p24-wide-carry-battery/, Aug 29):
**9 of 11 legs GREEN, 2 RED — the reds are a pre-existing P23 tooling
gap, not a carry divergence (finding F6, below).**

GREEN: fo (7,816 pairs), m (7,654), play (6,384,539), inj3 (7,487),
k1fo (K=1, 381,818), fo-st (7,697), play-st (10,732,400), fo-emu
(8,246), m-emu (7,049) — every one div=0, blk_mismatch=0,
gaps_over_k=0, endpoint pinned; ?LIB_ERROR(native) ran in seven legs
(the re-derived staged carries paired clean at K=1 and K=50, book,
stock, and all-emulated).

RED: inj (QUEST_INJECT=7016A896) and abort (QUEST_TERMINAL=7016871D),
both div=1 at the armed pc with end=clean instead of FATAL/WORLD-ABORT.

**F6 — inject/terminal arming is incompatible with IR mode at
non-block-entry pcs (P23 latent, first exposed here).** The armed-site
checks in Machine::run_steps test the arrival pc; the emulating master
passes through every instruction pc, but the IR clone arrives only at
block entries. 7016A896 is FIND_OBJECT+0x7 (mid-block in 7016A88F) and
7016871D is DIST+0x6 (mid-block in 70168717) — the master fired, the
clone sailed through the block, and the pair diverged structurally at
the armed pc (register scatter, no carry involvement in either dump).
The decisive cross-check: inj3's site 70176AA7 IS a block entry in
quest.blocks.split, and that leg is GREEN. Corroboration: task 031 ran
the same inj/abort legs GREEN on the pre-IR tree, and P23's own gates
(REPORT §9) never included inj/abort legs. Both red legs paired clean
for 6.0-7.6k pairs up to the armed pc (blk_mismatch=1 IS the
divergence pair). Per boundary 3 this is a STOP-and-report; the fix
options (emu-config those legs; or teach lower.py/IRExec to split or
exclude blocks containing armed pcs, absent=emulated) touch P23
machinery and are the user's ruling, not this session's.

Coverage line: the FIRST ADC.C cluster **executed live** — 70160E64/
65/73/74 (BEING_ATTACK) at 2 hits each (a play leg found combat) —
plus LOCK_FILE 70169B56 at 8. The KNIGHT_ATTACK cluster and the DIVX
sites did not fire in the scripted legs, as flagged in advance.

Coverage honesty, stated in advance: the four ADC.C sites live in
BEING_ATTACK/KNIGHT_ATTACK (combat) and several DIVX sites in
ATTACK/STORE paths; the scripted legs may not reach combat. Whatever
the battery records is reported as-is. The census classification
(NOVA-reached, fix-invariant) does not depend on that coverage; the
coverage demonstrates liveness, not safety. If the user wants live
combat-site pairs, a manual play session is the vehicle (Run.md rig).

## 8. Scope boundaries kept

No t-places, no `save`, no B-form extraction, no @/bit-15 work, no
grammar changes. Tempting adjacencies recorded instead: the stale
"instance methods" claim that motivated the original replicas (fixed as
part of F2's landing, in scope); ?URTB's dead entry-carry branch (F4,
census only); the bare-LEF rendering in the SWAT handler (checked
against §14 — an unimplemented-decode rendering, not a listing defect).

## 9. Deliverables ledger

CarryCensus.md ✓; fix landed + residue re-derived (atomic) ✓; doc
corrections ✓; local gates ✓; battery run (032): 9/11 GREEN, 2 RED
explained as pre-existing F6, ruling owed; REPORT_worklog.md ✓; CURRENT_STATE/NextSession updated with the
battery verdict and the owed rulings.

## 10. Rulings owed to the user (open at session end)

1. **F6 disposition** (§7): inject/terminal arming vs IR at
   non-block-entry pcs. Options: (a) run inj/abort battery legs
   all-emulated (no code; equivalent to the pre-IR 031 check);
   (b) IR-loader excludes blocks containing QUEST_INJECT/QUEST_TERMINAL
   pcs (absent=emulated — the clone then emulates through the armed
   site; small IRExec/loader change, P23 machinery); (c) both. The two
   red legs re-run green under any of these if F6's analysis is right.
2. **Consumer-site live coverage**: the battery demonstrated the
   BEING_ATTACK ADC.C cluster and LOCK_FILE live; KNIGHT_ATTACK's
   cluster and the 19 DIVX sites did not fire in scripted legs. Rule
   whether the census classification (NOVA-reached, fix-invariant)
   suffices for the landing bar, or a manual combat/store play session
   should demonstrate them as live pairs.
3. **Merge order**: p24-wide-carry carries the P23-integrated tree as
   its base (main lacks P23); integrator merges P23 then P24, or the
   branch wholesale.
