# Project 50 — THE L1 SUBSTITUTION HARNESS: DESIGN AND MEASUREMENT

## GOAL

Produce the **design of record** for L1 substitution — running a compiled C
routine in the clone, in place of the original, under lockstep — and answer
by measurement the questions that decide how large the build is.

**This phase writes NO CODE.** `docs/Project50/` only. P49 is running and
holds `emulation/`; the build phase follows it, as a separate prompt, and
will be shaped by what you find here.

That is not a consolation scope. Four projects in a row (P45–P48) landed
because their gates measured the real tree before anyone built anything, and
two of them (P46, P47) found errors in the design that would otherwise have
cost a project. P50 is the largest remaining piece and it currently rests on
one **unanswered** open item from `DESIGN.md`.

**Success = `docs/Project50/HarnessDesign.md`** plus the answers below, each
backed by a source citation rather than an inference.

---

## Context of record — read in this order

| path | why |
|---|---|
| `docs/Project44/DESIGN.md` | §8 (L1/L2), §9 (substitution mechanics — **§9.5 is the open item**), §5.2 (slot bijection), §11 (ON-conditions) |
| `docs/IR.md` | ir 7. §6 `call`/`rt_call`, §5.10 symbolic blocks, and what ir 7 REFUSES |
| `docs/Project46/REPORT.md` | F6, F7, and §3's note on the Mapper's view of a 0x76 pointer |
| `docs/Project48/REPORT.md` | F4 (parameter `v`s have nothing to place onto — the bridge WRITES them), F6 |
| `docs/M4aDesign.md` | the address book, the WSAVS hijack, "probe-class checking + pointer-normalized mediation" |
| `docs/Mapper.md` | forms, the three-call surface, directionality |
| `docs/Project47/CallGraph.md`, `Order.md` | bottom-up ordering, the §9.3 cost per edge |
| `docs/Salvage.md` | F1 static link, F4 slotpatch, the ABI rows |
| `emulation/hw/` | `Machine.cpp`, `Lockstep.cpp`, `BlockSync.cpp`, `IRExec.cpp`, `RTStubs.cpp`, `RTBridge.cpp`, `Mapper.hpp`, `AddressBook.cpp` |
| `docs/Plan.md` | Step 4's equivalence definition — *same OS-layer calls, same shared-data changes* |

**Do not read `docs/attic/`.**

---

## The questions, in priority order

### Q1 — does pointer-normalized mediation already solve escape placement?

**`DESIGN.md` §9.5 has been open since the design was written and nothing has
answered it.** M4a is described as doing *"probe-class checking +
pointer-normalized mediation."* If the checker already normalises pointer
VALUES at a rendezvous rather than comparing them raw, then a `v` whose
address escapes into Eagle game code does **not** need its true 0x74 address,
and L1 is free of the addrbook entirely. If it does not, escape analysis is
required work.

**Read the source and answer it.** This single answer changes the size of the
build substantially, and it is the reason this phase exists.

### Q2 — what does the Mapper do with a 0x76 pointer?

P46 §3 notes that a 0x76 pointer in a register at a rendezvous decodes as
Mapper form `None` and would MISMATCH against a master stack pointer. Nobody
has checked what that means in practice. Does it fault, mismatch loudly, or
pass silently?

### Q3 — the exit contract: what must agree, and where is "exit"?

DESIGN §9.2 sketches it. Make it precise and cite the code.

- **What must agree at the boundary**: shared/global game state, the return
  value (Salvage F4: `slotpatch` routines store a 16-bit result into the
  saved-ac0 image at `wp(ac3,-7)`/`-8`), the stack restored to the caller's
  expectation. **And the outgoing call sequence** — see below.
- **Where "exit" is.** The easy case is a return to the Eagle caller. The
  awkward ones: (a) a translated routine calling an Eagle *game* routine —
  that callee is still checked, unless it is delisted too (§9.3's cost, and
  why ordering is bottom-up); (b) a **signal unwinding out of translated code
  mid-region** (DESIGN §11). Say what each does to the region boundary.

**The outgoing call sequence is part of equivalence, not an extra.** Plan.md
Step 4 says *same OS-layer calls, same shared-data changes*. Worked example
to reason from: PICK_X_Y retries until it finds a point in the world
rectangle, burning `RANDOM_NUMBER$3` calls per iteration, and that routine
**advances the seed in shared data**. Its five separate `goto retry` tests
bail at different points, so the number of calls per iteration depends on
*where* it bails. A translation that reorders or merges two tests can return
**identical x and y with a different seed** — and every later random draw in
the game diverges, far from the cause. Recommend whether the sequence is
compared directly (logged: callee, args, order) or left to shared-data
comparison to catch, and say why.

### Q4 — the calling bridge, now that the mechanism is known

ir 7 REFUSES `call`/`rt_call` in symbolic blocks (P46 F7), so a compiled
routine cannot call anything, and that blocks P48's compiler at the call-free
subset — most of the game.

The mechanism turns out to be **much smaller than DESIGN implies**, and the
pieces exist. Confirm each against the source and design the change:

- **`rt_call` is a TERMINATOR** (IR.md). A call already splits the block and
  the continuation is a separate block at `site+4`. So the grammar change is
  a symbolic return label — `rt_call ?F(...) -> <ENTRY>.b<n>` — filling a
  slot the structure already has.
- **An LCALL replica already exists**: `RTStubs.cpp:563` — *"LCALL replica:
  push (psr<<16)|argc, ac3 = the injection pc"*, with
  `machine.wide_push((machine.get_psr()<<16) | argc)`. The bridge sets the
  return address to the continuation's **0x77** address rather than an
  injection pc.
- **Returning into 0x77 works by construction**: `Machine::run` tests
  `IRExec::instance->has(pc)` at the TOP of the fetch path, before any memory
  access, so a 0x77 pc dispatches the symbolic block with no page mapped.
- `RTBridge.cpp:170`'s `native_return` already pops the shadow frame.

Say what is genuinely new, what is reuse, and **what breaks if a compiled
routine calls an Eagle GAME routine** rather than a `$N` runtime one.

---

## The gate item that is not a question: the identity legs

Before any C is trusted, the harness must be shown to work on **no
translation at all**. Design two legs:

- **identity-green** — delist a routine's blocks, run the ORIGINAL in both
  master and clone, and the boundary check must PASS. Proves the check is not
  broken in the accepting direction. *(If the original itself fails the
  boundary check, the harness is wrong, not the translation — and that must
  be established before a translation is ever blamed.)*
- **identity-red** — run the original but perturb exactly one thing: an extra
  `RANDOM_NUMBER$3` call, or a returned coordinate off by one. The boundary
  check must **FAIL**.

**Without identity-red, a boundary check that compares nothing passes
identity-green perfectly.** That is the failure this project has now recorded
four times — P41's three instances, and a003's battery whose `FAILED` marker
was permanently set. Design the teeth in from the start; the existing
`forced` leg is the precedent.

---

## Part 1 — PLAN GATE

Write `docs/Project50/q001-plan-gate.md`, push to main, **STOP**. Report your
answers to Q1–Q4 with citations, the identity-leg design, and your estimate
of the build phase's size broken down by piece. If Q1 comes back "mediation
already normalises", say what that removes.

---

## Part 2 — the design document

`docs/Project50/HarnessDesign.md`: entry interception (M4a's hijack),
sync-list derivation from a replaced-routine set, the calling bridge, the
rendezvous contract, the identity legs, and the staging plan for the build.

State plainly the bargain L1 makes: **inside the region, lockstep verifies
nothing.** Combined with P48's F6 — bugs the routine's own arithmetic cannot
expose are invisible at L1 at any coverage — that is the second independent
reason L2 is not optional. Both belong in the design.

---

## Boundaries — BINDING

1. **You may WRITE `docs/Project50/**` and nothing else.** No code, no tests,
   no task scripts, no edits to any existing doc. Recommend edits in the
   report; the integrator lands them.
2. **You may READ anything** except `docs/attic/`.
3. **P49 is running and owns `emulation/`, `tasks/`, `bin/`,
   `docs/Project13/drive.py`, `docs/CheckerHistory.md`.** Do not write there.
   Note that P49 will change coverage and may change verdict machinery — say
   in the design what depends on its outcome.
4. **Measurements are against a named commit.** Record it; the build phase
   re-confirms before relying on anything here.
5. If the answer to Q1 contradicts `DESIGN.md` §9.5, that is a **finding** —
   report it, do not edit the design.

---

## Part 3 — Report

`docs/Project50/REPORT.md`: the four answers with citations, what in DESIGN
§9 did not survive contact, the build estimate, and the recommended staging.

---

## Coordination — questions and the plan gate (BINDING)

Write `docs/Project50/q00N-short-title.md`, commit, **push to main**, then
**STOP and tell the user it is there.** Do not work ahead while a question is
outstanding. You own `q*.md`; the integrator owns `a*.md`. Every question
states what you found, the decision needed, the options with your read, and
**your RECOMMENDATION**. SOP: `docs/INTEGRATOR.md` §10.

## Delivery

**Push to `p50-harness-design` at every stage boundary.** One `Work.tgz` with
the final report.
