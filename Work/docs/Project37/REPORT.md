# Project 37 — proving the constructs across several routines: REPORT

Date: Wednesday, September 9, 2026.
TREE VINTAGE: `main` commit **d7e27a7** (the P37 prompt commit, sitting on the
P36 merge 9f33953 — the prompt's "9f33953 or later" is satisfied).  The three
tarballs the session was handed were verified byte-identical to HEAD
(`diff -rq` clean across `Work/`, `Disassembled/`, `QUEST/`).
Work on branch **`p37-routines`**, head `5038015`, pushed.
Provenance: `ir2.book e2f18f14…`, `ir2.stock c4340b49…`, `arena 64ab09d4…`,
`addrbook e6fde2c2…` — all match their current rows in `docs/Provenance.md`.
**Nothing in `emulation/` or `Disassembled/` was modified**; no artifact, no
battery.  Files written: `compiler/`, `game/`, `docs/Project37/` only.

**PRAGMA COUNT: 0** — reported next to every match number below, as required.

---

## 1. Result in one paragraph

One routine completed (**OWNS, 152/152 MATCH primary, 114/114 folded**), two
staged with their constructs derived but deliberately not fitted, one routine
dropped as out of scope with a finding that outgrew the project, and the
project's chartered question answered: **the frame "relocation" does not exist,
and the pragma allowance is not needed.**  Fourteen rules were added, two were
amended, one was falsified and recorded as such, and three prior beliefs were
voided as inadmissible evidence.  The three P35 routines stayed **197/197
primary, 128/128 folded** with `--selftest` PASS after every rule change, and
all six routine sources compile clean under `gcc -std=c99` and `g++ -std=c++17`
with `-Wall -Wextra -Werror`.  P37 did not reach INIT_OBJ_TBL, the random
routine or DIED; what it delivers instead is a much better-founded model and
two discoveries about the program that change how earlier work must be read.

---

## 2. Per-routine table

| # | routine | stmts | result | pragmas |
|---|---|---|---|---|
| — | PICK_X_Y | 64 | 64/64 primary, 41/41 folded (never regressed) | 0 |
| — | UPDATE_SCREENS | 72 | 72/72 primary, 44/44 folded (never regressed) | 0 |
| — | REFRESH_SCREEN | 61 | 61/61 primary, 43/43 folded (never regressed) | 0 |
| 1 | **OWNS** @70175CBF | 152 | **152/152 MATCH primary, 114/114 folded** | **0** |
| 2 | GET_INPUT @7016AA35 | 22 | STAGED — refuses at the `BITS()` argument.  No match number. | 0 |
| 3 | RETURN_MESSAGE @70176FDD | 30 | STAGED — refuses at the VARYING accessors.  No match number. | 0 |
| 4 | FIRE.1 @7016A3BD | 89 | Rule DERIVED, translator change NOT built.  **No match number.** | 0 |
| — | UNLOCK_FILE | 38 | DROPPED at the plan gate — hand-assembly, out of scope by definition | — |
| 5 | INIT_OBJ_TBL | 173 | not reached | — |
| 6 | TRANSPORT_SUNDAR | 138 | drawn and pre-registered; not translated | — |
| 7 | DIED | 495 | not attempted; pre-registered as an experiment (§7) | — |

**Totals: 349/349 statements MATCH across four routines, 242/242 folded, 0
pragmas, 0 DIFF.**

### 2.1 OWNS — the completed routine

```c
int16_t OWNS(const int16_t *p, const int16_t *item)
{
    int16_t i;
    for (i = 1; i <= 10; i++) {
        if (PLAYER[SUB(*p, 10)].fm390[SUB(i, 10)] != *item) continue;
        return -32768;
    }
    if (*item == 8)   return BIT(PLAYER[SUB(*p, 10)].fm591, 15);
    if (*item == 3)   return BIT(PLAYER[SUB(*p, 10)].fm590, 0);
    if (*item == 106) return BIT(PLAYER[SUB(*p, 10)].fm591, 14);
    if (*item == 105) return BIT(PLAYER[SUB(*p, 10)].fm590, 5);
    if (*item == 112) return BIT(PLAYER[SUB(*p, 10)].fm590, 2);
    if (*item == 111) return BIT(PLAYER[SUB(*p, 10)].fm590, 4);
    if (*item == 114) return BIT(PLAYER[SUB(*p, 10)].fm590, 3);
    return 0;
}
```

Slot bijection identity (w2→2, w4→4).  Rules exercised: R1–R5, R7, R7c′, R8,
R8c, R11, R13, R13b, R17, R21, R21a, R21b, R21c, R23r, R23a, R26, R26a, R27
(amended), R28, R29, R35, R35a, R36, R37.  Comparison reports in
`docs/Project37/results/`.

---

## 3. The rule ledger

### 3.1 Promoted

| rule | from | to | on what |
|---|---|---|---|
| R26, R27, R28, R29 | B/A | **A** | OWNS' seven WSZB tests, a second independent routine |
| R21c | *new* | **A** | four witnesses (OWNS + all three P35 loops) |
| R35 | *new* | **A** | three witnesses, both widths (OWNS 16-bit, DISTANCE_TO_PLAYER 32-bit, 701703AA 16-bit) |
| R37 | *new* | **A** | OWNS, FIRE.1, INIT_OBJ_TBL |
| R38 | *new* | **A** | both direct X.CB call sites |
| R40 | *new* | **A** | two independent multiple-ENTRY pairs |
| R39 | *new* | **A** | the mutual-crossing signature, plus a completed sweep |

### 3.2 New at B or C

R35a (A), R23r (B), R23a (B), R8c (B), R2a (B), R33 (B), R34 (B),
**R36 (C — one witness)**, **R7c′ (C — release point fitted to one routine)**.

### 3.3 Amended

- **P36 ruling 1** — R16b does not reach a literal that already fits in 16 bits:
  `NLDAI` sign-extends and the store is a bare `trunc16`, with no `CVWN`,
  because there is nothing to check at run time.  The refusal stands for every
  non-literal.
- **R27** — *"a later use counts only if the flow can REACH it."*  Two
  statements in different arms of an `if` are not later statements for one
  another.  This is the best rule the project produced: it derives both DIED's
  nine consecutive WBTZs saving a `16*scaled` temp **and** OWNS' seven
  mutually-exclusive arms each recomputing, from one principle.  Implemented as
  `reachable_later_uses` over an arm-path pre-pass.

### 3.4 Falsified

- **R7c** — *"a variable's register is protected until its last reachable use,
  anywhere"*, the natural generalisation of R7b past the statement boundary.
  Predicted at the plan gate, built, and refuted: it fixed **no** OWNS DIFF and
  regressed **UPDATE_SCREENS from 72/72 to 47/72 (25 DIFF)**, because `*x` and
  `*y` are read by later *sibling* statements there and the compiler plainly
  does not protect them.  R8b stands as P35 wrote it.  Only the narrow **R7c′**
  survives.  Recorded per METHOD §11.

### 3.5 Voided as inadmissible (R39)

1. `RETURN_MESSAGE mixed:3/6` — the 3-argument caller is hand-assembly.
2. **`WSAVR` as a compiler entry variant** — two occurrences in 102 live
   entries, both hand-assembly.  A false rule nobody had asserted yet, sitting
   there waiting to be inferred.
3. `LOCK_FILE frame 0x01 dyn,push` / `UNLOCK_FILE frame 0x00` as evidence about
   frame layout.

### 3.6 R10 — still no verdict

Unchanged from P36, and honestly so: R10 needs both 701660F8 and 70166110
translated, which needs DIED, which needs the frame model and the twins.  P36's
reading stands — R10 as stated is falsified and the discriminating variable is
ac2 contention.  Confidence **C**.

---

## 4. The two findings that outgrew the project

### 4.1 R39 — inadmissible evidence: what hand-assembly contaminates

`docs/Project37/InadmissibleEvidence.md`.

> Not every addrbook entry is compiler output.  An observation taken from, or
> about, a hand-assembly routine is INADMISSIBLE as evidence about the PL/I
> compiler — and the contamination travels: it reaches the addrbook metadata of
> routines that are themselves ordinary compiled code.  Cite as *"void under
> R39."*

**Direction matters, and it is what makes the rule usable rather than a blunt
exclusion.**  Contamination flows *outward from an assembly call site*, not
inward to one: LOCK_FILE's `argc 2` remains admissible because it was inferred
from compiled call sites in SIGNAL_TURN.  The test is **"is the hand-written
side producing the behaviour I am about to generalise?"**

### 4.2 R40 — PL/I multiple ENTRY points into one procedure

A construct, not a rule.  Two independent pairs — CREATE_MAP/DISPLAY_MAP and
TRANSPORT_TERRAK/TRANSPORT_SUNDAR — each with the same frame, the same argc, a
real `WSAVS` at every entry, each entry initialising the same frame slots with
different constants before branching one-way into a shared body.

**Distinguishing it from R39 matters**, since both show as cross-family control
flow: R39's assembly crossing is **mutual** and the two sides differ in frame,
argc and entry variant; R40's is **one-way from a prologue into the other
entry's body**, with everything else identical.

---

## 5. The random routine (§6 of the prompt)

Method fixed and published at the plan gate **before** the draw; `draw.py`
committed as `a6c8586` **before** the drawn routine was inspected.  Frame: 27
live entries in the 50–150 statement band; pool 23 after excluding the four
already matched or targeted; LOCK_FILE deliberately left in.
`random.Random(37).choice(pool)` → **TRANSPORT_SUNDAR @7017D492**.

It was not translated.  But it produced the project's best vindication of the
method anyway:

**The drawn routine is one half of an R40 pair.**  Its 138 statements are its
own two-instruction prologue plus a body it shares with TRANSPORT_TERRAK, whose
prologue sits outside the slice.  It cannot be translated as a self-contained
procedure; the honest reconstruction is **one C function with two entry
prologues, compared against the union of both addrbook ranges** — now a known
shape rather than a surprise waiting for whoever tried it.

A hand-picked set would not have surfaced this.  Every routine anyone chose
deliberately — including all six I chose at the plan gate — was a single-entry
procedure.  The one routine nobody chose was not, and it exposed a construct
that silently mis-describes four addrbook entries.  **That is what the honesty
test is for, and it earned its place on the first draw.**

---

## 6. The frame relocation — the chartered question, answered

`docs/Project37/FrameRelocation.md`.  **Verdict: no `#pragma fp ac2`.**

P36 finding 1 described DIED 70166376..701663B3 as *"a register reassignment of
the frame pointer"* and *"a structural change [that] touches every emit site"*.
Reading **FIRE.1 first** — the small case, which is why the plan put it there —
and only then re-reading DIED makes a simpler story available that fits both:

> **There is no relocation.**  The frame pointer is not a thing that moves.  It
> is an ordinary VALUE in the register model, materialised on demand by `LDAFP`
> into whatever register R7 leaves free, living exactly as long as any other
> cached value would under R7/R8.  `ac3` is its default holder and is otherwise
> an ordinary allocatable register.  (**R33**; **R34** for the `WPSH 3,3` /
> `WPOP 3,3` preservation.)

DIED's 22 `wp(ac2, 8)` references are not a long-range phenomenon.  Through its
run of R29a bit assignments ac3 holds the record base — R28 *already* said one
`LWLDA` serves the whole run — ac1 holds the bit offset and ac0 the bit value.
ac2 is the only register left for the frame, and nothing in those ten blocks
wants ac2 for anything else.  **Long lifetime is an absence of register
pressure, not a policy.**  FIRE.1 and DIED differ only in how soon ac2 is
reused.

This **dissolves** the pragma question rather than answering it: the phenomenon
that motivated the allowance does not exist.  The plan gate's stated condition
for reaching for a pragma — a site where ac3 takes a base while a cheaper
register is free, with no discriminating variable — does not occur anywhere.

### 6.1 A methodological note, recorded here for future sessions

> **The LDAFP target was `ac2` in every observed instance, and this must NOT be
> coded as a rule.**  In each of those instances ac2 was also the only register
> R7 could have picked, so the observation carries no independent information
> about the compiler's preference.  Encoding it would manufacture a rule out of
> a forced choice.
>
> This is the `WSAVR` lesson (§3.5) applied prospectively rather than
> retrospectively: an observation that is *consistent* with a rule is not
> evidence *for* it unless the rule could have been violated.  Before promoting
> any uniform observation, ask what else could have happened — and if the answer
> is "nothing", there is no rule there.

### 6.2 Not implemented, stated plainly

`Regs` still models ac0–ac2 with ac3 pinned.  Making ac3 allocatable and
threading a "where does the frame live" state touches every emit site; P36's
estimate of the **size** of that change was right even though its description of
the **phenomenon** was not.  It was deliberately not started at the end of a
long session, because a structural change to the register model made in a hurry
against a routine as intricate as FIRE.1 is exactly how a fitted rule enters the
ledger.  **P37 delivers the rule and the verdict on routine 4, not the routine.**

---

## 7. Handoff — DIED as a pre-registered experiment

`docs/Project37/DIED_PREREGISTERED.md`, to be read **before** translating DIED.

DIED's value is no longer "the capstone" but **"the falsifier for four
one-witness beliefs"**.  Each is written down with what it predicts and what
would refute it, before the test, so that agreement is a result rather than a
description of tuning:

| belief | predicts | most likely outcome |
|---|---|---|
| **R29a** (bit assignment, B) | DIED's 5 WBTO / 15 WBTZ are set-then-undo | confirmation |
| **R36** (loop-invariant hoist, C) | an invariant body subscript is evaluated at the loop head | confirmation, or a *weak* refutation if neither DIED loop has one — which is still a result and must be stated |
| **R7c′ release point** (C) | the pin is released at the stride multiply | **flagged in advance as the most likely refutation** — it is the most fitted thing in the set |
| **the join self-move** (recorded, not ruled) | a redundant `WMOV r,r` at a join whose arms agree | unknown |

Plus the three **predicted** arm-path defects (CODEGEN_RULES §8.8) — R9's, R10's
and R3's later-use tests still ask the question the bare way R27 did before its
amendment.  Deliberately not fixed speculatively; if DIED or INIT_OBJ_TBL trips
one, it is a prediction confirmed rather than a discovery.

---

## 8. Findings list (the frontier)

1. **UNLOCK_FILE and LOCK_FILE are hand-written assembly**, and are one unit,
   not two routines — they branch into each other's ranges.  Out of scope for a
   compiler model by definition, and a source of false evidence (R39).
2. **`WSAVR` is not a compiler entry variant** (2 of 102 entries, both assembly).
3. **R40: PL/I multiple ENTRY points**, and the addrbook statement counts that
   mis-describe them.
4. **A PL/I BIT literal is never constant-folded** (R38).  `'001'B` is rebuilt
   at run time by `X.CB` at every evaluation.  This is a fact about 1986 PL/I
   codegen, not about the translator, and it will shape how any bit-heavy
   routine reads.
5. **PL/I CHARACTER is an unsigned byte** — caught by the native compile check,
   which the comparator structurally could not have caught (see §9).
6. **RETURN_MESSAGE is the game's fatal-error exit**; its default message is the
   literal at 0x70000CCD, *"Unexpected error"*, whose 16 bytes independently
   confirm the `NLDAI 16` default length.
7. **The two mixed-arity spellings cannot be separated — CLOSED, not open.**  A
   census shows the program contains exactly two `mixed:` routines, so no third
   instance exists and any binary discriminator fits.  A negative finding that
   closes the question rather than leaving it hanging.
8. **The compiler optimises, once** (R36): loop-invariant subscript evaluation
   is hoisted to the loop head.  The first optimisation observed in this
   compiler, and still at one witness.

---

## 9. The native compile check's first independent catch

Worth recording because it justifies the cost.  The first GET_INPUT draft used
`char`; both `gcc -std=c99` and `g++ -std=c++17` rejected `if (*c > 128)` as
*"comparison is always false due to limited range of data type"*.  The book
settles it — the byte is loaded with `WLDB` (zero-extending) and compared
`>s 128` — so PL/I CHARACTER is unsigned.

**The comparator could not have found this.**  GET_INPUT refuses before reaching
that statement, so nothing would have been compared; a wrong signedness would
have sat in the source of record until it silently mis-translated something
later.  The two checks are not redundant — they fail in different directions.

---

## 10. Assessment: how many of Quest's ~130 routines could be attempted today

**Honestly: about 20–25, and none of the large ones.**

Attemptable now (the subset covers them): routines whose bodies are arithmetic,
subscripts, DO loops, IF forms, runtime calls with dummy arguments, string
temporaries, literal assignment, bit references, record string fields,
game→game calls, indexed field reads, and value returns.  On the P37 census that
is most routines in the 20–90 statement band that contain no `ac2 = wfp`, no
`claim`, no float and no raw syscall.

**What stands in the way of the rest**, in order of how many routines it blocks:

| blocker | routines affected | state |
|---|---|---|
| the frame model (ac3 allocatable) | 25 contain `ac2 = wfp`; far more contain `ac3 = wfp` | **rule derived (R33/R34), not implemented** — the single highest-value next task |
| arena twins / claim groups | ~20 statements in DIED, 2 in INIT_OBJ_TBL, others | designed (P36 finding 2 + the INIT_OBJ_TBL sharpening), not built |
| floating point | DISTANCE_TO_PLAYER and the math subtree | untouched; a whole subsystem |
| raw SYSCALL statements | RETURN_MESSAGE and every I/O routine | not in the subset |
| the multi-dummy temp allocation order | every call with a call-materialised argument | **open, and named as a search target** |
| 32-bit bitwise ops, VARYING accessors, byte pointers | scattered | small, mechanical |
| R40 multiple-entry procedures | 4 entries (2 pairs) | shape known, not built |

The frame model is the bottleneck and it is now *derived*, which is the
difference between P36's position and this one: the next session implements a
rule rather than investigating a phenomenon.

---

## 11. Recommendations

1. **Run `docs/Project37/crossings.py` at the start of any session that adds
   routines**, not once.  It is sub-second, and both things it found — a
   contaminated metadata flag and an entire unmodelled construct — were
   invisible to every other check in the project.  *(To be folded into METHOD
   §16 alongside R39 at merge.)*
2. **Re-check any prior work that used per-routine statement counts from the
   addrbook.**  R40 invalidates a class of them: TRANSPORT_TERRAK counts 2 and
   TRANSPORT_SUNDAR 138; CREATE_MAP counts 7 and DISPLAY_MAP 902.  Known
   affected: **P34's census** and **P37's own routine-selection census** (the
   sampling frame for the draw included TRANSPORT_SUNDAR at a size that is not
   its own).  Any census, sampling frame or size-ordered worklist built on those
   counts inherits the error.
3. **Add METHOD §16** for R39 and the crossing detector.  P37's boundaries did
   not permit editing `docs/METHOD.md`; the companion text is in
   `docs/Project37/InadmissibleEvidence.md`.
4. **Implement R33/R34 first next session**, before any new routine.  It unblocks
   the most routines, the rule is already derived, and FIRE.1 is the small case
   to validate it against.
5. **Read `DIED_PREREGISTERED.md` before touching DIED**, and treat a
   contradiction as a finding rather than a tuning cue.
6. **Watch for a BITS()-shaped argument** — a call mixing a call-materialised
   argument with plain dummies — in every routine ahead.  It is the one thing
   that settles the GET_INPUT open question.  DIED's eight call sites are the
   first place to look.

---

## 12. Deliverables and commands

Branch `p37-routines`, eight commits, pushed to `origin`:

    a6c8586  the random draw, recorded before inspection
    7d3d24d  routine 1: OWNS 152/152
    0336344  routine 2: GET_INPUT staged; X.CB / R38
    70947a3  the three predicted arm-path defects
    15538a2  routine 3: RETURN_MESSAGE staged; the hand-assembly contamination
    202d1fe  R39 with its sweep, and DIED pre-registered
    bae97c1  the crossing detector run standalone; R40
    5038015  routine 4: the frame relocation derived; no pragma

    python3 compiler/gen_declarations.py
    python3 compiler/translate.py game/routines/OWNS.c --routine OWNS -o out.ir
    python3 compiler/ircmp.py emulation/quest.ir2.book out.ir --routine OWNS [--folded]
    python3 compiler/ircmp.py --selftest --book emulation/quest.ir2.book
    python3 docs/Project37/draw.py          # reproduces the routine-6 draw
    python3 docs/Project37/crossings.py     # the R39 detector

Runtimes: translate.py 0.05–0.2 s per routine; ircmp.py ~1.1–1.7 s per
comparison; `--selftest` ~1.2 s; `crossings.py` ~1 s.
