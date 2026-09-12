# Next session

## ★ RE-ENTRY BRIEF — written Sep 12 2026 (design session)

**Read `docs/Project44/DESIGN.md` first. It is the design of record.**

**Projects 35–43 are VOID and have been moved to `docs/attic/`, along with
`CODEGEN_RULES.md` and `translate.py`. Do not read them. Do not cite them.
Do not carry a rule forward from them.** `docs/attic/README.md` explains why
and lists what a future salvage project should mine. Anything in a live doc
that still references P35–P43 is stale and should be treated as such.

The previous re-entry brief is `docs/attic/NextSession.pre-P44.md`. It is
829 lines and most of it is compiler-line archaeology. Do not read it unless
you are the salvage session.

---

## Where the project stands

**What works and is trusted:**

- the C++ emulator and the lockstep harness (master + clone, one server,
  block-sync checker Gen 6.x)
- `docs/attic/REWRITE_PLAN.md` — superseded by absorption, not void. It
  reached P44's core independently on Sep 9; read it only if you want the
  second derivation
- `quest.ir2.book` / `quest.ir2.stock` — **ir 6**, ~80k lines covering
  essentially the whole game. The clone *executes* this and passes lockstep,
  so the IR is verified equivalent to the binary. This is the single most
  valuable artifact the project owns.
- the disassembly artifacts (`quest.symbols`, `.addrs`, `.blocks`,
  `.argmap`, `.dis`, `quest.addrbook`, `quest.arena`)
- M4a: locals and args at 0x74000000, 102 of 130 addrbook entries live;
  entry interception via the WSAVS hijack
- `compiler/`: `ircmp.py`, `readable.py`, `gen_declarations.py`,
  `crossings.py` — all live

**What is void:** the fitted model of the DG PL/I code generator and the
translator built against it. Four routines of ~130 matched in nine
projects; the last three closed zero.

**What replaces it:** P44. A choice-free compiler whose soundness is
established against gcc, plus a separate transformer driven by a supplied
oracle; and, crucially, a **behavioural acceptance level (L1)** that makes a
routine deliverable without any codegen model at all. See the design.

---

## The standing rule

> A match failure is NEVER a reason to edit the compiler.

If closing a diff appears to require a compiler change, that is a soundness
bug or an unimplemented construct — a **finding**, investigated on its own
terms. The compiler is tested against gcc, never against the book. This is
the discipline P35–P43 lacked, and it is the reason the line did not
converge.

---

## Next work, in order

Prompts are written per project; ask for the one you are running.

1. **P45 — attic salvage: DONE, MERGED.** `docs/Salvage.md` is the
   sanctioned extract from the attic — 22 facts, 7 measurements, 14
   phenomena, with a per-item confidence and live corroboration. **Read it
   instead of the attic.** Headline: a 15% wrong-citation rate among checked
   PCs, all from two patterns (PCs computed from assumed instruction
   lengths, and one transcribed constant). FIRE's frame slots re-verified —
   all DERIVED, P41's readings correct, its FIRE.3 call site wrong.
   R41 corrected post-merge: the base-register class is an ISA fact (F23),
   only the preference order is void.
2. **P46 — ir 7: DONE, MERGED.** `ir 7` is live: `v` declarations placed at
   0x76, symbolic `<ENTRY>.b<digits>` blocks at 0x77, the `s@` rename done by
   regeneration (236 tokens, token-only proven), width tripwire and refusals,
   `tests/run_vform_selftest.sh` (84 cases + teeth). A naive v-form program
   **loads, places and executes**. Strict surface untouched: the 16-leg
   battery runs 0 divergences on the renamed book.
3. **P47 — the call graph: DONE, MERGED.** `compiler/callgraph.py`,
   `CallGraph.md`, `Coverage.md`, `Order.md`. **758/758 call edges resolved,
   zero indirect calls in the game range, acyclic, 80 nodes, 20 leaves,
   QUEST at height 8.** And the number that changes scheduling: **the battery
   executes 15.5% of the book** (8,305 / 53,588 statements), 41 of 80 nodes.

4. **P48 — the naive compiler: DONE, MERGED.** `compiler/lower_c.py` (C →
   ir 7, choice-free, pycparser front end) + `compiler/difftest/`. **818
   programs, 0 disagreements, 52 constructs with no census zeroes**;
   UPDATE_SCREENS compiles, loads and runs end to end (65 `v`s, 16 symbolic
   blocks, 227 statements, zero `t`-places, zero effectful operators). Two
   bugs found and reported — an `ABS` result-type error surfacing three
   operators away as `/u` for `/s`, and a generator that could emit an
   infinite loop. Subset gaps to know about: **no user typedefs, no general
   structs, no calls, no strings, no floats, no bits.**
   *(superseded entry below)* **P48 (original) — the naive compiler** (`docs/Project48/PROMPT.md`). C → naive IR,
   choice-free, plus **its own differential tester against gcc**. The
   tester was DESIGN §13's separate project 4; it is folded in here, because
   obligation (a) is the compiler's deliverable and evidence that arrives as
   a follow-on arrives after the compiler has already been trusted. Target
   is the call-free subset — ir 7 refuses `call`/`rt_call` in symbolic
   blocks (P46 F7) — so UPDATE_SCREENS end to end, per Order.md.
4b. **THE PLAY DRIVER + LOGIN FIXTURES — now ahead of routine work.** P47
   measures the driver fix at **+7,474 statements** for one key sequence and
   two login fixtures (a killed-off record; an operator login) at **+4,989**
   — together worth more to L1 than the next ten routines. `emulation/` is
   free now that P46 has landed. Runs parallel with P48 (files disjoint:
   P48 writes `compiler/`, this writes `emulation/` + `tasks/`).
5. **P49 — the play driver, login fixtures, and task 051**
   (`docs/Project49/PROMPT.md`). **Scheduled ahead of routine work and ahead
   of P50**, because L1's oracle is gameplay and the battery executes 15.5%
   of the book; P47 costs the driver fix at +7,474 statements and the two
   logins at +4,989, against 1,768 statements for the twelve proposed first
   routines. Holds `emulation/`, so P50 cannot run beside it.
6. **P50 — L1 substitution harness: DESIGN PHASE**
   (`docs/Project50/PROMPT.md`) — docs only, runs beside P49. Answers the
   four questions the build rests on: does pointer-normalized mediation
   already remove escape placement (**DESIGN §9.5, open since the design was
   written**); what the Mapper does with a 0x76 pointer; the exit contract
   and where "exit" is; and the calling bridge, now known to be mostly reuse
   (`rt_call` is already a terminator, `RTStubs.cpp:563` already has an LCALL
   replica, and returning into 0x77 works because `Machine::run` tests
   `IRExec::has(pc)` before any fetch). Also designs the **identity-green /
   identity-red** legs — the harness must be shown to pass AND to fail on the
   untranslated original before any C is trusted. Build phase is a separate
   prompt after P49 lands.
6b. **P50-build — L1 substitution harness** — the keystone. Now known to include
   the calling bridge as a new IR production (P46 F7) and the slot
   bijection in `ircmp` (P46 F6 / a002). See DESIGN §9.
7. **First leaf routines at L1**, bottom-up per P47's order.
7b. **P51 — candidate C: DONE, MERGED.** Six routines derived
   (HIT_ANY_CHAR, PICK_X_Y, GET_INPUT re-derived **blind**; INIT_SCREEN,
   FAKE_OCEAN, FAKE_LAND_MASS new), each with a derivation header and a
   confidence; UPDATE_SCREENS as the calibration row. Two sessions read all
   seven independently and **disagreed nowhere in the machine reading** —
   only on spelling, both settled at the gate. Four runtime findings landed
   in `RTConventions.md` (X.CB, ?READ's arg roles, ?RANDOM_NUMBER dummy
   widths, the byte-displacement convention).
   **`docs/Project51/REPORT.md` §2 is the compiler gap list and is P52's
   specification.** Headline: the needed subset is much SMALLER than feared
   — no floats, no uplevel access, no twins, no `goto`, no ON-units, and
   strings/bits only as three literals. The big item is **bytes** (§2 item
   5): `char` is a WORD kind today, so GET_INPUT's 144-byte buffer would
   lower to 144 words, and the mask set has only `trunc16`.
7c. **P52 — extend the compiler** to P51's gap list, in its centrality
   order. **Generator-first**: extend the differential corpus to byte types
   BEFORE implementing them (P48's Stage-A discipline; a second width class
   is where P48's own type-vs-mask bug class lives). Also carries the
   `LANDMASS` JSON for `gen_declarations.py` and the `quest_rt.h` additions
   from P51/a001 Q-B.
7d. **P53 — compile, diff, census the rewrite rules.** The measurement
   DESIGN §7.2a only asserts: one rule applied many times versus many rules
   applied once. Head-to-head cases: HIT_ANY_CHAR (P38 abandoned at 8/10),
   GET_INPUT (P37 staged at `BITS()`); controls PICK_X_Y and UPDATE_SCREENS.
8. **Transformer + oracle + L2.**

**Also carried and still unscheduled:** the play driver (below). It is not
optional for P44 — L1's entire oracle is gameplay under lockstep, so
broken play coverage means a broken L1 evidence base. It needs
`emulation/` and so cannot run while P46 holds it.

**P48 AND THE PLAY-DRIVER PROJECT RUN IN PARALLEL** — P48 writes `compiler/`
(new files) and its own `emulation/tests/` entries; the play-driver project
owns the rest of `emulation/` and `tasks/`. **The play-driver project must
not start until P46 is released** (it is holding `emulation/` pending task
052). Their file sets are disjoint and each
prompt names the other's territory. P46 owns `docs/IR.md`,
`docs/Provenance.md`, `emulation/`, `compiler/readable.py`,
`compiler/ircmp.py`, `docs/Project33/p33.*`, `docs/Project31/strings.ledger`,
`docs/Project46/`. P47 owns `compiler/callgraph.py` and
`docs/Project47/`. Neither touches `docs/Project44/DESIGN.md`,
`docs/NextSession.md` or `docs/README.md` — those are the integrator's.

---

## Carried, still live, unrelated to the compiler line

These predate P35 and were never addressed. The first is a correctness
problem with current coverage claims.

- **THE PLAY DRIVER** (`docs/Project33/FINDINGS_SUMMARY.md` §2). The
  driver's post-auto-move keys never reach the command prompt, so every play
  leg since task 034 has ended by SIGTERM (`end=clean` was a silent kill),
  and HELP / OBSERVE / DISPLAY_MAGIC / LIST_PLAYERS are **never exercised by
  any scripted leg**. Until this is fixed, treat all play-leg coverage as
  partial. Fix the driver (validate the full sequence, foreground), require
  `I.STOP` for play verdicts, add a standing "statements never executed by
  any leg" verdict line. Then re-run the 16-leg battery.
- **THE BATTERY'S FAILED MARKER IS ALWAYS SET** (found Sep 12 2026, P46
  task 050). `results/*/FAILED` has read `exit=1` since at least 047b —
  the accepted reference run — while every leg was `leg=OK`. Cause: `want`
  values in the P27/P28/P31/P32 sections are stale relative to P33
  (`embeds_book` wants 729 where the same file's P33-B section correctly
  wants and gets 557; `string_statements` wants 1593 and gets 1689). **A
  battery whose failure marker is always set cannot detect a failure** —
  the inverse of P41's "a check that cannot fail reads like a check that
  passed", and more dangerous, because a real regression will look normal.
  Owed: task 051, refresh the expectations. Must land before any project
  leans on the battery as a gate.
- **TASK 051 — REFRESH THE BATTERY'S EXPECTATIONS. Now urgent, and it is
  costing real time.** The battery script exits 1 on three stale `want`
  values, so it never writes `DONE`; the runner treats a missing `DONE` as
  failure and **re-runs up to MAX_ATTEMPTS=3 at ~15 minutes each**
  (`PIPELINE.md`, `bin/runner.sh:67`). Every battery has therefore been
  running 3× and ending in a false failure. Identical three fails in 047b
  (the accepted reference run), 050 and 052. The fixes:
  1. `embeds_book` want 729 → **557**, `embeds_stock` 2608 → **2436**
     (the same file's P33-B section already wants and gets 557)
  2. `string_statements` want 1593 → **1689**, literals 857 → **871**
  3. the strings-ledger check is **structurally** stale, not merely
     mis-numbered: it diffs lower.py's ledger against `p31.tsv` + `p32.tsv`
     and never consults `p33.tsv`, so it cannot pass on any post-P33 tree.
     Widen it; do not retune it.
  **Do not simply set each `want` to what the tree currently prints** —
  that converts a test into a tautology. Each number needs a reason, as
  (1) has.
- **⚠ TASK 052 WAS NOT GREEN, AND THE BATTERY IS NONDETERMINISTIC.** Found
  by P49 (`docs/Project49/q002`). 052's `run.log` ends `TASK 052 RED (4
  fails)`; the fourth is a real leg, `inj-emu end=I.STOP want=FATAL
  leg=FAIL`. Worse: `ATTEMPTS=2`, and **the two attempts of the same script
  on the same tree disagree** — attempt 1 had all 16 legs OK (which the
  integrator read and wrongly certified), attempt 2 has inj-emu failing.
  `inj-emu` is a play-mode leg whose injected fault races the driver's quit,
  and the play-driver bug (stray ESC after `D` detaches) decides the race.
  **So every battery verdict accepted since that bug appeared was drawn from
  a distribution, not measured.** The driver fix is a CORRECTNESS item for
  the verification apparatus, not only a coverage item. P49 Stage B fixes
  it; the battery after that is the reference run, and 052 is not one.
- ~~**Task 052 is GREEN and marked DONE**~~ (integrator, Sep 12): 16/16 legs,
  self-tests green with teeth red, vform selftest green with the
  broken-allocator build red, ir 7 headers and the s@ census exact. P46's
  loader work is verified.
- **THE RUNNER DESTROYS RESULTS (found Sep 12, task 053).** Two defects:
  a second attempt can start while the first is still running, and
  `bin/runner.sh:85` does `rm -rf results/$name` **before** the overlap
  guard can refuse — so a refused attempt wipes the prior attempt's data and
  leaves only its refusal message. 053's 104-file result survived solely
  because it had already been committed (`3b4a3a2`); restored, see
  `results/053-p49-stage-b/RESTORED.md`. **Third instance today** of the
  retry machinery destroying or obscuring a result. A one-line reorder —
  guard before `rm -rf` — closes the worst of it.
- **THE EMULATOR CORE DUMPED** under task 053's wider coverage
  (`Aborted (core dumped)`, run.log). New crash on newly-executed code; open.
- **Gen 6.2 checker item**: the deterministic form of the P33-C fix — defer
  a halt to the next pair boundary (today `Lockstep::halting` only guards
  `compare_pair`).
- **Float + divides** (~80 embeds): the `f*()` same-helpers family over the
  FPACs, plus DIVX/WDIVS/WLOB. Small. Manual pages owed
  (`EmulatorDivergences.md` §5).
- **Milestone 5b**: calls as edges, on-error edges, discharge
  `Project27/assumed-foldable.txt`, frames → functions.

---

## Mechanics

**Tree of record: `main` in the repo.** The user kicks sessions off with a
`Work.tgz`, but sessions have the repo and use it — push branches, push
questions. Verify artifacts against `docs/Provenance.md`. *(A tarball can be
stale: the Sep 12 design session ran on one cut before the P41 merge and
before `REWRITE_PLAN.md` landed. Pull before trusting a tarball's vintage.)*

**Questions and plan gates go through the repo.** Worker writes
`docs/ProjectNN/qNNN-title.md`, pushes to main, and **stops**; integrator
answers in `aNNN-title.md`; the user carries only a nudge each way. Every
question states the worker's RECOMMENDATION. SOP: `docs/INTEGRATOR.md` §10.

**Build:** `cd Work/emulation && make`. Self-tests:
`tests/run_helpers_selftest.sh`, `tests/run_strings_selftest.sh`,
`tests/run_strhooks_selftest.sh`.

**Launch:** `docs/Run.md`, "Lockstep play session", with
`QUEST_BLOCKS=quest.blocks.split QUEST_SYNC_LIST=quest.synclist.p27
QUEST_IR=quest.ir2.book QUEST_ADDRESS_BOOK=quest.addrbook
QUEST_PUSH_MAP=quest.pushmap.M4` (book) or `QUEST_IR=quest.ir2.stock`
(stock). String checker: `QUEST_STRINGS_CHECK=1` with
`QUEST_STRHOOKS=quest.strhooks QUEST_ARENA=quest.arena`.

**Standing checks:** run `compiler/crossings.py` at the start of any session
that adds routines (the R39 hand-assembly detector — that finding survives
the attic).

**Runtime conventions: `docs/Project28/RTConventions.md` is the home of
record** — widened Sep 12 2026 from P28's 18 `?` routines to ANY runtime
routine, including the internal `X.*`/`I.*`/`O.*`/`D.*` helpers. **Work one
out, record it there.** A convention that lives only in a C file's header is
one the project has not learned; `X.CB` is the cautionary case.

**Governing docs:** `METHOD.md` for implementation sessions,
`INTEGRATOR.md` for review/integration, `IR.md` for the IR (normative,
spec-wins), `Plan.md` for milestones, `docs/Project44/DESIGN.md` for the
current direction.
