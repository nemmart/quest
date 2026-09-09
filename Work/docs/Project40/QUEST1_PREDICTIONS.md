# QUEST.1 — the divergences, pre-registered

Written BEFORE any change to translate.py, per the P40 prompt ("state the
divergence as a prediction that could fail before changing anything") and
P39's DO-limit test as the model.

First run: **8/101 MATCH, 93 DIFF** (expr 1, extra 31, missing 35, reg 2,
unknown 24). The routine translated without a refusal — every construct it
uses is in the subset — so all 93 are expression-level, which is what P40
was built to find out.

## Correction to my own plan-gate claim (METHOD §11)

At the gate I said QUEST.1's constant bit assignment was an unmodelled form,
that `translate.py:1891` refused it, and that it needed a new spelling. **That
was wrong.** Line 1891 is inside `bit_value_reg`, which types the *third
argument of `BIT_PUT`* — it is not a general bit-assignment refusal.
`quest_rt.h` already declares `BIT_SET(word, n)` and `BIT_CLR(word, n)`,
`translate.py:1016` dispatches them as statements, and `bit_stmt` (1862–1867)
emits exactly the bare `WBTO` / bare `WBTZ` QUEST.1 needs. The form was built
in P36 and simply never exercised — DIED used `BIT_PUT`, the diamond.

QUEST.1 uses `BIT_SET` / `BIT_CLR` and both bit statements are among the 8
that matched on the first run. **No new rule, no new spelling.** The user's
ruling of Sep 9 (`= -32768` / `= 0`) is therefore moot and is NOT being
implemented; adding a second spelling for a form that already has one would
create exactly the inconsistency the ruling was meant to prevent. The
rejected-alternative note belongs in `quest_rt.h` beside the UPLINK note all
the same, recording that `BIT_SET`/`BIT_CLR` is the spelling and why an
assignment form was considered and dropped.

I should have run the translator before claiming a gap. A code read is not a
verification (METHOD §10).

## D1 — the hoist is emitted AFTER the DO limit; the book emits it BEFORE

`do_loop` evaluates the limit (translate.py ~1126–1143) and only then calls
`hoist_invariant_subscripts` (~1148). The book's init block opens with the
hoist's stride multiply and reaches the limit load four instructions later.

R36 says the hoist's bound check and stride multiply come "BEFORE the loop's
own initialisation". OWNS, its only witness, has a **constant** limit
(R21a — no limit evaluation at all), so it could not distinguish "before the
loop-variable store" from "before the whole loop header". QUEST.1 is the
first routine with R36 and R21e together.

**Prediction.** Moving the hoist ahead of the limit evaluation puts the init
block in the book's order: stride multiply, base add, then `ac2 = OBJ_PTR`,
then the limit load, then its store to slot 4.

**This prediction can fail** — and one part of it is already known to fail.
See D4.

## D2 — the hoist temp holds the scaled subscript; the book stores the element ADDRESS

Book 7015C5EA: `ac0 = add(ac0, M32[0x70000210])` runs *before*
`M32[wp(ac3, 6)] = ac0`, and the body reloads `ac2 = M32[wp(ac3, 6)]` at
7015C60E and goes straight to `wp(ac2, -589)` with **no base add**. The
translator stores the scaled value and adds the base in the body — OWNS'
shape.

The mechanism (pre-registered at the plan gate): OWNS' reference is
`PLAYER(*p).fm390(i)`, whose INNER subscript varies with the loop variable,
so only the outer scale is invariant and the base add must stay in the body.
QUEST.1's `PLAYER(PLAYER_NUM).fm589` is invariant entire, so the whole
element address is invariant.

**Prediction.** R36 amended to hoist *the invariant part of the reference*,
with how much is invariant falling out of the reference, explains both
routines with one rule. Hoisting the address here should remove the body's
`LWADD` and make 7015C60B's reload match.

**This prediction can fail**: if the compiler always hoists only the scale
and QUEST.1's base add has some other cause, the body will still diverge.

## D3 — the limit is always reloaded for the entry test; the book keeps it live

`do_loop` unconditionally reloads the limit from its temp into a fresh
register (`lr2 = self.regs.pick(avoid=(lr, "ac2"))`). The book does not
reload in QUEST.1: `ac1` holds the limit from 7015C5F0 through the entry test
`(ac2 <=s ac1)` at 7015C5FD.

LIST_PLAYERS.3 *does* reload (7016F563). The difference is not the limit: in
LIST_PLAYERS.3 the initial constant took ac0, the register the limit was in;
in QUEST.1 it took ac2 and ac1 survived. R21e's "reloaded from its temp" was
fitted to the one routine where the register happened to be lost.

**Prediction (weaker and more general than R21e as written).** The limit is
an ordinary live value. It is reloaded only when its register was taken in
between. This explains both routines with no special case, and it removes the
`ac3` the translator currently emits (`ac3 = sx16(M16[wp(ac3,4)])` — a *value*
landing in the frame register, which is an R41 violation the current code can
reach because `pick` had nothing else left).

**This prediction can fail**: if the book reloads somewhere the register
plainly survived, the limit is not an ordinary value and R21e stands.

## D4 — OPEN, and not predicted: the stride constant takes ac2 where R7 says ac1

At the top of the init block the costs are ac0 = 3 (PLAYER_NUM, live), ac1 =
0, ac2 = 0, ac3 = 2 (FP_COST). R7 ties to the lowest number and gives **ac1**.
The book loads `NLDAI 686, 2` — **ac2**. Fixing D1 does not fix this; the
ordering change leaves ac1 free at that point either way.

The same question, both ways, inside QUEST.1 itself:

| site | context | book |
|---|---|---|
| 7015C5EA (hoist) | ac0 = PLAYER_NUM live, ac1 and ac2 free | `686 -> ac2` |
| 7015C621 (stmt 2) | ac0 = PLAYER_NUM live, ac1 and ac2 free | `686 -> ac2` |
| 7015C650 (stmt 4) | ac0 = PLAYER_NUM live, ac1 and ac2 free | `686 -> ac1` |
| 7015C60B (stmt 1) | ac0 = i live, ac1 and ac2 free | `23 -> ac1` |

Identical costs, two different answers. Whatever separates them is not in R7.

**Reading 1 — statement-wide allocation.** The compiler allocates over the
whole statement's expression tree, not instruction by instruction, and the
stride constant avoids a register a LATER operand of the same statement will
need. 7015C621's statement has a second element reference (`CASTLE(i)`) whose
arithmetic runs in ac1; 7015C650's has one operand and needs nothing more.
For the hoist this requires reading the `for` header as ONE statement whose
parts are the hoisted subscript, the limit and the initial constant — the
limit then takes ac1 and the constant 1 takes ac2, which is what the book
does.

**Reading 2 — a base-class effect.** Some variant of R41 in which the value
about to become a base biases the pick. This has no mechanism I can state and
§16 rules out a correlation without one.

Reading 1 fits all four rows and has a mechanism. It is also a **large**
claim — it changes when the register allocator runs, not just what it
prefers — resting on four sites in one routine. It is recorded here as a
reading, not implemented, and not counted as a rule. If D1–D3 land and this
is what remains, QUEST.1 is abandoned with D4 as the reason rather than
fitted.

The parent QUEST @7015C337 contains a structurally identical loop over the
same tables (`quest.dis` 7015c33d..7015c358) with the registers permuted
(PLAYER_NUM in ac1, 686 in ac2, the limit in ac0). It is a second instance of
the same question and may be what separates the readings — but QUEST is not a
P40 routine and reading it is a project of its own.
