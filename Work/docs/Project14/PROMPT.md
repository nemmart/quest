# Project 14 — the Mapper refactor (spec-first), then batch 3

Hi Claude! Solo implementation session; user reviews at the plan gate,
after the refactor lands, and at the landing. Two phases, strictly
ordered: **Phase A** rebuilds the mapping layer to docs/Mapper.md
(design of record for mapping); **Phase B** resumes the widening
(batch 3) on top of it. Phase B does not start until Phase A's
regression is green and reported.

## Boundaries — BINDING (Project 12/13 rules, unchanged)

1. **Phase A migrates NOTHING.** Same books, same live sets; the
   refactor is behavior-preserving by definition and proven by pure
   regression. Any behavior delta in Phase A is a bug in the refactor
   (boundary 3) or a finding about the old code (report it either way).
2. **Design-vs-reality: STOP AND REPORT.** Now with two designs of
   record: docs/Mapper.md for the mapping layer, docs/M4aDesign.md
   (+§8/§9) for everything else. A finding that either is wrong or
   incomplete — an unlisted pointer form (the §1.2 probe firing), an
   invariant I1–I5 that cannot be established, a fifth mutation site,
   a redirect on the listener task — is written up with evidence and
   candidate rulings; the session ENDS. Edit neither design doc.
3. Implementation bugs: fix and record (METHOD §11).
4. Batch discipline for Phase B as in Project 13: full battery green
   or bisect by the book; red batches stop, they don't shrink-to-fit.
5. Plan gate before code; go-ahead required again between phases.

## Read IN ORDER

1. docs/METHOD.md
2. **docs/Mapper.md — the spec you are implementing. Every section.**
3. docs/M4aDesign.md incl. §8/§9 (context; the history the spec
   supersedes for mapping)
4. docs/Project13/REPORT.md §6 (what the accreted code does today and
   why; §6.3/§6.5 signatures = named regression targets)
5. The accretion you will delete: hw/Machine.cpp T/T_any/T_inv +
   LiveArea/areas/shadow_wsp, os/OSContext.cpp clone_word_address,
   os/LockstepMediator.cpp replay guard, hw/EagleStack.cpp redirect
   sites, runtime/frames.cpp wrtn/area_unwind_to
6. docs/Project13/{run.sh,coverage.py,drive.py}, docs/Run.md,
   docs/NextSession.md gotchas

## Phase A — the refactor

**Stage A0 (plan gate).** Module layout (hw/Mapper.{hpp,cpp}, an
instance owned by each Machine, CONFIGURED by launch code — see
Mapper.md §3 "Placement and configuration": no file I/O, no env
reads, no os/-layer reaches inside the Mapper; show the
constructor/configure() shape), the V signature and its call sites (enumerate every current
T/T_any/T_inv consumer you will rewire, and classify each as an
equivalent() site or a clone_location() site), the assert plan for I1–I5,
the probe design, the regression matrix. Wait for go-ahead.

**Stage A1.** Implement `hw/Mapper.{hpp,cpp}` per spec §1–§3:
- A as the record list + derived stack leg, closed intervals,
  direction-flagged walk (NO separate inverse implementation)
- E as the total codec table + the unlisted-form probe
- The two-call public surface (Mapper.md §1.3): equivalent()
  (verdict RAW/MAPPED/MISMATCH + decodings for dumps) for ALL
  comparisons; clone_location() for ALL dereferences into clone
  memory. No direction-flagged public V; no comparison policy
  outside the module
- Asserts: I1 at book load; I2 wsl-constancy while records live;
  I4 round-trip on every non-identity mapping (checked builds);
  I5 LIFO at each mutation; I6 (clone_location of a verified pointer
  field == the clone's own field value) at mediation sites where
  both are in hand; main-task assert at redirect
- Move the record list and shadow accounting into the module;
  Machine keeps a Mapper member and forwards.

**Stage A2.** Rewire every consumer to V; DELETE T/T_any/T_inv and the
inline guards. Fix the stale Machine.cpp comment (spec §4). Grep-proof:
no caller left behind (`grep -rn "T_any\|T_inv\|clone_word_address"`
returns only the module/history).

**Stage A3 — pure regression (the phase gate).**
- Batch-1 book: full standing battery (m, fo, play, inj, abort),
  0 div, coverage identical to P13 §6.6's b1 re-proof.
- Batch-2 book (45 live): full battery, 0 div, coverage identical to
  P13 §6.6.
- The three named signatures (P13 §3 LOGON + GET_INPUT, §6.3
  one-past-end, §6.5 @-flag inverse) re-verified as pairing.
- Probe count: expected 0 unlisted forms; a fire is boundary 2.
REPORT Phase A (as-built table, deletions, assert inventory, regression
evidence) and WAIT for go-ahead.

## Phase B — REVISED (Aug 16 ruling): one nested family carefully, then flip the rest

The batch discipline earned its keep while the mechanism was unproven;
after 14 green runs it is proven. Phase B is now two stages:

**Stage B1 — the ONE unexecuted mechanism, carefully.** Nested
procedures have never RUN redirected (statically verified across all
63 XCALL sites; never executed). Migrate ONE nested family — parent +
its callable (non-nocall) XCALL children together. Nominate the family
at the gate based on what the current play driver can actually reach
(BARGAIN + .1/.2 at the store, or ATTACK's family via combat — say
which and why). Evidence required, with log excerpts in the report:
(a) parent→child static link = the parent's AREA wfp passed via
WMOV 3,1 and consumed; (b) sibling→sibling link reloaded from the
parent's saved-ac1 slot IN THE AREA (`XWLDA 1,[ac3+0x7FFA]`);
(c) child uplevel access `[ac2+d]` dereferencing into the parent's
area; (d) one inj with the family live so an unwind crosses a nested
pair. Full standing battery on this book (banded matrix). STOP and
report before B2.

**Stage B2 — flip the rest.** All remaining wave-one routines live in
one book regeneration. One `m` sanity run (login must survive, 0 div,
anchors), then inj + abort only — no fo/play scripting, no driver
growth. The user's LIVE PLAY session is the coverage run: hand off
with redirect+gcalls tracing on and QUEST_CAPTURE armed. If play
diverges: the dump names the routine; comment it in the book, continue
play; the named routine becomes a finding in the report. Roll-call
written from the traces (LIVE-VALIDATED / LIVE-UNEXERCISED /
EXCLUDED); UNEXERCISED is a legitimate landing state (P13 ruling).

Landing: final book; roll-call; CheckerHistory.md Gen-4 append
(include the stride-masking near-miss sentence from Phase A §4).
RETURN_MESSAGE fatal path: UNEXERCISED unless a fatal occurs naturally.

**Battery runs MAY be parallelized** — the five runs are independent;
separate scratch QUEST copies, concurrent launches (a run.sh change).
Serial habit was caution, not a requirement.

## Deliverables

- hw/Mapper.{hpp,cpp} (+ the deletions), warning-free build
- Regenerated books only if the tool changes (not expected; stride
  rule is already landed)
- docs/Project14/REPORT.md: Phase A as-built + regression, Phase B
  batches, roll-call delta, corrections, findings
- docs/Project14/evidence/: battery logs per phase/batch, probe
  counts, the three signature re-verifications
- Design docs (Mapper.md, M4aDesign.md): DO NOT EDIT

## Environment reminders

~49 s/turn; scratch-copy QUEST/ per run (COPY, not symlink — see P13
§6.2's recorded mishap); `stdbuf -o0 -e0`; QUEST_CAPTURE always with
QUEST_CAPTURE_DEST; login CL / Claude / quest / Y / any / F; startup
"Segment fault - block 0, page 1" + RESERVED ACCESS = noise; M-trigger
= M + direction + `abc` at "For how many turns?"; the fo driver ends
at socket close, not I.STOP (known).
