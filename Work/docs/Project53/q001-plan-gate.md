# Project 53 — q001: PLAN GATE

Worker session, Sep 12 2026. Nothing has been written yet except this file.
Read: `docs/IR.md` (all of it; §5.10 closely), `docs/Project52/REPORT.md`,
`docs/Project51/REPORT.md`, `docs/Project48/REPORT.md`, `docs/Salvage.md`,
`docs/Project28/RTConventions.md`, `compiler/lower_c.py` (all 1,112 lines),
`compiler/difftest/{cgen,difftest}.py`, the fifteen `game/routines/*.c`,
`game/{quest_rt.h,declarations.json}`, `emulation/tests/{run_lowerc_difftest.sh,
lowerc_rig.cpp}`, `emulation/quest.addrbook`, and `Disassembled/quest.dis` at the
three addresses of §6. **Not opened: `quest.ir2.book`, `docs/attic/`.**

One thing already running: the emulator object build (~50 min on this
single-core container, P48 §6's recorded fact), started while writing this so
Stage A is not blocked on it. It writes only gitignored `*.o`/`*.d` plus the
tracked `emulation/emulator` binary, which will be restored with
`git checkout --` before any commit (P48 §7's recorded hazard).

**Three of the five things this gate must report are clean. Two are boundary
collisions I cannot resolve myself, and they are §7's questions.**

---

## 1. The ir 7 → ir 8 migration: what `lower_c.py` spells differently, and how big

The compiler's whole output shape rests on one ir 7 fact — **a `v` name is an
address constant** — and ir 8 reverses it (§5.10.9). So this is not a patch in
a few places; it is every emitted line that touches storage. The good news is
that it is *mechanical*: one meaning changed, uniformly.

`lower_c.py` has **72 emit sites spelling `M32[%s]` or `M16[%s]` with a `v`
name inside**, out of ~80 memory forms in the file. Under ir 8 every one of
them is either a refusal (`M<n>[<cell that is not a pointer>]`, §5.10.5) or,
worse, silently means something else. The translation table:

| what it does | ir 7 (today) | ir 8 | where |
|---|---|---|---|
| read a node/local | `ac0 = M32[V]` | `ac0 = V` | `load_reg`, and ~50 inline `emit("ac0 = M32[%s]")` |
| write a node/local | `M32[V] = ac0` | `V = ac0` | `store_reg`, `const_v`, every walker method |
| read an `i16` local | `ac0 = sx16(M16[V])` | `ac0 = V` | `load_reg` — the extension moves into the DECLARATION |
| write an `i16` local | `M16[V] = trunc16(ac0)` | `V = ac0` | `store_reg` — the truncation likewise |
| address of an array base | `ac2 = V + ac1` | `ac2 = wp(V, ac1)` | `address_of_arrayref` |
| read/write through an address in `ac2` | `M16[ac2]` / `M32[ac2]` | unchanged | `load_through_ac2`, `store_through_ac2` |
| a parameter | `v P.vN u32` holding an address | `a P.aN *<pointee>` | `compile_routine` |
| header | `ir 7` | `ir 8` | `ir_text` |

Four consequences worth stating rather than discovering:

**(a) Node `v`s become `i32`/`u32` cells and the masks vanish.** Today every
expression node is a 32-bit `v` read with `M32[]`; in ir 8 it is an `i32`/`u32`
declaration read by name. The invariant the Walker maintains ("the 32-bit
pattern in that `v` IS the C value under its promoted type") is unchanged —
promoted types are always 32-bit, so no node cell is ever narrow and no node
read is ever extended. **All of the sign/width machinery concentrates on the
`local` rows**, which is exactly where C says a conversion happens.

**(b) The `i16` mask moves from the statement to the declaration, and that is a
behaviour-preserving move only if the declaration is right.** `v X.v3 i16`
makes every read `sx16` and every write truncating, by §5.10.4's table. That is
what `load_reg`/`store_reg` do today by hand. The `no_sign_extend` mutation
therefore has to move too: it can no longer corrupt the extension at the read
site because there is no read site — it must emit `u16` where `i16` belongs.
Same bug, one level down, and still caught by the same 7–8 of 18 programs.

**(c) `address_of_arrayref` is the one place where the overload matters.**
`wp(<cell>, d)` means *the address of that cell* plus `d` (§5.2, ir 8), and `d`
is an ordinary expression, so `ac2 = wp(V, ac1)` is the whole of it. I will
**not** fold the element-width multiply into it — `ac1 = ac1 * ew` stays its own
statement, because folding is cleverness and cleverness is a defect (ruling 1).

**(d) `const_text`'s 0x76/0x77 dodge stays.** IR.md §5.1 still refuses a hex
literal in `[0x76000000, 0x78000000)` anywhere, and a generated program's
constant pool can still land there in principle. Unchanged, untouched.

**Size: `lower_c.py` +180 / −120 for the migration alone** (the 72 sites plus
`ir_text`, `compile_routine`'s parameter handling, and the mutation
adjustment). This is the part I am most confident about; it is the rest that
carries the risk.

---

## 2. Byte types: the generator extension and the `KINDS` change

P51 §2 item 5 is the big one and the prompt is right that it is where P48's bug
class lives: `char`/`unsigned char` currently map to `u16`, a one-word cell, so
GET_INPUT's `unsigned char buf[144]` would lower to 144 words instead of 72,
and every byte access would be a word access that happens to agree on small
values. A third width class doubles the promotion surface.

### 2.1 The compiler side

| change | detail |
|---|---|
| `KINDS` gains `"u8"` | `("unsigned char", 0.5 words, False, 8)` — the half-word is the thing that does not fit the existing table, see below |
| `CNAME_TO_KIND` | `char`, `unsigned char`, `signed char` → `u8` (Salvage F13: PL/I CHARACTER is an UNSIGNED byte; `signed char` is not in any of the seven and I would rather refuse it than guess) |
| `promote()` | `u8` → `i32`, same as `i16`/`u16`: `int` represents every `unsigned char` value |
| `new_v` | a byte array becomes `char <n>` (n BYTES, ceil(n/2) words), not `words <n>`; a byte scalar becomes `char 1` |
| `load_reg`/`store_reg` | a byte cell is an AGGREGATE (`char n`) and §5.10.4 refuses a bare aggregate as a value — so a byte scalar is read `ac0 = M8[bp(V, 0)]`… **and that is wrong too**, see §2.3 |
| `address_of_arrayref` | for a byte array the address is `bp(V, ac1)` — bytes, no scaling — and the load is `M8[ac2]` |
| the mask set | `trunc8` on a byte store, `zx8` on a byte read (aliases, IR.md §5.3, P52 §3) |
| pointer parameters | a `unsigned char *` parameter is `a E.aN *char`, and the KIND tripwire enforces `M8` through it |

**`words` is a rational number now and the table does not hold one.** Every
other kind occupies a whole number of words; `u8` occupies half. `new_v` uses
`words * nelem` to size a declaration and the `.vmap` carries `words` and
`elemwords` as integers that the rig multiplies by. The honest fix is to stop
treating a byte array as "N elements of W words" and give `char <n>` its own
branch: `vtype = "char %d" % nelem`, `words = (nelem + 1) // 2`, and a new
`render` value. That is ~25 lines and it is contained.

### 2.2 The generator extension, which comes FIRST

Per the prompt and P48's Stage-A discipline, `cgen.py` learns bytes before
`lower_c.py` does:

- `TYPES` gains `u8` (`unsigned char`, render `w8`)
- `SCALAR_KINDS` gains `u8`, so byte scalars and byte arrays appear in
  `build_storage` at the same rate as the other four
- the constant pool needs no change (it already spans 0/1/127/128/255/256 and
  the negatives, which is precisely the byte boundary set)
- `index_expr` and the array machinery are width-agnostic already
- `prog_main_c` renders a byte as `(uint32_t)(uint8_t)x`
- the census gains `leaf.byte_read`, `narrow.byte_implicit`,
  `narrow.byte_explicit`, `cast.unsigned char` — **four new rows, and a zero in
  any of them is a real hole** (P48 a001)

Size: `cgen.py` **+60 / −15**.

**No UB is introduced.** `unsigned char` promotes to `int` and wraps on
assignment like the other narrow types; there is no new undefined corner.

### 2.3 Two byte questions the spec does not settle, which I will settle by
### reading rather than by asking

- **A byte SCALAR has no vtype of its own.** `char 1` is an aggregate, so
  `X.v3` as a value refuses (§5.10.4) and the access must be
  `M8[bp(X.v3, 0)]`. That is correct and it is also what the machine does
  (HIT_ANY_CHAR's CHAR(1) lives at a word pair and is reached by `XPEFB`), so I
  will take it and record it rather than ask for a `u8` scalar vtype. It costs
  one extra statement per byte scalar access, which is what naive costs.
- **`bp(cell, d)` scales the base to bytes and takes `d` already in bytes**
  (§5.2's recorded asymmetry). So a byte array index needs no `* elemwords`,
  and I must make sure the generic path does not apply one. This is exactly the
  P48 §2.1 shape — a width error surfacing as a wrong operator elsewhere — so
  it gets a hand case (`byte_index.c`) as well as generator coverage.

---

## 3. Which of the seven I expect to compile and load

Scoreable, as asked. "Loads" means `IRExec::load_file` accepts it; nothing
executes (P54 owns that).

| routine | expect | what it needs beyond §1/§2 | risk |
|---|---|---|---|
| **UPDATE_SCREENS** | **compiles + loads** | nothing — no call, no byte, no string (P48 §3.3) | none. It also still RUNS, and its 594/594/198 fixture legs are my regression net for the whole migration |
| **PICK_X_Y** | **compiles + loads** | valued `rt_call ?RANDOM_NUMBER(...)` ×3 with the §5.10.6 valued-call split; `TMP(e)` as a 32-bit dummy `v` ×6; `&SD_PTR->seed` as a `wp()` of a computed address | low. The split forces three extra block boundaries inside what C writes as one expression statement; `r = f(...)` is fine because the assignment is the whole statement |
| **INIT_SCREEN** | **compiles + loads** | two game→game `call`s; the callee's `a` cells DECLARED by the caller (loader requires it, `IRExec.cpp:1352`); **pass-through of its own `*i16` parameter into the callee's `*i16` `a` cell** | low. The pass-through is the line IR.md §5.10.4 calls its clearest improvement — `FAKE_OCEAN.a1 = INIT_SCREEN.a1`, one statement |
| **FAKE_OCEAN** | **compiles + loads** | `MIN`/`MAX` as lowered builtins (the `WSGE`/`WMOV` diamond, exactly as `ABS` is done today — a001 R1b's precedent) | low |
| **HIT_ANY_CHAR** | **compiles + loads** | two initialised `v char n = "…"`; a `varying 30` dummy built as length-word + `[@v, 30 varying] = [@bp(lit,0), 30]`; `rt_call ?WRITE_SCREEN` ×2; a game→game `call GET_INPUT` passing `bp(ch, 0)` into a `*char` `a` cell | **medium.** Three statements, three constructs each new. The `varying` dummy is the one I have not built before |
| **GET_INPUT** | **compiles + loads** | all of §2, plus `rt_call ?READ` with 6 args, `TMP(buf)` as a **pointer-valued** dummy (a `*char` held in a `u32` dummy cell whose ADDRESS is the argument), and `BITS("001")` as the widened non-`?` `rt_call X.CB` with an EMPTY argument list and three register statements before it | **highest.** Every hard item on P51's list is in this one routine. If anything refuses, it is here |
| **FAKE_LAND_MASS** | **REFUSES, unless §7 Q-3 goes my way** | everything FAKE_OCEAN needs, **plus the LANDMASS table** — which the file declares as a local `struct landmass_rec` + `extern ARRAY1(...)`, and `lower_c.py` refuses source-declared structs by design (P48 §5). The geometry belongs in `declarations.json`, which is **outside my write boundary** | this is a boundary question, not a compiler difficulty |

So: **six of seven expected to compile and load; FAKE_LAND_MASS expected to
refuse for a reason that is administrative rather than technical.** With Q-3
granted it becomes seven of seven and I would expect it to land with the other
two MIN/MAX routines.

**The one I would bet against myself on is GET_INPUT.** If it refuses it will be
at `TMP(buf)`: the argument is a dummy holding a byte pointer, so the C
`TMP(buf)` must lower to a `u32` cell assigned `bp(buf, 0)` and then have
`wp(dummy, 0)` pushed — a pointer to a pointer *by value*, which ir 8 permits
only because the dummy is a plain `u32` and not a `**`. I have convinced myself
on paper that this is expressible; I have not written it.

---

## 4. The native-view fixes — and a correction to the number

### 4.1 The count in P52 q002 §3 does not reproduce

The prompt carries P52's "ten of fifteen routine files fail `g++
-fsyntax-only`". **I measure seven.** The table is right; the count is not.

    cd Work/game && for f in routines/*.c; do g++ -fsyntax-only -I. $f; done

fails on FAKE_LAND_MASS, FAKE_OCEAN, GET_INPUT, HIT_ANY_CHAR, INIT_SCREEN,
RETURN_MESSAGE, UPDATE_SCREENS — **7 of 15**, and those are exactly the seven
routines q002's table names (its six rows name seven files). I tried to
reproduce ten and could not: `gcc` in C mode gives 2, and dropping `-I` gives
15. g++ 13.3.0. I am recording this because the number is quoted in a PROMPT
now, and a number that cannot be reproduced decays into folklore. Nothing
downstream depends on it.

### 4.2 The fixes, and whether `quest_rt.h` is enough

| file | error | fix | where |
|---|---|---|---|
| FAKE_OCEAN, FAKE_LAND_MASS | `MIN`/`MAX` undeclared | declare them the way `ABS` is declared — a real `inline` in the C++ view, a prototype in the `__TRANSLATOR__` view | `quest_rt.h` |
| INIT_SCREEN | `FAKE_LAND_MASS`, `FAKE_OCEAN` undeclared | add to the "game routines the pilot routines call" block, which already holds UPDATE_SCREENS/HIT_ANY_CHAR/REPOSITION | `quest_rt.h` |
| HIT_ANY_CHAR | `GET_INPUT` undeclared | same block. Note it is `void GET_INPUT(unsigned char *)` — P51 §2 item 11 | `quest_rt.h` |
| RETURN_MESSAGE | `DATA`, `RETURN$2` undeclared, and a `const void*`→`void*` assignment | prototypes in `quest_rt.h`; the const error is in the FILE | `quest_rt.h` **+ 1 line of the .c** |
| GET_INPUT | `TMP(buf)`: `tmp_arg → void*` ambiguous, `unsigned char* → int32_t` invalid | `tmp_arg` needs a `void*` conversion and `TMP` needs a pointer overload | `quest_rt.h` |
| UPDATE_SCREENS | `SD_PTR`, `PLAYER` undeclared | `#include "declarations.h"` — the one-line fix; its six siblings have it | the .c |

**`quest_rt.h` is enough for five of the six, and it is NOT enough for two
files.** UPDATE_SCREENS needs its missing include (one line, named in the
prompt as in scope). RETURN_MESSAGE needs a `const` corrected on one
assignment — a genuine source bug, not a prototype gap, and the prompt's write
boundary allows `game/routines/*.c` for "the native-view fixes named above".
The prompt names RETURN_MESSAGE's error as `DATA`/`RETURN$2` undeclared and
does not mention the const; **I read the boundary as covering it, because it is
the same file's same native-view failure**, and will say so in the report. If
that is wrong, say so and I will leave RETURN_MESSAGE red and name it.

**The `TMP` one is the interesting fix** and it is not cosmetic. `TMP(buf)`
must mean "a dummy holding buf's byte pointer", so in the C++ view `tmp_arg`
needs to be constructible from a pointer and convertible to `void*`
unambiguously. Today it holds a `union {int32_t; int16_t;}` and offers four
conversions, two of which are viable for `void*` — hence "ambiguous". I will
add a pointer member and an explicit `operator void*()`, and drop nothing. **The
`__TRANSLATOR__` view is unaffected** (the whole C++ block is invisible to it),
so pycparser's parse of all fifteen files does not change — which I will prove
the way P52 proved the rename: parse all fifteen before and after and diff.

Size: `quest_rt.h` **+35 / −6**; `game/routines/` **2 lines in 2 files**.

---

## 5. Anything in ir 8 that cannot express what P51's C says

P52 read all seven and found none (its §3: "Nothing in the seven is now
inexpressible in ir 8", explicitly *not* a claim that P53 can compile them). I
am the first to try compiling, which is the stronger test. **I found nothing
inexpressible either — but four places where the expression is not the one a
reader would guess, and one that is a genuine gap in the C, not the IR.**

1. **`RANGE_CHECK(e, n)` in an argument position needs a block boundary it
   cannot have.** `PLAYER[RANGE_CHECK(*who,10)].screen[…]` is fine — `assert`
   is a statement, not a terminator. But `RANGE_CHECK` inside an argument of a
   CALL is fine too, because the arguments are evaluated into cells before the
   `rt_call` terminator. No conflict. I list it because I expected one and had
   to check.

2. **The valued-call split bites `PICK_X_Y`'s `*x = RANDOM_NUMBER$3(…)`
   harmlessly and would bite a nested call fatally.** None of the seven nests a
   call inside an expression, so this is a refusal I will implement and never
   fire. Recorded so the report's refusal list is honest about what was tested.

3. **`TMP(e)` width comes from the CALLEE's parameter** (carried-in ruling 6),
   which is a lookup in `RTConventions.md` — and that file is prose, not data.
   `?RANDOM_NUMBER`'s dummies are 32-bit (P51 §3, three sites read); `?READ`'s
   arguments 3 and 5 are 16-bit; `?READ`'s argument 2 is a word address holding
   a byte pointer. **Six TMPs in PICK_X_Y, three in GET_INPUT, nine total, and
   I will put the table in `lower_c.py` as data with a comment citing the
   RTConventions row for each.** It is not derivable from the C and it is not
   in `declarations.json`. If a tenth TMP ever appears the compiler must REFUSE
   for want of a row, not guess a width — that is ruling 7 applied to a table.

4. **`BITS("001")` is a call whose result is a temp, in an argument position.**
   `rt_call X.CB` has an EMPTY argument list (§6's widened callee rule) and
   writes through `ac2`; the "result" is the temp cell `ac2` pointed at. So
   `READ$6(…, BITS("001"))` is: build the literal `v char 3 = "001"`, set
   `ac2 = wp(temp,0)`, `ac0 = bp(lit,0)`, `ac1 = 3`, `rt_call X.CB()
   ret=<next>`, then in the next block push `wp(temp,0)` as argument 6. **Two
   calls, three blocks, for one C argument.** Expressible; just not obvious.

5. **The one genuine gap is in the C, not in ir 8: `FAKE_LAND_MASS`'s
   `VARYING(30) name;` field.** The record has a CHAR VARYING member that the
   routine never touches, declared only so the struct's stride sums to 22. In
   `declarations.json` that is a `name_len` field at K 1035288 and the stride
   is a number — no member needs a type. So moving the table to the JSON (Q-3)
   does not just fix the boundary problem, it removes a construct
   (`VARYING(n)` inside a source struct) that `lower_c.py` would otherwise have
   to understand and that nothing needs.

---

## 6. `quest.assumptions` — and the provenance needs one correction

The prompt's seed file is right in substance and **its three writer pcs are a
few instructions early**, which matters because the check is supposed to be
mechanical.

Measured in `Disassembled/quest.dis`:

| address | DIRECT store (writes the word) | prompt says | also address-taken |
|---|---|---|---|
| 0x70000210 | **one**, `7015be48 LWSTA 0,[0x70000210]` | 7015BE43 (`WLSHI 0,10`, 5 instrs earlier) | `7015bed9 LPEF [0x70000210]` → arg 2 of `?GET_SHARED_PAGE @7015bedf` |
| 0x70000212 | **one**, `7015bee6 LWSTA 0,[0x70000212]` | 7015BEE3 (`LLEF 0,[0x70017C00]`, the load feeding it) | `7015bef4 LPEF [0x70000212]` → arg 2 of `?GET_SHARED_PAGE @7015befa` |
| 0x700007A0 | **one**, `7015c2d6 LWSTA 2,[0x700007A0]` | 7015C2CD (`NLDAI 686,1`, 4 instrs earlier) | **none** |

**Three findings:**

**(a) "Sole writer" is true of direct stores and would be FALSE as a claim
about who last wrote the word.** Both 0x70000210 and 0x70000212 are pushed BY
REFERENCE as argument 2 of `?GET_SHARED_PAGE`, so the OS writes them after the
game's own store. That is not a problem for the assumption — it is the *reason*
for it, and the prompt's prose says exactly that ("the buffer passed to
?GET_SHARED_PAGE as argument 2"). But a checker that counts only `LWSTA` sites
would be checking the wrong thing while looking right. **The invariant has two
halves and the file must say both: exactly one direct store, AND exactly the
recorded set of address-taken pushes at the recorded call sites.**

**(b) 0x70000212's game-side store is provable outright, not assumed.**
`7015bee3 LLEF 0,[0x70017C00]` loads a CONSTANT effective address; the store
puts 0x70017C00 in the word. Bit 31 of a ring-7 image constant is clear by
inspection. The assumption is only needed for what `?GET_SHARED_PAGE` writes
over it.

**(c) 0x700007A0's arithmetic is `SD_PTR + 686·n − 642`**, not just "an
offset": `7015c2cf WMUL 1,0` (×686), `7015c2d0 LWADD 0,[0x70000210]`,
`7015c2d4 XLEF 2,[ac2+0x7D7E]` (−642 on the 15-bit field). Worth recording
because 686 is PLAYER's stride and −642 is `declarations.json`'s own
"min K seen −642" for that table: **0x700007A0 is a cached PLAYER element
pointer.** Same provenance, more specific.

### The check I will ship

`compiler/check_assumptions.py`, ~90 lines, exit 2 on any failure, three legs:

1. **the disassembly leg** — for each row, exactly one direct store at the
   recorded pc, and exactly the recorded address-taken pushes at the recorded
   call sites, read out of `Disassembled/quest.dis`. Catches a re-disassembly
   that changes the program's shape.
2. **the translated-source leg** — the one the prompt actually cares about:
   *"the assumption silently stops holding the day a translated routine writes
   one of those words."* A translated routine is a `game/routines/*.c`, so the
   check greps them for an assignment to the declaring name
   (`SD_PTR = …`, `OBJ_PTR = …`) and for any write through them, and fails if
   one appears. Cheap, and it fires on the day it should.
3. **the emitted-IR leg** — any `.ir` given on the command line is scanned for
   a store to the three literal addresses. Wired into the difftest script's
   invariant section alongside the zero-`t` and pure-operator greps, so it runs
   on every corpus run rather than on demand.

**Teeth:** a `--break` flag that pretends a second writer exists, so the check
is demonstrated RED before it is believed green (the `-DP46_BROKEN_ALLOC`
precedent).

**Where the file goes: `emulation/quest.assumptions`** — literally beside
`quest.addrbook`, which lives in `emulation/`. That is inside the directory
boundary rule 2 forbids, which is Q-1.

---

## 7. QUESTIONS

### Q-1 — `quest.assumptions` goes in `emulation/`, which rule 2 forbids

**What I found.** Boundary rule 1 names `quest.assumptions` as mine to write
and says "put it beside `quest.addrbook`; say where in the gate". `quest.addrbook`
is `Work/emulation/quest.addrbook`. Rule 2 says do not touch `emulation/**`
because P54 owns it and runs in parallel.

**Decision needed.** Where does the file go.

**Options.**
- (a) `emulation/quest.assumptions` — literally beside the addrbook, as rule 1
  says. Rule 2's purpose is avoiding collision with P54, and a NEW file that
  P54 has no reason to open cannot collide.
- (b) `Work/quest.assumptions` — top of the work tree, no rule-2 exposure, but
  separated from the artifact family (`quest.addrbook`, `quest.arena`,
  `quest.pushmap`, `quest.synclist`) it belongs to, which is how the next
  session will fail to find it.
- (c) `compiler/quest.assumptions` — unambiguously mine, and wrong: the file is
  a whole-program fact, not a compiler input, and the prompt says a future
  SOLVER reads it.

**RECOMMENDATION: (a).** Rule 1 names the file explicitly and rule 2 exists to
keep P54's edits and mine from colliding; a file P54 never opens does not
collide. I will create it and nothing else in `emulation/`. If you would rather
I not put a byte in that directory, (b) and I will note the location in
`docs/Project53/REPORT.md` and in the file's own header.

---

### Q-2 — the corpus cannot be made GREEN from inside my boundary, in two ways

**What I found.** The prompt's second success criterion is *"the differential
corpus is GREEN again — it is currently gated expected-red (P52), and clearing
it is yours."* Both mechanisms live in `emulation/tests/`:

1. **The gate itself.** `run_lowerc_difftest.sh` exits 0 with a SKIP banner
   unless `QUEST_DIFFTEST_EXPECT_RED=0`. I can run green by setting the
   variable, but the repo's default stays "skipped" until someone flips
   `${QUEST_DIFFTEST_EXPECT_RED:-1}` to `:-0` and deletes the 20-line banner.
   That is a one-line edit to a P54-owned file.
2. **The rig cannot see a byte.** `lowerc_rig.cpp` reads observables out of the
   `.vmap` and renders `w16` (`read_word & 0xFFFF`) or `w32` (`read_wide`) —
   there is no byte class. A `char n` observable printed through `w16` is two
   bytes read as one word and the corpus goes red for a reason that is not the
   compiler's fault. **So "byte types in the generator" needs ~8 lines of
   `lowerc_rig.cpp`: a `w8` render using `M.read_byte`, and `at = base*2 + i`
   for a byte row.**

**Decision needed.** Do I get `emulation/tests/{run_lowerc_difftest.sh,
lowerc_rig.cpp}` — and if not, which half of the criterion do I drop.

**Options.**
- (a) **Grant both files, narrowly**: the banner/default line in the script,
  and a `w8` render class in the rig. ~10 lines total, both in `tests/`, which
  is the part of `emulation/` P54 has no business in (P54's work is `IRExec`,
  `RTBridge`, the LCALL replica). P48 wrote both files; they are the
  compiler's test harness that happens to live next to the emulator because it
  links its objects.
- (b) **Grant neither.** Then: the report says "GREEN under
  `QUEST_DIFFTEST_EXPECT_RED=0`, and the default flip is outstanding", and byte
  observables stay out of the corpus. I would instead generate byte storage
  that is **not** observable and drain it into word observables
  (`chk = chk*1000003u + buf[i];`), so byte loads/stores are exercised on both
  sides and compared through a word. **This has a real hole and I will not
  pretend otherwise: a byte-ADDRESS error that is consistent between the store
  and the load is invisible to it** — which is precisely the bug class §2.3
  says byte support introduces.
- (c) **Grant the rig only**, and leave the script's default to the integrator
  as a one-line follow-up. Gets the coverage, leaves the repo honestly showing
  a gate someone must clear.

**RECOMMENDATION: (a).** The hole in (b) is not a coverage gap, it is a
self-cancelling-error gap, and it sits exactly where P48 §2.1's bug lived. Ten
lines in a test harness is a small price. If parallel-session collision is the
worry, note that P54's prompt is the calling bridge — `IRExec.cpp`,
`RTBridge.cpp`, `vform_selftest` — and neither of these two files is plausibly
on its path. **(c) is my fallback and I would take it happily**; (b) is the one
I would argue against.

---

### Q-3 — FAKE_LAND_MASS needs `declarations.json`, which is not in my boundary

**What I found.** `FAKE_LAND_MASS.c` declares its own `struct landmass_rec`
with a `VARYING(30)` member and an `extern ARRAY1(struct landmass_rec, 1000)
LANDMASS`, because P51 §2 item 9 says the table is "data, not compiler: JSON in
§2.1; declared locally in the file for now". `lower_c.py` refuses
source-declared structs by design (P48 §5) and reads all record geometry from
`game/declarations.json` — which my write boundary does not include. P51 §2.1
has already written the exact JSON, including the `landmass_count` addition to
`obj_ptr_hdr`.

**Decision needed.** How FAKE_LAND_MASS gets its table.

**Options.**
- (a) **Grant `game/declarations.json`** (the P51 §2.1 block, verbatim) **and
  the ~10 lines of `FAKE_LAND_MASS.c` that delete the local struct and the two
  `extern`s.** Smallest change, puts the geometry where every other table's
  geometry lives, and deletes the one `VARYING(n)`-in-a-struct construct
  nothing else needs (§5 item 5).
- (b) **Teach `lower_c.py` to read a source-declared record.** Wrong on the
  merits: it is a second home for geometry that must agree with the first, and
  it is new compiler surface bought for one routine.
- (c) **Refuse FAKE_LAND_MASS and name it as a finding.** Legitimate — the
  prompt's success criterion explicitly allows "or are named as findings with
  the reason" — and it costs one of the seven for an administrative reason.

**RECOMMENDATION: (a).** The data is already written and reviewed in P51 §2.1;
what is missing is only permission to paste it. (c) is a real fallback and I
will take it without argument if you would rather `declarations.json` move
under a single owner — but then six of seven is the ceiling and the report will
say so.

**A caveat I want on the record either way:** P51 §1 marks FAKE_LAND_MASS's
field NAMES as *claimed*, not derived — `cell`, `x1..y2` rest on their uses in
this one routine — and P51 §6.1 asks for a stage-2 reader to check CREATE_MAP's
writer. **Landing the JSON does not upgrade that confidence**, and I am not
proposing to read CREATE_MAP (it is not in this project's scope and the
compiler does not care what the fields are called). If the JSON lands, its
comment should carry P51's `claimed` marker forward.

---

## 8. Plan of work, and sizing

Stage boundaries are pushes to `p53-compiler`; the gate and every `q*.md` go to
`main` (INTEGRATOR §10).

| stage | content | size |
|---|---|---|
| **0 — native view** | `quest_rt.h` prototypes + MIN/MAX + the `TMP` pointer overload; UPDATE_SCREENS's include; RETURN_MESSAGE's const. Proof: 15/15 `g++ -fsyntax-only` clean, AND all 15 still parse identically under `__TRANSLATOR__` | +35/−6 header, 2 lines of .c |
| **A — the generator** | `cgen.py` byte types + 4 census rows; `cases/byte_index.c` + `.obs`; the 6 `SUB(` tokens in `cases/{sub_traps,shortcircuit}.c` renamed (P52 §4 item 5 assigns these to me) and the 2 stale docstrings | +60/−15 cgen, +50 cases |
| **B — the migration** | `lower_c.py` to ir 8, §1's table. Regression net: UPDATE_SCREENS' four fixtures must still pass | +180/−120 |
| **C — bytes in the compiler** | `KINDS`/`char n`/`M8`/`bp`/`trunc8`, §2.1 | +120/−30 |
| **D — calls** | `rt_call` (valued and not), `call`, `a` cells, `TMP` widths as data, `BITS`/X.CB, initialised `v`, the `varying` dummy | +260 |
| **E — the seven** | compile and load each; refusals named | — |
| **F — assumptions** | `quest.assumptions` + `check_assumptions.py` with its `--break` teeth | ~90 + the data file |
| | **total new/changed** | **≈ 900 ± 250** |

The corpus runs at each of A, B, C, D. **Mutations are part of the deliverable,
not an afterthought:** `no_sign_extend` moves to the declaration (§1b), and I
will add **two new ones** — `byte_as_word` (lower `char` to a one-word cell,
the P51 item-5 bug exactly) and `bp_scaled` (apply element scaling to a byte
index, the §2.3 bug) — because a mutation that cannot be injected is a bug
class nobody has looked for.

**What I expect to go wrong**, so the report can be scored against it: the
`.vmap`'s integer `words`/`elemwords` columns meeting a half-word type (§2.1),
and `TMP(buf)` (§3). Both are type-shaped, and P48 §2.1's lesson is that a
wrong type surfaces three operators away as a wrong operator suffix.

---

## 9. What I am NOT doing

- not opening `quest.ir2.book` or `docs/attic/` (and nothing here needed them);
- not running or trying to run compiled IR beyond UPDATE_SCREENS, which already
  ran in P48 and is my regression net — no call executes (rule 4, P54's bar);
- not touching `emulation/` outside whatever Q-1/Q-2 grant;
- not editing the compiler because a match failed — nothing is being matched,
  and the rule stands;
- not reading CREATE_MAP (Q-3's caveat).

**STOP. Waiting for `a001`.**
