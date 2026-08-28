> **DEFERRED (Aug 28 2026, same-day).** Superseded in ORDER, not in
> content, by docs/Project22/BlockSyncDesign.md (Gen-6 block-sync checker): the
> census will be rewritten to run over the ORACLE-VERIFIED IR after
> Gen-6.1/6.2 land, gaining the M-notation, parametric by-ref
> summaries, and the producer-audit certificate discussed Aug 28.
> Do NOT run this project as written. The classification criterion
> (three-layer state-vs-temp) and sanity-gate discipline below carry
> forward unchanged.

# Project 21 — M5 opening: the access census (state vs. temp, per-routine summaries)

Hi Claude! Solo implementation session; the user reviews at the plan
gate and at the landing. This is a STATIC-ANALYSIS TOOLING project:
it reads the disassembly and existing data files and writes reports.
It does NOT touch the emulator, the checker, the address book, or any
runtime behavior. Low blast radius by construction — but the findings
will carry M5, so conservatism and fail-loud rules apply with full
force.

## Context (from the Aug 28 planning discussion)

M4 is complete: frames, args, and borrows are all off the stack; the
game program is flat. M5 (live range analysis) needs a graph without
spurious edges, and M5Notes.md sketches the cure: ?-routines become
intrinsics with summaries; high-fan-in game routines likely get the
same treatment; the rest keep matched call/return.

The call-site census (from Disassembled/quest.callsites, self-checked
754 = 566 CLEAN + 188 CLEAN-EMPTY over 101 distinct game targets):

- 47 routines: exactly 1 call site → WRTN becomes a jump, no analysis
  needed.
- 19 routines: 2 sites; 19 routines: 3–9 sites → clonable/indexable.
- 16 routines: 10+ sites (~610 of the 754 sites): OWNS 93, GET_INPUT
  77, HIT_ANY_CHAR 64, FIND_OBJECT 64, RANDOM 44, WRITE_OBJECT 42,
  UPDATE_SCREENS 36, READ_IN 33, GET_QUEST 23, DISPLAY_SCREEN 23,
  INIT_SCREEN 19, TERRAIN 13, REFRESH_SCREEN 13, DISPLAY_INVENTORY 12,
  DIED 12, DIST 11.

User's hypothesis: many of the top 16 are pure output functions
(read state, write only their own buffers, call ?WRITE_SCREEN). If
proven, they summarize away and the flat graph shrinks to actual game
logic. This project builds the proof machinery.

**The classification criterion (user-agreed, Aug 28):** "game state"
is a usage property, not an address property. Bootstrap in layers:

1. **By construction:** a routine's own M4 frame area (its book
   entry) is temporary by definition.
2. **By region:** shared multi-user pages are game state, period.
   Player records reached via [0x70000210] likewise.
3. **Static globals — classify by cross-routine access pattern:**
   - single owner routine AND written-before-read on every path →
     PRIVATE BUFFER (temp);
   - multiple owners, OR any read-before-write at entry → GAME STATE;
   - read-only everywhere → CONSTANT.
   Anything ambiguous stays GAME STATE. Precision may be lost;
   soundness may not.

NOTE: the raise census (M5Notes.md candidate direction §1) is a
SEPARATE project. Do not build it here. But keep the output schema
compatible: the summary table this project produces is the artifact
the raise census will later add its column to.

## Boundaries — BINDING

1. **Read-only project.** New files: the tool(s) under c_src/tools/,
   generated data under c_src/, report + notes under docs/Project21/.
   Nothing else changes. No emulator edits, no doc edits outside
   Project21/ (banner/index updates at landing are fine).
2. **The disassembly wins** (METHOD §1). Summaries are derived from
   quest.dis instruction by instruction. The c_src/quest/*.cpp
   reconstructions are NOT evidence — user ruling, Aug 28: they are
   untrusted M1-era behavioral guesses (dist.cpp uses std::sqrt where
   the binary goes through SQR31?3 — METHOD §1's own example).
3. **Fail-loud over guess.** Every memory access the tool cannot
   classify goes on the UNKNOWN list with pc, instruction, and reason.
   An empty UNKNOWN list is a claim; a populated one is a work item.
   Silent binning of an unresolved access is the one unforgivable bug
   in this project.
4. **Conservatism is directional.** Misclassifying a temp as state
   costs precision. Misclassifying state as temp poisons M5. All
   defaults break toward GAME STATE.
5. **Design-vs-reality: STOP AND REPORT.** If the disassembly shows
   an addressing pattern the design below cannot express (aliasing
   between frame areas and globals, writes through computed addresses
   that cross region boundaries, self-modifying anything), write it
   up in REPORT.md with evidence and candidate rulings and end the
   session. Do not improvise a fourth region kind.
6. Plan before code; go-ahead at the plan gate; loud failure over
   silent.

## Read IN ORDER

1. docs/METHOD.md — §1 (disassembly wins), §4 (XCT / hidden code —
   static reading lies), §5 (verify from emulator source).
2. docs/M5Notes.md IN FULL — the intrinsic-summary direction this
   project serves.
3. docs/Layering.md — the strata; this project's subject is L1 game
   code only.
4. Disassembled/README (if present) + the data files:
   quest.dis, quest.symbols, quest.callsites (incl. its census
   footer), quest.addrs, quest.blocks, quest.targets, quest.argmap.
   Understand each format before consuming it.
5. c_src/quest.addrbook + tools/build_address_book.py — frame-area
   ranges per routine (region 1 of the criterion).
6. docs/GAME_REFERENCE.md — known data layouts (player records,
   shared pages) to seed region 2.
7. c_src/tools/gen_pushmap.py — prior art for walking the dis and
   recognizing arg-store patterns; reuse its parsing conventions.

## The tool

`c_src/tools/access_census.py` (split into modules if it wants to be;
keep the entry point single). Three passes.

### Pass 1 — per-routine access extraction

Routine boundaries from quest.symbols (+ quest.blocks for sanity).
For each memory-touching instruction in each of the 101 game routines
(and any nested .N@ variants — they are separate rows; their
containment relationship is recorded):

- Target in own frame area (book range) → TEMP, record but segregate.
- Target a literal static address → record (addr, R|W, pc, order).
- Computed addressing: recognize the base+stride idioms —
  LWADD off [0x70000210] with the 686-word player stride is the
  canonical one; enumerate the others found rather than generalizing
  speculatively. Record as symbolic region (base, stride, field
  offset) with R|W.
- Anything else → UNKNOWN list (boundary 3).

Also per routine, from the same walk:
- ?-calls made (target, argc) — the runtime-summary hook points;
- game-calls made (feeds the call-graph tiering);
- DERR sites (they are terminal exits, not accesses — count them);
- XCT / opcode-shaped constants / WBR-2 holes: METHOD §4 says static
  reading lies. The known XCT sites are in the RT, not the game, but
  VERIFY that for the game region (scan for XCT and for the hole
  pattern) and state the result in the report either way.

### Pass 2 — cross-routine inversion

Per static address / symbolic region: owner set, reader set, writer
set. Apply the criterion:

- PRIVATE BUFFER: single owner, and write-before-read on every path
  within that owner. Path-sensitivity is approximated CONSERVATIVELY:
  straight-line dominance within the routine's block structure
  (quest.blocks); any access reachable through a loop back-edge or a
  join that merges a written and an unwritten path → NOT proven,
  stays GAME STATE, flagged AMBIGUOUS-PATH so a later pass (or a
  human) can upgrade it.
- CONSTANT: read-only everywhere.
- GAME STATE: everything else, including all region-2 addresses
  regardless of pattern.

### Pass 3 — routine summaries

One row per routine:

- reads(state): the state addresses/regions read;
- writes(state): the state addresses/regions written;
- private buffers owned;
- ?-calls made;
- game-calls made;
- returns-result: does ANY caller consume the return value —
  determined from the CALLER side (the continuation after each LCALL:
  a result-register read before redefinition = consumed; the
  RETURN_MESSAGE noreturn shape and the FIND_OBJECT post-call idiom
  in M5Notes are the worked examples). Report per-site and summarize
  per-routine (ALL/SOME/NONE sites consume).
- classification: PURE-OUTPUT (reads state, writes no state, output
  only via ?-calls) / PURE-FUNCTION (no state writes, no output,
  result consumed) / STATE-WRITER / MIXED — plus the site count from
  quest.callsites.

Machine-readable output: `c_src/quest.access` (grammar documented at
the top of the file, following the pushmap convention of
comment-annotated lines). Human report: the table in REPORT.md,
top-16 first, then the rest.

## Sanity gates (the tool checks its own work)

- Call-site cross-check: game-calls found in pass 1 == quest.callsites
  (754, per-target counts equal). Any delta is a bug in one of them —
  find which.
- Arg-slot cross-check: every frame-area write pass 1 sees at a
  decorated caller site must be an address the M4 pushmaps know.
  Disagreement = extraction bug.
- Coverage: every instruction in every game routine visited exactly
  once; byte-range accounting against quest.blocks.
- The UNKNOWN list, AMBIGUOUS-PATH list, and any XCT findings printed
  in full in the report. Zero is a result to state, not a default.

## Deliverables

1. c_src/tools/access_census.py (+ modules).
2. c_src/quest.access.
3. docs/Project21/REPORT.md: method as-built, the summary table,
   region-2 seed list used, UNKNOWN/AMBIGUOUS lists, sanity-gate
   evidence, and a findings section — which of the top 16 proved
   PURE-OUTPUT, which did not and why, and anything that surprised.
4. docs/Project21/PROMPT.md — this file, moved into place.
5. Index rows in docs/README.md; CURRENT_STATE.md entry at landing.

## What this project does NOT decide

- The raise census and fatalize ruling (M5Notes) — separate project.
- Cloning vs. matched-edge for the 2–9-site middle tier — that
  ruling wants this project's data first.
- Whether any c_src/quest reconstruction is kept — re-validation is
  downstream, against these summaries and the DERIVATIONs.
- Handler-region (O.ON/O.REVERT) mapping — the ON-handler loose end
  stays open; this census only records where those calls appear.

## Stop conditions

Boundary 5 shapes (report and end); an UNKNOWN list that grows past
roughly 50 entries with a common cause (that is a missing idiom —
report the idiom, don't hand-enumerate 50 exceptions); any evidence
of game-region self-modifying code or hidden executable holes.
