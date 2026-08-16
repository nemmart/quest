# Project 13 — M4a widening: the pure list off the stack

> **CONTINUATION NOTE (Aug 15, after the batch-2 stop).** Read
> docs/Project13/REPORT.md §3 and M4aDesign.md §9 FIRST. The user
> RULED: base → 0x74000000; T_any → prefix dispatch (no ordered
> guessing); the two saved b2 divergence signatures are regression
> targets; rename hijack→redirect while the tool/book/trace regenerate.
> Batch 1 is landed. Resume at "Stage 0b" below, then batch 2.
>
> **Stage 0b (new):** (1) change the base constant in the tool and
> regenerate the book (batch-1-live); (2) AddressBook/T/T_any/T_inv:
> prefix dispatch — 0x74 word, 0xE8 byte, 0xF4 @-word → area record;
> 0x70/0xE0/0xF0 → real-stack shift; anything else identity; (3)
> rename `hijack`→`redirect` (trace type, `area_hijack_enabled`,
> log text, coverage.py, run.sh, Run.md row); (4) re-run batch 1's
> full battery — must be 0 div and coverage-identical to the landed
> batch 1; (5) run the batch-2 book: the two saved signatures must
> pair. Then continue at Batch 2 with the original stop-rules.

Hi Claude! Solo implementation session; the user reviews at the plan
gate, after batch 1, and at the landing. Design of record:
**docs/M4aDesign.md incl. §8 amendments** (Project 12's rulings folded
in). Predecessor: docs/Project12/{PROMPT,REPORT}.md — READ_IN migrated,
0 divergences, machinery built. This project migrates the REST of the
wave-one list, in batches, under the oracle.

## Boundaries — BINDING

1. **Scope = the pure list minus `nocall`, nothing else.** No dyn/push
   routines, no `nocall` entries (boot, task entry, ON-unit bodies —
   M4aDesign §8), no book-format or base changes, no new hijack
   points. If a pure routine turns out to need something the design
   lacks → boundary 2, not a workaround.
2. **Design-vs-reality: STOP AND REPORT** (Project 12 rule, unchanged):
   a finding that means M4aDesign.md is wrong/incomplete — a new
   STRUCTURAL consumer of wfp/wsp, an L2 assumption, an escape T()
   cannot map, a closed-form shadow_wsp failure (§8 marker) — is
   written up in REPORT.md with evidence and candidate rulings, and
   the session ENDS. Do not edit M4aDesign.md.
3. **Implementation bugs: fix and record** (METHOD §11).
4. **Batches are the unit of progress.** A batch lands only when the
   FULL regression battery is green with the batch's routines live.
   A red batch is bisected by commenting the book (halve, re-run,
   repeat) until the guilty routine names itself; then boundary 2 or
   3 applies to THAT routine, and the rest of the batch may land.
5. Plan before code; go-ahead at the plan gate and after batch 1;
   loud failure over silent.

## Read IN ORDER

1. docs/METHOD.md.
2. docs/M4aDesign.md IN FULL, §8 last.
3. docs/Project12/REPORT.md — as-built tables §1–§2, audits §3–§4,
   the evidence runs §5, §6 (mediation corrections), §9 (not done).
4. hw/AddressBook.*, hw/Machine.cpp T/T_any/T_inv/shadow_wsp/
   area_wrtn_fixup/area_unwind_to, hw/EagleStack.cpp WSAVS/WRTN,
   hw/Lockstep.cpp compare_pair, os/LockstepMediator.cpp,
   os/OSContext.cpp verify paths, runtime/frames.cpp — know what you
   are exercising.
5. tools/build_address_book.py, c_src/quest.addrbook,
   docs/Project12/addrbook_report.md, docs/Project12/{run.sh,drive.py}
   (the drivers you inherit).
6. docs/Run.md, docs/NextSession.md gotchas.

## Stages

**Stage 0 — plan gate.** (a) Tool update: add the `nocall` flag
(no LCALL/XCALL static caller) and exclude it from the wave-one
filter; regenerate the book with READ_IN live and everything else
commented; confirm 0 diff in behavior (empty-battery run). (b) Present
the batch plan below with the exact routine names per batch from the
regenerated pure list. Wait for go-ahead.

**Standing regression battery** (every batch, all at 0 divergences,
shadow_wsp check armed, `hijack` trace on):
- `m`: login + M-trigger (M, dir, `abc` → CONVERSION chain) + ESC →
  I.STOP detach + write-back
- `fo`: QUEST_FAIL_OPEN=USER_DATA_FILE + L→P handled signal
- `inj`: QUEST_INJECT inside a LIVE migrated frame — pick a site in
  the batch (a hot routine's body pc) so the unwind crosses THAT batch's
  area frames; the READ_IN site from P12 stays as a second inject run
- `abort`: the :ABORT test terminal (QUEST_TERMINAL=<pc>:ABORT)
- `play`: a longer scripted free-play driver (create character, O/D
  screens, store visit, several turns, ESC) — build it in Stage 0 from
  P12's drive.py; it is the routine-coverage run
- Standing check: the `hijack` line count per run is recorded in the
  report and must be > 0 for every routine claimed live in that run's
  coverage (a routine that never fires in the battery is NOT
  validated — say so per routine).

**Batch 1 — the hot leaves + slot-patchers: OWNS, FIND_OBJECT, RANDOM,
DIST, DISTANCE_TO_PLAYER.** Slot-patch return values through the area
image are the first-class target here (M4aDesign §4). Full battery.
REPORT the batch and wait for go-ahead.

**Batch 2 — the remaining named pure routines** that are NOT parents of
nested procedures (leaf-level named routines: e.g. GET_INPUT,
INIT_SCREEN, HIT_ANY_CHAR, RETURN_MESSAGE, REFRESH_SCREEN, REPOSITION,
GET_OBJECT_INDEX, TERRAIN, TERRITORY, TAKE, THIEF, SPYGLASS, PICK_X_Y,
PLACE_PLAYER, MOVE, LOOK, ...). REFRESH_SCREEN (argc 0/1) and
RETURN_MESSAGE (3/6) are the mixed-arity cases — watch the args-mirror
residue; report what you see. Full battery.

**Batch 3 — parents of nested procedures and the nested (XCALL-reached)
pure routines** (static links passed in ac1/ac2 — M4aDesign §2), e.g.
ATTACK + ATTACK.1/.2/.3/.5, BARGAIN + .1/.2, KILL_PLAYER (+ its callable
children), OP_EDIT + callable children, CAVE_ATTACK + children, ...
`nocall` children stay out. Migrate a parent and its callable children
in the SAME batch so both link directions (parent→child WMOV 3,1;
sibling XWLDA 1,[ac3+0x7FFA]) are exercised together. Full battery.

**Batch 4 — stragglers** from the pure list not yet covered (STORE,
HELP, OP_HELP, LIST_PLAYERS, CREATE_MAP, ALCHEMIST_HOME, ...), i.e.
large-frame routines and menu screens; the `play` driver must reach
them or they are reported UNVALIDATED and left commented.

**Landing.** Book = the whole validated set live; battery green; the
report names every routine as LIVE-VALIDATED / LIVE-UNEXERCISED
(commented) / EXCLUDED (dyn, push, nocall) with why. Then STOP. The
user runs a live play session on the final build (the M3-completion
shape) before M4a-wave-one is called done.

## Deliverables

- tools/build_address_book.py (nocall flag), c_src/quest.addrbook
  (final), docs/Project13/addrbook_report.md
- docs/Project13/{run.sh,drive*.py,evidence/} — every battery run's
  divergence count + hijack line count + detach/abort line
- docs/Project13/REPORT.md: per-batch tables, bisections (if any),
  mixed-arity observations, corrections, the LIVE/UNEXERCISED/EXCLUDED
  roll-call, boundary-2 findings if any
- docs/CheckerHistory.md: Generation 4 entry extended with the
  widening evidence (append-only)
- M4aDesign.md: DO NOT EDIT.

## Environment reminders

~49 s/turn; scratch-copy QUEST/ per run; `stdbuf -o0 -e0`;
QUEST_CAPTURE always with QUEST_CAPTURE_DEST; login CL / Claude /
quest / Y / any / F; startup "Segment fault - block 0, page 1" and
RESERVED ACCESS lines are noise; the L→P FAIL_OPEN driver ends at
socket close, not I.STOP (known); use the M driver for detach checks.
