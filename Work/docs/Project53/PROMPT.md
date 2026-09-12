# Project 53 — THE COMPILER AGAINST ir 8

## GOAL

Make `compiler/lower_c.py` compile P51's seven routines to **ir 8**, and make
the resulting IR **load**.

**Nothing executes in this project.** Calls validate but do not run — the
bridge is P54, running in parallel. Your acceptance is *compiles and loads*,
which P52's loader checks and which needs no executor.

**Success:**

- **all seven routines compile to ir 8 and LOAD** (`IRExec::load_file`), or
  are named as findings with the reason
- **the differential corpus is GREEN again** — it is currently gated
  expected-red (P52), and clearing it is yours
- **byte types are in the generator BEFORE they are in the compiler**

---

## Context of record

| path | why |
|---|---|
| `docs/IR.md` | **ir 8, normative.** §5.10 is the new surface: variable form, pointer vtypes, `a`/`arg_count`/`ret` cells, initialised `v`, the KIND tripwire, the valued-call split |
| `docs/Project52/REPORT.md` | what ir 8 landed and what it deliberately did not; §7 "what did not survive contact" |
| `docs/Project51/REPORT.md` §2 | **the gap list — your specification**, in its centrality order |
| `docs/Project48/REPORT.md` | the compiler and its differential tester; §2's bug is the cautionary tale |
| `compiler/lower_c.py`, `compiler/difftest/` | what you are changing |
| `game/routines/*.c` | the seven, plus eight older files |
| `docs/Salvage.md` | F1/F2 static link, F4 slotpatch, F12 X.CB, F13/F14 bytes and `?READ` |

**Do not read `docs/attic/`.**

---

## FIRST TASK — the native view is broken

**Ten of fifteen routine files fail `g++ -fsyntax-only`** (P52 q002 §3),
identically before and after the `RANGE_CHECK` rename:

| routine | error |
|---|---|
| FAKE_OCEAN, FAKE_LAND_MASS | `MIN`/`MAX` not declared |
| INIT_SCREEN | `FAKE_LAND_MASS`, `FAKE_OCEAN` not declared |
| HIT_ANY_CHAR | `GET_INPUT` not declared |
| GET_INPUT | `TMP(buf)`: `tmp_arg → void*` ambiguous; `unsigned char* → int32_t` invalid |
| UPDATE_SCREENS | `SD_PTR`, `PLAYER` not declared — **missing `#include "declarations.h"`**, which its six siblings have |
| RETURN_MESSAGE | `DATA`, `RETURN$2` not declared |

**The native view is one of P48's two differential oracles.** "The candidate C
compiles natively" is exactly the kind of thing that gets assumed; it does
not hold. Most are P51 §2 item 11 (missing prototypes) and belong in
`quest_rt.h`. UPDATE_SCREENS' missing include is a one-line fix in the only
file P51 did not write.

Do this before running any corpus.

---

## Carried-in rulings

1. **Naive means naive** (DESIGN §7.1, P48 R1). No hoisting, no reuse, no
   register cost model, no peepholes. Cleverness is a **defect**: anything
   clever the compiler does is something a rewrite cannot be credited with
   later, and rewrite count is how we measure what we understand.
2. **Pure operators only** (P48 R2). `add`/`sub`/`mul`/`cvwn`/`ash` are
   effectful and are rewrites with a flag-liveness precondition.
3. **`R[]` is NEVER emitted.** The naive dereference is `M32[M32[a]]`.
   `M32[M32[a]] → M32[R[a]]` is a rewrite. IR.md's census establishes the
   precondition holds at every site in the program (see `quest.assumptions`
   below).
4. **Two argument mechanisms** (P52 a001 R4). `rt_call` arguments go on the
   **real stack** — the runtime reads argument *n* at `wsp−2n`
   (`RTBridge.cpp:111`). game→game arguments go in **`a` cells**, legal only
   because the game is non-reentrant.
5. **The valued-call split** (IR.md §5.10.6): `r = f(...)` is a call
   terminating one block and `r = ac0` opening the next, so **a call cannot
   sit inside a larger expression**.
6. **`TMP(e)` width comes from the CALLEE's parameter**, not the expression
   (`?RANDOM_NUMBER` 32-bit; `?READ` args 3 and 5 16-bit). That is a lookup
   in `docs/Project28/RTConventions.md`.
7. **REFUSE, never approximate** (P48 R7). A construct you cannot lower is a
   loud refusal naming it and the source line.
8. **A match failure is NEVER a reason to edit the compiler.** You are not
   matching anything in this project, but the rule stands: the compiler is
   tested against gcc, never against the book. **Do not open
   `quest.ir2.book`.**

---

## Generator-first for bytes

P51's gap item 5 is the big one: `char`/`unsigned char` currently map to
`u16` in `KINDS`, a **one-word cell**, so GET_INPUT's `unsigned char buf[144]`
would lower to 144 words instead of 72. ir 8 has `char n`; the compiler must
emit it, plus `trunc8`, byte load/store (`M8`), and byte pointers (`*char`,
`XPEFB`).

**Extend `cgen.py` to byte types BEFORE implementing them in `lower_c.py`**
(P48's Stage-A discipline). A third width class doubles the promotion
surface, and P48's own bug was a type error surfacing three operators away as
a wrong operator suffix. That bug class lives exactly here.

---

## Also in scope: `quest.assumptions`

A small file of **whole-program facts the transformer may rely on**, so a
future solver does not re-derive them per routine. Seed it with the one
established fact:

```
# quest.assumptions — established whole-program facts, with provenance.
# Each row: the claim, the evidence, and what would FALSIFY it.
ptr-bit31-clear 0x70000210  SD_PTR. Sole writer 7015BE43 (startup); the
                            buffer passed to ?GET_SHARED_PAGE as argument 2.
ptr-bit31-clear 0x70000212  Sole writer 7015BEE3, immediately after
                            ?GET_SHARED_PAGE @7015BEDF returns a page address.
ptr-bit31-clear 0x700007A0  Sole writer 7015C2CD; computed as
                            add(ac0, M32[0x70000210]) — SD_PTR + an offset.
```

This upgrades IR.md's census from *"1,008 provable, 27 assumed"* to **provable
everywhere**: two values come from the OS's shared-page mapping and the third
is arithmetic on one of them, so bit 31 cannot be set.

**Make it checkable, not merely readable.** The facts hold *because* each
address has exactly one writer and that writer is Eagle startup code. Ship a
check that the writer count is still one — the assumption silently stops
holding the day a translated routine writes one of those words.

---

## Part 1 — PLAN GATE

`docs/Project53/q001-plan-gate.md`, push to main, **STOP**. Report:

1. **The ir 7 → ir 8 migration**: what `lower_c.py` emits today that ir 8
   spells differently, and the size of the change.
2. **Byte types**: the generator extension and the `KINDS` change, sized.
3. **Which of the seven you expect to compile and load**, with reasons for
   any you expect to refuse. Scoreable against the result.
4. **The native-view fixes** you intend, and whether `quest_rt.h` is enough.
5. **Anything in ir 8 that cannot express what P51's C says.** P52 read all
   seven and found none; you are the first to try *compiling* them, which is
   a stronger test.

STOP. Wait for `a001`.

---

## Part 2 — Build

Native view first, then the generator's byte support, then the compiler, then
the seven. Push at every stage boundary.

---

## Boundaries — BINDING

1. **You may WRITE:** `compiler/**`, `game/quest_rt.h`, `game/routines/*.c`
   (**only the native-view fixes named above** — do not rewrite a
   derivation), `docs/Project53/**`, and `quest.assumptions` (put it beside
   `quest.addrbook`; say where in the gate).
2. **Do NOT touch** `emulation/**` — **P54 owns it and runs in parallel.**
   If you need a loader change, that is a **STOP-and-report**: the IR is
   P52's and executing calls is P54's.
3. **Do NOT open `quest.ir2.book`** or `docs/attic/`.
4. **Nothing executes.** If you are trying to run compiled IR, you have left
   the project — that is P54's bar.

---

## Part 3 — Report

`docs/Project53/REPORT.md`: which of the seven compile and load and which
refuse, with reasons; the differential corpus numbers and its **construct
census** (P48/a001: every zero in that table is a real hole); every soundness
bug the tester found in your own compiler — **a report with zero there is a
report I will suspect the tester for**; the native-view fixes; and anything
in ir 8 or in P51's C that did not survive contact.

---

## Coordination — questions and the plan gate (BINDING)

Write `docs/Project53/q00N-short-title.md`, commit, **push to main**, then
**STOP and tell the user it is there.** You own `q*.md`; the integrator owns
`a*.md`. Every question states what you found, the decision needed, the
options with your read, and **your RECOMMENDATION**. SOP:
`docs/INTEGRATOR.md` §10.

**P54 runs in parallel and owns `emulation/`.** Coordinate through a question
if you need anything there.

## Delivery

**Push to `p53-compiler` at every stage boundary.** One `Work.tgz` with the
final report.
