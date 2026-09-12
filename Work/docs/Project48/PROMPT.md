# Project 48 — THE NAIVE COMPILER

## GOAL

Build the C → IR compiler of `DESIGN.md` §3: **choice-free, deliberately
uniform, deliberately stupid**, emitting `v` declarations and symbolic blocks
for the loader to place. And build the evidence that it is **sound**, because
a compiler without that evidence is not finished.

Success is two numbers and one artifact:

- **`UPDATE_SCREENS` compiles, loads, and executes correctly** from C the
  session writes, as a standalone v-form program under a test rig.
- **N ≥ 200 generated programs agree with gcc**, run through the
  differential tester, zero disagreements.
- **`compiler/lower_c.py`** (name yours) plus `tests/` entries that run
  without a lockstep session.

**This project does not match the book.** No `ircmp`, no oracle, no
rewrites, no placement to 0x74. If you find yourself looking at
`quest.ir2.book` to decide what to emit, you have left the project — see
§"The standing rule" below, which is the whole reason this project exists
separately.

---

## Context of record — read in this order

| path | why |
|---|---|
| `docs/Project44/DESIGN.md` | **Read first.** §2 (the two obligations + the standing rule), §3 (three components), §4 (storage classes, incl. 4.1a/4.1b nesting), §5 (address spaces), §7.1 (what naive lowering looks like) |
| `docs/IR.md` | **ir 7**, normative. §5.1 (width/sign in the operator, never in storage), the `v`/symbolic-block productions P46 added, §5.8 strings, §5.10 |
| `docs/Project46/REPORT.md` | what ir 7 actually landed, and its refusals. **F7: `call`/`rt_call` are REFUSED in symbolic blocks** |
| `emulation/tests/vform_selftest.ir` | a hand-written v-form program that works. Your output should look like this |
| `docs/Project47/Order.md` | why UPDATE_SCREENS is the target |
| `docs/Salvage.md` | verified program facts. F4 slotpatch, F13 unsigned CHARACTER, F14 buffers, the ABI rows |
| `docs/METHOD.md` | law for implementation sessions |

**Do not read `docs/attic/`.** `translate.py` in particular is the P35–P43
translator; it is void, it was edited to close diffs against the book, and
reading it will pull you toward the failure this project is designed to
avoid. Salvage is the sanctioned extract.

---

## The standing rule (DESIGN §2) — the most important line in this prompt

> **A match failure is NEVER a reason to edit the compiler.**

The compiler is tested against **gcc**, never against `quest.ir2.book`. This
separation is the entire lesson of the nine projects in the attic: there, the
translator was edited to close diffs against the book, so the artifact under
test and the artifact doing the testing were the same thing, and every rule
ended up with one witness.

Practically, for you: **your test programs must not be game routines** (with
the single exception of the UPDATE_SCREENS end-to-end check, which tests that
the compiler *runs*, not that it matches). Generated programs, hand-written
programs, edge cases — all synthetic.

---

## Carried-in rulings

1. **Naive means naive.** Every address materialised into a base register,
   every value into its own `v`, no reuse, no hoisting, no register cost
   model, no lookahead, no peepholes. DESIGN §7.1's worked example is the
   house style. Cleverness is not a bonus here — it is a defect, because
   every clever thing the compiler does is a thing a *rewrite* cannot be
   credited with later, and the rewrite count is the metric that tells us
   how much of the DG compiler we understand.
2. **`v` is the only storage you choose.** `t` falls out of lowering one
   instruction and is never chosen; `s@` twins are for concatenation groups.
   Names are `<ENTRY>.v<digits>`, uppercase entry, per DESIGN §4.1.
3. **You do not place anything.** Emit declarations; the loader assigns
   0x76/0x77. If you are computing an address, stop.
4. **Uplevel access is free** (DESIGN §4.1a). `FIRE.1` reads `FIRE.v0` by
   name. No static link, no parent frame pointer. The parent map is flat
   (§4.1b); a chain deeper than one level is a **hard error**.
5. **Per-family `v` namespace** — `FIRE` and `FIRE.1` share one.
6. **Types compile away.** IR.md §5.1 puts width and sign in the *operator*.
   A `v`'s type drives mask/extend generation (`sx16`/`zx16`/`trunc16`) and
   then is gone. **Mask placement is the most likely place for a soundness
   bug**, so it deserves the most differential-test attention.
7. **REFUSE, never approximate.** A construct you cannot lower is a loud
   refusal naming the construct and the source line. A compiler that quietly
   emits something plausible for an input it does not understand is the one
   failure mode that the differential tester might not catch and `ircmp`
   certainly will not explain.
8. **No calls in this project.** ir 7 refuses `call`/`rt_call` in symbolic
   blocks (P46 F7). The calling bridge is the harness project. Your C subset
   therefore excludes function calls; refusing one is correct behaviour.

---

## The C subset — v1

In: `int16_t`/`int32_t` (and unsigned) scalars; locals and parameters;
arrays and record field access as `game/quest_rt.h` spells them
(`ARRAY1`, `SUB`); assignment; arithmetic, bitwise and comparison
operators; `if`/`else`; `goto` and labels; `while`/`for`; `return`.

Out (refuse, with a named reason): function calls, varying strings and any
string operation, floats, bit fields, `ON`-conditions, pointers beyond
by-reference parameters, anything else.

**Propose changes to this list at the gate** if UPDATE_SCREENS needs
something not in it — you will have read the routine and I will not have.

---

## Part 1 — PLAN GATE

Write `docs/Project48/q001-plan-gate.md`, push to main, **STOP**. Report:

1. **The lowering design.** How a statement becomes blocks and statements;
   how `v`s are allocated per source variable; where masks are inserted and
   why that placement is sound; what a `goto`/loop lowers to.
2. **The differential tester's design** — and be specific, because this is
   the evidence base for everything downstream. How are programs generated?
   How are the two runs compared (final memory state? a trace? both)? How do
   you make gcc and the IR executor agree on *observable* state when one runs
   natively and the other in the emulator? **What class of bug would this
   design MISS?**
3. **Your read of UPDATE_SCREENS** against the v1 subset — what it needs
   that is not there.
4. **Sizing**, so it can be checked at the end.

STOP. Wait for `a001`.

---

## Part 2 — Build, in this order

**Stage A — the differential tester first.** Deliberately before the
compiler. It is small, it is the soundness evidence, and if it lands second
it will be shaped to pass whatever the compiler already does.

**Stage B — the compiler**, driven by the tester.

**Stage C — UPDATE_SCREENS end to end.** Write the C fresh from
`Disassembled/` and the readable rendering. `game/routines/UPDATE_SCREENS.c`
exists as P35 output — **reference only, never trusted** (Plan.md's standing
policy for prior-generation translations). Say in the REPORT whether you
consulted it and whether it was right.

---

## Boundaries — BINDING

1. **You may WRITE:** `compiler/` (new files only — do NOT modify
   `ircmp.py`, `readable.py`, `gen_declarations.py`, `callgraph.py`),
   `docs/Project48/**`, new `emulation/tests/` entries for your own tests,
   and `game/routines/` files you author.
2. **Do NOT touch** `emulation/` outside `tests/`, `docs/IR.md`,
   `docs/Provenance.md`, `Disassembled/**`, any artifact,
   `quest.ir2.*`, `quest.addrbook`, `quest.arena`. **Regenerate nothing.**
3. **The play-driver project may run in parallel** and owns `emulation/`
   (excluding your new test files) and `tasks/`. Coordinate through a
   question if you need anything there.
4. **Do not read the attic.**
5. If ir 7 cannot express something the compiler needs, that is a
   **STOP-and-report** — it is an IR change and IR.md is not yours.

---

## Part 3 — Report

`docs/Project48/REPORT.md`:

- the differential tester: programs run, disagreements found, **and the bug
  classes it cannot catch** (restated from the gate, honestly)
- every soundness bug it found in your own compiler — these are the most
  valuable lines in the report, and a report with zero is a report I will
  suspect the tester for
- UPDATE_SCREENS: compiled, loaded, ran; whether P35's C was right
- the refusal list as implemented
- sizing estimate vs actual
- **anything in DESIGN that did not survive contact.** P46 found three; you
  are the first project to build against §4 and §7.1 rather than read them

---

## Coordination — questions and the plan gate (BINDING)

Write `docs/Project48/q00N-short-title.md`, commit, **push to main**, then
**STOP and tell the user it is there.** Do not work ahead while a question is
outstanding. The integrator answers in `a00N-short-title.md`.

You own `q*.md`; the integrator owns `a*.md`. Never edit an `a*.md`.

Every question states: what you were doing and what you found, the decision
needed, the options with your read on each, and **your RECOMMENDATION**.
Full SOP: `docs/INTEGRATOR.md` §10.

---

## Delivery

**Push to `p48-compiler` at every stage boundary.** **One `Work.tgz` with the
final report.**
