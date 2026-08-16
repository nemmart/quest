# Project 12 — M4a, first migration: one routine off the stack

Hi Claude! Solo implementation session; the user reviews at the plan
gate and at the landing. Design of record: **docs/M4aDesign.md**
(planning session, Aug 15 2026). Read it in full before anything else;
this prompt does not restate it.

## Boundaries — BINDING

1. **ONE routine. Then STOP.** This project ends when a single
   migrated game routine runs under lockstep at 0 divergences through
   the agreed regression. Do NOT broaden to a second routine, do NOT
   start the pure-list campaign, even if it is "obviously" going to
   work. Widening is Project 13 and is planned with the user.
2. **Design-vs-reality: STOP AND REPORT.** If a bug or finding means
   the DESIGN in M4aDesign.md is wrong or incomplete — an instruction
   that consumes wfp/wsp structurally that the design did not
   anticipate, an L2 assumption that wfp and wsp share a stack, an
   escape the translator T() cannot map, a layout that cannot be
   byte-identical — do not patch around it. Stop, write it up in
   REPORT.md (what, where, evidence, candidate rulings), and end the
   session. The user rules; the design gets amended; the next session
   continues.
3. **Implementation bugs: fix them.** Off-by-ones, missed hook
   points, wrong offsets, a compare rule coded wrong — fix, record in
   REPORT.md as a correction (METHOD §11), continue. The test: "would
   the design as written still be right if I fix this?" Yes → fix.
   No → boundary 2.
4. Plan before code; explicit user go-ahead at the plan gate; every
   stage regression-clean before the next; loud failure over silent
   (METHOD §8). No checker changes outside the Generation-4 additions
   the design specifies. No edits to game code, the book format, or
   the address base.

## Read IN ORDER

1. docs/METHOD.md — binding.
2. docs/M4aDesign.md — the whole thing. §4 (hijack) and §5 (checker
   Gen 4) are what you build; §6 open items are yours to hit.
3. docs/CheckerHistory.md + docs/CrossingsChecker.md — what you are
   extending; you add the Generation 4 entry.
4. hw/EagleStack.cpp WSAVS/WSAVR/WRTN/WPOPB, hw/Machine.cpp
   wide_push/wide_pop, hw/RTBridge.{hpp,cpp} (frame-word/argc reads,
   the layout ground truth), hw/Lockstep.cpp compare_pair,
   os/LockstepMediator.cpp (packet compare).
5. runtime/frames.cpp, o_on, i_goto, i_epilog, i_prolog — for the
   L2 audit (stage 2 below).
6. docs/Run.md, docs/NextSession.md gotchas (container cadence,
   drivers, capture env). Setup: Work/, QUEST/, Disassembled/ (was
   Tools/).

## Stages (each gated)

**Stage 0 — plan gate.** Restate the design in your own words, list
the hook points you intend to touch (file:function), name the first
routine and the regression, and wait for go-ahead.

**Stage 1 — the tool.** `tools/build_address_book.py` per M4aDesign
§2–§3 over Disassembled/quest.dis + quest.symbols → `quest.addrbook`
(wave-one filter: pure only — no WMSP/STASP/LDASP/WPSH/WPOP; others
present, commented, flagged) + `addrbook_report.md` (per-routine call
sites, max argc, mixed-arity flags, slot-patch flag, dyn/push flags,
nested parent). Verify against the facts recorded in §2 (74 named,
49 nested, 63 XCALL sites all conforming, 18 slot-patchers, 2 WSAVR).
Show the pure list to the user before Stage 3.

**Stage 2 — two audits, BEFORE hijacking anything.**
(a) Every reader of `machine.wfp` / wfp-as-address in hw/ and
runtime/ (EagleStack, EagleIntegration, RTBridge, CallStack, the
native L2): classify VALUE (fine) vs STRUCTURAL (assumes wfp is on
the stack, or derives wsp from wfp or vice versa). WRTN is the known
STRUCTURAL case (design §4). Any other STRUCTURAL case not covered by
the design → boundary 2.
(b) The native L2: does I.PROLOG/O.ON/I.GOTO/I.EPILOG anywhere assume
the establisher's wfp and wsp live on the same stack, or compute one
from the other? Record the answer with line cites. Design the unwind
hook (shadow wsp reset + live-table drop) here.
Also: the page-map census ride-along (§1) — bitmap + trace + dump,
one battery run; attach the dump. Expected empty above 0x78000000.

**Stage 3 — emulator.** Book loader (`QUEST_ADDRESS_BOOK`), launch-
time mapping of exactly the book's pages (RW, clone only), hijacked
WSAVS/WSAVR, WRTN wsp fixup gated on the area range test, live flag
tripwire, shadow master-wsp simulation, live allocation table, T(),
register rule in compare_pair, T() at the mediator and the L2 door.
Build warning-free. Book empty → byte-identical behavior to today
(regression: M-trigger + FAIL_OPEN at 0 div with an EMPTY book, before
any routine goes in).

**Stage 4 — the one routine.** Default READ_IN (frame 6, argc 0, 33
sites, has an ON-unit → exercises I.PROLOG/O.ON/unwind with an area
wfp on day one). If Stage 2(b) makes READ_IN a boundary-2 case,
INIT_SCREEN (frame 5, argc 1, 19 sites) is the fallback — say which
and why at the plan gate. Regression: login + ESC (I.STOP detach,
write-back), the M-trigger CONVERSION chain, FAIL_OPEN, all at 0
divergences with the shadow-wsp check armed and the routine's frames
provably in the area (log every hijacked WSAVS/WRTN with routine,
area base, shadow wsp, real wsp). Then STOP.

## Deliverables

- tools/build_address_book.py, quest.addrbook, docs/Project12/
  addrbook_report.md
- The emulator changes (list them file:function in the report)
- docs/Project12/REPORT.md: as-built decision table for the hijack
  and Gen-4 checker, the Stage-2 audit tables (wfp readers; L2
  assumptions) with verdicts, the census dump, the regression
  evidence, corrections, and — if boundary 2 fired — the design
  finding written for the user's ruling
- docs/CheckerHistory.md Generation 4 entry (append-only)
- docs/M4aDesign.md: DO NOT EDIT. Findings go in REPORT.md.

## Environment reminders (they cost hours once)

~49 s/turn cadence; scratch-copy QUEST/ per run; `stdbuf -o0 -e0`;
QUEST_CAPTURE always with QUEST_CAPTURE_DEST; login CL / Claude /
quest / Y / any / F; the startup "Segment fault - block 0, page 1"
and RESERVED ACCESS lines are noise. Cheap signal trigger: `M` +
direction + `abc` at "For how many turns?".
