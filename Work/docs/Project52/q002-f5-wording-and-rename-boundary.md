# Project 52 — q002: the `Salvage.md` F5 wording you asked for, and one boundary the rename needs

Stage A is on `main` @ `0b011db` and on `p52-ir8`. Holding Stage B for your
signal per a001/R0. Two items, plus a note on four gate rulings a001 did not
name.

---

## 1. `Salvage.md` F5 — the exact replacement, as requested

Drop-in for the F5 row of §1.1. Kept in the table's format; the `depends`
column loses "any C for these two routines" for RETURN_MESSAGE because the
C no longer needs an optional-argument spelling for it.

> | F5 | **Optional arguments have ONE spelling, and it is the frame marker word.** REFRESH_SCREEN (`mixed:0/1`) reads the argument count from the marker at `wp(ac3,-9)` and branches on it — `ac0 = sx16(M16[wp(ac3,-9)])`, tested `== 0`, block@70176A93. **CORRECTION (P52):** F5 previously recorded a second spelling, RETURN_MESSAGE (`mixed:3/6`) testing the argument slot `M32[wp(ac3,-16)]` for null. That is **not** an optional-argument mechanism. Three independent reasons: (a) `wfp−10−2N` puts −16 at argument **3**, which is supplied at BOTH arities, so a null there cannot discriminate 3 from 6; (b) the body reads only arguments 1, 2 and 3 (`R[ac3+-12]`, `R[ac3+-14]`, `R[ac3+-16]`) and **never touches 4, 5 or 6**, so `mixed:3/6` produces no arity-dependent code at all; (c) all five call sites supply a non-null argument 3, so the default-message arm is **dead code** in this program. It is a **null-POINTER test on a supplied by-reference argument** — the caller asking for the default text — and F11 already names the consequence (the literal at 0x70000CCD, "Unexpected error"). | REFRESH_SCREEN block@70176A93 (`XNLDA 0,[ac3+0x7FF7]`, `MOV.# 0,0,SZR`); RETURN_MESSAGE 70176FDD..70177005 read out of `quest.ir2.book` — the null test, the two message arms, and the three argument reads | **1** (REFRESH_SCREEN) | **Verified**; the correction **Verified** (P52 against `quest.ir2.book`, Sep 12 2026) | `IR.md` §5.10.1c / §5.10.8 (`a <ENTRY>.arg_count u16` is this mechanism and there is no second case to support) | any C for REFRESH_SCREEN |

Two consequential edits elsewhere in the file, if you want them in the same
push:

- **F6** gains a sentence: *"P52 note — F6 is true and turns out not to be
  load-bearing: what disqualifies the null test as an arity mechanism is in
  RETURN_MESSAGE's own compiled body, not in the caller, so R39 need not be
  invoked."* Worth saying, because F5 and F6 currently read as though the
  hand-assembly objection were doing the work.
- **§3.5** (`quest.addrbook`, "two flags are misleading") — `mixed:3/6` on
  RETURN_MESSAGE is misleading for a **second, independent** reason now: not
  only is the 3-argument caller assembly (F6), but the callee reads three
  arguments at every arity, so the flag describes its call sites and not its
  body.

I have not touched `Salvage.md`; it is yours.

---

## 2. **RULING R8, re-asked** — the rename breaks two files outside my boundary

This was C-5/R8 in the gate and a001 did not rule on it. It is now concrete
rather than predicted, because the rename has landed.

**What landed** (`main` @ `0b011db`, `Work/game/` only): 118 `SUB` → `RANGE_CHECK`,
2 `SUBs` → `RANGE_CHECKs` (prose plurals), 3 `quest_sub_check` →
`quest_range_check`, across `quest_rt.h` and 13 `routines/*.c`.

**Proof it is token-only**, stronger than a `sed` yardstick: reverse-
substituting the three token pairs in the 14 new files **reproduces the 14
originals byte for byte, 14/14**. `git diff --numstat` is equal adds and
deletes in every file (10/10, 3/3, 1/1, 15/15, 18/18, 6/6, 16/16, 3/3, 8/8,
5/5, 4/4, 5/5), so no line moved, split or merged. `WSUB`/`NSUB` mnemonics
in the comments are untouched by `\bSUB\b` and are still there.
Native-view check: `g++ -fsyntax-only` error counts are **identical before
and after for all 15 files** — the rename is behaviour-neutral in the C++
view (the counts themselves are not all zero; see §3).

**Second token flagged honestly.** I renamed `quest_sub_check` →
`quest_range_check` as well. The prompt named only `SUB`, but leaving the
helper would keep the word the rename exists to remove (`SUB` reads as
subtraction) visible in the one file the rename is about. If you would
rather it stayed, say so and I will revert those 3.

**What is now broken, outside my boundary:**

| file | lines | what breaks |
|---|---|---|
| `compiler/lower_c.py` | :681 `if name == "SUB":`, :730, :733 | the translator dispatches on the identifier, so it now refuses every routine that uses `RANGE_CHECK` |
| `compiler/difftest/cgen.py` | :159, :161 emit `SUB(…)`; :365 emits `#include "quest_rt.h"` | the generated differential programs no longer compile natively — and the harness that runs them, `emulation/tests/run_lowerc_difftest.sh`, **is** in my boundary |
| `compiler/difftest/make_update_screens_fixture.py` | :106, :107, :156 | comments only; cosmetic |

**Options.**

- **(a) Extend the boundary to those 5 functional lines** (+3 comment
  lines) and nothing else in `compiler/**`. **My recommendation**, on the
  P46/a001 R2 precedent, which granted `ircmp.py`'s four lines for exactly
  this shape and gave the reason: *"a knowingly-red selftest is not an
  acceptable resting state."* The risk that made you split this project —
  two sessions in one file — does not apply: P53 has not started and these
  are five token sites.
- **(b) Leave them; P53 renames when it takes `compiler/**`.** Costs a red
  difftest and a `lower_c.py` that refuses its own corpus, with the reason
  living only in a REPORT.
- **(c) Revert the rename until P53.** I do not recommend it: the prompt put
  the rename in P52 deliberately, and a001 confirmed it belongs in Stage A.

Note this is **separate from R10**, which is about the version bump, not the
rename — see §4.

---

## 3. A finding, no ruling needed: the candidate C does not compile natively today

While checking the rename was behaviour-neutral I ran `g++ -fsyntax-only`
over all 15 routine files in the C++ view. **Ten of fifteen already fail,
before and after identically**, for reasons that are all P51 §2 gap-list
items seen from a second direction:

| routine | pre-existing error |
|---|---|
| FAKE_OCEAN, FAKE_LAND_MASS | `MIN`/`MAX` not declared — P51 §2 item 7 and item 11 |
| INIT_SCREEN | `FAKE_LAND_MASS`, `FAKE_OCEAN` not declared — item 11 |
| HIT_ANY_CHAR | `GET_INPUT` not declared — item 11 |
| GET_INPUT | `TMP(buf)`: `tmp_arg → void*` ambiguous, and `unsigned char* → int32_t` invalid — item 3, the pointer-valued dummy |
| UPDATE_SCREENS | `SD_PTR`, `PLAYER` not declared — the file includes `quest_rt.h` but **not** `declarations.h`, unlike its six siblings |
| RETURN_MESSAGE | `DATA`, `RETURN$2` not declared |

None is mine to fix (`quest_rt.h` is P44/P48-owned and my boundary on it is
"the rename only"), and P51 §2 item 11 already flags the prototypes. I am
recording it because **the native view is one of P48's two differential
oracles**, and "the candidate C compiles natively" is a thing one could
easily assume holds. It does not. UPDATE_SCREENS's missing include is the
odd one out — a one-line fix in a file P51 explicitly did not touch, and
probably P53's first five minutes.

---

## 4. Four gate rulings a001 did not name — how I read them, so you can correct me cheaply

a001's header granted R4/R5/R6 and its body ruled R0 and G-4. R1, R2, R3 and
R7 went unnamed. Since a001 opened with "APPROVED" and instructed me to
"land the whole grammar… `trunc8`/`zx8`, the `X.CB` form", I took the gate's
recommendations as carried and **they are in `docs/IR.md` now**:

| # | landed as | where |
|---|---|---|
| R1 | a bare aggregate cell name REFUSES as value or lvalue | §5.10.4, §5.1 refusals |
| R2 | `trunc8` as the store-intent twin of `zx8`; described as a name, not new expressiveness | §5.1, §9 |
| R3 | `rt_call` callee rule widened (resolves into the runtime range + empty argument list) rather than a new production | §6 |
| R7 | top-bit-set literal refusal **scoped to address positions**; the bit-31 guarantee derived from `wp`/`bp` instead | §5.10.8 |

R7 is the one worth a second look if any: it is the one where I changed what
the prompt said rather than adding to it, and the evidence is that the
unscoped rule refuses the book 660 times while the scoped one costs 0.

**R9 and R10 are Stage B and can wait for the same signal.** For the record
R9 (accept an `ir 7` header iff the file declares no `v`/`a`) is written into
§5.10.9 as spec, because the version history has to state why no artifact was
regenerated — but it is the loader that implements it, so you can still
overrule it without the spec having lied about anything.

---

## Recommendation summary

1. Land the F5 replacement in §1 (plus the two consequential edits if you
   want them).
2. **Grant R8 option (a)** — 5 functional lines in `compiler/**`.
3. §3 is a finding, not a question.
4. Correct me on R1/R2/R3/R7 if any of them went the wrong way; otherwise
   they are landed.

STOP — awaiting `a002`, and awaiting the Stage B signal (a001/R0). P49
merged Stage B and queued task 054 at `c297af5` but has no REPORT, so I have
assumed it is still live and have touched nothing in `emulation/`.
