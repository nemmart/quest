# Project 56 — q001: PLAN GATE

Worker session, Sep 12 2026. **Nothing was modified and nothing was written
into `compiler/`.** The measurements below were taken with a throwaway script
outside the repo; the tool this project actually ships is specified in §3 and
is not yet built. `docs/attic/` was not opened.

Everything here is measured against `emulation/quest.ir2.book` (P55 `a001`
Q1(a)) with `compiler/lower_c.py` at HEAD, in P53's compilation units.
`compiler/blockcmp.py` reproduces the block columns independently and agrees.

---

## 0. The one-paragraph version

**HIT_ANY_CHAR is the case.** It is the only routine of the seven with a 1-1
block correspondence *and* an isomorphic CFG. It does **not** match at operator
granularity — 1 of its 4 blocks does — but the whole residual is 6 statement
positions, every one of which I can name, and it needs **4 rewrite rules and 4
book-side normalisations**, none of them structural. The prompt's hoped-for
case (1-1 blocks *and* matching operators, leaving only local differences) does
not exist in the seven; HIT_ANY_CHAR is the nearest thing to it and the
remaining differences really are local.

Two things fell out of the measuring that I did not expect and that bear on
more than this project: **the book contains 557 unlifted machine instructions
no rewrite can ever produce** (§7 F1), and **GET_INPUT's equal block count is a
coincidence rather than a correspondence** (§1.2).

---

## 1. The ranking of the seven at operator granularity

### 1.1 The table

Ours and the book, sliced by addrbook range. `iso` is CFG isomorphism under the
positional block bijection. `ops` is blocks whose full operator sequence
matches, counting only where the block counts are equal.

| rank | routine | our blks | book blks | Δ | ΔR (merged) | **iso** | our stmts | book stmts | ratio | **ops** |
|---:|---|---:|---:|---:|---:|:---:|---:|---:|---:|:---:|
| **1** | **HIT_ANY_CHAR** | **4** | **4** | **0** | **0** | **YES** | **8** | **10** | **0.8** | **1/4** |
| 2 | GET_INPUT | 5 | 5 | 0 | 0 | no | 62 | 22 | 2.8 | 0/5 |
| 3 | PICK_X_Y | 20 | 18 | −2 | −1 | no | 285 | 64 | 4.5 | — |
| 4 | UPDATE_SCREENS | 13 | 18 | +5 | +1 | no | 233 | 72 | 3.2 | — |
| 5 | INIT_SCREEN | 25 | 32 | +7 | −1 | no | 429 | 134 | 3.2 | — |
| 6 | FAKE_OCEAN | 43 | 60 | +17 | +2 | no | 638 | 224 | 2.8 | — |
| 7 | FAKE_LAND_MASS | 29 | 64 | +35 | +20 | no | 691 | 255 | 2.7 | — |

The book columns reproduce P55's exactly (5, 4, 18, 18, 32, 60, 64), which is
the check that my address ranges are right.

**The ranking key, stated so it is scoreable.** Residual difficulty is ordered
by (i) whether a block bijection exists at all, then (ii) `|ΔR|`, then (iii)
book statement count — the size of the target — then (iv) our statement count,
the elimination burden. Ranks 3 and 4 are close and the order between them is
a judgement call: PICK_X_Y wins on every block metric, UPDATE_SCREENS wins on
elimination burden and is the only routine that has ever run end-to-end (P48).
I would take **UPDATE_SCREENS third in practice** for the second reason, and
have left the table ordered by the stated key so the disagreement is visible.

**Note that the ratio column does not rank difficulty and must not be used as
if it did.** FAKE_LAND_MASS has the second-best ratio in the table and is the
hardest routine in the set by a wide margin. Ratio measures how naive our
lowering is, not how far the routine is from closing.

### 1.2 GET_INPUT's block count is a coincidence, not a correspondence

5 blocks each, `ΔR = 0`, and the CFGs are **not** isomorphic:

```
ours  b2 -> [b4, b3];  b3 -> b4;  b4: ret          (a shared join that is a ret)
book  AA5E -> [AA66, AA67];  AA66: ret;  AA67: ... ret   (no join at all)
```

Polarity agrees; the arms agree. The book's `WSGTI 2,128` **skips a lone
`WRTN`**, so the early return is the fall-through and the correction path runs
off the end of the routine into its own `WRTN`. We emit a jump into a shared
`ret` block.

I took this for P55 REPORT-2 §4.1's exit-block duplication with a second
witness, and **censused it before writing it down, and it is not that.**

| detector, book-wide over 13,507 blocks | count |
|---|---:|
| blocks that are a bare `ret` | 94 |
| unconditional `goto` **into** a bare-`ret` block | **67** |
| bare-`ret` blocks with more than one predecessor | 15 |
| 2-way blocks whose arm-0 is a bare `ret` and whose arm-1 also ends in `ret` (the GET_INPUT shape) | **1** |
| the same, but arm-1 jumps back to arm-0 (our shape) | **0** |

The book jumps to a `ret` 67 times, so "the book never jumps to a return, it
duplicates it" is **false** and I am recording it as a killed hypothesis. The
GET_INPUT shape has **one witness book-wide, and it is GET_INPUT.** Loosening
the detector (arm-0 ending in `ret` rather than being a bare `ret`) gives the
same 1. Two detectors, same answer, per REPORT §8's rule.

**Consequence: no rewrite may be built for this at the gate.** A one-witness
structural rule is the attic's exact error. GET_INPUT is rank 2 on its numbers
and is harder than its numbers say.

---

## 2. HIT_ANY_CHAR, statement by statement — the whole residual

This is the case, so here is all of it. Ours left, book right, under the
bijection `b0↔7016DE91, b1↔7016DEA7, b2↔7016DEAD, b3↔7016DEBB`.

| # | ours | book | what closes it |
|---|---|---|---|
| — | *(nothing)* | `@7016DE91 WSAVS 0x0009` | **N1** prologue normalisation |
| 1 | `[@wp(v2,0), 30 varying] = [@bp(v1,0), 30]` | `[@wp(ac3,4), 30 varying] = [@0x7016DE07:0, "\x0BHit…"]` | **R1** place `v2`; **R4** relocate `v1` |
| — | *(nothing)* | `ac3 = wfp` | **N3** dead after N2 |
| 2 | `rt_call ?WRITE_SCREEN(1879048800, wp(v2,0)) ret=b1` | `rt_call ?WRITE_SCREEN(0x70000260, wp(ac3,4)) site=7016DEA3` | **R1**; **N2**; **N4** decoration |
| 3 | `GET_INPUT.a1 = bp(v0,0)` | `M32[0x74003F1C] = bp(ac3,4)` | **R1** place `v0`; **R2** arg cell |
| 4 | `GET_INPUT.arg_count = 1` | *(folded into `marker=74003F1E`)* | **R2** |
| 5 | `call GET_INPUT args=1 ret=b2` | `call 7016AA35 args=1 marker=… site=… ret=7016DEAD` | **R2**; **N4** |
| 6 | `[@wp(v4,0), 2 varying] = [@bp(v3,0), 2]` | `ac1 = 0x00020D0B` ; `M32[wp(ac3,4)] = ac1` | **R3** packed immediate |
| 7 | `rt_call ?WRITE_SCREEN(1879048800, wp(v4,0)) ret=b3` | `rt_call ?WRITE_SCREEN(0x70000260, wp(ac3,4)) site=7016DEB7` | **R1** (`v4` merged onto `v2`) |
| 8 | `ret` | `ret` | **matches already** |

`1879048800 = 0x70000260`: a base-10/base-16 rendering difference, not a
divergence. `0x00020D0B` is length 2 in the high half and bytes `0D 0B` in the
low — our `v3` is `char 2 = "\x0D\x0B"`.

**HIT_ANY_CHAR.c predicted three of these before any of this was measured.**
Its header says *"NOT reproduced: the temp's reuse, the packed-immediate form,
LDAFP"* — which is R1's merge, R3, and N3. The C also names R3 as work for a
later stage in as many words. I record this because it is evidence the C is a
good reading, and because it means the first case's rewrite set was largely
derived by the P51 session from the disassembly and is being *confirmed* here
rather than invented.

**And the one place I expected to find the C wrong, it is right.** `bp(ac3,4)`
looked like the caller passing its own varying buffer where our C passes a
separate `char ch`. It is not: `bp` displacements are **bytes** and `wp`
displacements are **words** (IR.md §5.2), so `bp(ac3,4)` is frame word 2 —
`&ch` — while `wp(ac3,4)` is frame word 4, the varying. Two different objects
that happen to spell `4`. The C's header had already worked this out. §7.2a's
escalation order stopped at step 2 with the C exonerated.

---

## 3. What comparison machinery the first case needs

**Very little, and `ircmp.py` is not it.** P55 reused the idea and not the code;
I am doing the same. It parses ir 6, the book is ir 7, we emit ir 8, and its
seven equivalences and slot-bijection dependency were built to serve a
different question.

`compiler/irmatch.py`, new, roughly 300 lines:

1. **Two parsers** — ir 7 (book) and ir 8 (ours) to `(block, [statement])`.
   Comment tails strip first; the book's disassembly provenance after ` ; ` is
   audit trail (IR.md §3) and is not compared.
2. **Book slicing** by addrbook range. Validated: the slices give 5, 4, 18, 18,
   32, 60, 64, which is P55's table.
3. **Block bijection** by CFG order from the entry, then an isomorphism check.
   **If the CFGs are not isomorphic, it refuses** rather than reporting a
   distance — see §7 F2.
4. **A canonicaliser**: numeric literals to a single radix, decoration fields
   per N4, labels mapped through the bijection.
5. **A lockstep walk** reporting the first divergence as *"block 3 of 4 (ours
   `b2`, book `7016DEAD`), statement 1: ours `…`, book `…`"*.

**Not built, and the justification for each:** no alignment or matching
algorithm — the bijection is free once §3 passes; no cost model; no search; no
rewrite-precondition oracle in the divergence report (DESIGN §7.3 wants one
eventually; the first case has 6 divergences and does not need it).

**One thing beyond a lockstep walk, and it is only for §7.2b.** Monotonicity
needs a *distance*, so: `distance = Σ over blocks of Levenshtein(canonicalised
statement lists)`. About twenty lines, defined only where the bijection exists,
and **undefined — reported as such, never as a number — where it does not.**
This is not a search metric and I am not proposing one.

---

## 4. How much slot bijection the first case needs

**The arithmetic is fully determined by the addrbook and needs no dataflow.**
Column 4 of an addrbook line *is* `wfp`: for HIT_ANY_CHAR, alloc_base
`740056EC` + 2·argc(0) + 10 = `740056F6`. Then

- `wp(ac3, d)` → `0x` + (wfp + d)  — displacement in **words**
- `bp(ac3, d)` → `0x<wfp + d/2>:<d%2>` — displacement in **bytes**

Checked against the caller: HIT_ANY_CHAR writes `M32[0x74003F1C]`, and
GET_INPUT's addrbook line gives arg 1 at wfp−10−2·1 = `74003F28` − 12 =
`74003F1C`, with `marker=74003F1E` at +2. It resolves exactly, in both
routines, from the addrbook alone.

**Yes, normalise the BOOK side** (the prompt's suggestion, and I agree). Then
placement stays "substitute one constant for another" and the naive side never
learns frame-relative spelling. Direction matters for the reason the prompt
gives: a bug in a target normalisation mismatches loudly.

**And the first case needs no reaching-definitions at all.** Instead of the
two-register dataflow, a **single-base check**: assert that the routine's only
base-register definition is `LDAFP`-to-frame, and **hard-error otherwise**. For
HIT_ANY_CHAR that check passes. On a FIRE.1-shaped routine it stops the build,
which is what §5.2's inverted check is for. Salvage F23's dataflow gets built
when a routine demands it — not before.

### 4.1 The hazard this creates, which is ruling 4's failure mode exactly

The book's `ac3 = wfp` at `7016DE9D` looks redundant *in the IR text*: nothing
visibly writes `ac3` before it. It is not redundant. The preceding statement is
a folded varying assignment, and **IR.md §5.8's residue table says that
statement writes `ac0`–`ac3`** — `ac3 = src + min(n, len)`. The registers
appear nowhere in the statement's text; the clobber is in the semantics.

So a base-register analysis read off the IR *text* concludes ac3 still holds
the frame after the `WCMV`, which is **wrong**, and it would nonetheless
produce a correct answer at this site because the `LDAFP` restores the frame
one statement later. Wrong analysis, correct match, no signature. That is
ruling 4 in the first routine of the project.

**Therefore, binding:** anything that reasons about base registers over book IR
must consume §5.8's residue table, and the single-base check above must treat
every string statement as a definition of `ac0`–`ac3`. I would rather state
this at the gate than discover it in a report.

---

## 5. The minimum rewrite set for HIT_ANY_CHAR — a prediction

Scoreable. I will be marked against this in Part 3.

**Book-side normalisations** (N — no soundness obligation, but each must
*check* something rather than merely delete):

| id | what | check it performs | applications here |
|---|---|---|---:|
| **N1** | strip the entry block's `@<entry> WSAVS 0x00NN` | NN equals the addrbook frame column. **100/102 live entries agree, 0 mismatches, 2 have no entry WSAVS** | 1 |
| **N2** | `wp/bp(ac3, d)` → absolute, per §4 | the single-base check; §4.1's clobber rule | 4 |
| **N3** | delete `ac3 = wfp` once N2 leaves it with no reader | reported per site, never silent | 1 |
| **N4** | drop `site=` / `marker=`; map labels through the bijection | every label is in the bijection | 4 |

**Rewrites** (R — sound, precondition hard-checked):

| id | rule | precondition | applications here |
|---|---|---|---:|
| **R1** | **placement** `v` → address | for merged `v`s, disjoint live ranges (`v2`/`v4`) | 4 |
| **R2** | **arg-cell bridge**: `ENTRY.aN = X` → `M32[<argcell>] = X`; `ENTRY.arg_count = n` deleted into the call decoration | argcell from the addrbook; `n` agrees with `args=` | 2 |
| **R3** | **packed varying immediate**: `[@wp(X,0), n varying] = [@bp(K,0), n]` → `acR = (n<<16)\|bytes` ; `M32[wp(X,0)] = acR` | `n ≤ 2`; `K` a compile-time constant; `X` capacity `≥ n`; `acR` free | 1 |
| **R4** | **literal relocation**: `bp(v,0)` on an initialised `char n` → the located-literal form `[@0xW:b, "…"]` | `v` is read-only and initialised | 1 |

**Prediction: 4 rules, 8 applications, 4 normalisations, and the routine
closes.** Against §7.2a's ~20-rule tripwire that is 4 entries. R4 may collapse
into R1 (it is placement into code space; only the *form* differs), which would
make it 3.

### 5.1 What this case does not test, stated plainly

HIT_ANY_CHAR exercises **none** of the prompt's expected rewrite classes: the
dereference rewrite (~994 sites), pure→effectful (~every arithmetic
statement), comparison materialisation (36), block merge/split (71), and
**register binding — 0 applications, because the routine contains no
arithmetic at all.**

Closing it validates the *rig* — parser, bijection, lockstep walk, oracle
format, slot bijection, prologue normalisation — and almost nothing about the
*transformer*. That is the right first case for exactly the prompt's reason,
and "HIT_ANY_CHAR closed" must not be read as more than it is.

**And the gap is wider than rank 1.** `docs/Indirection.md` landed on main
while this gate was being written; its per-routine census gives **HIT_ANY_CHAR
0 `R[]` sites and GET_INPUT 0**, with the first sites appearing at ranks 3–7
(FAKE_LAND_MASS 3, UPDATE_SCREENS 4, PICK_X_Y 4, INIT_SCREEN 5, FAKE_OCEAN 5;
21 between them). So the largest rewrite in the program is untouched by the
**first two** cases, not just the first. GET_INPUT is still where the
elimination burden first bites — 62 statements to the book's 22 — but the rule
that bites there is expression-temporary elimination, not dereference.

Two consequences I am taking as binding unless told otherwise:

- **The rewrite is `M32[a] → R[a]` on the inner fetch**, not
  `M32[M32[a]] → M32[R[a]]` on the outer shape. Indirection.md §4 supersedes
  the form in this project's PROMPT table, and the counts are the argument:
  the outer-shape spelling covers 35 of 896 sites because most dereferences
  read a 16-bit datum through a 32-bit pointer. Part 3's `Rewrites.md` will use
  the inner-fetch form.
- **Rank 3 is where the transformer proper is first tested.** That is an
  argument for not lingering on ranks 1–2 once they close.

---

## 6. The oracle file format

**Sites are the `v` itself.** DESIGN §7.4 wants construct-anchored site IDs,
and for placement we already have them for free: `v` names are qualified by
addrbook entry and derived from the source (§4.1), so `HIT_ANY_CHAR.v2` is
stable under every earlier transformation and under renumbering of any other
routine. No IR position is ever named. Our blocks already carry
`; anchor=fn/entry`, `anchor=if/then`, `anchor=X.CB/ret`, which covers the
block-level sites a later case will need.

Worked example — the complete oracle for the first case:

```
# docs/Project56/oracles/HIT_ANY_CHAR.oracle
routine HIT_ANY_CHAR

# placement — site is the v.  frame+N resolves through the addrbook (§4).
place  v0            frame+2          # ch, CHAR(1), even pair (Salvage F22)
place  v2            frame+4          # the 30-byte varying temp
place  v4            frame+4          # SAME temp -- merge; disjoint live ranges, checked
place  v1            0x7016DE07:0     # the literal, in the code image
place  GET_INPUT.a1  argcell          # 0x74003F1C, from GET_INPUT's addrbook line

# rewrites — rule name, then the site, then what the oracle supplies
rewrite pack_varying_imm  at=v4  reg=ac1
```

Six lines. `frame+N` rather than an absolute is deliberate: it is the spelling
HIT_ANY_CHAR.c's own frame comment already uses ("ch at slot 2", "length word
at 4"), so the oracle is checkable by eye against the C and against the
disassembly, and it keeps the addrbook as the single source of the base.

**Oracle length for the first case is 5 placements.** Under §5.1's split only
`v`s-to-place count, and all five are real frame objects, so this routine has
an oracle-length of 5 and **zero** `v`s-to-eliminate — which is another way of
saying it has no expression temporaries, which is another way of saying it has
no arithmetic. Consistent with §5.1's ~92% figure being about routines that
compute something.

---

## 7. Monotonicity — PRE-REGISTERED, before anything is built

Distance as defined in §3. §7.2b asks whether distance is monotone **under
single rewrites**, which is not the same question as whether it is monotone
under the order I happen to write the oracle in. I will measure both.

**I predict monotonicity FAILS, and I predict the mechanism.**

1. Every **placement** rewrite (R1, R2, R4) strictly decreases distance. It
   substitutes one token in one statement and cannot change statement count.
   *Confidence: high.*
2. **R3 applied before `v4` is placed INCREASES distance by 1.** It turns one
   mismatched statement into two mismatched statements; edit distance goes
   1 → 2. *Confidence: high. This is the falsifiable claim.*
3. R3 applied **after** placement decreases distance by 2.
4. Therefore the honest §7.2b answer will be **"monotone under an ordering
   discipline, not monotone under arbitrary single rewrites"**, and the
   discipline is: *rewrites that preserve statement count first; rewrites that
   change it last.*
5. In the shipped oracle order I predict **0 increases and 0 plateaus**. I will
   also run a deliberately reordered replay, because only that answers the
   question §7.2b actually asks, and I predict **≥1 increase** there.

If (2) is wrong — if distance falls or holds flat under R3-before-placement —
then the metric is not counting what I think it counts, and that is the finding
rather than the monotonicity result.

P55's Part 3 is why this is written down first: its prediction was wrong *and*
the fix it had pre-registered against would have moved the number for an
unrelated reason. Filing it is what makes that visible.

---

## 8. What in DESIGN §7 does not survive contact

Four, and the first is the substantial one. §7 has never been built against.

### F1 — §7 has no class for unlifted machine instructions, and the book is full of them

The book carries **557 `@<addr> <OPCODE>` lines** that no rewrite can produce.
Our IR has no addresses to put on them, and IR.md §3 reserves `save <hex>` with
the loader refusing it, so there is no statement form to emit either.

| raw opcode | book-wide |
|---|---:|
| `LCALL` | 159 |
| `WSAVS` | 130 |
| `LJSR` | 130 |
| `XCALL` | 37 |
| `DIVX` | 19 |
| others (FP, `SYSCALL`, `WPSH`, `WFLAD`, …) | 82 |

**Every one of the seven carries at least one** — the entry `WSAVS` — and
GET_INPUT carries a second (the undecorated `LCALL` to `X.CB`, which we emit as
an `rt_call`). §7's four transformation classes all carry naive IR *toward* the
book; none of them can reach a line like this, so as §7 is written the seven
are unmatchable for a reason that has nothing to do with the transformer.

**What §7 needs is a fifth category it does not have: NORMALISATION OF THE
TARGET.** The prompt introduces the idea for the slot bijection and correctly
notes it differs in kind from a rewrite — a bug in it mismatches loudly rather
than breaking the proof. F1 is a second, independent instance, which argues the
category is general rather than a convenience for §5.2. My N1–N4 are written as
that category, and the discipline I am proposing for it is: **a normalisation
must check something, not merely delete.** N1 checks 130 WSAVS operands against
the addrbook and finds 100/102 agreeing with 0 mismatches; that turns a
deletion into a verification.

### F2 — §7.2b's distance presupposes a block bijection that exists for one routine of seven

"Replay each oracle one rewrite at a time and record whether distance falls"
reads as though distance were always available. It is not: 5 of 7 routines have
different block counts, and of the 2 that match, 1 is not isomorphic (§1.2). A
distance over a non-bijection is an alignment guess dressed as a number.
§7.2b should say that distance is **defined only where the block correspondence
is established, and undefined otherwise** — and `irmatch` will refuse rather
than report.

### F3 — §7.4 anchors our sites and says nothing about the book's

§7.4's argument is entirely about our half: don't key oracle sites on IR
position. Good, and already satisfied (§6). But the *book's* only identifiers
are machine addresses — `site=`, `marker=`, and the block labels themselves —
and the comparator has to decide what to do with every one of them. §7.4 offers
no story. N4 is mine; it belongs in §7.4 rather than in a project report.

### F4 — §7's soundness reasoning is over statement text, and the string statements are lossy

§4.1 above in full. A folded `[@a, n varying] = piece` writes `ac0`–`ac3`
(IR.md §5.8's residue table) while naming none of them. Any §7 transformation
whose precondition is a register fact — **binding, the largest oracle-supplied
class there is** — is exposed to this, and the exposure is silent by
construction. §7 should point at §5.8 explicitly where it states the binding
precondition.

This is adjacent to a gap §7.2a has and P55 REPORT §4 already flagged from the
other side: the escalation order runs *C → rewrite* with no rung for **suspect
the lifting**. F4 is not the C being wrong and not a missing rewrite; it is the
IR text being lossy relative to the instructions it stands for.

---

## 9. Questions

### Q1 — the unlifted-instruction class (F1). What does "matches the book exactly" mean?

**Found:** 557 raw `@addr OPCODE` lines book-wide, ≥1 in every one of the
seven, unreachable by any rewrite.

- **(a)** Normalise the book side. Strip the entry `WSAVS`, *checked* against
  the addrbook frame column; report every **other** raw line as a named
  unmatched residual with its opcode and address, so nothing is hidden.
- **(b)** Emit `@addr` lines from the addrbook. Our IR stops loading (IR.md §3
  refuses `save`), which trades P53's standing acceptance bar for a text match.
- **(c)** Declare raw lines out of scope and skip them silently.

**RECOMMENDATION: (a).** It is the only one that both lets the first case close
and leaves a count of what was not matched. (c) is (a) with the evidence
deleted. (b) breaks a bar an earlier project paid for, to buy a line that is
provably derivable from the addrbook anyway.

A consequence worth agreeing explicitly: under (a), "HIT_ANY_CHAR matches the
book exactly" means *exactly, modulo one normalised prologue line, reported*.
I would rather that phrase be ruled now than negotiated in Part 3.

### Q2 — where the oracle files live

**Found:** DESIGN §7 says the oracle sits "alongside the C source", which is
`game/routines/` — inside boundary 2, which I may not write.

- **(a)** `docs/Project56/oracles/<ENTRY>.oracle` for now; move when a project
  owns `game/**`.
- **(b)** Extend my boundary to `game/routines/*.oracle` only.

**RECOMMENDATION: (a).** The move is mechanical, and keeping boundary 2 intact
costs nothing this project needs.

### Q3 — N3 deletes a statement from the target

**Found:** after N2 substitutes absolute addresses, the book's `ac3 = wfp` has
no reader and must go or we can never match. Deleting from the target is a
stronger act than substituting in it, and §4.1 shows the analysis that
authorises the deletion is exactly the one with a silent failure mode.

- **(a)** Delete, and emit a report line per deletion with its site (1 in
  HIT_ANY_CHAR; unmeasured elsewhere).
- **(b)** Keep it, and require our side to produce it with an insertion
  rewrite — faithful, but it makes the naive side learn frame-relative
  spelling, which §4 argues against.
- **(c)** Census `LDAFP` book-wide first, then decide.

**RECOMMENDATION: (a), with the §4.1 clobber rule binding** — every string
statement counts as a definition of `ac0`–`ac3`, so N3 never fires on an
`LDAFP` that is doing real work. (c) is the cautious answer and I do not think
it earns its cost for one site; if you want the census I will run it, and I
would rather be told to than decide to.

---

## 10. What Part 2 does if this is granted

1. Build `compiler/irmatch.py` (§3) and the oracle reader (§6).
2. Close HIT_ANY_CHAR against the oracle in §6, or report exactly what stopped
   it.
3. Run the monotonicity replay in both orders (§7) and report against §7's
   prediction either way.
4. Start GET_INPUT, which is where the elimination burden first has to be paid
   (62 statements to 22), and where §1.2's one-witness structure has to be
   handled without inventing a rule for it.
5. Then move to rank 3 without lingering, because §5.1 is where the
   transformer proper is first tested and ranks 1–2 do not test it.

Nothing in `compiler/` has been written yet; the measurements here came from a
scratch script outside the repo, and `blockcmp.py` independently reproduces
every block column in §1.1.

**STOP. Waiting for `a001`.**
