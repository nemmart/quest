# Project 51 — THE C, AND THE FIRST MEASURED REWRITE CENSUS

## GOAL

Write the C we believe is right for a handful of routines, compile it to naive
ir 7, and **measure the distance to the book** — grouped by kind.

This produces the number `DESIGN.md` §7.2a currently only asserts:

> **How much of the distance is ONE rule applied many times, versus MANY
> rules applied once each?**

One rule applied 200 times is a compiler model working. Two hundred rules
applied once each is fitting. Nobody has measured which this is, and the
whole L2 plan rests on the answer.

**Nothing runs.** Matching is a static, text-level comparison — naive IR
beside the book's IR. The calling bridge (P50) is needed to *execute* an
`rt_call`; it is not needed to *emit* one. So this project needs no executor
support and no harness.

**Success = `docs/Project51/Census.md`**: per routine, the naive IR, the
book's IR, and the differences classified by kind and counted. Plus the C
itself, with its derivation.

---

## The routines, in priority order

Work down the list. **Stop and report when budget runs short** — a complete
census of four routines beats a thin one of seven.

| # | routine | stmts | why this one | compiler work needed |
|---|---|---|---|---|
| 1 | **HIT_ANY_CHAR** | 10 | **P38 ABANDONED it at 8/10** rather than "fit a cross-procedural register model on three sites of one callee" — exactly what an oracle SUPPLIES instead of predicting. The head-to-head against the old approach | emit-only `rt_call` ×2, one located string |
| 2 | **PICK_X_Y** | 64 | **CONTROL — known-good C.** Matched 64/64 under the old translator, so its C is very likely right. Also exercises `rt_call` where the C is trusted, so an emission bug shows up where it cannot be confused with bad C | emit-only `rt_call` ×3, by-reference temporaries (`TMP()`) |
| 3 | **GET_INPUT** | 22 | **P37 STAGED it** at the `BITS()` argument — the other refusal case the new design is supposed to dissolve. 27 callers / 77 sites, 82% covered | `?READ` with 6 args, `BITS()` via `X.CB`, unsigned char, a 144-byte buffer |
| 4 | **UPDATE_SCREENS** | 72 | **CONTROL — known-good AND already compiles and runs** (P48). Zero new work; it calibrates the census | none |
| 5 | **INIT_SCREEN** | 134 | loop cohort; no compiler change at all | none |
| 6 | **FAKE_OCEAN** | 224 | loop cohort | none |
| 7 | **FAKE_LAND_MASS** | 255 | loop cohort | none |

**Why the loop cohort matters even though it is last:** the attic's most
refitted rules were the loop ones — R36/R36′/R36a hoisting, R21c/R21d/R21e′
loop registers and DO limits, four of which P40 amended in a single project.
If a small general rewrite set works anywhere, that is where it shows.

**Why two controls.** When naive IR does not match, there are two causes — a
missing rewrite, or **the C is wrong** — and a diff alone cannot tell them
apart. On PICK_X_Y and UPDATE_SCREENS the C is known good, so their
differences are *purely* rewrite distance. That calibrates every other
routine's number.

---

## Context of record

| path | why |
|---|---|
| `docs/Project44/DESIGN.md` | §7 (the four transformation classes, the tripwire, §7.2a's metric split), §5.1 (`v`s-to-place vs `v`s-to-eliminate) |
| `docs/Project48/REPORT.md` | what the compiler does and does not do; §3.2's UPDATE_SCREENS derivation is the model for yours |
| `compiler/lower_c.py`, `compiler/difftest/` | the compiler and its tester |
| `docs/IR.md` | ir 7 |
| `docs/Salvage.md` | verified program facts: F1 static link, F4 slotpatch, F13 unsigned CHARACTER, F14 buffers, F17 bit fields, F23 the two base registers |
| `docs/Project47/Order.md` | the per-routine "needs" column these priorities came from |
| `emulation/quest.ir2.book` | the target. **Read it for COMPARISON, never to decide what to emit** |
| `Disassembled/quest.dis`, `docs/Project34/readable/` | where the C is derived from |

**Do not read `docs/attic/`.** The void rule set is precisely what this
project is measuring against, and reading it will bias the classification.

---

## Carried-in rulings

1. **RE-DERIVE the C. Do not inherit it.** `game/routines/` holds P35–P43
   output written by sessions fitting against the book; using it to derive
   rewrite rules risks measuring the old translator's shape. You may consult
   it **after** writing your own, and must report whether it agreed. (P48 did
   exactly this for UPDATE_SCREENS and said plainly that it had read the old
   file first — that honesty is the standard.)
2. **Every routine ships its DERIVATION**, in the file header: addresses,
   instruction readings, and the **cross-checks that constrain it**. P48's
   UPDATE_SCREENS header is the model — the bounds-consistency check that a
   wrong stride would break is worth more than three restatements.
3. **Every routine carries a CONFIDENCE.** `verified` (it ran, or it is a
   control), `derived` (cross-checked), `claimed` (single reading). A diff on
   a `claimed` routine is weak evidence about rewrites.
4. **The standing rule still holds: a match failure is NEVER a reason to edit
   the compiler.** If closing a diff seems to need a compiler change, that is
   a soundness bug or a missing construct — a **finding**, fixed against the
   differential tester, never against the book.
5. **Suspect the C first** (DESIGN §7.2a's escalation order). Do not record
   "we could not close it" as "a rewrite is missing" — that is the laundering
   the tripwire exists to catch.
6. **Emit-only is legitimate.** The loader will REFUSE `rt_call` in a
   symbolic block (P46 F7). That is expected and is not a bug. Nothing in
   this project loads or runs.
7. **Expect R2's systematic difference to dominate.** The compiler emits pure
   operators; the book uses the effectful family. That is one rewrite rule
   firing on nearly every arithmetic statement, and it must be counted as
   **one rule, many applications** — not as many differences.

---

## Part 1 — PLAN GATE

`docs/Project51/q001-plan-gate.md`, push to main, **STOP**. Report:

1. **Your classification scheme for differences.** This is the project's
   central instrument — the census is only as good as its categories. At
   minimum: which of DESIGN §7's four classes (elimination, binding,
   placement/merge, reordering) a difference belongs to, whether it is
   systematic or one-off, and whether it might instead be **bad C**.
2. **The compiler work items 1–3 need**, sized. Say what you would cut if
   they overrun.
3. **How you will diff.** `ircmp.py` exists from the old line — does it
   serve, or does it need the slot bijection (DESIGN §5.2) first? Note
   §5.2's refinement: the bijection is mostly **derivable** from `LDAFP`
   dataflow over a two-register state space, not supplied.
4. **Your prediction** for the headline number — roughly what fraction of the
   distance you expect to be one-rule-many-applications. Written before you
   measure, so it is scoreable.

STOP. Wait for `a001`.

---

## Part 2 — Build

Per routine: derive and write the C → compile → diff against the book →
classify. Compiler work only where the table says so, and **only additive**:
`--selftest` and the differential corpus must stay green after every change.

---

## Part 3 — Report

`docs/Project51/REPORT.md`:

- **the headline census**: differences by kind, with the one-rule-many-times
  versus many-rules-once split, and the controls' numbers called out
  separately
- your prediction versus the result
- the candidate rewrite rules the census implies, each with its application
  count — this is the **first evidence-based rewrite set** the project has
  had, and it should be presented as candidates, not as rules
- every routine where you suspect the C rather than a missing rewrite
- HIT_ANY_CHAR specifically: **does the oracle approach dissolve what P38
  abandoned at 8/10?** Answer it directly, either way
- anything in DESIGN §7 that did not survive contact

---

## Boundaries — BINDING

1. **You may WRITE:** `compiler/` (additive changes to `lower_c.py`, new
   files), `game/routines/` files you author, `docs/Project51/**`.
2. **Do NOT touch** `emulation/**` (**P49 owns it**), `docs/Project50/**`
   (**P50 owns it**), `Disassembled/**`, `docs/IR.md`, `docs/Provenance.md`,
   `docs/Project44/DESIGN.md`, any artifact. **Regenerate nothing.**
3. **Nothing loads and nothing runs.** If you find yourself needing the
   executor, you have left the project.
4. If ir 7 cannot express something you need to emit, **STOP and report** —
   IR changes are not yours.

---

## Coordination — questions and the plan gate (BINDING)

Write `docs/Project51/q00N-short-title.md`, commit, **push to main**, then
**STOP and tell the user it is there.** Do not work ahead while a question is
outstanding. You own `q*.md`; the integrator owns `a*.md`. Every question
states what you found, the decision needed, the options with your read, and
**your RECOMMENDATION**. SOP: `docs/INTEGRATOR.md` §10.

## Delivery

**Push to `p51-census` at every stage boundary.** One `Work.tgz` with the
final report.
