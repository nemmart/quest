# Project 54 — THE CALLING BRIDGE

## GOAL

Make a call **out of a symbolic block actually execute**, and return into a
0x77 block.

ir 8 (P52) lets a compiled routine *spell* `rt_call` and `call` with a
symbolic return label, and the loader validates them. **Nothing runs them.**
That is the single thing standing between "a naive program loads" and "a
naive program is substitutable", and it blocks every routine in the game that
calls anything — which is almost all of them.

**Success:**

- **a hand-written v-form IR program makes an `rt_call`, the callee runs, and
  control returns into a 0x77 block** — proven by a self-test
- **the same for a game→game `call`**
- **PICK_X_Y executes end to end** with a synthetic caller (see below), once
  P53's compiler can produce it; if P53 is not ready, a hand-written
  equivalent proves the same path

This is an **executor project**. `emulation/` is yours. The compiler is P53,
running in parallel.

---

## The mechanism mostly exists — confirm each piece, then wire it

Established in design discussion; **verify each against the source rather
than taking it on trust**:

1. **`rt_call` is a TERMINATOR** (IR.md). A call already ends a block and the
   continuation is a separate block. The symbolic return label fills a slot
   the structure already had.
2. **An LCALL replica already exists**: `RTStubs.cpp:563` —
   *"LCALL replica: push (psr<<16)|argc, ac3 = the injection pc"*, with
   `machine.wide_push((machine.get_psr()<<16) | argc)`. The bridge sets the
   return address to the continuation's **0x77** address rather than an
   injection pc.
3. **Returning into 0x77 works by construction**: `Machine::run` tests
   `IRExec::instance->has(pc)` at the **top of the fetch path, before any
   memory access** (Machine.cpp:276), so a 0x77 pc dispatches the symbolic
   block with no page mapped.
4. **`RTBridge.cpp:170`'s `native_return`** already pops the shadow frame.

**What is genuinely new** is the argument handling, and it is **two
mechanisms, not one** (P52 a001 R4):

- **`rt_call` arguments go on the REAL STACK.** The runtime reads argument
  *n* at `wsp−2n` from the LCALL marker (`RTBridge::arg_pointer`,
  `RTBridge.cpp:111`). Writing them into `a` cells would put them where the
  callee never looks.
- **game→game arguments go in `a` cells** — the callee's declared argument
  cells, written by the caller before transfer. Legal only because the game
  is non-reentrant.

Say in the REPORT which of (1)–(4) held and which did not.

---

## Context of record

| path | why |
|---|---|
| `docs/IR.md` | **ir 8.** §5.10.6 (symbolic call returns), §6 (`call`/`rt_call`), §5.10.1c/d (`a`, `arg_count`, `ret`) |
| `docs/Project52/REPORT.md` | what P52 landed and what it deliberately left unbuilt |
| `emulation/hw/RTStubs.cpp`, `RTBridge.cpp`, `Machine.cpp`, `IRExec.cpp` | the four mechanism pieces |
| `emulation/tests/vform_selftest.cpp` | the model for your self-test |
| `docs/Salvage.md` | **F4 slotpatch** — a valued game routine returns through the saved-ac0 image (`wp(ac3,-8)` 32-bit, `wp(ac3,-7)` 16-bit); F1 the static link |
| `docs/Project28/RTConventions.md` | per-callee argument roles and widths |
| `game/routines/PICK_X_Y.c` | the end-to-end target |
| `docs/Project50/**` | the harness design, if it has landed — the bridge is its first component |

**Do not read `docs/attic/`.**

---

## Why PICK_X_Y is the end-to-end target

It is the only routine in P51's seven that exercises the whole call surface,
and **its C is known good** — it matched 64/64 under the old translator, so a
failure is the bridge's and cannot be confused with bad C.

- **three valued `rt_call`s** to `?RANDOM_NUMBER` — the result arrives in
  `ac0` and the valued-call split applies
- **six `TMP()` dummies**, widths from the callee (32-bit)
- **two write-back parameters** — `*x` and `*y` store into the *caller's*
  cells
- **a retry loop**, so the call count is path-dependent

**It needs a synthetic caller to be a real test.** Running it with a rig that
seeds `a1`/`a2` proves the write happens; it does not prove the calling
convention round-trips. Write a small `REPOSITION`-shaped caller — two
locals, passing `wp(v,0)` for each — and check the **caller's** cells after
the call, not PICK_X_Y's state.

**And check the seed.** `?RANDOM_NUMBER` advances `SD_PTR->seed`, which is
shared data. PICK_X_Y's five separate retry tests bail at different points,
so the number of calls per iteration depends on *where* it bails. A bridge
that loses or duplicates a call returns the right coordinates with the
**wrong seed**. Compare the seed, not only the outputs.

---

## The identity legs — build the teeth in from the start

Before any of this is trusted:

- **identity-green** — run a routine through the bridge with no substitution
  in play, and the boundary must PASS
- **identity-red** — perturb exactly one thing (an extra `?RANDOM_NUMBER`
  call, or a coordinate off by one) and it must **FAIL**

**Without identity-red, a check that compares nothing passes identity-green
perfectly.** This project has recorded that failure six times — P41's three,
the battery's permanently-set `FAILED` marker, 052's stale `want` values, and
P52's three teeth legs that loaded when they should have refused. P52's was
the first caught by machinery rather than by someone noticing. Keep that.

---

## Part 1 — PLAN GATE

`docs/Project54/q001-plan-gate.md`, push to main, **STOP**. Report:

1. **Each of the four mechanism claims, verified or corrected**, line-cited.
2. **The design for both argument mechanisms**, and what the stack looks like
   at the moment of transfer for each.
3. **What happens on RETURN** — for an `rt_call` (result in `ac0`), for a
   valued game call (F4 slotpatch into the saved-ac0 image), and for a void
   call. `ret` cells are declared in ir 8; say how they are filled.
4. **The self-test plan**, including the identity legs.
5. **Sizing**, line-cited, as P46 and P52 did.
6. **Anything above that does not survive contact.** Every one of the four
   claims is mine from a design conversation, not measured.

STOP. Wait for `a001`.

---

## Boundaries — BINDING

1. **You may WRITE:** `emulation/**`, `docs/Project54/**`.
2. **Do NOT touch** `compiler/**` (**P53 owns it and runs in parallel**),
   `game/**`, `docs/IR.md` (**P52's; an IR change is a STOP-and-report**),
   `Disassembled/**`, any artifact. **Regenerate nothing.**
3. **The strict surface is untouched.** Both artifacts must load and run
   unchanged; the 16-leg battery must not regress. Any change to existing
   block semantics is a **STOP-and-report**.
4. If the bridge needs a grammar change, **STOP and report** — `docs/IR.md`
   is P52's and the change would have to be ruled.

---

## Known hazards

- **`bin/runner.sh` is fixed in the repo but NOT DEPLOYED**; the poller runs
  the old version. Ask before trusting a results directory.
- **The runner box filled its disk** (Sep 12): `/tmp/run<NNN>-<leg>`
  directories are never cleaned up, and a play leg now costs ~700M–2G. Check
  free space before queueing a battery.
- **Coverage deltas under a few hundred statements are noise** — world
  generation is random, among three sources of run-to-run variation.

---

## Part 3 — Report

`docs/Project54/REPORT.md`: which mechanism claims held; both argument
mechanisms as built; the self-test including both identity legs; PICK_X_Y end
to end **including the seed comparison**; sizing estimate vs actual; battery
regression evidence; and what did not survive contact.

---

## Coordination — questions and the plan gate (BINDING)

Write `docs/Project54/q00N-short-title.md`, commit, **push to main**, then
**STOP and tell the user it is there.** You own `q*.md`; the integrator owns
`a*.md`. Every question states what you found, the decision needed, the
options with your read, and **your RECOMMENDATION**. SOP:
`docs/INTEGRATOR.md` §10.

**P53 runs in parallel and owns `compiler/`.** You will need its output for
the PICK_X_Y leg; if it is not ready, hand-write the equivalent IR and say so.

## Delivery

**Push to `p54-bridge` at every stage boundary.** One `Work.tgz` with the
final report.
