# Project 42 — Stage 1: the production set (PLAN GATE, not yet built)

TREE VINTAGE: `main` after the P41 merge and the R41′ ruling — `62e4a8c`.
Provenance verified: `quest.dis` 5c1db5fb75c8c26a, `quest.mem`
1d44317d0995843e, both matching docs/Provenance.md. Baseline at the gate:
crossings clean, `ircmp --selftest` PASS, four routines **349/349 primary,
242/242 folded**.

Nothing is built yet. This document is the thing to rule on.

---

## 1. The first question: tiles, not per-node actions

**Answered from the book, and the answer is tiles.** Four pieces of
evidence, in increasing order of force.

**(a) One instruction covers several nodes.** The book has 69 `XNADI`s.
One of them, in PICK_X_Y:

    XNADI 1,[ac2+0x2B]     ->   M16[wp(ac2, 43)] = nadd(M16[wp(ac2, 43)], 1)

That single instruction is the assignment, the `+`, the reference and the
literal `1` — four AST nodes. There is no reduction order over those four
nodes that emits one instruction; only a tile rooted at the assignment
does.

**(b) One instruction covers several *statements*, in the other
direction.** `XNDO` expands to five IR statements (limit load, increment,
store, test, branch). A per-node walk has no node to hang five statements
on.

**(c) The decisive one: a tile can consume a subtree that emits
NOTHING.** `element_address` has seven emission paths — 0, 1, 1, 1, 2 and
4–6 instructions. In five of the seven the subscript expression is
**never evaluated at all**: the element address is reloaded from a temp
and the subscript, the stride multiply and the base add simply do not
happen. The book, QUEST.1 7015c611:

    XWLDA 2,[ac3+0x6]        ac2 = M32[wp(ac3, 6)]       <- the whole reference
    XNLDA 1,[ac2+0x7DB3]     ac1 = sx16(M16[wp(ac2,-589)])

`T(i).f` in two instructions, with `i` nowhere. A strict post-order walk
must reduce the subscript node before it can reduce its parent — and then
discover the parent wanted to emit nothing for it. That is not a
reduction order; that is a tile deciding, at its root, how much of its
own subtree to consume.

**(d) Our own emit methods are already tiles.** `element_address`,
`bit_address`, `indexed_store` and `for_stmt` each match a subtree shape
and emit a template. The restructuring is not introducing tiling; it is
*admitting* the tiling that is already there and giving it names.

### What this costs, and the constraint that pays for it

A tile that may swallow an arbitrary amount of its subtree makes the
reduction address unstable — the thing P43's choice files cannot afford.
So the tiling is constrained:

> **A production declares (i) its root AST node type and (ii) the exact
> set of descendant positions it consumes. Consumed descendants are not
> separately reduced and receive no reduction address. Everything not
> declared consumed is reduced normally, bottom-up.**

Reduction order is then **post-order over tile roots**, which is
well-defined and stable under any C edit that does not change the parse —
exactly the property P43 needs. The declaration is checkable: a tile that
emits for a node it did not declare is a hard error, which is what stops
"tile" from becoming "arbitrary method" again.

**Recommended: tiles, with declared spans.** Ruling requested.

---

## 2. The second question (D4): inherited or synthesised — NOT answered here

The prompt asks that P42 not decide this. It cannot, but the reason is
worse than "not enough evidence", and it needs saying:

> **The current translator has already answered it, by construction, and
> the answer is not evidence.** `value(e, want_reg, avoid)` passes
> `want_reg` and `avoid` **down**. `binop` passes `avoid + (rv.reg,)`
> into its left operand. `element_address` passes `want_reg=True` into
> the subscript. `bit_base_reg` takes `avoid=(r,)`. These are inherited
> attributes in everything but name. If P42 ports them as they stand,
> the production table will encode "down" as a structural commitment and
> P44's census will have nothing left to decide.

So the design keeps them but makes them **declared and visible**: every
production names its inherited attributes explicitly, and the table
carries a column for them. That way the census can ask, per production,
whether the book needs one.

**The productions that would need an inherited attribute if D4 answers
"down", and are therefore P44's census targets:**

| production | the inherited thing | why it is a census target |
|---|---|---|
| **`const_materialise`** | the target register | **D4's actual site.** 686 to ac2 twice, ac1 once. Today `pick(avoid=(r,))` — synthesised with an exclusion. If the answer is "down", this is a passed target. |
| `binop_const_scale` | the constant's register | R11's stride multiply, the other half of the same site |
| `binop_*` | `avoid` of the other operand's register | the operand-order decision (R20) already reads as an inherited constraint |
| `element_address` | `want_reg`, and the ac2 convention | R5's `WMOV r,2` looks like a *forced* target, i.e. inherited |
| `bit_base` | `avoid` of the offset register | R28; and the R41′ negative case |
| `field_direct` / `link_load` | the base-register class | R41′ — see §4 |
| `rt_call_arg` | `avoid` of the sibling temps | R18 |

Seven of thirty-seven. That is the honest size of the question.

---

## 3. The production set

37 productions. Columns: the AST shape it roots, what it consumes (a tile
span, `—` if it consumes nothing beyond its root), the choice points it
publishes, and the rules that move into it.

Choice kinds are P43's four and no others: **`reg`**, **`slot`**,
**`order`**, **`spelling`**.

### 3.1 Statements (13)

| # | production | roots / consumes | choice points | rules |
|---|---|---|---|---|
| S1 | `assign_local` | `Assignment` = / value | `reg` result | R16, R16b |
| S2 | `assign_indexed` | `Assignment` / the whole `ArrayRef` chain + lvalue | `slot` partial-sum temps, `order` address-then-value | R23 |
| S3 | `assign_rt` | `Assignment` / the `FuncCall` | — (result in ac0) | RTConventions |
| S4 | `assign_uplevel` | `Assignment` / `UP`/`UPARG` lvalue | `reg` link base | R43, R44 |
| S5 | `if_oneword` | `If` / the one-word `iftrue` | `spelling` skip vs branch | R13, R13c, R8a |
| S6 | `if_multi` | `If` | `order` arm emission | R13b, R19 |
| S7 | `do_loop` | `For` | `reg` loop + limit register, `slot` limit temp | R21, R21c′, R21e′, R22, R36 |
| S8 | `goto` | `Goto` | — | — |
| S9 | `return_void` | `Return` (no expr) | — | R13 |
| S10 | `return_value` | `Return` / the expr | `reg` | R35 |
| S11 | `rt_call_stmt` | `FuncCall` `$n` | `slot` TMP temps, `order` defer-all-stores | R18 |
| S12 | `game_call_stmt` | `FuncCall` game | `order` push order | R40 |
| S13 | `bit_stmt` | `FuncCall` BIT_SET/CLR/PUT / the bit address | `reg` value register | R29 |

### 3.2 Addressing and references (11) — where R41′ lives

| # | production | roots / consumes | choice points | rules |
|---|---|---|---|---|
| A1 | **`element_address`** | `ArrayRef` / subscript, stride, base — **7 paths, and 5 consume the subscript without emitting for it** | **`reg` base (R41′ class)**, `slot` scaled + element temps, `order` base-add vs temp-add (R31), `spelling` XWADD vs LWADD | R4, R5, R6, R9, R9a, R10, R11, R31, R36, R36a |
| A2 | `field_direct` | `StructRef` `->` | **`reg` base (R41′ class)** | R7, R41′ |
| A3 | `field_element` | `StructRef` `.` / the `ArrayRef` (delegates to A1) | — | R4 |
| A4 | **`bit_address`** | BIT word / subscript, stride, displacement | `reg` offset, `slot` the `16*scaled` temp | R26, R27 |
| A5 | **`bit_base`** | the record base of a bit reference | **`reg` — plain R7 over all four; reaches {ac0, ac1}. The R41′ NEGATIVE case.** | R28 |
| A6 | `link_load` | `UP`/`UPARG` | **`reg` (R41′ class)** | R42–R45 |
| A7 | `uplevel_ref` | `UP` / the link | `reg` | R43 |
| A8 | `uplevel_arg_ref` | `UPARG` deref / the link | `reg` | R44 |
| A9 | `local_ref` / `static_ref` / `arg_ref` | `ID`, `*param` | `reg` | R7, R8 |
| A10 | `marker_ref` | the arity marker | — (fixed `wp(ac3,-9)`) | R12 |
| A11 | `address_of` | `UnaryOp &` / the lvalue | `spelling` `wp` vs `bp` | R32, XPEF forms |

### 3.3 Expressions (9)

| # | production | roots / consumes | choice points | rules |
|---|---|---|---|---|
| E1 | `binop_reg_reg` | `BinaryOp` | `reg`, `order` operands | R20 |
| E2 | `binop_reg_mem` | `BinaryOp` / a 32-bit memory rhs | `spelling` direct memory operand vs load-first | R15 |
| E3 | `binop_const_inc` | `BinaryOp` + 1 | **`spelling` WINC vs WNADI** — P43's worked example | R15 |
| E4 | `binop_const_addi` | `BinaryOp` ± k16 | `spelling` | R15 |
| E5 | `binop_const_scale` | `BinaryOp` × / ÷ k | **`reg` for the constant (D4)** | R11, R16 |
| E6 | **`const_materialise`** | `Constant` | **`reg` (D4's site)**, `spelling` NLDAI/WLDAI, and R37's `WSUB r,r` for zero | R37, R29 |
| E7 | `convert` | `cvwn`/`sx16`/`trunc16` | — (explicit in the source) | P36 ruling 1, R16 |
| E8 | `abs_builtin` | `ABS` | `reg` | — |
| E9 | `bit_value` | `BIT` as a value | `reg` | R29 |

### 3.4 Conditions and plumbing (4)

| # | production | roots / consumes | choice points | rules |
|---|---|---|---|---|
| C1 | `condition_cmp` | `BinaryOp` in a test | `order`, `spelling` immediate vs register compare | R14, R14b |
| C2 | `condition_bit` | `BIT` in a test | `reg` | R29 |
| P1 | `temp_place` | a CSE temp store/reload | **`slot`** | R3, R3b′ |
| P2 | `prologue_epilogue` | the function | `slot` frame size | R1, R2, R34 |

---

## 4. R41′ enters as a class constraint on the addressing productions

The ruling's consequence, made concrete. R41′ is *not* a rule about
register loads; it is a constraint published by three productions and
denied by a fourth:

- **A1 `element_address`**, **A2 `field_direct`**, **A6 `link_load`** —
  legal set for their `reg` choice is **{ac2, ac3}**, ac2 preferred.
- **A5 `bit_base`** — legal set is **all four**, and the SD_PTR census
  says it lands in {ac0, ac1} 302 times out of 306.

That is the shape the scattered `if` could not express: the same physical
operation (load a record base) has two different legal sets **because the
two productions are for different things**. If the table survives Stage 3
with A1/A2/A6 and A5 carrying different classes and no routine-specific
`if`, that is evidence the production framing is right. If a fourth
production turns up needing a third class, that is evidence against it,
and it goes in the report.

---

## 5. The walk

One pass over the pycparser AST, bottom-up. At each node the driver asks
the table for a production whose root shape matches; the first match wins
and its declared span is marked consumed, so the driver does not descend
into consumed positions. Each production fires a template against the
existing `Regs` / `Frame` / `Layout` state, used unchanged — no allocator
rewrite in this project. Reduction addresses are assigned in firing
order as `<production>#<n>`, n counting that production's own firings, so
an address survives edits elsewhere in the routine. The driver records
every firing (address, node, choices taken) to a side-channel ledger,
which is what P43 reads and P44 censuses.

## 6. Staging

| stage | deliverable | gate |
|---|---|---|
| 1 | this document | **your ruling** |
| 2 | the driver + ledger, old path still live behind `--productions` off | 349/349, 242/242 on both paths |
| 3 | port productions one at a time, in the order S8/S9 → E6/E3/E4 → A9/A10 → A2 → E1/E2 → C1 → A1 → A4/A5 → S1–S4 → S5–S7 → S11–S13 | re-translate all four after **each** |
| 4 | `RULE_MAP.md` + report | orphans and multi-homed rules named |

Order rationale: leaves first, `element_address` (A1) late because it is
the biggest tile and the most rules, the DO-loop (S7) last because R36
interacts with A1's hoisting.

Re-baseline at every stage: `crossings.py`, `ircmp.py --selftest`, and
**translate all four routines end to end** (the selftest does not invoke
the translator — P40's lesson).

## 7. What I expect to find, recorded now so it cannot be retrofitted

1. **R36 will be multi-homed.** It is in `hoist_invariant_subscripts`,
   `for_stmt` and `element_address` today. Under the table it must land
   on A1 with S7 supplying a flag, or it is an orphan. It is the rule
   P40 amended from one routine, so it is the most likely to be wrong.
2. **R20 (operand order) may be an orphan** — it is a property of the
   walk, not of any one production.
3. **E6 `const_materialise` will accumulate the most choices** in P43,
   because D4 is unanswered and the register is genuinely free.
4. **A1 will not port without a span declaration** that looks
   uncomfortably wide. If it cannot be written without an `if` on the
   routine, that is a Stage 3 finding and A1 stays on the old path.

## 8. Ruling requested

1. **Tiles with declared spans** (§1) — the reduction address is
   post-order over tile roots, not over AST nodes.
2. **The 37 productions** (§3) — names, spans, choice points.
3. **R41′ as a published class constraint** on A1/A2/A6 with A5 denying
   it (§4).
4. **D4 left open, with the seven census targets named** (§2) — and the
   observation that the current code has already answered it by
   construction, so the inherited attributes must be declared rather
   than inherited silently.
