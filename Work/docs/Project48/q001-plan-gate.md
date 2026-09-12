# Project 48 — q001: PLAN GATE (Part 1 report, Sep 12 2026)

No code written, no existing doc changed. Read in the prompt's order:
`Project44/DESIGN.md`, `IR.md` (ir 7), `Project46/REPORT.md`,
`emulation/tests/vform_selftest.cpp` + `run_vform_selftest.sh` (the prompt
names `vform_selftest.ir`; the program is a string constant inside the
`.cpp`, which is the file that exists), `Project47/Order.md`, `Salvage.md`,
`METHOD.md`. `docs/attic/` not opened; `quest.ir2.book` not read. For
UPDATE_SCREENS I read `Disassembled/quest.dis`,
`docs/Project34/readable/UPDATE_SCREENS.txt` and `game/declarations.json`.

Rulings needed are marked **RULING** and collected in §6.

---

## 1. The lowering design

### 1.0 Shape of the artifact

`compiler/lower_c.py SRC.c --routine NAME [--entry ENTRY] -o OUT.ir` emits
an ir 7 file (`mode stock`), symbolic-block-only, plus a sidecar
`OUT.vmap` (TSV) that is the compiler's *manifest*: one row per `v` with
name, vtype, size in words, kind (`param` / `local` / `node` / `ret`), the
source anchor, and the source line. The `.vmap` is what the test rig and
the differential driver read; nothing in the IR file depends on it.

Refusals are `REFUSE <construct> at <file>:<line>: <reason>` on stderr,
exit 2. Never a plausible-looking substitute (carried-in ruling 7).

### 1.1 The storage rule

**Every value in the program lives in a `v`, and the compiler emits zero
`t`-places.** `t` is block-local single-assignment scratch that falls out
of lifting one machine instruction (IR.md §5.4); this compiler lifts no
instructions, so it has no occasion to mint one. That is a crisp,
greppable property of the output, and I will assert it in the tests:
`grep -c '\bt[0-9]' OUT.ir == 0`.

`v` allocation, in one pass over the AST, in source order:

| kind | one `v` per | vtype |
|---|---|---|
| `ret` | the routine's return value, if non-void | by return type |
| `param` | each parameter | `u32` (see §1.6) |
| `local` | each declared scalar local | i16/u16/i32/u32 |
| `local` | each declared array local | `words <n>` |
| `node` | **each expression node in the AST** | i32 or u32 (§1.3) |

No reuse, no liveness, no hoisting, no lookahead, no peepholes. Two
occurrences of `i + 1` in a routine are two nodes and therefore two `v`s.
Names are `<ENTRY>.v<digits>` in allocation order, `<ENTRY>` the addrbook
spelling uppercased with `@ADDR` dropped (IR.md §5.10.2). The namespace is
**per family**, not per entry (DESIGN §4.1a): the allocator is keyed on the
family, so `FIRE` and `FIRE.1` draw from one counter. Nothing in the v1
subset produces nested entries (§3), so this is structure-for-later, not
exercised code.

Volume check: ~13 `v`s per source statement in the worst case I hand-traced
(§3.4 below: UPDATE_SCREENS, ~250 `v`s for 72 source statements). The
per-entry 0x76 range is 0x10000 words = 32,768 two-word `v`s. Not close.

### 1.2 The statement shape

Uniform and choice-free, DESIGN §7.1's house style. Operand 1 to `ac0`,
operand 2 to `ac1`, result in `ac0`, addresses in `ac2`:

```
                                  ; node 41: i + 1   (line 12)
ac0 = sx16(M16[UPDATE_SCREENS.v3])      ; read i (i16)
ac1 = 1                                 ; node 40: literal
M32[UPDATE_SCREENS.v40] = ac1
ac0 = ac0 + ac1
M32[UPDATE_SCREENS.v41] = ac0
```

Every node ends with a store of its value to its own `v`; every use begins
with a load from a `v`. There is no in-register forwarding even between
adjacent statements. This is deliberate: every load/store pair we do NOT
emit is a rewrite that cannot be credited later, and the rewrite count is
the metric (carried-in ruling 1, DESIGN §7.2a).

A memory access through a computed address materialises the address into
`ac2` first — `ac2 = M32[<addr node v>]` then `ac0 = sx16(M16[ac2])` — so
that "every address materialised into a base register" holds literally, and
so the base-register dataflow of DESIGN §5.2 has something to find later.

**Only pure operators are emitted.** `+ - * & | ^ /s /u %s %u`, the
comparisons, `lsh`, `tf`, `sx16`/`zx16`/`trunc16`, `~`. No `add`, `sub`,
`mul`, `div`, `cvwn`, `ash`, `nadd` — the effectful family owns c/ovr
semantics (IR.md §5.5) and each effectful statement runs the emulator's
`ovk && ovr` check. The C source says nothing about carry, so emitting an
op that writes flags would be the compiler inventing semantics. Converting
`a + b` to `add(a, b)` where c and ovr are provably dead is a **rewrite**
with a flag-liveness precondition — exactly MathDesign §5's parked
flag-conversion, in the direction already ruled. **RULING R2.**

Consequence worth stating now, because it will look like a regression at
L2: a naive routine's IR and the book's IR differ on *every arithmetic
statement*, not just on allocation. That is the expected starting distance.

### 1.3 Types, promotion, and where masks go

This is the part the prompt says deserves the most differential-test
attention, so it is written as a rule with a stated soundness argument
rather than as a procedure.

**The model is C's, not the machine's.** Every expression node carries a
promoted type, computed by the integer promotions and the usual arithmetic
conversions of C99 §6.3.1.1/§6.3.1.8. After promotion every type is 32-bit:
`int32_t` or `uint32_t`. `int16_t` promotes to `int32_t`. **`uint16_t` also
promotes to `int32_t`** — `int` represents every `uint16_t` value — so
`uint16_t / uint16_t` is a *signed* divide in C. That is the single most
counter-intuitive rule in the set and I expect it to be the first thing the
tester catches me on.

**The invariant, which is the soundness argument:** *the 32-bit pattern in
node N's `v` equals the C value of that node under its promoted type,
represented exactly.* Since every promoted type is 32 bits wide, "represented
exactly" means the pattern IS the value, with no residual mask obligation.
An invariant that holds at every node is checkable one node at a time, which
is why the generator emits flattened programs (§2.2).

Masks therefore appear at exactly the three places C says a conversion
happens, and nowhere else:

1. **Reading a 16-bit object.** `int16_t` → `ac0 = sx16(M16[v])`;
   `uint16_t` → `ac0 = zx16(M16[v])`. This *is* the integer promotion.
2. **Assigning to a 16-bit object.** `M16[v] = trunc16(ac0)`. C's
   conversion to a narrower type is modulo 2^16 for unsigned and
   implementation-defined-but-modular on gcc for signed; `trunc16` plus the
   M16 store rule (IR.md §5.2: stores write `value & 0xFFFF`) is that.
3. **An explicit cast.** `(int16_t)e` → `sx16`; `(uint16_t)e` → `zx16`;
   `(int32_t)`/`(uint32_t)` → nothing (reinterpretation, no code).

Nowhere else. In particular **no mask after an arithmetic operator**,
because the operands were already 32-bit and the result type is 32-bit.
A compiler that masks after `+` on 16-bit-typed operands is wrong, and the
generated corpus will contain `(int16_t)a * (int16_t)b` overflowing 16 bits
specifically to catch that.

Signedness reaches the IR only through the operator: `/s` vs `/u`, `%s` vs
`%u`, `<s`/`<=s`/`>s`/`>=s` vs the `u` forms, and the right-shift form
(§1.4). `==`/`!=` compare 32-bit patterns and need no suffix.

### 1.4 The operators, one line each

| C | IR | notes |
|---|---|---|
| `+ - * & \| ^` | same, pure | 32-bit wrap, no flags |
| `/ %` | `/s %s` or `/u %u` | by the promoted type of the *operation* |
| `~` | `~` | |
| unary `-` | `0 - ac0` | |
| `!e` | `(ac0 == 0)` | yields 0/1 |
| `< <= > >=` | `<s` … or `<u` … | suffix from the promoted type |
| `== !=` | `== !=` | |
| `<<` | `lsh(ac0, ac1)` | |
| `>>` unsigned | `lsh(ac0, 0 - ac1)` | |
| `>>` signed | pure decomposition, §1.4a | no pure `ash` primary exists |
| `&& \|\|` | **control flow**, §1.5 | NOT the IR's `&&`/`\|\|` |

**§1.4a — signed right shift.** IR.md §8 parks a pure arithmetic-shift
primary as "none needed by the census"; `ash` is root-effectful and writes
ovr. So signed `x >> n` (n in 0..31) lowers to the pure form

```
ac0 = lsh(<x>, 0 - <n>)                  ; logical part
ac1 = 0 - (lsh(<x>, -31) & 1)            ; 0 or 0xFFFFFFFF
ac1 = ac1 & ~lsh(0xFFFFFFFF, 0 - <n>)    ; the high n bits
ac0 = ac0 | ac1
```

each step through its own `v` in the real emission. Six statements for one
operator is fine — naive is the point — but it is also *the most intricate
thing the compiler does*, so it is a named suspect in §2.5. The alternative
is to refuse signed `>>`; I recommend against, because it is a good
mask-placement test and a real PL/I construct.

**`&& || !` of the IR are not used at all**, except that `!` appears
nowhere either (C's `!` becomes `== 0`). The IR's booleans are eager
(IR.md §5.3); C's are short-circuit. Lowering C `&&` to IR `&&` would be a
silent semantic change the moment the right operand has a side effect or a
trap — and with `SUB()`'s `assert` in the language (§3.3), the right operand
of a `&&` CAN trap. This is exactly the class of bug the tester exists for,
and I am recording it here as a known trap rather than discovering it later.

### 1.5 Control flow

A block is closed by its terminator; the compiler emits a canonical
partition and never merges (DESIGN §6 — merge and split are rewrites).
A new block opens at: routine entry, each source label, and each arm/join
of a control construct.

| construct | lowering |
|---|---|
| `if (c) A else B` | evaluate c into node `v`; `goto [b_else, b_then] tf(M32[v])`; both arms end `goto [b_join] 0` |
| `while (c) S` | `goto [b_head] 0`; head evaluates c, `goto [b_after, b_body] tf(…)`; body ends `goto [b_head] 0` |
| `for (i; c; u) S` | init inline; then `while`, with a separate `b_step` block holding `u` so `continue` has a target |
| `goto L` | `goto [b_L] 0` (forward references are legal, IR.md §5.10) |
| `break` / `continue` | `goto [b_after] 0` / `goto [b_step] 0` — **RULING R1e** |
| `return;` | `ret` |
| `return e;` | store e to the `ret` `v`, then `ret` |
| `a && b`, `a \|\| b` | short-circuit: a two-block diamond writing 0/1 into the node's `v` |

The condition expression is **always** wrapped `tf(...)`, even when the node
is already a comparison yielding 0/1. Uniform beats clever, and an index
outside [0, count) is a loud executor fault (IR.md §5.1) — I would rather
never be able to produce one.

`goto [false_target, true_target] tf(c)` fixes the polarity choice. DESIGN
§6 (as corrected by P46 F5) says polarity is an oracle/`ircmp` matter with
no ordering behind it, so fixing it at the compiler is free.

### 1.6 Parameters, and the absence of a caller

PL/I passes by reference and the arguments sit at real addresses
(DESIGN §9.1: locals and args are already at 0x74, not on the MV stack). In
the naive form there is no caller and no calling bridge (P46 F7), so:

**A by-reference parameter is a `v` of type `u32` holding the argument's
word address.** `*x` reads as `ac2 = M32[<param v>]` then
`ac0 = sx16(M16[ac2])`. The test rig writes the parameter `v`s before
entering the entry block; the differential driver's native harness passes
real pointers. **RULING R4.**

This is the naive, correct, and boring thing, and it is the same shape the
calling bridge will have to produce later — the bridge's job becomes
"put the right addresses in the parameter `v`s", which is a contained
problem. It also means the compiler needs no notion of argc, frame marker
words or slot layout in v1.

A non-void routine gets a `ret` `v` allocated first. Salvage F4's slotpatch
(the value goes into the saved-ac0 image at `wp(ac3,-7)`/`-8`) is a
*rendezvous* fact and belongs to the harness, not here.

### 1.7 Provenance

Every block header carries `; anchor=<construct path>`
(`fn/stmt3.for/body/stmt1.if/then`) and every statement a
`; <file>:<line> node=<id>` comment. This is DESIGN §7.4's
construct-anchored site addressing, carried from the start rather than
retrofitted — the `b<digits>` name is the runtime realisation, the anchor is
the identity. Costs nothing now and is the thing that makes oracle files
survive an edit later.

---

## 2. The differential tester

**Built first (Stage A), and deliberately so.** Its design below is
committed before the compiler exists, because a tester written second gets
shaped to pass what the compiler already does.

### 2.1 The pipeline

```
seed ──▶ cgen.py ──▶ prog.c ──┬─▶ gcc -fwrapv -O0/-O2 + harness ──▶ native.txt
                              │
                              └─▶ lower_c.py ──▶ prog.ir + prog.vmap
                                        └─▶ lowerc_rig (emulator objects) ──▶ ir.txt
                                                                   compare ──▶ AGREE / DISAGREE
```

`difftest.py` drives it, stores every disagreeing seed, and shrinks it
(delete statements / simplify expressions while the disagreement survives)
to a minimal reproducer. A disagreement is a **stop-and-investigate**, not a
counter to increment.

### 2.2 How programs are generated

`cgen.py --seed N` emits a single `void t(void)` routine plus a manifest of
observable variables. Two classes, both from the same generator:

- **Class A — flattened.** Exactly one operator per statement, every
  intermediate a named variable. The set of named variables then *equals*
  the set of AST nodes, so final-state comparison checks the §1.3 invariant
  **at every node**. This is where the mask evidence comes from.
- **Class B — nested.** Multi-level expression trees, mixed types, full
  control flow. Only the source variables are observable, so it tests the
  flattener, the block partition and the control lowering rather than
  per-node values.

Generation parameters, all seeded and recorded: variable count and type mix
(i16/u16/i32/u32 scalars and arrays); expression depth; operator weights;
statement mix (assignment, `if`/`else`, `while`, `for`, `goto`/label,
`break`/`continue`, nested blocks); constant pool.

**The constant pool is biased, not uniform.** Uniform random 32-bit values
almost never sit on a 16-bit boundary, and the narrowing rules are where the
bugs are. The pool is: `0, 1, -1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000,
0x7FFFFFFF, 0x80000000, -32768, 32767, 65535`, small integers, and uniform
randoms — in that order of weight. It excludes `[0x76000000, 0x78000000)`,
which the loader refuses as a literal anywhere (IR.md §5.1).

**Undefined behaviour is engineered out at generation time**, because UB
makes gcc not an oracle:

| UB | how it is removed |
|---|---|
| signed overflow | `-fwrapv` on the native side; both sides then wrap |
| `/` or `%` by zero | divisors emitted as `(d \| 1)`, never zero |
| `INT_MIN / -1` | divisors additionally `\| 1` makes −1 reachable; guarded by forcing the dividend through `\| 1` on `/s` sites, or by emitting `/u` there |
| shift count ≥ 32 or < 0 | counts emitted as `(n & 31)` |
| uninitialised read | every variable initialised at declaration |
| unspecified evaluation order | the subset has no side-effecting subexpressions at all (§3.2); `++`/`--` are statements only |
| aliasing | `-fno-strict-aliasing`; and the generator creates no aliases (noted as a gap in §2.5) |

Both `-O0` and `-O2` are run. Agreement of gcc with itself across `-O`
levels is a **cheap UB detector**: if the two gcc runs disagree, the
generator emitted UB and the program is discarded with a loud counter, not
compared against us. That counter appearing at all is a generator bug.

### 2.3 How the two runs are compared, and how they agree on "observable"

This is the question the prompt asks, and the answer is a design choice
rather than a mechanism: **the observable state is a set of ordinary source
variables, and both back ends are asked for nothing they cannot naturally
produce.** Neither gcc nor IRExec is instrumented.

- **Native side.** The generator emits a `main()` that calls `t()` and then
  prints each observable variable, in the manifest's order, as
  `name=%08X` after an explicit widening cast fixed by the manifest. Plain
  `printf`, nothing clever.
- **IR side.** `lowerc_rig` (a new `emulation/tests/` file, modelled on
  `vform_selftest.cpp`) loads `prog.ir` with a synthetic addrbook, calls
  `ir->map_pages`, runs `Machine::run_steps(<entry block address>, LIMIT)`
  on a scratch Machine with `lockstep_role = CLONE` — the real dispatch
  path, as P46 established — then reads memory at `ir->v_address(name)` for
  each row of `prog.vmap` whose kind is `local`/`param`/`ret`, and prints
  the same `name=%08X` lines.

String compare. The bridge between the two is the `.vmap`, which is the
compiler's own claim about where it put things; if that claim is wrong the
comparison goes red, which is the correct outcome.

**Final state alone is not enough, so there is a trace — and the trace is
also an ordinary source variable.** The generator threads a checksum through
the program:

```
uint32_t chk = 0;                       /* declared first, observable */
...
chk = chk * 1000003u + (uint32_t)v;     /* at every loop-body head and if-arm */
```

It is an ordinary statement in the subset, so *both* compilers handle it
with no special support, and its final value is order-sensitive and
path-sensitive: a loop that runs a different number of times, an if that
takes the other arm, or an intermediate that is transiently wrong and later
overwritten all change `chk` even when every other final value agrees.

So: **both**, and neither costs an instrumentation hook. Final state gives
the per-node mask evidence (class A); the checksum gives control-flow and
transient-value evidence.

Localisation, since the prompt's §7.3 lesson is that the divergence report
is critical path: on a disagreement, the shrinker runs first, then the
driver reports the first differing observable in manifest order together
with the `.vmap` row (source variable, line, node id, `v` name, 0x76
address). For class A programs that is a source line and one operator.

### 2.4 Corpus and reporting

Target ≥ 2,000 programs across the seed space, of which the prompt's ≥ 200
is the acceptance floor. Every run reports:

- programs generated / compiled / run / compared; refusals by reason;
  gcc-vs-gcc discards (should be 0)
- a **construct census** over the corpus: how many programs contained each
  operator, each type pair, each statement kind, each mask site class. This
  is the honest denominator for the claim, and it is the thing that turns
  §2.5's "constructs not generated" from an unknown into a number.
- a hand-written edge-case suite (`compiler/difftest/cases/*.c`) alongside
  the generated corpus: boundary constants, `uint16_t` division, `(int16_t)`
  cast of a value that does not fit, signed `>>` at n = 0 and 31, an empty
  loop body, a `goto` backwards into a loop, `&&` whose right arm traps.

### 2.5 What class of bug would this design MISS

Written before building it, and it will be restated in the REPORT.

1. **Anything the executor gets wrong.** The rig runs the real `IRExec`, so
   this tester proves the lowering faithful to *the IR as implemented*, not
   to the IR as specified. METHOD §2's lesson, one layer up: both sides of
   *this* comparison are not the same artifact, but the IR side and the
   eventual production side are. An IRExec bug is invisible here and
   invisible in lockstep.
2. **Constructs the generator never emits.** This is the real ceiling and
   the reason §2.4 reports a census. Any operator/type/shape combination
   with a zero in that table is untested, however many thousands of programs
   ran.
3. **Values the generator never produces.** Mitigated by the biased pool,
   not removed. A bug that only fires at one specific 32-bit value survives.
4. **UB, in both directions.** Where a generated program is UB, gcc is not
   an oracle and a "disagreement" may be noise; conversely a generator that
   silently avoids a legal-but-hard region reports green for the wrong
   reason. The gcc-vs-gcc discard counter detects the first, nothing detects
   the second.
5. **Everything outside the value domain.** c/ovr, the `ovk` throw, execution
   counts, block ordinals, register residues at a rendezvous, Mapper's view
   of a 0x76 pointer (P46 §3 lists that one explicitly), memory outside the
   declared `v`s, and timing. The naive form touches none of these; the
   harness project will, and nothing here speaks to it.
6. **Uninitialised reads.** IR `v`s read zero from fresh pages (IR.md
   §5.10.6); C locals are indeterminate. The generator initialises
   everything, which means the behaviour on an uninitialised read is
   **deliberately untested** rather than tested-and-equal.
7. **Aliasing.** With every `v` private, two source objects can never alias
   in the IR. The generator creates no aliases, so a lowering that assumed
   non-aliasing would pass. By-reference parameters are the natural way to
   create one; I will add aliased-parameter cases to the hand suite, but the
   generated corpus will not have them in v1.
8. **Wrong acceptance.** The tester compares programs the compiler
   *accepted*. It cannot see a construct the compiler should have refused
   and instead lowered to something that happens to agree on the generated
   values — carried-in ruling 7's failure mode exactly. Only the refusal
   list and its own tests guard that.
9. **The `SUB()` bounds check.** Natively `SUB(i, n)` is identity
   (`quest_rt.h`, non-translator view), so the `assert` we emit is compared
   against nothing. §3.3 proposes fixing this rather than living with it.
10. **Shared misunderstanding between generator and compiler.** Both are
    mine and both read the same subset definition. If I have the C semantics
    of a construct wrong in the same way in both, the corpus agrees. The
    hand suite is the only guard, and it is a weak one; gcc-vs-gcc at two
    `-O` levels catches the subclass where my misunderstanding is UB.

Items 1, 2 and 10 are the ones I would bet on being the residual risk.

---

## 3. UPDATE_SCREENS against the v1 subset

Read fresh from `Disassembled/quest.dis` 7017d635–7017d6a8 (68
instructions, 18 blocks) plus `docs/Project34/readable/UPDATE_SCREENS.txt`
and `game/declarations.json`. I have **not** yet opened
`game/routines/UPDATE_SCREENS.c` beyond noting that it exists; the REPORT
will say whether P35 was right, per the prompt's Stage C.

### 3.1 What the routine does

`UPDATE_SCREENS(x, y, cell)`, argc 3, frame 0x05, all three by reference.
`SD_PTR` is the static pointer at 0x70000210; `PLAYER` is the 686-word
record table based on it (bound 10, `declarations.json` `tables.PLAYER`).

```
n = SD_PTR->player_count               ; XNLDA 0,[ac2+0x2B]  → K=43
for i = 1 to n:
    if ABS(PLAYER[i].fm589 - *x) > 4:  continue     ; 7017d64e..d65e, bound 4
    if ABS(PLAYER[i].fm588 - *y) > 5:  continue     ; 7017d65f..d679, bound 5
    PLAYER[i].screen[ *x - (PLAYER[i].fm589 - 5) ]
                    [ *y - (PLAYER[i].fm588 - 6) ] = *cell
```

Checks that fix the reading: `WSGE 2,2` is the XX==YY compare-against-zero
form (IR.md §5.6), so `WSUB;WSGE;WNEG` is the ABS diamond, not a
self-compare. The three `DERR 17` clusters are `SUB(i,10)` twice and
`SUB(·,9)` / `SUB(·,11)` on the two subscripts, matching
`PLAYER` bound 10 and `screen` dims [9,11]. The final store's displacement
`0x7D9D` is −611 on the 15-bit signed field, and
`declarations.json` gives `screen` K = −611 with strides [22, 2] — i.e. the
1-based origin is already folded into K, and −611 = −587 − 22 − 2 puts the
array immediately below `fm588` at −588. The two ABS bounds (4, 5) and the
two subscript bounds (9, 11) are consistent: |Δx| ≤ 4 gives dx in 1..9,
|Δy| ≤ 5 gives dy in 1..11. The reading closes.

Two things the *book* does that the naive form will not, both expected:
`local_6` is a hoisted scaled-subscript temp reused across the second half
(P3/P11 in Salvage), and `local_8` is a hoisted element pointer. Those are
DESIGN §4.2 transformation-created `v`s. The naive form recomputes.

### 3.2 In the subset, present and sufficient

`int16_t`/`int32_t` scalars and by-reference parameters; assignment;
`+ - *`; comparison; `if`/`else`; `for`; `return`. Nothing needs strings,
bits, floats, twins, ON-units, optional arguments or a static link, and
**there is no runtime or game call at all** — which is why Order.md puts it
second and calls it "the bridge-free first if P48 is late".

### 3.3 What it needs that the v1 list does not have — five items

**(a) Statics and record tables.** `SD_PTR->player_count` and
`PLAYER[i].fm589` are reads through a *static pointer at a fixed word
address*, not through a parameter. The subset's "arrays and record field
access as `game/quest_rt.h` spells them (`ARRAY1`, `SUB`)" clearly intends
this, but its "pointers beyond by-reference parameters" clause excludes it
on a literal reading. Needed: the statics and tables of
`game/declarations.json`, accessed by the `declarations.h` spellings —
`SD_PTR->field` and `TABLE[i].field[a][b]`, lowered to
`base = M32[<static addr>]`, `addr = base + i*stride + a*s1 + b*s2 + K`.
The compiler reads `declarations.json` for stride/K/width; it invents no
geometry. **RULING R1a.**

**(b) `ABS`.** A PL/I builtin, spelled as a function call in `quest_rt.h`
and `inline` in the C++ view. Function calls are refused, so either ABS is
a recognised **builtin lowered inline** (a compare/negate diamond, three
blocks) or the C is rewritten with `if`. I recommend the builtin: it is
what PL/I has, gcc agrees with the inline definition, and rewriting it away
would be the C accommodating the compiler. **RULING R1b.**

**(c) `continue`** (and `break`, for free). Both routine `continue`s are
the natural spelling of `WBR -25`/`-52` back to the loop step. Pure
structural lowering, no new IR. **RULING R1e.**

**(d) `++` as a statement.** `for (i = 1; i <= n; i++)`. Statement position
only — never inside an expression — so evaluation order stays a non-issue
(§2.2). **RULING R1c.**

**(e) Explicit casts** between the four integer types, which §1.3 needs as
its only conversion spelling. **RULING R1d.**

Not needed by UPDATE_SCREENS but proposed with them because they are the
same one-line-each change and the tester wants them: 2-D subscripts (implied
by "arrays"), `&&`/`||` (short-circuit, §1.5), `!`, and `unsigned char` as
`u16`-backed per Salvage F13 (GET_INPUT's, not this routine's).

**One item I want changed rather than added: `SUB()`.** In `quest_rt.h`'s
non-translator C view `SUB(i, n)` is `(i)` — identity. So the `assert` the
compiler emits for a subscript check is compared against nothing on the
native side (§2.5 item 9), and UPDATE_SCREENS has three of them. I propose
`SUB` become, in the native harness view, a real check that prints
`TRAP DERR17 <file>:<line>` and exits; the rig catches IRExec's assert
throw and prints the same line; the driver compares traps as ordinary
observable output. That makes the bounds checks differentially tested for
one macro's worth of work. It touches `game/quest_rt.h`, which is outside
my write boundary — hence a **RULING (R5)**, not a change.

### 3.4 Sizing the routine

Hand count from the disassembly: 3 params, 2 scalar locals (`i`, `n`), 2
loop-implicit blocks, 3 `SUB` checks, 2 ABS diamonds, 1 two-dimensional
store. Expression nodes ≈ 120, so ≈ 125 `v`s (≈ 250 words) and ≈ 25
symbolic blocks. Well inside the per-entry ranges.

The rig fixture needs: a page at 0x70000210 holding SD_PTR; a player table
of 10 × 686 words plus the negative-displacement window (min K seen −686),
so ≈ 7,600 words ≈ 8 pages; the three argument cells; and hand-computed
expectations for a handful of (x, y, cell) inputs — one hit, one miss on
each ABS bound, one at each subscript boundary, one that must trap DERR 17.

---

## 4. Sizing

| item | lines | note |
|---|---|---|
| `compiler/lower_c.py` | 1,300 ± 300 | front end glue, type/promotion model, lowering, emitter, refusals |
| `compiler/difftest/cgen.py` | 450 ± 150 | generator + native harness emission |
| `compiler/difftest/difftest.py` | 300 ± 100 | driver, compare, shrinker, census |
| `compiler/difftest/cases/*.c` | 250 ± 100 | hand edge cases |
| `emulation/tests/lowerc_rig.cpp` | 300 ± 80 | generic v-form runner + `.vmap` dump |
| `emulation/tests/run_lowerc_difftest.sh` | 60 | |
| `emulation/tests/update_screens_rig.cpp` | 220 ± 60 | fixture + hand-computed expectations |
| `game/routines/UPDATE_SCREENS.c` (fresh) | 35 | |
| `docs/Project48/REPORT.md` | 450 | |
| **total new** | **≈ 3,100 ± 900** | |

Wall time, in-container: Stage A ≈ 3–4 h, Stage B ≈ 5–7 h, Stage C ≈ 2–3 h.

**One measured environment fact that affects all of it.** This container has
`nproc = 1` and the emulator tree has 76 translation units at `-O2` with no
object cache in the repo (`*.o` is gitignored). I started a build while
writing this gate: 7 objects in 4.5 minutes, so **≈ 50 minutes for the
first full `make`**, once, before any rig can link. Incremental relinks
after that are seconds. I have accounted for it above; recording it because
P46's sizing note says the equivalent surprise cost it time, and because
the play-driver project owning `emulation/` may rebuild concurrently.

---

## 5. Two things I want on the record before building

**5.1 ir 7 is sufficient for the v1 subset — no IR change requested.**
I checked each construct against IR.md §5.1/§5.10: pure operators, `tf`,
the extensions, `M8/M16/M32` on computed addresses, `words` aggregates by
offset, symbolic `goto` with forward references, `assert`, `ret`,
cross-entry `v` references. The only refusal that bites is `call`/`rt_call`
(P46 F7), which the prompt already excludes. So no STOP under boundary 5.

**5.2 The standing rule, operationally.** Every test program in Stage A and
Stage B is synthetic. The only game routine in this project is
UPDATE_SCREENS, and it is an end-to-end check that the compiler *runs* —
its acceptance criterion is "loads and executes and produces the
hand-computed values in the rig", never "resembles the book". I will not
open `quest.ir2.book` at any point. If UPDATE_SCREENS' naive IR turns out
to be uncomparable to the book in some structural way, that is a fact for
the transformer project, not a defect here.

---

## 6. Rulings needed

| # | ruling | my recommendation |
|---|---|---|
| **R1a** | subset += statics and record tables from `game/declarations.json`, via the `declarations.h` spellings (§3.3a) | **grant** — UPDATE_SCREENS cannot be written without it and the geometry is a read, not an invention |
| **R1b** | `ABS` is a lowered **builtin**, not a call (§3.3b) | **grant** — it is a PL/I builtin; refusing it would make the C accommodate the compiler |
| **R1c** | `++`/`--` in **statement position only** | **grant** — keeps evaluation order a non-issue |
| **R1d** | explicit casts between the four integer types | **grant** — §1.3's only conversion spelling |
| **R1e** | `break` / `continue`; also `&&`/`\|\|` (short-circuit), `!`, 2-D subscripts | **grant** — structural lowering, no new IR |
| **R2** | the naive form emits **pure operators only**; `add`/`sub`/`mul`/`cvwn`/`ash` are rewrites with a flag-liveness precondition (§1.2) | **grant** — the C says nothing about carry; and it makes the expected L2 distance explicit now rather than surprising later |
| **R3** | the compiler emits **zero `t`-places**, asserted in its tests (§1.1) | **grant** |
| **R4** | a by-reference parameter is a `u32` `v` holding the argument address; the rig seeds it (§1.6) | **grant** — it is also the shape the calling bridge will fill |
| **R5** | `game/quest_rt.h`'s native `SUB(i, n)` becomes a real trapping check so subscript checks are differentially tested (§3.3). **Outside my write boundary** | **grant**, and tell me who edits it — otherwise the three DERR sites in UPDATE_SCREENS and every generated subscript check go untested |
| **R6** | signed `>>` is supported by the pure decomposition of §1.4a rather than refused | **grant** — it is the sharpest mask test available; refusing is the safe alternative if you disagree |
| **R7** | the gcc oracle is `-fwrapv -fno-strict-aliasing`, run at both `-O0` and `-O2`, with gcc-vs-gcc disagreement treated as a generator bug | **grant** — without `-fwrapv` gcc is not an oracle for a wrapping machine |

STOP. Waiting for `a001`.
