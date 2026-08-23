# Where things stand

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
