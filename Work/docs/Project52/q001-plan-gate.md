# Project 52 — q001: PLAN GATE (Part 1 report, Sep 12 2026)

No code and no existing doc changed. Everything below was measured on `main`
@ `d52bd80`. Rulings needed are marked **RULING**; corrections to the prompt
are marked **C-n**; things the seven routines need that ir 8 cannot say are
**G-n** (§3, the item the prompt calls the most valuable).

Reading order followed as given. `docs/attic/` was not opened.

---

## 0. One coordination item first — P49

Boundary 3 says to check with the integrator before starting if **P49 is
still running**, because it owns `emulation/` and `tasks/`. From the tree I
cannot tell: `docs/Project49/a004` ends *"Proceed: fix the LIST_PLAYERS exit,
look at the core dump, queue as 054"*, and `results/` stops at
`053-p49-stage-b` with no `054`. So P49 was mid-flight at its last push.

Part 1 touches nothing but `docs/Project52/`, so this gate is safe either
way. **Part 2 is not**: it lands in `emulation/hw/IRExec.cpp` and
`emulation/tests/`. **RULING R0:** is P49 finished, and is `emulation/` mine
for Part 2? If P49 is live I will need a serialisation, not a merge — two
sessions editing `IRExec.cpp` is not something a diff resolves.

---

## 1. The ir 8 grammar (IR.md notation)

### 1.1 Pointer vtypes

    vtype  := i16 | u16 | i32 | u32                  ; ir 7, unchanged: 1, 1, 2, 2 words
            | char <n> | varying <n> | words <n>     ; ir 7, unchanged
            | *i16 | *u16 | *i32 | *u32              ; WORD pointer   — 2 words
            | *char                                  ; BYTE pointer   — 2 words

One level only: `**` REFUSES at load, and so does `*` on an aggregate
(`*char <n>`, `*varying <n>`, `*words <n>` — see **G-2**, which asks for two
of those back).

A pointer is 2 words in every case (a wide; the machine has no narrow
pointer). The declaration carries two properties the loader consumes:

- **KIND** (word vs byte) — **ENFORCED**, per the prompt §2;
- **pointee width** — **ADVISORY**, per the prompt §2; carried for the
  compiler and for `ircmp`, never checked.

### 1.2 The variable form

    primary += <ENTRY>.v<digits>          ; the CELL'S CONTENTS
             | <ENTRY>.a<digits>          ; ditto, an argument cell
             | <ENTRY>.arg_count
    lvalue  += the same three

Width and signedness come from the declaration, not the statement:

| declared | read is | write is |
|---|---|---|
| `i16` | `sx16(M16[addr])` | `M16[addr] = v & 0xFFFF` |
| `u16` | `zx16(M16[addr])` | `M16[addr] = v & 0xFFFF` |
| `i32` `u32` `*…` | `M32[addr]` | `M32[addr] = v` |

**Aggregates have no contents.** `char n`, `varying n` and `words n` name a
region, not a value, so a bare aggregate name as an rvalue or an lvalue is a
category error. It is legal only as the first operand of `wp()`/`bp()` and
as an address inside the §5.8 string forms. **RULING R1:** refuse a bare
aggregate name (recommended — the alternative, silently meaning "the first
word", is the kind of implicit that §5.1 exists to prevent).

### 1.3 `wp`/`bp` resolve by operand kind

    wp(<name>, d)   = the cell's ADDRESS + d          (word)
    wp(<expr>, d)   = the expression's VALUE + d      (ir 7 behaviour, unchanged)
    bp(<name>, d)   = byte pointer to the cell's address
    bp(<expr>, d)   = ir 7 behaviour

Decided **statically, at parse time**, as the prompt says: the parser knows
whether it just consumed a qualified name or a register/expression. This
matters for the sizing (§2): it keeps the overload entirely in the parser
and out of `Ctx::eval`.

Note the composition, because it is the part that reads oddly at first and
is right: if `v3` is `*i32`, then `v3` is the pointer value, `M32[v3]` is the
pointee, and `wp(v3, 0)` is the address of the pointer cell itself.

### 1.4 Dereference and the rewritten tripwire

ir 7's width tripwire (§5.10.4) does not survive: it asked "does this v fit
a direct `M<n>[v]` index", and in ir 8 `M<n>[v]` no longer means that. It is
**replaced**, not extended:

    M8[<word pointer>]              REFUSE
    M16[<byte pointer>]             REFUSE
    M32[<byte pointer>]             REFUSE
    M16[<word pointer>]             allowed
    M32[<word pointer>]             allowed      ; a wide is two consecutive words
    M8[<byte pointer>]              allowed
    M<n>[<non-pointer name>]        REFUSE
    [@<name>, …] / [@<name>, … varying]          ; see G-2

`R[e]` is unchanged and stays in the grammar because the book uses it. The
compiler never emits it (prompt §4).

### 1.5 Argument cells

    decl   += a <ENTRY>.a<N> <vtype>        ; N >= 1
            | a <ENTRY>.arg_count u16

Loader rules, mirroring the `v` rules so there is one story and not two:
declared before first reference, in file order; duplicates refuse; the `a`
numbers must be **contiguous from 1** (a hole means the signature is wrong,
and that is exactly what these declarations exist to catch); placed
sequentially in the same per-entry 0x76 range, in declaration order; the
§1.4 tripwire applies identically.

A `call` to `<ENTRY>` then checks, at load, the argument count and the
pointer KIND of each argument against the callee's `a` declarations.

**C-1 — `arg_count` and the sign of the read.** The prompt fixes it as
`u16`, which is right for the datum. But the book reads that word
sign-extended: `REFRESH_SCREEN` block 70176A93,
`ac0 = sx16(M16[wp(ac3, -9)]) ; XNLDA 0,[ac3+0x7FF7];`. `sx16 ≡ zx16` for
every argc ≤ 0x7FFF, so nothing behaves differently — but it is a *text*
difference `ircmp` will meet, and it belongs in the spec next to the
declaration rather than being rediscovered at L2. Recommend: keep `u16`,
record the note.

### 1.6 `trunc8` — **C-2: the prompt's premise is wrong**

Prompt §5 says the mask set is "`sx16 | zx16 | zx8 | trunc16` — a
zero-extend at byte width but **no truncate**". `zx8` **is** `& 0xFF`
(IR.md §5.3: *"`zx16`/`trunc16` are both `& 0xFFFF` (the names record
intent: load vs store); `zx8` is `& 0xFF`"*), and an M8 store truncates by
rule anyway (§5.2). So GET_INPUT's `(unsigned char)(*ch - 128)` is already
expressible today as `zx8(...)`, and `WANDI 2,255` already lowers.

`trunc8` is therefore an **audit-trail name**, the store-intent twin of
`zx8` exactly as `trunc16` is of `zx16` — not new expressiveness.
**RULING R2:** add it as a pure alias for symmetry (recommended: four
lines, and the compiler's output reads correctly), but land it described as
what it is. It is not a gap and it should not be counted as one.

### 1.7 `X.CB` — widen the callee rule, do not add a production

Both sites are already **block-final** in the book, with the three register
arguments as ordinary preceding statements:

    block 7016AA35 …                                 block 701703A0 …
      ac2 = wp(ac3, 76) ; XLEF 2,[ac3+0x4C];           ac2 = wp(ac3, 22) ;
      ac0 = 0x7016A9B9:0 ; XLEFB 0,…                   ac0 = 0x7017024D:0 ;
      ac1 = 0x00000003 ; NLDAI 3,1;                    ac1 = 0x00000001 ;
      @7016AA41 LCALL [0x7017E708],0; # X.CB          @701703A6 LCALL […],0; # X.CB

So the shape rt_call already validates — a block-final LCALL to a resolved
runtime target — fits without change. What refuses it is one rule:
*"a callee that is not a `?` symbol"* (IRExec.cpp:715).

**RULING R3 (recommended): widen it.** A non-`?` callee is accepted when it
(a) resolves to an address in the runtime range, and (b) carries an **empty
argument list** — the register convention, with the setup as ordinary
statements, which is what both sites already are. Reasons for widening
rather than adding a production: the call shape is byte-identical to
`rt_call`'s, a second production would duplicate its whole belief-check
apparatus for one callee, and `NextSession.md` has already widened
`RTConventions.md` to be the home of record for *any* runtime routine
including the `X.*`/`I.*`/`O.*`/`D.*` helpers — the IR should follow the
same widening rather than treat `?` as a type distinction it was never
meant to be.

Against: `?` is currently a cheap, greppable invariant. I think that is
worth less than one vocabulary.

### 1.8 Calls from a symbolic block

    rt_call <callee>(e1, …, eN) ret=<ENTRY>.b<k>
    call    <tgt> args=<n> ret=<ENTRY>.b<k>

`site=` and `marker=` are absent (there is no LCALL word to validate
against) and `ret=` is a symbolic label filling the slot that already
exists. Still a TERMINATOR; the continuation is a separate block, so the
prompt's subset consequence holds: **a valued call cannot sit inside a
larger expression** — `r = RANDOM_NUMBER$3(…)` is a call ending one block
and `r = ac0` opening the next.

**G-1 (below) is the thing this section does not settle**: which mechanism
carries the arguments.

---

## 2. Sizing, line-cited against `IRExec.cpp` (1,588 lines today)

P46's gate estimated ~250 lines and landed 307 insertions. I am using the
same method and the same unit so the two are comparable.

### 2.1 Parser-only (no executor change)

| where | change | est. |
|---|---|---|
| `primary()` :269–282 — the qualified-name branch, today returns `CONST` with `vref` | return a new `VREF` node carrying the placed address **plus** type/width/sign/kind baked in at parse time; refuse a bare aggregate (R1) | ~35 |
| `primary()` :283–284 — `wp(`/`bp(` via `fn2()` | resolve by operand kind: peek whether the first operand is a name (§1.3). The whole overload lives here | ~25 |
| the `v` declaration parser :587–627 | pointer vtypes; `*` rejected on aggregates and on `*` | ~30 |
| new: the `a` declaration | `a <ENTRY>.a<N>` / `a <ENTRY>.arg_count`, contiguity, placement in the same `vcursor` (:470, :612) | ~55 |
| `parse_qualified()` :184–208 | accept `.a<digits>` and the shape exception `.arg_count`; `want` gains 'a' | ~20 |
| the width tripwire :487–503 (17 lines today) | **replaced** by §1.4 | ~45 (−17) |
| `check_piece` :853–868 | the same rework for string pieces — it currently insists a varying piece's `vref` is `Var::VARYING`, which is exactly what G-2 trips over | ~20 |
| the lvalue parser :1001–1012 | accept a `VREF` lvalue | ~14 |
| literal guard :336, :339 (`is_synthetic`) | the top-bit-set rule — **scoped**, see C-3 | ~10 |
| `trunc8` | one line in `primary()`, one `case` | ~4 |
| `rt_call` callee rule :715 | R3 | ~12 |
| symbolic-block refusals :671, :678, :700 + `call`/`rt_call` operand parsing :678–760 | symbolic `ret=`, no `site=`/`marker=`; arity/kind check against the callee's `a` cells | ~55 |

**Parser subtotal ≈ 325 lines added, ~30 removed.**

### 2.2 Executor (small, by design)

The variable form looks like it should be expensive in `Ctx` and is not,
because §1.3 puts the overload in the parser and the `VREF` node can carry
its own address, width and sign. `Ctx` never needs the `Var` table.

| where | change | est. |
|---|---|---|
| `Ctx::eval` :1157–1224 | one `case VREF`: `read_word`/`read_wide` + `sx16`/`zx16` per the baked-in type | ~12 |
| `run_block` STMT store :1374–1388 | one branch for a `VREF` lvalue (width from the node) | ~12 |
| `Ctx::wrap`/`addr_of` :1147–1152 | **no change** — §5.2's `(e & 0x0FFFFFFF) | seg` is unaffected | 0 |
| `Expr::WP`/`BP` :1212–1217 | **no change** — the kind decision already happened | 0 |

**Executor subtotal ≈ 25 lines.** Plus `IRExec.hpp` ~30 (the `Var::Type`
enum gains the pointer kinds, `Expr` gains three small fields, `Stmt` gains
a symbolic `ret`).

### 2.3 What existing machinery already covers

- **Placement.** `a` cells are cells: the `vcursor` allocator (:470, :612)
  and the per-entry 0x10000 range take them with no new mechanism.
- **The segment wrap.** Unchanged, and it is why this works at all: a 0x76
  address survives `(e & 0x0FFFFFFF) | seg` from any ring-7 block.
- **Dispatch, 0x77, `map_pages`, the `blocks` provenance relaxation, the
  addrbook namespace** — all ir 7, all untouched.

### 2.4 Estimate

**~350 lines in `IRExec.cpp` (+~30 hpp), of which ~25 are executor.** Wider
than P46's 250 because the tripwire is a rewrite rather than an addition and
the `a` declarations are a second namespace. The self-test will be larger
than `vform_selftest.cpp`'s 328 — call it ~500, on the same model.

---

## 3. The seven routines — read, and what ir 8 still cannot say

All seven read in full (720 lines exactly: 67 + 103 + 84 + 120 + 137 + 157 +
52). Everything the P51 REPORT §2 gap list names was checked against the ir 8
grammar of §1. Most of it lands: `unsigned char` scalars and arrays are
`char n`; `TMP(e)` dummies are ordinary `v`s whose address is passed;
`TMP(buf)` is a `*char` cell, exactly as the prompt §2 predicts; byte
load/store through a byte-pointer parameter is `M8[<*char>]`; the 8-bit
narrowing is `zx8` (C-2); `MIN`/`MAX`/`ABS` are diamonds, i.e. blocks;
`SUB`/`RANGE_CHECK` is `assert`; materialised booleans are `|`/`&` over
parenthesised comparisons, which is one operator class and legal;
16-bit N-op arithmetic is `nadd`/`nsub` at statement root, one statement
each; the descending loop, the computed bounds and `continue` are all
`goto`.

Four things do not land. **G-3 is the blocker.**

### G-1 — there is no production for a call's ARGUMENTS in the naive form

Prompt §3 says the caller writes the `a` cells and `arg_count` "then
transfers". Prompt §6 says `call`/`rt_call` become legal with a symbolic
return. These are two different mechanisms and the prompt does not say which
applies where — and it cannot be one mechanism, because:

- **`rt_call` arguments must go on the real stack.** The runtime reads
  argument *n* at `wsp−2n` from the LCALL marker
  (`RTBridge::arg_pointer`, hw/RTBridge.cpp:111; IR.md §6). Writing
  `?WRITE_SCREEN`'s arguments into `a` cells would put them somewhere the
  callee never looks. So `rt_call` from a symbolic block still **pushes**,
  through `Machine::wide_push`, as it does today.
- **game→game arguments go in `a` cells**, which is the whole point of §3
  and is legal only because the game is non-reentrant.

Needed by: HIT_ANY_CHAR (2 rt, 1 game), GET_INPUT (1 rt), PICK_X_Y (3 rt,
valued), INIT_SCREEN (2 game, both passing the incoming pointer through),
UPDATE_SCREENS (0). **RULING R4:** confirm the two-mechanism reading and let
me write it into §6 that way. The pass-through case is worth noting as the
one place ir 8 reads *better* than the book: `FAKE_OCEAN.a1 = INIT_SCREEN.a1`
is a copy of a pointer value, one statement, where the book has
`XPEF @[ac3+0xFFF4]`.

### G-2 — a pointer to a string has no vtype, and the string tripwire refuses the workaround

The pointer vtypes are `*i16 *u16 *i32 *u32 *char`, where `*char` is a
**byte** pointer. There is no pointer whose pointee is a `varying n` or a
`words n`. The census (§4.1) says the program is full of them: **28 of the
55 non-argument `R[]` sites** are a local holding the WORD address of a
CHAR VARYING — the routine writes the length word through it and then
passes the same pointer as a text argument:

    M16[R[ac3 + 8]] = trunc16(ac0) ; XNSTA 0,@[ac3+0x8008];
    rt_call ?WRITE_SCREEN(0x70000260, R[ac3 + 8]) site=701661A3

Declaring such a cell `*i16` *loads* (pointee width is advisory) but then
`[@<name>, 32 varying] = …` **refuses**: `check_piece` (IRExec.cpp:859)
requires the address's `vref` to be `Var::VARYING`. So the workaround is
closed by a rule ir 8 keeps.

**None of the seven needs this** — HIT_ANY_CHAR's VARYING dummy is addressed
directly, and GET_INPUT's is a `*char`. It is a census finding, and it will
be P53's the day it reaches DIED, GET_QUEST or LIST_PLAYERS.
**RULING R5 (recommended): add `*varying <n>` and `*words <n>`** and let
`check_piece` accept a pointer-to-varying as a varying address. Cheap now,
and the alternative (relaxing the tripwire for any word pointer) throws away
the check that makes the tripwire worth having.

### G-3 — **a string literal that is not already in the image cannot be spelled. This is P53's blocker.**

§5.8's literal piece is `[@0xW:b, "<text>"]` where `0xW:b` is *"its byte
address in the image"*, the loader refuses a literal without a `0xW:b`
constant address or outside the block's segment, and the executor **faults
if the bytes differ from memory** (verified lazily through
`Memory::read_byte`). Every one of those rules presumes the literal is
already in `quest.mem` because the 1986 compiler put it there.

**A compiled routine's literal is not in the image.** There is no address to
name, nothing to verify against, and no production that allocates the bytes.

Sites in the seven:

| routine | literal | why it is central |
|---|---|---|
| HIT_ANY_CHAR | `"\vHit any character to continue"` (30 bytes) | 1 of 3 statements |
| HIT_ANY_CHAR | `"\r\v"` (2 bytes, the packed-immediate form) | 1 of 3 statements |
| GET_INPUT | `BITS("001")` — X.CB's `ac0` is *a byte pointer to the character form* | without it the `?READ` call cannot be built |

(and, outside the seven but immediately: REFRESH_SCREEN's three.)

So **two of HIT_ANY_CHAR's three statements and one of GET_INPUT's two are
unexpressible in ir 8 as specified**, and widening the `X.CB` callee rule
(§1.7) does not help, because X.CB's second argument is a pointer to bytes
that do not exist yet.

**RULING R6 — this needs a decision and it is the most consequential one in
this gate.** Options:

- **(a) An initialised `v`.** `v <ENTRY>.v<k> char <n> = "text"` — the
  loader places it at 0x76 as it places any `v` **and writes the bytes** at
  load. The literal piece then spells `[@bp(<name>, 0), n]`. Costs: one
  declaration form, ~25 lines, and one honest admission — 0x76 stops being
  "nothing is initialised" (§5.10.6). **My recommendation.** It keeps
  literals inside the storage model that already exists, it needs no new
  space, placement to 0x74 later is the same substitution as for any `v`,
  and the existing lazy image-verification rule stays exactly as it is for
  image literals (which is what the book uses) rather than being weakened
  for everyone.
- **(b) A data section.** A new space and a new provenance input. More
  machinery, and it invents a second storage concept for one purpose.
- **(c) Defer to P53.** Then P53 cannot compile HIT_ANY_CHAR or GET_INPUT,
  which are two of its seven targets. I do not recommend finding this out
  in P53.

### G-4 — the valued GAME call has no home yet

PICK_X_Y's three `?RANDOM_NUMBER` results arrive in `ac0` and §6's split
covers them. A valued *game* routine returns by **slotpatch** (Salvage F4:
the callee stores into the saved-ac0 image of its own frame, `wp(ac3,-8)` /
`wp(ac3,-7)`, 16 addrbook entries). None of the seven calls one, so this is
not a blocker — but `a` cells give a routine a declared *signature* and a
declared signature with no return is half a signature. Recording it; no
ruling asked.

---

## 4. The two censuses

### 4.1 The non-argument `R[]` sites — **C-4: the prompt's arithmetic is off, and in a way worth stating**

Measured over `quest.ir2.book`, comments stripped (`;` first, IR.md §2):

| | prompt | measured (book) | measured (stock) |
|---|---|---|---|
| lines containing `R[` | "1,032 total" | **1,032** | 914 |
| `R[]` **occurrences** | — | **1,035** | 917 |
| argument slots `R[acN + -d]` | 978 | **978** + 2 spelled `R[wp(ac3, -12)]` = **980** | — |
| statics | "25" | **27** | — |
| positive frame offsets | "~24" | **24** + 4 spelled `R[wp(ac3, d)]` = **28** | — |
| **non-argument total** | **"49"** | **55** | — |

The prompt's three static counts (×22, ×3, ×2) are each correct and sum to
**27**, not 25; and four sites hide in the `wp()` spelling, which a
`R\[ac\d` grep misses. This is the same slip P46 found and named — *"the
prompt's 198 sites was the line count"* — arriving from the other direction:
1,032 is lines, 1,035 is occurrences.

**The statics (27).** Three cells, all pointer words in the shared-data page:

| cell | sites | shape | levels | **is the cell written?** |
|---|---|---|---|---|
| `0x70000212` (OBJ_PTR) | 22 | `sx16(M16[R[…]])` ×20, `M16[R[…]] = trunc16(ac0)` at 701745E8, `M16[R[…]] = nadd(M16[R[…]], 1)` at 70174209 | 1 | the **pointee** is written (a counter, ×2); the pointer word itself is not written by any of the 22 |
| `0x70000210` (SD_PTR) | 3 | `M32[0x74003BFC] = R[…]` — `LPEF @[…]`, the resolved value pushed as an argument | 1 | no |
| `0x700007A0` | 2 | `M32[R[…]] = acN` — a store through the pointer, in LOCK_FILE's hand-built range (block 7015C2CD) | 1 | no |

One statement is worth quoting because it proves the equivalence the rewrite
rests on, in the artifact, without needing an argument:

    [@((sx16(M16[R[0x70000212]]) * 20) + M32[0x70000212] - 15), 30 varying] = …   (70174265)

`R[0x70000212]` and `M32[0x70000212]` appear in the same expression — the
resolved form and the plain fetch, side by side, on the same cell.

**The positive frame offsets (28), and the question the prompt flags.** All
28 are one idiom — a local holding the word address of a CHAR VARYING, the
length word written through it, the same pointer handed to `?WRITE_SCREEN`:

| routine | slot | `R[]` sites | **writes to the cell** |
|---|---|---|---|
| DIED | +8 | 4 | **12** |
| DISPLAY_MAGIC | +106 | 2 | 2 |
| DISPLAY_INVENTORY | +42 | 2 | 7 |
| GET_QUEST | +562 / +510 | 2 / 2 | 3 / 8 |
| LIST_PLAYERS (in the `.1` range, F10) | +16 / +568 | 3 / 2 | 0 / 2 |
| KILL_PLAYER.2 | +18 | 3 | 0 |
| OP_EDIT.4 / .6 / .8 | +20 / +32 / +16 | 2 / 2 / 2 | 2 / 3 / 2 |
| ALLY_PLAYER.1 | +16 | 1 | 0 |
| STORE.1 | +64 | 1 | 1 |

**So yes — a local's address value is assigned, and often: 42 writes across
11 cells, twelve of them to DIED's one slot.** The prompt asks whether that
is "a case this design assumed away". My read: **it is not, and the design
is better for it than the prompt expects.**

- ir 8 already gives the construct. A pointer-valued local is a `v` of
  pointer vtype, and `DIED.v4 = wp(…)` is the variable form doing exactly
  what it was added for. The book's `M16[R[wp(ac3,8)]]` is one star deeper
  than ir 8's `M16[DIED.v4]` — the artefact prompt §2 describes, confirmed
  at 28 sites.
- It **strengthens** prompt §4. The rewrite's precondition (bit 31 of the
  fetched word is clear) was said to be provable for arguments and merely
  assumed for statics. For these 28 the routine computes the address itself,
  from `wp()`/`bp()`, so it is provable too. The assumed-only bucket is just
  the 27 statics.
- What it does expose is **G-2**: 28 sites want `*varying`, and the type
  list has no such thing.

**The 41 `R[ac2 + -d]` sites are not what the bucket says.** Every one is in
a nested (`.N@`) entry, every one is preceded by
`ac2 = M32[wp(ac3, -6)]` — the static-link load — so these are **uplevel**
argument reads: Salvage F2's triple indirection (link → arg slot → datum),
not the routine's own arguments. Distribution: ALCHEMIST_HOME.1 14,
FIRE.3 14, BOAT.1 5, FIRE.2 3, MOVE_PLAYER.1 3, BARGAIN.1 1, BARGAIN.2 1.
They matter here because DESIGN §4.1a makes them **free** in the naive form:
`ALCHEMIST_HOME.1` names `ALCHEMIST_HOME.a1` directly, cross-entry, and no
link exists. Their L2 rewrite is the static-link reintroduction, not the
plain deref rewrite — so "one rule, many applications" covers 994 of the
1,035 sites, not all of them. Worth saying in the spec, since the prompt's
§4 claims the rewrite covers "all ~1,000 dereference sites".

**Levels followed: one, everywhere.** No `R[]` in either artifact contains
another `R[]`, and no site follows a chain in practice — see C-3.

### 4.2 The optional-argument mechanism — **F5's second spelling is neither PL/I nor hand assembly. It is not an optional-argument mechanism at all.**

The prompt asks whether F5's null test is a PL/I convention or a hand-written
artefact, and suspects it is unsound because an unsupplied argument was never
pushed. The answer is a third thing, and it is visible in the body.

RETURN_MESSAGE @70176FDD, from the book:

    block 70176FDD   ac2 = M32[wp(ac3, -16)]          ; arg 3's SLOT
                     goto [70176FE2, 70176FE3] (ac2 == 0)
    block 70176FE3   ac0 = 0x70000CCD:0               ; "Unexpected error" (F11)
                     …
    block 70176FED   ac2 = bp(ac2, 2)                 ; the data, past the length word
                     ac0 = sx16(M16[R[ac3 + -16]])    ; the length word
    block 70176FF5   ac0 = M32[R[ac3 + -14]]          ; arg 2 — the code
                     …
                     ac0 = M32[R[ac3 + -12]]          ; arg 1 — the severity

**Three observations, each decisive on its own.**

1. **The test is on argument 3, which is supplied at BOTH arities.** Arg *N*
   lives at `wfp−10−2N`, so −16 is arg 3. A 3-argument call pushes arg 3; a
   6-argument call pushes arg 3. A null there cannot discriminate 3 from 6.
   It is a **null-pointer test on a supplied by-reference argument** — the
   caller passing "no message" — and F11 already names the consequence: the
   default literal at 0x70000CCD.
2. **The body never reads arguments 4, 5 or 6.** Its only argument reads are
   −12, −14 and −16. `mixed:3/6` produces no arity-dependent code whatsoever;
   it records only that call sites vary.
3. **The null arm is dead in this program.** All five sites supply a
   non-null arg 3. The four 6-argument sites push a message temp the caller
   just built by WCMV (`XPEF [ac3+0xC]` at 7015BE6D and its three siblings).
   The hand-built 3-argument site pushes the address of a word it had just
   pushed —

       70169b75 NLDAI 3769,0 ; WPSH 0,0 ; LDASP 0      ac0 = &(a word holding 0x00000EB9)
       …        WLDAI 1,0x0C00 … WSUB 2,2 …
       70169b81 WPSH 0,2                               first push = arg 3 = ac0
       70169b82 LCALL [0x70176FDD],3

   — non-null, so even LOCK_FILE takes the *other* arm, reading a length
   word of 0 out of the wide's high half.

**Conclusion.** F5 records two spellings of one mechanism; there is only
**one** optional-argument mechanism in the program, and it is
REFRESH_SCREEN's marker-word read of `arg_count`
(`sx16(M16[wp(ac3,-9)])`, block 70176A93, tested `== 0`). The prompt's
instinct that the null test "looks unsound" is right, and for a stronger
reason than the one given: it is unsound *as an arity test* because it tests
a slot that is always supplied — it was never an arity test.

**Consequences, which is what the prompt says this decides:**

- **`arg_count` is the only discriminator**, exactly as prompt §3 assumes.
  Nothing else is needed and no second spelling has to be supported.
- **The mechanism rests on a single witness.** REFRESH_SCREEN is it. F5's
  "two spellings, so which one PL/I picks is unfalsifiable" is replaced by
  "one spelling, one witness" — weaker in one sense, much cleaner in
  another, and no longer unfalsifiable in principle.
- **R39 does not need to be invoked.** F6 is still true (the 3-argument site
  is hand assembly) but it turns out not to be load-bearing, because the
  disqualifying fact is in the *callee's* compiled body, not in the caller.
- **`docs/Salvage.md` F5 needs a correction** (outside my boundary,
  recommended in §6).

---

## 5. What in the prompt did not survive contact

**C-3 — §4's literal ban would refuse the book 660 times.** *"Refuse a
literal in the top-bit-set range, as the loader already refuses 0x76/0x77
literals."* Measured: **660 top-bit-set hex literals in `quest.ir2.book`**
(660 in stock too) — `ac1 = 0xFFFFEEEE ; NLDAI 61166,1;`,
`ac0 = add(ac0, 0xFFFFDB10) ; WNADI 0,56080;`, `ac2 = 0xFFFF8000`. These are
sign-extended 16-bit immediates, not addresses. The rule as written breaks
the strict surface on the first load, which makes it a STOP under Part 2's
own rule.

It is also unnecessary, because the guarantee §4 wants comes from somewhere
better. **Every address an ir 8 program can produce comes from `wp()`,
`bp()`, or a `v`/`a` name.** `wp` is `((b+d) & 0x0FFFFFFF) | seg` and `bp`
is `set_byte_segment(seg&7, …)` (IRExec.cpp:1212–1217), with `seg` =
0x70000000 for every block including a 0x77 one; a name is a 0x76 constant.
**Bit 31 is clear by construction in all three.** Measured confirmation:
literals in an M/R **index** position with bit 31 set — **0**; `wp`/`bp`
base literals with bit 31 set — **0**.

**RULING R7 (recommended): scope the refusal** to a literal used (a)
directly as an `M8`/`M16`/`M32`/`R` index, or (b) as the value assigned to a
pointer-typed cell. Costs nothing on the book (0 sites) where the unscoped
rule costs 660, and it is the form that actually bears on the rewrite's
precondition. The derivation above should go in the spec next to it — it is
the reason, and the literal rule is only the belt.

**C-5 — the `SUB` → `RANGE_CHECK` rename does not fit inside the boundary.**
Measured: 118 `SUB` tokens (9 in `game/quest_rt.h`, 109 across
`game/routines/*.c`), all inside the boundary. But two files outside it
break, and one of them takes a test I *do* own down with it:

- **`compiler/lower_c.py`** dispatches on the identifier by name —
  `if name == "SUB":` (:681), plus :730 and :733. After the rename it
  refuses every routine that uses `RANGE_CHECK`.
- **`compiler/difftest/cgen.py`** *generates* C containing `SUB(…)`
  (:159, :161) and emits `#include "quest_rt.h"` (:365). So the generated
  programs stop compiling natively the moment the header's `SUB` goes away —
  and the harness that runs them, `emulation/tests/run_lowerc_difftest.sh`,
  **is** in my boundary.

This is P46's R1/R2 shape exactly: the rename's chain crosses a line the
prompt drew without measuring it. **RULING R8 (recommended): extend the
boundary to those five lines** — `lower_c.py` :681/:730/:733 and `cgen.py`
:159/:161, plus two comment lines in `make_update_screens_fixture.py` — and
nothing else in `compiler/**`. Precedent: a001/R2 granted `ircmp.py`'s four
lines for the same reason, with the same reasoning: *"a knowingly-red
selftest is not an acceptable resting state."* The alternative — renaming in
`game/` and leaving the compiler to refuse it — is a red difftest that P53
inherits with no note of why.

**C-6 — the `ir 8` bump collides with "regenerate no artifact", and the
difftest goes red regardless.** Two separate problems from one line:

1. Part 2's teeth leg wants `ir 7` in the header to REFUSE. But
   `quest.ir2.book` and `quest.ir2.stock` say `ir 7`, are outside my
   boundary, and boundary 2 says **"Regenerate no artifact."** Refusing
   `ir 7` therefore breaks every lockstep run and the strict-surface leg the
   same prompt requires. Every prior bump regenerated the artifacts in the
   same push; this one is forbidden from doing so.
   **RULING R9 (recommended): accept an `ir 7` header IF AND ONLY IF the
   file declares no `v` and no `a`.** Measured: `quest.ir2.book` and
   `quest.ir2.stock` declare **0** of each, so both keep loading with their
   meaning provably unchanged (the only construct whose meaning moved is
   absent), and the artifacts need not be touched. The rule is checkable,
   narrow, and says out loud what it is: a compatibility window, not a
   dialect. Alternative: extend the boundary to regenerate both artifacts
   through the P33-B chain for a one-line header change — which, as P46
   found, drags `p33.tsv`, `p33.ledger` and `strings.ledger` along with it.
2. **`compiler/lower_c.py` already emits `ir 7` with `v` names in the ir 7
   meaning** (:1025, and its docstring: *"the NAIVE C -> ir 7 compiler"*).
   ir 8 changes what those names mean. Under R9 its output is **refused,
   loudly** — which is the correct failure and far better than being
   silently reinterpreted. But `emulation/tests/run_lowerc_difftest.sh` goes
   red between P52 and P53 no matter which option is taken, because
   updating the emitter **is** P53.
   **RULING R10 (recommended): gate the difftest** with an explicit
   expected-red banner naming P53 and this ruling, in `emulation/tests/`
   (my boundary). Do **not** bump `lower_c.py`'s version string as a
   workaround: that would make its output load under a meaning it does not
   have, which is the one outcome worth avoiding.

**C-1, C-2, C-4** are above in place. Prompt §§1, 2, 3, 6, 7 otherwise hold
up against the artifacts as far as I can measure them at the gate.

---

## 6. `docs/Salvage.md` — recommended corrections (file outside my boundary)

1. **F5** — replace *"Optional arguments have two spellings"*. There is one:
   REFRESH_SCREEN's `arg_count` read. RETURN_MESSAGE's `M32[wp(ac3,-16)]`
   test is a **null-pointer test on argument 3**, which is supplied at both
   arities; the body reads only args 1–3 and never touches 4–6; all five
   call sites supply a non-null arg 3, so the default-message arm is dead in
   this program. Confidence on the surviving half: **Single** (REFRESH_SCREEN).
   §4.2 has the evidence.
2. **F14** — P51 §5 already recommends a wording fix; unchanged by anything
   here.
3. Nothing else I touched needed correction: F1/F2/F3 (the 41 uplevel `R[]`
   sites are consistent with F2 and add ALCHEMIST_HOME.1, BOAT.1,
   MOVE_PLAYER.1, BARGAIN.1/.2 as witnesses), F4, F6, F11, F12, F13, F18,
   F23 all re-seen as stated.

---

## Rulings requested

| # | question | my recommendation |
|---|---|---|
| R0 | is P49 finished — is `emulation/` mine for Part 2? | need the answer before Part 2 starts |
| R1 | bare aggregate name (`char`/`varying`/`words`) as a value or lvalue | REFUSE; legal only in `wp`/`bp` and the string forms |
| R2 | `trunc8` | add as a pure alias of `zx8`, described as an audit-trail name, not a new capability (C-2) |
| R3 | `X.CB`: widen the `rt_call` callee rule, or a new production | widen, with "resolves into the runtime range + empty argument list" |
| R4 | two argument mechanisms — `a` cells for game→game, real pushes for `rt_call` (G-1) | confirm; I write it into §6 that way |
| R5 | add `*varying <n>` and `*words <n>` (G-2, 28 census sites) | add both; relax `check_piece` for pointer-to-varying only |
| R6 | **how a compiled string literal is spelled (G-3) — the blocker** | **(a) an initialised `v`**, written by the loader at placement |
| R7 | scope §4's top-bit-set literal refusal (C-3, 660 book sites) | scope to address positions; put the `wp`/`bp` derivation in the spec |
| R8 | boundary: 5 lines in `compiler/lower_c.py` + `cgen.py` for the rename (C-5) | extend, on the a001/R2 precedent |
| R9 | accept an `ir 7` header when the file declares no `v`/`a` (C-6.1) | yes — 0 declarations in both artifacts, so nothing is regenerated |
| R10 | `run_lowerc_difftest.sh` expected-red until P53 (C-6.2) | gate it with a banner naming P53; do not bump `lower_c.py`'s version |

STOP — awaiting `a001`. (Verified on `main` @ `d52bd80`: every file, line
number and count cited above was measured on that tree this session.)
