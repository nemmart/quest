# Project 14 — Phase B2, Stage 2 Report (sanity battery on the 101-live book)

**Status: STOPPED at boundary 2 — two mapper-invariant breaks on the widened
book. No Mapper patch applied. Live-play handoff NOT started.** This was run
in-container (tarball flow per CURRENT_STATE.md), not on the runner box; run
tags below stand in for runner task numbers.

Stage 1 (book regen + build + book-load smoke) was already landed on entry.
This session ran Stage 2 (m, fo, inj, abort) and hit **two independent mapper
aborts**, both introduced by the B2 widening. Per boundary 2 they are written
up with dumps and candidate rulings and handed back for a ruling; they are NOT
patched. The battery is otherwise green: with the two straggler entries removed
for diagnosis, m/inj/abort all pass with exact endpoints.

## 0. Environment / tool state (confirmed, not changed this session)

- Build: warning-free, single-core, ~59 s. Book-load smoke: **101 live, 40320
  words, 40 pages**, no layout inconsistency (loader R-C check passes).
- Tool filter (Stage 0, already applied): `build_address_book.py:220`
  `wave_one = not nocall` — drops the prior `pure and not nocall`. This is the
  documented B2 change; the book is its output. No further tool edit this session.
- Live-count delta: batch-2 = 45 live; **101 live** now (130 entries − 29
  nocall). 23 of the 101 are flagged `dyn` or `push`; the remainder of the
  widening is the never-landed pure "batch 3" + the B1 ATTACK family.
- Drivers: `docs/Project13/drive.py` (modes m, failopen, play). In-container the
  driver runs at NORMAL speed (fixed waits); `QUEST_DRIVE_SPEED` (the 10× runner
  knob) does not exist in this tree, so **every leg here is normal speed** —
  which is exactly what inj requires; m/fo are simply slower than on the runner.
- Harness: `docs/Project14/run.sh` (redirect+gcalls trace, coverage cross-check).

## 1. Battery results

| leg | book | result | endpoint |
|---|---|---|---|
| m | **101 (full)** | **RED — MAPPER I4 round-trip** (DISPLAY_SCREEN); 0 divergences up to the abort; reproduced ×2 | WORLD ABORT (mapper) |
| m | 100 (−DISPLAY_SCREEN) | **GREEN** — 0 div, anchors exact, probes 0, cross-check OK | I.STOP detach 7017FCE8 |
| fo | 100 (−DISPLAY_SCREEN) | **RED — MAPPER I2 wsl-moved** on the fail-open path; 0 divergences up to the abort | WORLD ABORT (mapper) |
| fo | 99 (−DISPLAY_SCREEN,−LIST_PLAYERS) | **STILL RED — MAPPER I2**, different record live (GET_INPUT); not isolable | WORLD ABORT (mapper) |
| inj | 99 | **GREEN** — 0 div, cross-check OK, probes 0 (play mode, normal speed) | ?FATAL detach 7017F036 |
| abort | 99 | **GREEN** — 0 div, cross-check OK, probes 0 | TERMINAL-ABORT 7016871D, both engines (banner wides 1B8A7016 AC037000) |

Anchors on the green m leg: READ_IN=4, LOGON=1, GET_INPUT=8, INIT_SCREEN=1,
REFRESH_SCREEN=1, HIT_ANY_CHAR=1 — all exact. inj reproduced P13's ?FATAL
7017F036; abort reproduced the WORLD-ABORT-both-engines banner. So **inj and
abort validate on the widened book, and m validates once DISPLAY_SCREEN is
out.** The two reds are the story.

Evidence: `docs/Project14/evidence/b2/` (per-finding summaries, the two
diagnostic books, and key stderr/redirect tails per run).

## 2. Finding A (boundary 2) — DISPLAY_SCREEN: I4 round-trip breaks when the
dynamic frame descends real_wsp below its own frame

**Routine:** DISPLAY_SCREEN, `70166AF9`, flags **`dyn,push`**, static frame
**547 wides** (0x223) — the widest dyn/push routine in the book. Newly live in
B2. Exercised by the m driver's status render (gcalls=2, redirect=2 before the
abort).

**Abort dump (full 101-book, m):**
```
MAPPER I4: round trip failed for 700010E6 (fwd): mapped=70001150 back=74003332
pc=7016705A ac0=700010E6 ac1=00000209 ac2=E8006C38 ac3=7400333C wsp=700010E6 wfp=7400333C
```
(reproduced: `... for 700010E2 (fwd): mapped=70001B80 back=74003332` — same routine,
exact values shifted by the ?GTOD wall-clock band.)

**Mechanism (arithmetic verified against the trace):** DISPLAY_SCREEN is
pushed twice in the run; between the pushes its `master_wfp` collapses from
`70001B8A` down to `7000115A` while `real_wsp` stays put (~`700010E2`):
```
WSAVS DISPLAY_SCREEN area_wfp=7400333C frame=547 real_wsp=700010E2 shadow_wsp=70001FD0 master_wfp=70001B8A
WSAVS DISPLAY_SCREEN area_wfp=7400333C frame=547 real_wsp=700010E6 shadow_wsp=700015A0 master_wfp=7000115A
```
At the crash-time record master_wfp=`7000115A`, so the frame's master extent is
`[lo=7000114E, hi=700015A2)`. The real stack pointer `700010E6` has descended
**below** `lo` (by the dynamic WMSP/push residue). The compression leg
(`map_word` ToMaster, `s + shift_after`) lifts it back **up** into the band:
`700010E6 + 0x6A = 70001150 = master_wfp − 10`, which lies inside `[lo,hi)`. The
inverse walk (ToClone) therefore resolves `70001150` to the **clone area**
(`area_wfp + (s − master_wfp) = 7400333C − 10 = 74003332`) instead of back to
`700010E6`. Round trip `700010E6 → 70001150 → 74003332` — bijectivity (I4)
broken. `divergences=0` throughout: the game ran identically on both engines;
it is the mapper's own self-check that trips.

**Why it contradicts the symmetry argument (PROMPT_B2 rationale):** the argument
grants that dyn ops are symmetric (both wsp move identically — true, hence 0 div)
and concludes "the closed-form shadow accounting still holds; the compression leg
already maps addresses above redirected frames." The unstated assumption is that
the compression image of the stack leg and the frames' master extents are
**disjoint**. For a routine whose *dynamic* descent carries real_wsp well below
its own (large, 547-wide) static frame, that disjointness fails: the compressed
image of a sub-frame address lands **inside** the frame's own master extent.
This is "the closed-form shadow accounting genuinely breaks / an escape the
Mapper can't map appears" — squarely the boundary-2 trigger.

**Isolation:** commenting DISPLAY_SCREEN greens the m leg (100 live, anchors
exact). No other dyn/push routine that the m driver exercises trips — but m
exercises only a subset of the 23 dyn/push routines; large-frame dyn/push
routines are latent suspects (see §4).

**Candidate rulings (for the planning session — not mine to pick):**
1. **Extent-band separation invariant.** Add a load/push-time assert that a
   routine's block cannot be reached by compression of any legal descended
   real_wsp — i.e. formalize the disjointness the symmetry argument assumed, and
   *exclude to M5* (or split) any routine that violates it. DISPLAY_SCREEN would
   move to M5 under this. Cheap, honest, keeps M4a's closed form intact.
2. **Bound the descent in the accounting.** Extend the shadow accounting so the
   compression leg is aware of the dynamic residue below the static frame (the
   WMSP delta), so the image never enters the band. This is a Mapper design
   change (M4c-adjacent — the in-body MSP dynamic allocs) and should not be done
   under B2's boundary-1 scope.
3. **Frame-size gate on dyn/push.** Keep dyn/push live only below a frame-size
   threshold where the descent cannot reach the band; DISPLAY_SCREEN (547) and
   peers above the threshold go to M5. A stopgap, not a real fix.

Recommendation for the ruling: (1) is the minimal, in-scope move; (2) is the
"right" fix but belongs to M4c, not B2.

## 3. Finding B (boundary 2) — fail-open signal path moves wsl while a game
record is live (I2), independent of routine

**Abort dump (fo, DISPLAY_SCREEN commented):**
```
FAIL_OPEN: QUEST1 denied :USER_DATA_FILE
MAPPER I2: wsl moved while records live (latched 7001715A, now 7001714C)
pc=7017EC7C ac0=... wsp=... wfp=74005AAA   (LIST_PLAYERS live)
```
The move is `7001715A → 7001714C` (−0xE = −14 wides) at `pc=7017EC7C`, inside the
L2 error/signal delivery region (O?SIGNAL 7017EDED, R?SIGNAL 7017EF54 sit just
above), fired immediately after the fail-open denial.

**Not per-routine (the important part).** Commenting the routine live at the
abort does **not** fix it. With LIST_PLAYERS also commented, the fo leg aborts
again with the **identical** wsl move `7001715A → 7001714C` at the **same** pc,
now with **GET_INPUT** the innermost live record:
```
MAPPER I2: wsl moved while records live (latched 7001715A, now 7001714C)
pc=7017EC7C ... (GET_INPUT live, area 74003ECC)
```
So this is **not** a book/tooling bug in one routine (the boundary-3 "comment it
and continue" remedy provably does not apply — it just relocates the abort to the
next live record). It is the **fail-open handler path moving wsl while any game
record is live across the signal.**

**Why it is newly visible in B2.** The latched-wsl invariant (I2) asserts wsl is
constant while records live. On the batch-2 book the fail-open signal fired with
`records_` effectively empty at the wsl-adjust point (the routines active across
that path were stacked, not live), so I2 never armed; fo was clean (P14 §7,
"fo 0 div clean"). The widening keeps a game record live across the same path,
so the pre-existing wsl motion now trips I2. The motion itself is a property of
the fail-open path, not of the widening — the widening only exposed it.

**Scope of the exposure.** It is specific to the **fail-open** handler: the
**inj** ?FATAL path (0x2006 at FIND_OBJECT → 7017F036) and the **abort** terminal
path (7016871D) both run to their correct endpoints on the widened book with 0
div (§1). So of the signal paths the battery drives, only fail-open moves wsl
while records are live.

**Candidate rulings:**
1. **Characterize the −14 wsl motion in the fail-open path** (what in
   O?SIGNAL/R?SIGNAL/DEFAULT_ERROR_HANDLER lowers wsl by 14 wides) and decide
   whether it is a legitimate handler stack adjustment. If legitimate, I2 is too
   strict: the latched-wsl bound must **tolerate a signal-frame wsl delta** while
   records are live (re-latch across the handler window, or bound by the
   post-adjust value), which is a Mapper/error-handler contract change — M4a/L2
   territory, needs a ruling, not a B2 patch.
2. **If the motion is spurious** (an accounting slip in the native fail-open
   handler rather than a real wsl change), fix it there (boundary-3 in the error
   handler, not the Mapper) and re-run fo.
3. Interim, do **not** rely on commenting routines to green fo — it cannot work
   (shown above).

This finding likely shares a *root theme* with Finding A — the widened live set
puts records where the closed-form M4a accounting did not previously have to
hold — but the two are mechanically distinct (I4 frame-extent collision vs I2
latched-wsl motion) and want separate rulings.

## 4. Roll-call (Stage-2 partial; play NOT run)

- **LIVE-VALIDATED this session** (redirect > 0, 0 div on a green leg): the m/inj/abort
  green legs exercised, with cross-check strict — DIST, GET_INPUT, READ_IN,
  INIT_SCREEN, REFRESH_SCREEN, HIT_ANY_CHAR, LOGON, START_TURN, SIGNAL_TURN,
  DISPLAY_INVENTORY, DISPLAY_MAGIC (dyn), DISPLAY_CAVE (dyn,push), DIED (dyn,push),
  DROP (dyn), MOVE_FAMILIAR, TERRITORY, TERRAIN, OWNS, REGEN_SPELLS, STORMS_AT_SEA,
  FAKE_LAND_MASS, FAKE_OCEAN, INIT_OBJ_TBL, GET_QUEST, QUEST.1, LOCK_FILE,
  FIND_OBJECT (inj site). (Full per-leg coverage in the run dirs.) Note several
  **dyn/push** routines here validated cleanly — DISPLAY_CAVE and DIED are
  dyn,push and did not trip — so the break is **not** "all dyn/push," it is the
  large-frame descent class (Finding A) plus the fail-open path (Finding B).
- **STRAGGLERS (boundary 2, awaiting ruling):** DISPLAY_SCREEN (dyn,push, 547) —
  Finding A; fail-open path (record-agnostic) — Finding B.
- **LIVE-UNEXERCISED (pending play):** the combat/store/bargain/cave/castle set
  (ATTACK family, BARGAIN family, CAST family, CAVE_ATTACK family, STORE, BOAT,
  CATAPULT, SEIGE, TOWER_ATTACK, KNIGHT_ATTACK, KILL_PLAYER, THIEF, LIST_PLAYERS,
  REPORT, HELP, LOOK, TAKE, MOVE, MOVE_PLAYER, OBSERVE, …). Legitimately
  unexercised — the play session is where they land, and it has not run.
- **EXCLUDED (nocall → M5):** the 29 nocall entries, unchanged.

## 5. Deliverables status vs the prompt

- Tool change + regenerated book: already landed (Stage 1); documented in §0.
- Sanity results with tags: §1, evidence in `evidence/b2/`.
- Bisected stragglers: §2 (DISPLAY_SCREEN, isolable), §3 (fail-open, NOT isolable).
- Boundary-2 findings: §2, §3, with dumps and candidate rulings.
- **Book handed back UNCHANGED at 101 live** (Stage-1 state). I did not comment
  the stragglers into the authoritative book: DISPLAY_SCREEN is a boundary-2
  finding (stop-and-report, not comment-and-continue), and the fail-open I2 is
  not fixable by commenting. The two diagnostic books (`−DISPLAY_SCREEN`,
  `−DISPLAY_SCREEN,−LIST_PLAYERS`) are in `evidence/b2/` for the planning session.
- **DEFERRED pending ruling** (would misrepresent state if written now):
  - CheckerHistory.md Gen-4 append — the queued line is "dyn/push are symmetric,
    closed-form holds." Stage 2 **contradicts** that as written (Finding A). The
    Gen-4 append should be written *after* the ruling, and should carry the
    corrected result (symmetric ≠ bijective-under-large-dynamic-descent) alongside
    the still-owed B1 stride-masking near-miss sentence.
  - Play-handoff doc — Stage 3 is not reachable until the book runs the battery
    clean; not written.

## 6. Stop

Per boundary 2: two mapper invariants (I4, I2) break on the widened book; both
are machinery/design, not tooling; Mapper untouched; handed back for a ruling.
The planning session should rule on Findings A and B (§2, §3 candidate rulings),
after which Stage 2 re-runs clean and the play handoff (Stage 3) proceeds.
