# Project 57 — THE STRING CENSUS: WHAT THE GAME ACTUALLY DOES

## GOAL

**Map every string-moving site in the book into a proposed `v`-with-`string`
type, and size the type so it is general enough for all of them and no more.**

This is a **measurement project**. You write no compiler code and change no
artifact. The deliverable is a specification derived from 1,689 sites, plus the
evidence for every clause in it.

`docs/StringModel.md` is a **parked design worked out in conversation**. It has
one settled question and six open ones, and **none of it was derived from the
program**. Your job is to replace guesswork with counts — and to say plainly
where the parked model is wrong, which is the most valuable thing you can
return.

**Success = `docs/Project57/StringCensus.md`** (the measurement) and a revised
`docs/StringModel.md` (the specification, now evidence-backed).

---

## The corpus

| instruction | sites |
|---|---:|
| `WCMV` | **1,637** |
| `WCMP` | 40 |
| `WBLM` | 12 |
| `WSCN` / `WCTR` / `WEDIT` | 0 — confirm and move on |

The IR's string statements already show a cross-product of length forms in the
destination and the source:

```
[@A, acN]          = [@B, acN]              117
[@A, N), acN]      = [@B, N), acN]           40
[@A, N), N varying] = [@B, N), N]            39
[@A, N), N]        = [@B, N), varying]       16
[@A, N]            = [@B, "literal"]         11
[@A, acN]          = [@B, N]                  9
```

**That cross-product is the thing to pin down.** Which combinations occur, how
often, and what each means.

---

## Context of record

| path | why |
|---|---|
| `docs/StringModel.md` | **the parked design you are replacing with evidence.** Read it, then be willing to contradict it |
| `docs/IR.md` §5.8 | the string statements as lifted; §5.9 arena twins, `claim`/`release` |
| `docs/Project29/StringsDesign.md` | design of record for the string family, plus its corrections log |
| `emulation/quest.arena` | 57 twins with their computed sizes |
| `docs/Salvage.md` | F13 CHARACTER is unsigned, F14 buffers, F16 twin sizing, F22 |
| `emulation/quest.ir2.book`, `quest.mem` | the corpus and the initialised data |
| `Disassembled/quest.dis` | where a reading needs the instruction rather than the lift |
| `docs/Project51/REPORT.md` | what the seven do and do not use — **none of them concatenates** |

**Do not read `docs/attic/`.**

---

## The questions — all seven, answered by counting

**Q1 — the cross-product.** Every (destination length form × source length
form) combination that occurs, with counts. Fixed constant, `varying`,
register-supplied, literal. **Which combinations never occur is as important as
which do** — a type general enough for absent cases is over-general.

**Q2 — PADDING. This is the one that must not be assumed.**

A wrong pad byte is **invisible until it is not**: blank-padded and
zero-padded fields compare equal on every prefix operation and differ only when
something reads past the source length. That is P48 F6's class — unobservable
at L1 on realistic data, caught at L2 only if the instruction does not match.

Six combinations, three fills × two sides:

| fill | side | where it would come from |
|---|---|---|
| blank `0x20` | right | PL/I fixed `CHARACTER`, the default rule |
| `0x00` | right | a buffer cleared rather than a field filled |
| `'0'` `0x30` | **left** | a PL/I numeric PICTURE, e.g. `PICTURE '9999'` |

**These may not be options within one construct — they may BE three different
constructs.** If so the question is *which does the program contain*, not
*which did the compiler choose*.

Find fixed-size destinations whose source is shorter and establish what the
bytes past the source become. **If it is always one kind, say so and we are
done. The point is that nobody has looked.**

**Q3 — does the destination's declared type alone select the semantics?**
Plausibly `varying n` stores a length word then moves `min(len, n)`, while
`char n` has no length word and pads or truncates. **Check whether the book
shows different instruction shapes for the two.**

**Q4 — what is `n` at a given site?** IR.md §5.8 says capacity with
`min(len, n)` semantics; P51's evidence (27/17/16 at one frame slot) suggests
the compiler **folds** `min(len, cap)` for a constant source. Capacity or
folded value — the answer decides whether a declaration supplies it or a
rewrite computes it.

**Q5 — is a count ever needed independently of the destination type?** If yes,
two forms are needed (assignment and a counted move). If no, one.

**Q6 — assignment semantics: SET the length, or overwrite a span?** The user's
stated preference is overwrite-span — `s = ""` copies zero bytes and does not
resize — and it is consistent with the machine's primitive, since `[@a, n] =
piece` is a counted copy with no length word. **But PL/I assignment sets the
length**, so a varying assignment may be that primitive *with a length store in
front*, making both readings right at different layers. **Settle it: is the
length store a separate statement or folded in?**

**Q7 — computed capacity.** Twins are `cap=8192 bound=unbounded size=>>2((N[W[
(fp-12)]]+6))` — a runtime expression over a parameter's length, with `cap`
a provisional number somebody picked. StringModel §4 proposes a `string` vtype
carrying a size **expression** rather than a constant. **Is that workable, and
is it needed for anything other than twins?**

---

## The ruling already made, and the principle behind it

**A string is a `v` with a `string` vtype. There is NO separate `s` class.**
(StringModel Q4, settled Sep 13.) Everything else in ir 8 follows *one storage
class, the type says what it is*.

**And the load-bearing principle: twinning is a CLONE ARTIFACT, not a program
feature.** The master pushes each concatenation intermediate with `WMSP`; the
clone cannot, so every claim needs a shadow address — hence `s@block.k`, the
arena, `claim`/`release`, the slot arithmetic. **A compiled routine has no
WMSP to shadow.** The naive form should model PL/I concatenation, not the
clone's shadowing of it — which reframes `quest.arena` as *a table the matcher
consults*, the way the addrbook already is.

**Test that principle against the census.** If it does not survive, say so.

---

## Scope — general enough, and NOT more

The prompt's title is the discipline. **A type that covers combinations the
program never uses is over-general**, and over-generality is not free: every
unused clause is surface the compiler must implement, the loader must check,
and a future reader must understand.

So for every clause you propose: **the count of sites that require it.** A
clause with zero sites does not go in. A clause with one site is named as
resting on one witness.

---

## Part 1 — PLAN GATE

`docs/Project57/q001-plan-gate.md`, push to main, **STOP**. Report:

1. **Your census method** — how you classify 1,689 sites without hand-reading
   them, and how you spot-check that the classifier is right
2. **A first pass at Q1's cross-product**, rough, so the shape is visible
   before budget is committed
3. **Which of Q2–Q7 you expect to be decidable from the book alone**, and which
   need `quest.dis` or `quest.mem`
4. **Anything in `StringModel.md` that already looks wrong**

STOP. Wait for `a001`.

---

## Part 3 — Report

`docs/Project57/StringCensus.md` — the counts, per question, with method.
Revised `docs/StringModel.md` — the specification, every clause carrying its
site count. `docs/Project57/REPORT.md` — what changed from the parked model and
why, **what you could not settle**, and the temptation register (carried from
P55 as standing practice: every place you wanted to generalise beyond the
evidence and did not).

---

## Boundaries — BINDING

1. **You may WRITE:** `docs/Project57/**`, `docs/StringModel.md`, and census
   tooling in `compiler/` (**new files only**).
2. **Do NOT modify** `lower_c.py`, `emulation/**`, `docs/IR.md`, `game/**`,
   `docs/Project44/DESIGN.md`, or any artifact. **Regenerate nothing.**
3. **Nothing executes.** This is a census over text.
4. If the census implies an IR change, that is a **recommendation in the
   report**, not an edit.

---

## Coordination

`docs/Project57/q00N-short-title.md`, push to **main**, then **STOP and tell
the user**. You own `q*.md`; the integrator owns `a*.md`. Every question states
what you found, the decision needed, the options with your read, and **your
RECOMMENDATION**. SOP: `docs/INTEGRATOR.md` §10.

## Delivery

**Push to `p57-stringcensus` at every stage boundary.** One `Work.tgz` with the
final report.
