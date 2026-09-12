# Project 48 — REPORT: THE NAIVE COMPILER

Worker session, Sep 12 2026. Branch `p48-compiler`. Gate: `q001-plan-gate.md`,
rulings `a001` (all eleven granted; `game/quest_rt.h` added to the write
boundary under R5; DESIGN §7.2a amended by the integrator to split the
rewrite-RULE count from the rewrite-APPLICATION count).

**Outcome — the prompt's two numbers and one artifact.**

| criterion | required | actual |
|---|---|---|
| UPDATE_SCREENS compiles, loads, executes | yes | **yes** — 594 + 594 + 198 memory expectations over three scenarios, 0 failed, plus a DERR 17 trap leg |
| generated programs agreeing with gcc | N ≥ 200, zero disagreements | **750**, zero disagreements (plus 8 hand cases) |
| the compiler, runnable without a session | `compiler/lower_c.py` + tests | **`compiler/lower_c.py`**, `compiler/difftest/`, `emulation/tests/run_lowerc_difftest.sh` |

Nothing in this project read `quest.ir2.book` or `docs/attic/`. No test
program is a game routine except the single end-to-end check, whose
acceptance criterion is that it *runs correctly*, never that it matches.

---

## 1. The differential tester

`emulation/tests/run_lowerc_difftest.sh` is the one command. Built in Stage A,
before the compiler existed, exactly as the prompt required — and that
ordering paid for itself twice (§2).

### 1.1 Programs run

| leg | programs | verdict |
|---|---|---|
| generated, seeds 1–100 × classes A/B/C | 300 | 300 AGREE |
| generated, seeds 101–250 × classes A/B/C | 450 | 450 AGREE |
| hand-written edge cases | 8 | 8 AGREE |
| **total** | **758** | **0 disagreements** |

Discards: **0** UB discards across 750 generated programs (gcc `-O0` vs `-O2`
never disagreed with itself), and **0** refusals. The one ERROR seen at any
point was a native timeout, and it was a generator bug (§2.2).

### 1.2 The construct census — the headline

a001: *"the construct census is the central instrument of the whole project.
Every zero in that table is a real hole."* **52 distinct constructs, no
zeroes.** The thinnest rows over the 450-program corpus:

| construct | programs containing it (of 450) |
|---|---|
| `stmt.continue` | 83 |
| `stmt.break` | 80 |
| `index.sub_unguarded` (a subscript that may trap) | 91 |
| `index.sub_inrange` | 120 |
| `cmp.>` | 143 |

Everything else is above 150. So the claim this report makes is: *over these
52 constructs, at these densities, the lowering agrees with gcc.* Anything
outside the 52 is untested, and the 52 are listed in the tool's own output
rather than in prose so the denominator cannot drift from the claim.

Not generated, and therefore not covered: aliased by-reference parameters
(§4.7), compound assignment, `?:`, nested functions / uplevel access, and
every construct outside the v1 subset by design (strings, bits, floats,
calls). Aliasing is the one I would fix first.

### 1.3 How the two sides agree on observable state

As designed at the gate, and it held: **neither back end is instrumented.**
Observables are ordinary file-scope C variables; the native harness prints
them with `printf`, and the rig reads them out of the `v` addresses the
compiler's own `.vmap` claims. Rendering is the raw bit pattern zero-extended
to 32 bits on both sides, so there is no width or sign convention to agree on
separately. A wrong `.vmap` goes red, which is correct — the compiler's claim
about where it put things is under test too.

The trace is also an ordinary source variable: `chk = chk * 1000003u + v`,
threaded through every loop head and if-arm. a001 called this the best idea in
the gate and it earned that — it is order- and path-sensitive, costs nothing,
and needs no hook on either side.

Traps compare as text. `SUB()` now really aborts natively (a001 R5) printing
`TRAP DERR17 <file>:<line>`; the rig captures its own stderr, pulls the
message out of IRExec's `IR ASSERT FAILED` report, and prints the same line.
"Both trapped at the same site" is a string compare.

### 1.4 Teeth — six deliberate bugs, all caught

A tester that has never gone red proves nothing (P46's `-DP46_BROKEN_ALLOC`
precedent). `lower_c.py --mutate` injects one plausible wrong lowering;
`difftest.py --expect-red` succeeds only if the corpus disagrees. Over 18
programs each:

| mutation | programs that caught it (of 18) |
|---|---|
| `no_sign_extend` — read `int16_t` with `zx16` | 7 |
| `cmp_unsigned` — ordering comparisons always `u` | 6 |
| `u16_unsigned` — `uint16_t` promotes to `uint32_t` | 1 |
| `shift_logical` — signed `>>` as a logical shift | 1 |
| `abs_argtype` — the real bug of §2.1, re-injected | 1 |
| `eager_bool` — C's `&&`/`\|\|` as the IR's eager pair | 1 |

**The 1-of-18 rows are the interesting ones, and `eager_bool` is a finding in
its own right (§4.5).**

---

## 2. Every soundness bug the tester found

a001 and the prompt both say a report with zero here is a report to suspect.
There is one in the compiler and one in the tester, and both are worth the
space.

### 2.1 `ABS` propagated its argument's type instead of its declared type

**Found:** seed 2, class B, in the first 120-program run. Shrunk automatically
from 96 lines to 29.

`ABS` is declared `int32_t ABS(int32_t)`. I lowered it as a builtin (a001
R1b) and gave the result node *the argument's* promoted type. For a
`uint32_t` argument that made `ABS(chk)` unsigned. Nothing went wrong there —
the diamond is correct either way, because `0 - x` and `x >=s 0` are the same
bits. The damage was downstream, through the type:

```
(int32_t)chk / (((ABS(chk)) & 0x7FFFFFFF) | 1)
```

`ABS(chk)` unsigned made the divisor unsigned, which made the divide `/u`
instead of `/s`, which turned `-5 / 5 = -1` into a huge positive, which
flipped the `<=` above it into `<=u` and the comparison from 0 to 1, which
landed as an off-by-one in a `uint16_t` three operators away from the cause.

**This is exactly the failure mode the gate predicted for mask placement, and
it is worth noticing that it was not a mask at all.** No `sx16`/`zx16`/
`trunc16` was wrong anywhere. A *type* was wrong, and the type only ever
manifests as an operator suffix. Carried-in ruling 6 says "types compile away"
— they do, into `/s` vs `/u` and `<s` vs `<u`, and that is where they can be
wrong silently.

**Fix:** a builtin's result type is its declared type, exactly as a call's
would be. Re-injected as the `abs_argtype` mutation so it cannot come back.

Why the hand suite missed it: `abs_edges.c` tests ABS's *values* thoroughly,
including `ABS(INT_MIN)`. It does not test ABS's *type*, because a type only
shows up when something else consumes it. Only the generator's willingness to
put `ABS` under a divide under a comparison found it.

### 2.2 The generator could emit a non-terminating program

**Found:** the driver's own 20-second native timeout, seed 29 class A.

`cgen.py`'s `stmt_if` did not thread loop depth, so a loop nested inside an
`if` inside a loop was handed the *enclosing* loop's counter, reset it, and
the outer loop never ended. A generator that can emit an infinite loop turns
every timeout into a question, so this is fixed structurally rather than by
raising the limit: loop counters are depth-indexed, are never in the
assignable set, trip counts are literals, and the `while` form increments at
the top of the body so a `continue` cannot skip it.

---

## 3. UPDATE_SCREENS, end to end

### 3.1 It compiles, loads and runs

`game/routines/UPDATE_SCREENS.c` (52 lines, mostly the derivation) →
**65 `v`s, 16 symbolic blocks, 227 statements, zero `t`-places, zero effectful
operators.** Loaded with the real `quest.addrbook` and run through
`Machine::run_steps` with `lockstep_role = CLONE`.

Three scenarios, expectations generated by
`compiler/difftest/make_update_screens_fixture.py` — an independent model of
the disassembly plus `declarations.json`, which never runs the compiler:

| scenario | expectations | failed |
|---|---|---|
| positive coordinates (six players) | 594 | 0 |
| mirrored (negative) coordinates | 594 | 0 |
| 16-bit-boundary coordinates | 198 | 0 |
| `player_count` past PLAYER's bound | `TRAP DERR17 UPDATE_SCREENS.c:45` | — |

Each scenario checks **every cell of every player's screen** — all 9 × 11,
for all six players — not just the cell a correct reading would touch. That
is not belt-and-braces; it is the fix for a real hole (§4.6).

The six players sit on every boundary the routine has: both ABS limits at
their exact value, and the column and cell subscripts at 1 and at 9 / 11 —
the four places an off-by-one in the folded K would show.

### 3.2 Was P35's C right?

**I consulted it, and yes, it was right.** My independently written body is
token-for-token identical to P35's modulo whitespace.

**That is weaker evidence than it looks, and I want to be exact about why.**
I read `game/routines/UPDATE_SCREENS.c` early in the session, while sizing the
subset for the gate, *before* writing my own. So this is not a blind
rederivation and I cannot claim it as one. What *was* done independently is
the reasoning, and it is written into the file's header so it can be checked:
the `WSGE 2,2` compare-against-zero reading that makes `WSUB;WSGE;WNEG` an ABS
diamond; `0x7D9D` as −611 on the 15-bit field; `−611 = −587 − 22 − 2` placing
`screen` immediately below `fm588`; and the cross-check that the two ABS
bounds (4, 5) and the two subscript bounds (9, 11) are consistent — which is a
genuine independent constraint rather than a restatement, since a wrong K or a
wrong stride would break it.

The one place the two files could have differed and do not is the placement of
`SUB()`: two checks on the guards, none on the store, because the original
reuses the base it hoisted into frame slot 6 and does not re-check. Both files
have it that way.

### 3.3 Subset changes needed — the gate's five, all used

All five of a001's R1a–R1e were needed and all were used: statics and record
tables from `declarations.json`; `ABS` as a builtin; `continue`; `++` as a
statement; explicit casts. Nothing further was needed. UPDATE_SCREENS needs no
call, no string, no bit, no float, no twin and no static link, which confirms
Order.md's placement of it as the bridge-free first target.

---

## 4. What did not survive contact

P46 found three; the prompt says I am the first project to build against
DESIGN §4 and §7.1 rather than read them. Eight below; F3 and F5 are the two I
would want a ruling on.

**F1 — §7.1's worked example is not the naive form.** The example spells the
value into `t0`:

```
t0 = 1000
ac3 = fp
M32[ac3+0x100] = t0
```

but carried-in ruling 2 says `t` is never *chosen*, and ruling 1 says every
value goes into its own `v`. Those cannot both describe the same output. I
resolved it toward the rulings — this compiler emits **zero** `t`-places, and
the run script asserts it by grep. §7.1 should say `v` where it says `t0`, or
say that it is illustrating the *machine-lifting* house style rather than the
compiler's.

**F2 — §4.1's storage table has no row for the thing the compiler actually
allocates.** §4.1 lists source variables; §4.2 adds "compiler-invented
storage", but frames it as created *by a transformation* (the hoisted
subscript). In the naive form the overwhelming majority of `v`s are expression
nodes created by **lowering**, not by any transformation: UPDATE_SCREENS has
65 `v`s of which 4 are source-level (two locals, plus the return-slot
convention and three parameters — 5 counting those) and **~60 are expression
temporaries**.

**F3 — and that corrects §5.1's headline metric, sharply.** "Oracle length is
literally a count of unexplained decisions, starting at *all of them*" reads as
"one entry per `v`". But ~92% of UPDATE_SCREENS' `v`s have nothing at 0x74 to
be *placed onto* — the original never had them. They must be **eliminated**
(consumed within a block, never materialised), which §4.1 already lists as one
of the three ways a `v` can disappear, but which is not a placement decision
and does not belong in the same count. So the metric needs the same treatment
a001 just gave the rewrite count: **`v`s-to-place** (small, bounded by the
original's frame) and **`v`s-to-eliminate** (large, and a measure of how naive
the lowering is) are different numbers and only the first is oracle length.
This extends P46's F15, which made the same observation for blocks.

**F4 — a parameter `v` has nothing to be placed onto either.** a001 R4 made a
by-reference parameter a `u32` `v` holding the argument's address. The
original does not have that: its arguments live at `wfp-10-2N` and are reached
by indirection *through the frame*, so there is no 0x74 cell holding a pointer
that our parameter `v` could be laid on top of. This is not a problem for L1 —
the naive form runs — but the calling bridge will have to *write* these `v`s
rather than place them, and the placement count should not include them.

**F5 — the differential tester is nearly blind to short-circuit evaluation,
and a001 R5 is what saves it.** `&&` and `||` produce the same *value* eagerly
or lazily; the difference is observable only when evaluating the right operand
has an effect. In this subset the only such effect is `SUB()`'s trap. Measured:
the `eager_bool` mutation was caught by **1 program in 18**, and every catch
was a class C program whose right arm contained an out-of-range subscript.

Before a001 R5, `SUB()` was the native identity, so an out-of-range subscript
was an out-of-bounds C read — undefined behaviour, which the generator must
not emit. **So without the trapping `SUB`, the corpus would have contained no
construct capable of distinguishing eager from short-circuit evaluation at
all, and `eager_bool` would have been completely invisible.** R5 was granted to
test the DERR sites; its larger effect was to make a whole evaluation-order
bug class observable. Worth knowing before the subset grows constructs that
can trap or fault in other ways — every one of them is also a
short-circuit detector.

**F6 — UPDATE_SCREENS is structurally blind to sign extension, and no amount
of coverage fixes that.** Building the end-to-end check, a `--mutate
no_sign_extend` build passed it. Not a fixture weakness at first — a property
of the routine: *every 16-bit datum it reads is used inside a difference of
two 16-bit data*, and such a difference is invariant under a uniform +65536.
Positive coordinates cannot distinguish `sx16` from `zx16`; nor can negative
ones (mirroring preserves the invariance). Only a scenario where the
difference itself crosses the 16-bit boundary can — one player at `x = 32767`
against `*x = -32765`, where the signed reading skips (|65532| > 4) and the
unsigned reading hits (|4| ≤ 4).

Such coordinates do not occur in the game: Salvage F18 has the world in the
raw, positive 0x3Bxx space. **So this bug class is unobservable on realistic
data in this routine at L1, at any coverage, forever.** DESIGN §8 says L1 is
behavioural truth on executed paths only; F6 sharpens that to *and only for
bugs the routine's arithmetic can expose* — which is a different axis from
path coverage and is not currently anywhere in §8. L2 would catch it (the
book spells `XNLDA`, a sign-extending load); this is a concrete instance of
why both acceptance levels are needed, and it arrived from the L1 side.

**F7 — a deliberate departure from `quest_rt.h`'s P36 prose, reported as
required.** The header says a bare `int16 = int32` is a translator refusal,
because the DG compiler emits a checked convert (`CVWN`, effectful). This
compiler follows **C**: implicit narrowing on assignment is legal and lowers to
`trunc16` on the store. Under a001 R2 the point is moot at L1 (`cvwn` is an
effectful op we do not emit anyway, and reintroducing it is a rewrite). Both
spellings — with and without the explicit cast — are generated, at 30/23
programs per 36, so the choice is tested rather than assumed.

**F8 — the ir 7 width tripwire never fires in practice.** IR.md §5.10.4's
tripwire checks `M16[<v>]` / `M32[<v>]` used *directly*. The naive form
reaches every array element and every record field through `ac2` (an address
materialised into a base register, per ruling 1), and `<v> + <expr>` forms are
explicitly not checked. So the tripwire covers the scalar case only. That is
not wrong — it caught nothing because nothing was wrong — but P46 F11 expected
it to guard the compiler's aggregate access, and by construction it cannot.

---

## 5. The refusal list as implemented

Every one is a loud `REFUSE <construct> at <file>:<line>: <reason>`, exit 2.

**Refused by design (the project's boundary):** any function call that is not
`ABS`/`SUB` — naming P46 F7, that ir 7 refuses `call`/`rt_call` in a symbolic
block; indirect calls.

**Refused as outside the v1 subset:** floats and doubles; character and string
constants; pointers other than a by-reference parameter, and a dereference of
anything but such a parameter; taking the address of anything; structs and
unions declared in the source; compound assignment (`+=` and friends), with
the message telling you to write it out; `++`/`--` *inside an expression*
(statement position only); `?:`; declarations with initialisers (declare, then
assign — a C initialiser runs once at load, a `v` is only memory); an array
without a constant bound, or one outside `words 1..32767`; a whole-array
assignment; an integer constant wider than 32 bits; a type name not in the
four-kind table; a `goto` to a label not in the routine; `break`/`continue`
outside a loop; a `return` with a value from a void routine and vice versa; a
field not in `declarations.json`; a record table subscripted with other than
one index, or a field given the wrong number of dimensions.

**Not reached, and worth saying so:** no refusal fired during the 750-program
corpus. The refusal paths are exercised only by the subset's own boundary,
which means they are the least-tested code in the compiler.

---

## 6. Sizing: estimate vs actual

| item | gate estimate | actual |
|---|---|---|
| `compiler/lower_c.py` | 1,300 ± 300 | **1,003** |
| `compiler/difftest/cgen.py` | 450 ± 150 | **448** |
| `compiler/difftest/difftest.py` | 300 ± 100 | **373** |
| hand cases | 250 ± 100 | **194** |
| `emulation/tests/lowerc_rig.cpp` | 300 ± 80 | **348** |
| `run_lowerc_difftest.sh` | 60 | **98** |
| UPDATE_SCREENS fixture generator | (folded into the rig estimate, 220 ± 60) | **166** |
| `game/routines/UPDATE_SCREENS.c` | 35 | **52** (mostly the derivation) |
| `game/quest_rt.h` (R5) | not estimated | **+44 / −4** |
| **total new code** | **≈ 3,100 ± 900** | **≈ 2,750** |

The estimate held. The compiler came in ~300 lines under because the type
model turned out to be smaller than expected once it was C's own rules rather
than a machine-shaped one: `promote()` and `usual()` are four lines between
them and everything else follows.

**The environment fact recorded at the gate held too:** the first full
emulator build on this single-core container took ~50 minutes (76 translation
units at `-O2`, no object cache in the repo). Relinks of the rig after that
are seconds. It was started while writing the gate, so it cost nothing.

---

## 7. Files

Branch `p48-compiler`. Written: `compiler/lower_c.py`,
`compiler/difftest/{cgen,difftest,make_update_screens_fixture}.py`,
`compiler/difftest/cases/*` (8 cases + their `.obs`),
`emulation/tests/{lowerc_rig.cpp,run_lowerc_difftest.sh}`,
`game/routines/UPDATE_SCREENS.c`, `game/quest_rt.h` (a001 R5),
`docs/Project48/{q001-plan-gate,REPORT}.md`.

Not touched: `emulation/` outside `tests/`, `docs/IR.md`,
`docs/Provenance.md`, `Disassembled/**`, `quest.ir2.*`, `quest.addrbook`,
`quest.arena`, `compiler/{ircmp,readable,gen_declarations,callgraph}.py`,
`tasks/`, `docs/attic/**`. Nothing regenerated.

**To re-run everything:** `emulation/tests/run_lowerc_difftest.sh [seeds]`.
It builds the rig, runs the hand suite, runs `seeds × 3` generated programs
with the census, asserts the zero-`t` and pure-operator invariants, requires
all six mutations to be caught, and runs UPDATE_SCREENS through four fixtures
including its own teeth check. No lockstep session, no runner box.
