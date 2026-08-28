# Project 22 — Gen-6.0: the block-sync checker (re-sync only)

Hi Claude! Solo implementation session; the user reviews at the plan
gate and at the landing. Design of record: **docs/Project22/BlockSyncDesign.md**
(+ docs/Project22/IRDesign.md for where this is headed — read it for context,
implement NONE of it). This project replaces the sync MODEL only. No
IR, no translation, no lowering. Both sides keep emulating original
instructions; at the end of this project the harness behaves
identically to today except that rendezvous are denominated in basic
blocks instead of instruction counts. That is the whole project.
Small on purpose — P23 (simple 1:1 lowering) and P24 (temps +
folding) build on this and have their own sessions.

## The change

Replace the 500-insn heartbeat with the K-block rendezvous
(BlockSyncDesign "The sync model"):

- Master and clone each count GAME block entries against the SYNC
  LIST — an input artifact (BlockSyncDesign "The sync list is a
  translation artifact"). This project builds the list loader (with
  its validation: entries must be quest.blocks starts, gate addresses
  mandatory) and ships the IDENTITY list — every quest.blocks entry.
  No removal is exercised in 6.0; the machinery just exists. Every K
  entries: full-state rendezvous —
  pc/next-block, block ordinal, wide ACs, carry, FP state per ruling
  Q3, exception text.
- All existing gates RETAINED UNCHANGED: syscall gate, L1↔L2
  crossings pairing both directions, L3 door, mirrored shared pages
  with compare-on-read + periodic audit, terminal machinery. Gate
  events force a rendezvous regardless of K (blocks end at
  traps/calls/crossings by construction — verify, don't assume).
- The 500-insn batch mechanism is REPLACED, not paralleled. One sync
  model, permanently (the user's standing ruling from the crossings
  landing).

## Rulings to collect at the plan gate (proposals in BlockSyncDesign)

- Q1: K default (proposed 50); K counts game blocks only, RT crossed
  at gate events (proposed yes).
- Q3: FP ACs + float status in the rendezvous surface always
  (proposed yes — float-shadow history).
- Q5 is NOT this project's problem (accounting for translated blocks
  — no translated blocks exist yet). Note it stays open.

## Boundaries — BINDING

1. **Scope = the sync model swap, nothing else.** No IR data
   structures, no per-block dispatch machinery beyond entry counting,
   no mapper/book/pushmap changes, no translation of anything. If
   6.0 seems to "want" a piece of 6.1, it goes in the report, not the
   code.
2. **Q4 is a PRECONDITION**: quest.blocks (source of the identity
   sync list) was cut on the pre-M4 program. M4 redirects change no pc flow, so boundaries should be
   identical — regenerate the blocks file with the tool and diff. Any
   difference: STOP AND REPORT (that is a finding about M4, not a
   nuisance).
3. **Design-vs-reality: STOP AND REPORT** (standing rule). A gate
   event that does NOT land on a block boundary; a block-entry count
   the master and clone cannot agree on by construction; anything
   that makes BlockSyncDesign.md wrong — write it up with evidence
   and candidate rulings, end the session. Do not edit the design
   doc.
4. **Implementation bugs: fix and record** (METHOD §11).
5. **The recalibration gate is the landing bar**: the FULL regression
   battery (all legs including inject, abort, fail-open) at 0
   divergences, plus a play session. Master == clone by construction
   in 6.0 — any divergence is a checker implementation bug by
   definition; find it, don't tune around it.
6. Plan before code; go-ahead at the plan gate; loud failure over
   silent.

## Read IN ORDER

1. docs/METHOD.md.
2. docs/Project22/BlockSyncDesign.md IN FULL; docs/Project22/IRDesign.md for context.
3. docs/CheckerHistory.md — every generation, especially what the
   crossings landing looked like (the recalibration-gate precedent)
   and the Gen-4/5 addenda (what the current checker compares).
4. docs/CrossingsChecker.md + docs/Run.md — the live checker and how
   the battery runs.
5. The harness code: hw/Lockstep.cpp, os/LockstepMediator.cpp,
   hw/Machine.cpp run_steps (the batch/heartbeat mechanics being
   replaced), and wherever the 500-insn constant lives — know the
   full blast radius before touching it.
6. Disassembled/quest.blocks format (+ the Tools generator for Q4).

## Suggested stages

- **Stage 0**: Q4 regenerate + diff (precondition). Build the
  block-entry set loader; instrument entry counting on both sides
  with the OLD sync model still live; verify master and clone counts
  agree at every existing rendezvous across a battery leg. Evidence
  before surgery.
- **Stage 1**: the swap. Rendezvous every K entries; gates force
  rendezvous; remove the instruction-count heartbeat. K=1 smoke run.
- **Stage 2**: recalibration gate — full battery at ruled K, plus
  K=1 on one leg (both must be 0 div), plus play session. Battery
  task numbers continue from 028.
- **Stretch (optional, report-only)**: the Q2 flag scan over
  quest.blocks — blocks reading carry/skip state before writing it.
  Pure static analysis, informs P23's surface ruling. Skip without
  guilt if the session is long.

## Deliverables

1. The Gen-6.0 checker, landed, battery green at the recalibration
   gate.
2. docs/Project22/REPORT.md — as-built, Q4 evidence, the
   count-agreement evidence from Stage 0, battery tables, anything
   that surprised.
3. CheckerHistory.md Generation 6 section (append-only, per the
   file's own rules).
4. Run.md lockstep section updated (K flag, K=1 debug mode).
5. Index rows in docs/README.md; CURRENT_STATE.md entry;
   NextSession.md pointer to P23.

## Stop conditions

Boundary 2 or 3 shapes; a battery leg that stays red after honest
bisection; session length — landing Stage 1 with a partial Stage 2
is NOT a landing (the gate is all-or-nothing; report where it stands
and hand off cleanly).
