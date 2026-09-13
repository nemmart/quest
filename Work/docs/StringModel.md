# The string model — DESIGN, NOT BUILT

**Status: PARKED DESIGN.** Worked out in design discussion Sep 13 2026.
**Nothing here is implemented and none of it is validated against a routine.**

**Why it is parked:** none of P51's seven routines concatenates. HIT_ANY_CHAR
passes two literals as arguments; GET_INPUT passes a `char buf[144]` by
address; the other five have no strings. So there is no `v = <string
expression>`, no `WCMV` with a computed count, and no twin anywhere in the
work that is live.

**What would unpark it:** C written for a routine that concatenates — DIED,
REFRESH_SCREEN or RETURN_MESSAGE. **Validate this model against that C before
building any of it.** The forms below are reasoned from the artifacts, not
derived from a routine.

---

## 1. The principle: twinning is a CLONE ARTIFACT, not a program feature

This is the load-bearing idea and it should survive even if every spelling
below changes.

The master evaluates `A || B || C` by pushing each intermediate onto its stack
with `WMSP`. The clone cannot push the master's stack, so **every claim needs a
fixed shadow address** — hence `s@<block>.<k>`, `quest.arena`, `claim` /
`release`, and the slot arithmetic asserted at release. That is an entire
subsystem built to make the clone track the master's stack allocation.

**A compiled routine has no such problem.** Its storage is in cells. There is
no WMSP to shadow, so there is nothing to twin.

**Therefore the naive form should model PL/I concatenation, not the clone's
twinning of it.** Our compiler should not inherit an artifact of the lockstep
mechanism.

**The consequence for matching**, which is where the difficulty actually sits:
the book's intermediate addresses have to come from somewhere. Either a rewrite
materialises them from `quest.arena` — derived data we already have, with the
size expressions in it — or those sites are normalised. **This reframes the
arena from "a thing the compiler must reproduce" into "a table the matcher
consults", the same category shift the addrbook made.**

## 2. What the arena actually shows

DIED's group at block 70166144, three twins, same base with a climbing
constant:

```
s@70166144.1   cap=8192  bound=unbounded  size = >>2((N[W[fp-12]] +  6))
s@70166144.2   cap=8192  bound=unbounded  size = >>2((N[W[fp-12]] + 29))
s@70166144.3   cap=8192  bound=unbounded  size = >>2((N[W[fp-12]] + 31))
```

An accumulating concatenation — `X || lit6`, then `|| lit23`, then `|| lit2` —
built as **three separate allocations at three distinct arena addresses**
(75000000, 75002000, 75004000), each copying the previous result plus the new
piece. Blocks carry **up to 5** twins; there are 57 in total.

`cap=8192` is a **provisional number somebody picked**, not a derived maximum:
`bound=unbounded` with the real size a runtime expression over a parameter's
length. Some are `bound=exact size=8`.

## 3. The proposed form

Positional. **There is no separate `s` class** (question 4): a string is a `v`
with a `string` vtype, so the forms below are one set, not two. `s` is written
below only where a twin is meant.

```
s + <u32> = "literal"        overwrite starting at that byte offset
v + <u32> = "literal"        same
s          = "literal"       means  s + 0 = "literal"
                             so `s = ""` copies ZERO BYTES and does NOT
                             resize (user's stated preference, Sep 13; this
                             is the overwrite-span reading of question 5 and
                             is NOT yet checked against the book). It is
                             consistent with the machine's own primitive —
                             the located form `[@a, n] = piece` IS a counted
                             copy with no length word. A varying assignment
                             may then be that primitive with a length store
                             in front, in which case both readings are right
                             at different layers: the IR primitive overwrites
                             a span, the STATEMENT sets the length.

s += "literal"               append
v += "literal"               append
s += <u32>                   extend the length by N
v += <u32>                   extend, blank-filling
```

**`+=` is disambiguated by the OPERAND type, not the destination's** — a string
operand appends, a numeric operand extends. That keeps `s` and `v` coherent
under the same spelling.

## 4. Dense assignment, located form by rewrite

The naive form says what the C says; the machine's spelling is earned by a
rule — the same principle as dropping `ac` staging (§ P57).

```
naive:    v2 = "\vHit any character to continue"
matched:  [@0x740056FA, 0x1E varying] = [@0x7016DE07:0, "\vHit…"]
```

Two oracle-supplied facts and nothing else: `place v2 → frame+4`, and the
literal's image address. P56's R4 already does the relocation half.

## 5. Open questions — ALL need evidence, none should be guessed

1. **Does the destination's declared type alone select the semantics?**
   Plausible: `varying n` stores a length word then moves `min(len, n)`;
   `char n` has no length word so pads or truncates to a fixed field. **Check
   whether the book shows different instruction shapes for the two.**
   HIT_ANY_CHAR may already contain the contrast — a 30-byte literal into a
   varying temp, and `ch` as `CHAR(1)`.
2. **Is a count ever needed independently of the destination type?** If yes,
   two forms are needed (assignment and a counted move); if no, one.
3. **What is `n` in `[@dst, n varying]` at a given site?** IR.md §5.8 says
   capacity, with `min(len, n)` semantics; P51's evidence (27/17/16 at one
   frame slot) suggests the compiler **folds** `min(len, cap)` for a constant
   source. If it is the folded value, the rewrite computes it; if it is the
   capacity, the declaration supplies it.
4. **~~Why are `s` and `v` separate classes at all~~ — SETTLED (Sep 13): use
   `v` with a `string` vtype, not a separate `s` class.**

   Everything else in ir 8 follows *one storage class, the type says what it
   is* — `v QUEST.v0 i16`, `*char`, `char 30`, `varying 27`, `words 12`. A
   separate `s` would make strings the only thing whose CLASS encodes its
   nature rather than its type, and by §3's model `s` and `v` take the same
   operations with the same disambiguation. The remaining differences are no
   declared capacity, and a different home — and the home is a PLACEMENT
   fact, which the design already treats as separate from the class
   (§5.1's place-vs-eliminate split).

   **So the real distinction is fixed capacity vs COMPUTED capacity**, which
   is the twins' `size=>>2((N[W[fp-12]]+6))` case that no current vtype can
   express. A `string` vtype carrying a size EXPRESSION rather than a constant
   covers twins without a new class.

   *Still to check:* whether a size expression in a declaration is workable at
   all, given it references a parameter's runtime length.
5. ~~(was: why separate classes)~~ **Does an assignment SET the length, or overwrite a span?**, if they behave identically
   under these operations? Candidates: no declared capacity on `s`, and a
   different home. Neither has been tested.
5. **Does an assignment SET the length, or overwrite a span?** They differ,
   and the positional form makes the difference visible:
   - `s = "ab"` onto a 10-byte `s` — length 2, or length 10 with the first
     two bytes replaced?
   - `s = ""` — a reset to zero length, or a zero-byte write that changes
     nothing?
   - if assignment sets the length, then `s + 5 = "ab"` cannot mean the same
     thing, and one spelling means *assign* at offset 0 and *splice*
     elsewhere.

   PL/I assignment sets the length, so the first reading is likely — but that
   is an inference, and the splice case then needs its own justification.
   **Settle it against a real routine's disassembly.**
6. **PADDING FOR FIXED-SIZE DESTINATIONS — check this, do not assume it.**
   A wrong pad byte is **invisible until it is not**: a blank-padded and a
   zero-padded field compare equal on every prefix operation and differ only
   when something reads past the source length. That is P48 F6's class of bug
   — unobservable at L1 on realistic data, caught at L2 only if the
   instruction does not match.

   **Six combinations**, three fill bytes × two sides:

   | fill | side | where it would come from |
   |---|---|---|
   | blank `0x20` | right | PL/I fixed `CHARACTER` — the default rule |
   | `0x00` | right | a buffer being cleared rather than a field being filled |
   | `'0'` `0x30` | **left** | a PL/I numeric PICTURE, e.g. `PICTURE '9999'` |

   **These may not be options within one construct — they may BE three
   different constructs.** `CHAR(n)` blank-right, a picture zero-left, a
   buffer 0x00 or unpadded. If so the question is not "which padding did the
   compiler choose" but "which of these does the program contain", and that is
   a census rather than a design decision.

   **If it is always one, we are fine and the model needs nothing.** The point
   is that nobody has looked.

   What would settle it: a `[@a, n]` site — **non-varying** — whose source is
   shorter than `n`, and what the bytes past the source become; whether
   `WCMV`'s two counts ever differ in the book and what the executor does with
   the gap; and `HIT_ANY_CHAR`'s `ch`, a `CHAR(1)` that already matched — if
   padding never arose at n=1 that tells us nothing, and knowing which is
   which matters.
7. **Twin capacity is computed, not fixed**, so a declaration cannot always
   carry a constant — "sized at claim time" may be needed.

## 6. Also parked with this

**`s@<block>.<k>` → an entry-qualified `v` with a `string` vtype** (question 4
settles the class; this is the spelling). Everything else is entry-qualified
(`QUEST.v0`, `QUEST.a1`, `QUEST.b3`); `s@` is keyed on a block address, which
is position-dependent in the way DESIGN §7.4 warns about. **But the `.k`
carries the group structure**, and the group matters for the sizing arithmetic
— so the rename needs the arena file to carry grouping first. Same shape as
P52's `t@` → `s@` (236 tokens, proven token-only by reverse substitution);
57 twins here.

---

## Cross-references

`docs/IR.md` §5.8 (string statements), §5.9 (arena twins, claim/release) ·
`emulation/quest.arena` · `docs/Project29/StringsDesign.md` (design of record
for the string family) · `docs/Salvage.md` F16 (twin sizing) ·
`docs/Project51/REPORT.md` §2 (what the seven do and do not need) ·
`docs/Project56/Rewrites.md` R3, R4
