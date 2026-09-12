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

1. **P45 — attic salvage** (`docs/Project45/PROMPT.md`). Mine `docs/attic/`,
   produce `docs/Salvage.md`, mark tainted rows in `game/declarations.json`
   in place. Salvage is the deliverable; reorganising the attic is not.
2. **P46 — ir 7: `v` declarations, symbolic blocks, the `s@` rename**
   (`docs/Project46/PROMPT.md`). `<ENTRY>.v<digits>` / `<ENTRY>.b<digits>`,
   loader placement at 0x76 / 0x77, `t@` → `s@` by regeneration.

3. **P47 — the basic compiler** — C → naive IR, choice-free.
4. **P48 — compiler differential tester** — gcc vs our lowering, on programs that
   are not game routines. Small, and it is the entire evidence base for
   compiler soundness. Do not let it land after the compiler.
5. **P49 — L1 substitution harness** — the keystone. See DESIGN §9.
6. **First leaf routines at L1**, PICK_X_Y first, **bottom-up** by call
   graph (DESIGN §9.4).
7. **Call-graph extraction** from the book.
8. **Transformer + oracle + L2.**

**P45 and P46 RUN IN PARALLEL.** Their file sets are disjoint and each
prompt names the other's territory. P45 owns `docs/Salvage.md`,
`docs/Project45/`, `game/declarations.json`, attic annotations. P46 owns
`docs/IR.md`, `docs/Provenance.md`, `emulation/`, `compiler/readable.py`,
`docs/Project46/`. Neither touches `docs/Project44/DESIGN.md`,
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

**Governing docs:** `METHOD.md` for implementation sessions,
`INTEGRATOR.md` for review/integration, `IR.md` for the IR (normative,
spec-wins), `Plan.md` for milestones, `docs/Project44/DESIGN.md` for the
current direction.
