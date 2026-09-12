# Project 52 — REPORT: ir 8, the variable form, pointer types and argument cells

Worker session, Sep 12 2026, branch `p52-ir8`. Gate `q001-plan-gate.md`,
rulings `a001-plan-gate.md` (R0 split the project, R4/R5/R6 granted, G-4
added) and `a002-stage-b-released.md` (R8 granted, Stage B released,
R1/R2/R3/R7 confirmed). Follow-up `q002-f5-wording-and-rename-boundary.md`.

**Outcome.** `docs/IR.md` is ir 8 (1,144 → 1,630 lines). The loader
implements it (`IRExec.cpp` +421/−88, `IRExec.hpp` +28/−4). The self-test is
**GREEN, 129 cases**, with a teeth leg for every refusal and the
broken-allocator build RED on the disjointness assertion. The `RANGE_CHECK`
rename landed across `game/` with a proof stronger than the `sed` yardstick.
**No artifact was regenerated** — `quest.ir2.book` and `quest.ir2.stock` load
unchanged through the ir 8 loader. Calls out of a symbolic block LOAD and
VALIDATE; they do not execute, and say so by name.

---

## 1. The grammar as landed

All of §1 of the gate landed as proposed, with the rulings folded in. The
normative text is `docs/IR.md` §5.10; this is the index.

| construct | where | note |
|---|---|---|
| the variable form — a cell name is its CONTENTS, width and sign from the declaration | §5.10.4 | both a primary and an lvalue |
| `wp`/`bp` resolve by operand kind | §5.2 | static, in the parser; no `&` operator needed |
| pointer vtypes `*i16 *u16 *i32 *u32 *char` | §5.10.1a | one level, 2 words, KIND enforced / pointee width advisory |
| **`*varying <n>` and `*words <n>`** (a001 R5) | §5.10.1a | 28 census sites need them |
| **initialised `v`** `char <n> = "text"` (a001 R6) | §5.10.1b | the gate's blocking finding |
| `a <ENTRY>.a<N>`, `a <ENTRY>.arg_count u16` | §5.10.1c | contiguous from 1; one placement cursor with `v` |
| **`a <ENTRY>.ret <vtype>`** (a001 G-4) | §5.10.1d | Salvage F4's slotpatch, declared now rather than retrofitted |
| the KIND tripwire, replacing ir 7's width tripwire | §5.10.5 | ir 7's rule asked a question ir 8 no longer poses |
| `call`/`rt_call` in a symbolic block with `ret=<ENTRY>.b<k>` | §5.10.6, §6 | **two** argument mechanisms, not one |
| the valued-call split as a subset rule | §5.10.6 | a call cannot sit inside a larger expression |
| `trunc8` | §5.1, §5.3 | an alias of `zx8` — a name, not new expressiveness |
| the widened `rt_call` callee rule (`X.CB`) | §6 | non-`?` callee + EMPTY argument list |
| the bit-31 guarantee and the SCOPED literal rule | §5.10.8 | derived from `wp`/`bp`, not assumed |
| the ir 7 compatibility window | §5.10.9 | accept `ir 7` iff no `v` and no `a` |

---

## 2. The two censuses

Both are in `docs/IR.md` §5.10.8, which is where they belong — they are
facts about the program, not about this project. Summary and the numbers
that differ from the prompt:

**Census 1, the `R[]` sites of `quest.ir2.book`.** 1,032 lines, **1,035
occurrences** (stock 914/917). 939 own-frame argument slots (two of them
spelled `R[wp(ac3,-12)]`), **41 uplevel** argument slots through the static
link, **27** statics, **28** positive frame offsets (four spelled
`R[wp(ac3,d)]`). **Non-argument total 55, not the prompt's 49**; the prompt's
three static counts are individually right and sum to 27, not 25, and four
sites hide in the `wp()` spelling. "1,032 total" was a line count, exactly as
P46's "198 sites" was.

Three findings the spec keeps:

1. **The positive-offset cells ARE written — 42 writes across 11 cells**,
   twelve of them to DIED's slot +8. The prompt asked whether a local's
   address value being assigned was "a case this design assumed away". It is
   not: it is what `<ptype>` and `*varying <n>` are for, and it *strengthens*
   §4 of the prompt, because the routine builds those addresses from
   `wp`/`bp` and the rewrite's precondition is therefore provable there too.
   **1,008 of 1,035 sites are provable; only the 27 statics are assumed.**
2. **The 41 `R[ac2 + -d]` sites are a different rewrite.** Every one is in a
   nested `.N@` entry preceded by `ac2 = M32[wp(ac3,-6)]` — Salvage F2's
   triple indirection. In the naive form they are free (DESIGN §4.1a); their
   L2 rewrite reintroduces the static link. So "one rule, many applications"
   covers 994 sites, not all of them.
3. **One level, everywhere.** No `R[]` contains another in either artifact.

**Census 2, the optional-argument mechanism.** `Salvage.md` F5 recorded two
spellings; the second is not an optional-argument mechanism. RETURN_MESSAGE's
null test is on argument 3, supplied at both arities; the body reads only
arguments 1–3 and never touches 4–6; all five call sites supply a non-null
argument 3, so the default-message arm is dead code. It is a null-POINTER
test on a supplied by-reference argument. **Exactly one mechanism exists —
REFRESH_SCREEN's `arg_count` read — and it rests on one witness.** F5 is
corrected on `main` (integrator, `1039481`), with the F6 annotation that the
hand-assembly objection was never load-bearing.

---

## 3. What P51's seven routines still cannot say — and what changed

The gate found four gaps (G-1..G-4) by reading all seven. Three are closed
by rulings and are in the spec; one was closed by the prompt's own §6 once
the two mechanisms were separated.

| gap | status |
|---|---|
| **G-3, the blocker** — a string literal not already in the image cannot be spelled (2 of HIT_ANY_CHAR's 3 statements; GET_INPUT's `BITS("001")`) | **CLOSED** by the initialised `v` (a001 R6, §5.10.1b) |
| G-2 — no pointer-to-string vtype, and `check_piece` closed the workaround | **CLOSED** by `*varying n`/`*words n` (a001 R5) |
| G-1 — no production for a call's arguments | **CLOSED** by stating two mechanisms (a001 R4, §5.10.6) |
| G-4 — the valued game call had no home | **CLOSED** by `<ENTRY>.ret` (a001 G-4, §5.10.1d) |

**Nothing in the seven is now inexpressible in ir 8.** That sentence is
about the IR only. It is *not* a claim that P53 can compile them, and §4
says what stands in the way.

**Recorded from the gate, unchanged:** `trunc8` was never a capability gap —
`zx8` is already `& 0xFF` and an M8 store truncates by rule, so GET_INPUT's
`(unsigned char)(*ch - 128)` was expressible all along. It landed as an
audit-trail name and the loader maps it to the same node as `zx8`.

---

## 4. What remains before a call actually runs

ir 8 SPECIFIES and VALIDATES a call out of a symbolic block. It does not
execute one, and the executor says so by name rather than transferring to a
pc that does not exist (self-test leg 3b). Outstanding, all P50/P53:

1. **The LCALL replica** — a game→game transfer with no LCALL word to
   execute: the frame push, the marker, `ac3` = the return pc, and the
   return landing on the symbolic `ret=` label.
2. **The `rt_call` bridge from a symbolic block.** The argument *pushes*
   already work (they are unchanged), but the LCALL at a `site=` is what
   currently drives `RTBridge`; a symbolic call has no site, so the callee
   must be dispatched from the symbol rather than from the instruction.
3. **`arg_count` and `<ENTRY>.ret` have no reader yet.** The loader checks
   they are declared and that arities agree; nothing consumes them at run
   time until the bridge exists.
4. **The compiler.** `lower_c.py` still emits `ir 7` with `v` names in the
   old meaning (`lower_c.py:1025`), so its output is refused — loudly, which
   is correct. The ir 8 emitter is P53.
5. **`run_lowerc_difftest.sh` is EXPECTED RED and gated** (q001 R10, granted
   in a002), with a banner naming both causes and
   `QUEST_DIFFTEST_EXPECT_RED=0` to re-enable. Cause 1 is item 4 above.
   **Cause 2 is a boundary item the gate did not foresee:**
   `compiler/difftest/cases/{sub_traps,shortcircuit}.c` still call `SUB()` —
   6 tokens in 2 files, outside a002/R8's eight granted lines. I did not
   rename them, because doing so would not restore green while cause 1
   stands, and spending boundary for no green is the wrong trade. **P53
   should rename those 6 tokens in the same push as the emitter.**

Also left untouched deliberately, being outside the eight granted lines:
two stale docstring mentions of `SUB()` at `lower_c.py:627` and
`cgen.py:154`.

---

## 5. The `RANGE_CHECK` rename — diff evidence

`game/quest_rt.h` and 13 `game/routines/*.c`. Three token pairs:

| token | before | after |
|---|---|---|
| `SUB` | 118 | 0 |
| `RANGE_CHECK` | 0 | 118 |
| `SUBs` (prose plurals) | 2 | 0 |
| `quest_sub_check` | 3 | 0 |
| `quest_range_check` | 0 | 3 |

**Proof, stronger than the `sed` yardstick the prompt asked for:**
reverse-substituting the three pairs in the 14 new files **reproduces the 14
originals byte for byte, 14/14**. `git diff --numstat` shows equal adds and
deletes in every file, so no line moved, split or merged. `WSUB`/`NSUB`
mnemonics in the comments are untouched by `\bSUB\b` and are still present.
**Behaviour-neutral in the native view:** `g++ -fsyntax-only` error counts
are identical before and after for all 15 files. a002 asked that this method
be used for future renames.

`quest_sub_check` → `quest_range_check` was a second token the prompt did
not name; flagged in q002 and confirmed in a002 ("leaving the helper would
preserve the word the rename exists to remove").

**Granted compiler-side lines (a002/R8), all applied:** `lower_c.py`
:681/:730/:733, `difftest/cgen.py` :159/:161,
`difftest/make_update_screens_fixture.py` :106/:107/:156.

---

## 6. Sizing: estimate vs actual

| | gate estimate | actual |
|---|---|---|
| `IRExec.cpp` | ~350 lines, of which ~25 executor | **+421 / −88**, of which ~13 executor |
| `IRExec.hpp` | ~30 | **+28 / −4** |
| self-test | ~500 | **+144 / −51** on a 328-line file (472 lines) |

**The estimate held for the header and the self-test and ran ~20% over on
the loader.** What it under-counted: the declaration parser absorbed the
initialiser scanner (a001 R6 arrived after the estimate), and `check_piece`
needed more than the predicted ~20 lines because the address now arrives in
three shapes rather than one (§7, F3).

**The executor came in UNDER** — ~13 lines against ~25 — and for the reason
the gate gave: putting the `wp`/`bp` overload and the width/sign decision in
the PARSER means `Ctx` never consults the declaration table. That prediction
is the one worth carrying forward.

---

## 7. What did not survive contact

**F1 — §4's literal ban would have refused the book 660 times.** The gate
measured it (C-3) and a002 confirmed the scoping: *"a rule that rejects the
artifact it is meant to describe is wrong, whatever its motivation."* The
replacement is better than a ban: **every address an ir 8 program can produce
comes from `wp()`, `bp()` or a cell name, and all three are masked into ring
7, so bit 31 is clear by construction.** The literal refusal is now a belt,
scoped to address positions, and costs the book 0. Self-test: refused as an
index and into a pointer cell; a top-bit-set literal in a VALUE position
still loads.

**F2 — the `ir 8` bump collided with "regenerate no artifact".** Resolved by
§5.10.9's compatibility window: an `ir 7` header is read iff the file
declares no `v` and no `a`. Measured: both artifacts declare zero of each,
and both **load unchanged through the ir 8 loader**. The window is checked
both ways in the self-test.

**F3 — a string address now arrives in three shapes, and `check_piece`
assumed one.** Because a bare aggregate name is refused (R1), what used to be
`[@ALPHA.v2, 8 varying]` is now `[@wp(ALPHA.v2, 0), …]`, `[@bp(cell, 0), n]`
or a bare pointer cell. The first version of the tripwire only looked at a
bare name, so three teeth legs LOADED that should have refused. Found by the
self-test, not by reading. The rule now resolves through `wp`/`bp` and uses
*which* wrapper it found as the word/byte evidence.

**F4 — two regressions the strict-surface check caught on its first run**,
both invisible to the self-test:

- `looks_qualified` accepted any uppercase start, including `M32[`. Harmless
  in ir 7, where memory forms were matched first; fatal the moment `wp`/`bp`
  began peeking at their first operand. `quest.ir2.stock` refused with
  `malformed qualified name: M32`. Fixed with a stricter `at_cell_name`
  (requires a dot, rejects a following `[`).
- A decorated call always carries `marker=`, so dispatching the naive form on
  `!m.empty()` selected it for **every** book call. `quest.ir2.book` refused
  at its first `call`. Fixed by dispatching on the `ret=` spelling.

Both argue for the same thing: **load the real artifacts early.** Neither
would have surfaced before a lockstep run otherwise.

**F5 — the rename's blast radius was larger than either the prompt or R8
measured**, by one more file set: the hand-written difftest corpus
(§4, item 5). The gate measured `lower_c.py` and `cgen.py` and missed
`cases/`. The lesson is P46's, one level deeper: grep the generators *and*
the corpora.

**Unchanged and confirmed:** the prompt's §§1, 2, 3, 6, 7 held up; `trunc8`
was the one factual error inside them (§3 above); `arg_count`'s sign note
(C-1) is recorded in §5.10.1c.

---

## 8. Evidence

- **Self-test GREEN, 129 cases**, 0 failures: `tests/run_vform_selftest.sh`.
  40 cell placements, 57 block placements, 9 symbolic blocks first-executed.
  Broken-allocator build RED on the disjointness assertion (teeth confirmed).
- **Teeth, by group:** the ir 7 refusals all retained; ir 8 adds the KIND
  tripwire (M32/M16 on a non-pointer, M8 on a word pointer, M16/M32 on a byte
  pointer) with a positive leg proving pointee width is advisory; the
  aggregate-as-value and aggregate-as-lvalue refusals; `**` and `*float`;
  both halves of the scoped literal rule plus the value-position positive;
  `a` holes, `a0`, `arg_count` not u16, `v`/`a` line crossover, call arity
  mismatch, call on an undeclared signature, call without `arg_count`;
  the non-`?` callee with arguments, plus `X.CB()` loading; three initialiser
  refusals; and the ir 7 window refused with a `v`, refused with an `a`, and
  accepted with neither.
- **Strict surface:** `quest.ir2.book` and `quest.ir2.stock` both LOAD
  through the ir 8 loader (0 cells, 0 symbolic blocks each), no artifact
  touched.
- **No regressions:** `run_helpers_selftest.sh`, `run_strings_selftest.sh`,
  `run_strhooks_selftest.sh` all GREEN.
- **Not run, and said plainly:** no lockstep play leg. `bin/runner.sh` is
  fixed in the repo but **not deployed** (a002), so a queued task's results
  are not automatically trustworthy; I did not queue one. The K=1 book leg
  that would prove the strict surface *behaviourally* rather than
  *at load* is therefore outstanding, and it is the one piece of evidence
  this report does not have.

**Not covered, carried forward from P46's practice:** the Mapper/checker's
view of 0x76 pointers at a rendezvous; ordinal counting for 0x77 arrivals;
placement to 0x74 (no placement input exists); execution of any call out of a
symbolic block (P50/P53); the compiler's emission shape (P53).

---

## 9. One recommendation for `Salvage.md` (file outside my boundary)

a002 did not take the §3.5 edit and asked for exact wording. For the
addrbook row of §3.5, replacing the RETURN_MESSAGE clause:

> RETURN_MESSAGE `mixed:3/6` is misleading for **two independent** reasons:
> the 3-argument caller is hand assembly (F6), **and** the callee reads
> arguments 1–3 at every arity and never touches 4–6 (F5, as corrected by
> P52), so the flag describes the routine's call sites and not its body.

---

## 10. Files

Branch `p52-ir8` (and `main`). Written: `docs/IR.md` (ir 8),
`docs/Project52/{q001,q002,REPORT}.md`, `emulation/hw/IRExec.{cpp,hpp}`,
`emulation/tests/{vform_selftest.cpp,run_vform_selftest.sh,run_lowerc_difftest.sh}`,
`game/quest_rt.h`, `game/routines/*.c` (rename only), and the eight granted
lines in `compiler/{lower_c.py,difftest/cgen.py,difftest/make_update_screens_fixture.py}`.

Not touched: `Disassembled/**`, `docs/Salvage.md`, `docs/Project44/DESIGN.md`,
`quest.ir2.*`, `quest.addrbook`, `quest.arena`, `tasks/`, `bin/`, and the rest
of `compiler/**`. **No artifact regenerated.**
