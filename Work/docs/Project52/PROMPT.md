# Project 52 — ir 8: THE VARIABLE FORM, POINTER TYPES, AND ARGUMENT CELLS

## GOAL

Make the IR **expressive enough that simple C compiles to it and runs** — and
readable enough that a `v` looks like the variable it is.

This is an **IR and loader project**. The compiler update is P53. You write
`docs/IR.md`, `emulation/hw/IRExec.cpp` and friends, and a self-test.

**Success = ir 8 spec + loader + a hand-written self-test** in which every
construct below loads, places and executes, with a teeth leg for every
refusal.

**The bar, stated as the design intent:** the seven routines P51 wrote
(`game/routines/`, ~720 lines, bodies of 10–40 lines each) should be
*expressible*. You do not compile them — P53 does — but **read all seven** and
report anything they need that ir 8 still cannot say. That list is P53's
blocker, and finding it now is worth more than anything else in this project.

---

## Context of record — read in this order

| path | why |
|---|---|
| `docs/IR.md` | **ir 7. You are writing ir 8.** Normative, self-contained; keep it so |
| `docs/Project46/REPORT.md` + `q001-plan-gate.md` | how ir 7 was built and measured. **Its method is the one to copy** — line-cited sizing, teeth on every refusal |
| `game/routines/*.c` (the seven P51 wrote) | **the expressiveness bar.** HIT_ANY_CHAR, PICK_X_Y, GET_INPUT, INIT_SCREEN, FAKE_OCEAN, FAKE_LAND_MASS, UPDATE_SCREENS |
| `docs/Project51/REPORT.md` §2 | the compiler gap list; several items are IR questions |
| `docs/Project44/DESIGN.md` | §4 storage classes, §5 address spaces. **§4.1c is now WRONG — see ruling 0** |
| `docs/Salvage.md` | F1/F2 (static link, uplevel), F4 slotpatch, F5 optional args, F12 X.CB, F13/F14 bytes and `?READ` |
| `emulation/hw/IRExec.cpp` | the loader. ir 7's `v`/block work was ~250 lines here |
| `emulation/tests/vform_selftest.cpp` | the model for your self-test |

**Do not read `docs/attic/`.**

---

## Ruling 0 — DESIGN §4.1c is VOID

§4.1c argues a `v` name must be an address constant and says not to
relitigate. **It was relitigated the same day and lost.** Ignore it; I am
removing it. Its one surviving observation: `wp`/`bp` already carry the
word/byte distinction, so **no `&` operator is needed**.

---

## The changes

### 1. The variable form

A `v` name denotes **the cell's contents**, as a C variable does.

| operation | ir 7 (old) | **ir 8** |
|---|---|---|
| read a local | `M32[PXY.v3]` | `PXY.v3` |
| write a local | `M32[PXY.v3] = e` | `PXY.v3 = e` |
| address of a local | `PXY.v3` or `wp(PXY.v3,0)` | `wp(PXY.v3, 0)` / `bp(PXY.v3, 0)` |
| read through a pointer | — | `M32[PXY.a1]` |
| write through a pointer | — | `M16[PXY.a1] = e` |

**Access width and signedness come from the declaration**, not the statement.
That is the cost of this change and it is accepted: `ircmp` will have to
consult declarations when comparing against the book's explicit
`M32[wp(ac3,12)]`.

**`wp`/`bp` resolve by operand kind** — a register operand means *its value*
plus offset; a `v`/`a` name means *the address of that cell* plus offset. The
parser knows which, statically. This is an overload and it is deliberate.

### 2. Pointer types

`*i16`, `*u16`, `*i32`, `*u32`, `*char`. **One level only — no `**`.**

The book's apparent double indirection is an artefact of addressing: it
indexes by *address of cell* where we *name* the cell, so our types are one
star shallower. `?READ`'s arg 2 (a dummy holding a byte pointer) is `*char`,
not a pointer-to-pointer.

**Word/byte kind is ENFORCED; pointee width is ADVISORY.**

- `M8[<word pointer>]` → REFUSE
- `M16`/`M32[<byte pointer>]` → REFUSE
- `M16` vs `M32` on a word pointer → **allowed either way** (a wide is two
  consecutive words; `XNLDA`/`XWLDA` both take word addresses)
- `M<n>[<non-pointer v>]` → REFUSE

The kind also selects the instruction family at codegen: `XPEF` vs `XPEFB`,
`XNLDA`/`XWLDA` vs `XLDB`/`WLDB`/`WSTB`.

### 3. Argument cells

```
a <ENTRY>.a<N>        <vtype>
a <ENTRY>.arg_count   u16
```

`a` cells are cells like `v`s — same type system, same placement, same
tripwire. They make a routine's **signature declared**, so the loader can
check a call's arity and pointer kinds against the callee.

`arg_count` is **u16** — the LOW WORD of the LCALL marker `(psr<<16)|argc`
(the addrbook's own layout table). The psr half is machine state no source
construct touches; it becomes a rewrite's business at L2.

**The caller writes the `a` cells and `arg_count`**, then transfers. This is
legal only because the game is non-reentrant, and it is the naive form:
the original's push-and-prologue machinery is a *rewrite* to reintroduce,
not something the compiler emits. (Same principle as pure-vs-effectful
operators, P48 R2.)

### 4. Dereference is `M32[M32[a]]`, NOT `M32[R[a]]`

The naive form spells a dereference plainly: fetch the pointer, fetch through
it. **`R[]` stays in the IR because the book uses it, but the compiler never
emits it.**

`R[e]` means "deref, then follow bit 31 until clear" — equivalent to a plain
double fetch *only when* bit 31 is clear. Emitting `R[]` naively would assert
that invariant at every dereference instead of checking it once.

**`M32[M32[a]] → M32[R[a]]` is a REWRITE** with the precondition "bit 31 of
the fetched word is clear". For arguments we pushed the address ourselves, so
it is provable; for statics it is an assumption about data, and the two
deserve different treatment. One rewrite rule covers all ~1,000 dereference
sites in the program — which is exactly the *one rule, many applications*
shape DESIGN §7.2a wants.

**Enforce the invariant at load:** every address the program can produce lies
in ring 7 (0x70–0x77), so bit 31 is never set. Refuse a literal in the
top-bit-set range, as the loader already refuses 0x76/0x77 literals. That
turns a reasoning step into a guarantee.

### 5. Two gaps found by reading P51's C

**`trunc8`.** The mask set is `sx16 | zx16 | zx8 | trunc16` — a zero-extend at
byte width but **no truncate**. GET_INPUT's `(unsigned char)(*ch - 128)`
needs it, and HIT_ANY_CHAR stores a byte.

**`X.CB` has no expressible form.** IR.md refuses an `rt_call` whose callee is
*"not a `?` symbol"*. `X.CB @7017E708` is an internal helper reached by an
**undecorated** `LCALL` (Salvage F12: ac2 = destination word address, ac0 =
byte pointer to the character form, ac1 = its length), and GET_INPUT's
`BITS("001")` needs it. Widen the callee rule or add a production — your
call, with reasons.

### 6. Calls from symbolic blocks

ir 7 REFUSES `call`/`rt_call` in a symbolic block (P46 F7) because they are
validated against a real `LCALL` at `site=`, and a compiled routine has none.
**ir 8 must allow them with a symbolic return label.**

The structure already fits: **`rt_call` is a TERMINATOR**, so a call already
ends a block and the continuation is a separate block at `site+4`. The change
is a symbolic return target filling a slot that already exists.

**Consequence for the subset, which P53 needs stated:** a valued call splits
the expression. `r = RANDOM_NUMBER$3(...)` is a call terminating one block and
`r = ac0` opening the next, so **a call cannot sit inside a larger
expression**.

You are **not** required to build the execution side (the LCALL replica,
the bridge) — that is P50/P53. **Spec and loader only**, and say plainly in
the REPORT what remains before a call actually runs.

### 7. Rename: `SUB` → `RANGE_CHECK`

`SUB` reads as subtraction or substitute; it is a bounds check that traps.
The IR does not name it (it lowers to `assert(e)`), so this is a **C-side
rename**: `game/quest_rt.h` (both views) and the `game/routines/*.c` files.

Mechanical, but it touches P48's and P51's output — do it deliberately, the
way P46 did `t@` → `s@`: change the source, then diff to prove nothing but
the token moved.

---

## Part 1 — PLAN GATE

`docs/Project52/q001-plan-gate.md`, push to main, **STOP**. Report:

1. **The ir 8 grammar**, in IR.md's notation: the variable form, pointer
   vtypes, `a`/`arg_count` declarations, `trunc8`, the `X.CB` form, symbolic
   call returns.
2. **Sizing**, line-cited against `IRExec.cpp` as P46's gate did. Which
   changes are parser-only and which touch the executor.
3. **All seven of P51's routines read**, with anything ir 8 still cannot
   express. **This is the most valuable item in the gate** — P51's C was
   written before this design existed, so it cannot have been shaped to fit.
4. **Two censuses:**
   - **The 49 non-argument `R[]` sites.** 1,032 total; 978 are `R[acN + -N]`
     (argument slots — arg 1 at −12, arg 2 at −14, …), but **25 are statics**
     (`R[0x70000212]` ×22, `R[0x70000210]` ×3, `R[0x700007A0]` ×2) and **~24
     are positive frame offsets** (locals holding addresses). For each: where
     does the value come from, how many levels are followed, and **is the cell
     ever written by the routine?** If a local's address value is assigned in
     C, that is a case this design assumed away.
   - **The optional-argument mechanism.** Salvage F5 says two spellings exist
     — REFRESH_SCREEN reads a frame marker word, RETURN_MESSAGE tests an
     argument slot for null. **The null test looks unsound**: an unsupplied
     argument was never pushed, so the slot holds whatever the previous frame
     left, and only `arg_count` can discriminate. Note also that
     RETURN_MESSAGE's 3-argument site is the **hand-assembled** one inside
     LOCK_FILE (F6), and R39 says hand assembly is inadmissible evidence about
     compiler behaviour. **Is F5's second spelling a PL/I convention or a
     hand-written artefact?** This decides how `arg_count` is used and it
     touches a live Salvage fact.
5. **Anything above you disagree with.** Sections 1–6 were settled in design
   discussion, not measured. P46 found three errors in DESIGN by building
   against it; the same is expected here.

STOP. Wait for `a001`.

---

## Part 2 — Build

Spec, then loader, then self-test. Keep IR.md normative and self-contained,
with a version-history entry.

**The strict surface is untouched:** the book names no `v`, no `a` and no
symbolic block, so a renamed ir 7 program must load and run identically.
Any change to existing block semantics is a **STOP-and-report**.

Self-test on `vform_selftest.cpp`'s model: every construct exercised, and a
**teeth leg** for every refusal — `M8` on a word pointer, `M16` on a byte
pointer, `M32[v]` on a non-pointer, a top-bit-set literal, a call arity
mismatch, an undeclared `a`, `ir 7` in the header.

---

## Boundaries — BINDING

1. **You may WRITE:** `docs/IR.md`, `docs/Provenance.md`,
   `docs/Project52/**`, `emulation/**` (loader, executor, tests),
   `game/quest_rt.h` and `game/routines/*.c` (**for the `RANGE_CHECK` rename
   only** — no other change to those files).
2. **Do NOT touch** `compiler/**` (P53's), `Disassembled/**`,
   `docs/Salvage.md` (recommend corrections in the report),
   `docs/Project44/DESIGN.md`, `quest.ir2.*`, `quest.addrbook`,
   `quest.arena`. **Regenerate no artifact.**
3. **Check with the integrator before starting if P49 is still running** — it
   owns `emulation/` and `tasks/`. If it is, its files are off limits and you
   must coordinate through a question.
4. **You do not make calls execute.** Spec and loader only.

---

## Part 3 — Report

`docs/Project52/REPORT.md`: the grammar as landed; the two censuses; **what
in P51's seven routines ir 8 still cannot express**; the `RANGE_CHECK` rename
diff evidence; what remains before a call runs; sizing estimate vs actual;
and **anything in this prompt that did not survive contact**.

---

## Coordination — questions and the plan gate (BINDING)

Write `docs/Project52/q00N-short-title.md`, commit, **push to main**, then
**STOP and tell the user it is there.** Do not work ahead while a question is
outstanding. You own `q*.md`; the integrator owns `a*.md`. Every question
states what you found, the decision needed, the options with your read, and
**your RECOMMENDATION**. SOP: `docs/INTEGRATOR.md` §10.

## Delivery

**Push to `p52-ir8` at every stage boundary.** One `Work.tgz` with the final
report.
