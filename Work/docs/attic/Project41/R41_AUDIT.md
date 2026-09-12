# Item 2 — the R41 exclusion audit

The question P40 left: R21c named a register CLASS but was implemented as
an exclusion (`avoid=("ac2",)`), so it could fall through to ac3, the frame
register. R41 names the class {ac2, ac3} for base loads. Is it enforced,
or is it an exclusion of its complement?

## The R41 verdict: **ENFORCED**

`Regs.pick_base()` (compiler/translate.py) is a closed enumeration of the
class, not a filtered pick over all four registers:

    if self.cost("ac2") <= self.cost("ac3"):
        return "ac2"
    return "ac3"

ac0 and ac1 are unreachable by construction. There is no `avoid=` and no
fall-through. R41 does **not** have the R21c defect.

Both call sites are also better guarded than R21c's was. `base_reg()`
(2260) and `link_reg()` (2288) each check the two class members for a
cached hit first, and then **refuse by name** rather than leaving the class
when neither is available:

    if self.regs.cost(r) >= COST["live"]:
        raise Refuse("R41: both base registers hold live addresses at a
                      base load of %s -- not witnessed, no rule")

That is the right shape: the class is closed, and exhausting it is an
admission rather than a silent escape. Nothing to fix.

### One caveat inside the class, recorded not changed

R41's text says ac3 is taken "when ac2 is occupied by a **live address**";
`pick_base` implements "whichever of ac2/ac3 is cheaper, ties to ac2".
These agree on every witnessed case, and diverge only where ac2 is
expensive for a reason **other** than holding a live address while ac3 is
cheaper than it — e.g. ac2 holding a constant loaded this statement
(cost 2) with the frame displaced out of ac3 (cost 0). No witness exists
either way, and the cost form is the weaker claim, so it stands. Recorded
because the rule text and the code do not say the same thing.

## The sweep: every site that names a register

| site | rule | names | implemented as | verdict |
|---|---|---|---|---|
| `pick_base` 333 | R41 base class {ac2,ac3} | a CLASS | closed enumeration | **enforced** |
| `pick_fp` 349 | R33 frame target, ac3 preferred | a PREFERENCE | ordering over all four, ac3 tie-break | **correct as a preference** — FrameRelocation.md §4 explicitly forbids coding it as a class |
| 1205 | R21c′ loop register {ac0,ac1} | a CLASS | `pick(only=("ac0","ac1"))` | **enforced** (P40's fix) |
| **1230** | **R21e′ DO-limit reload** | **a class, in the comment** | **`pick(avoid=(lr,"ac2"))`** | **EXCLUSION — FIXED, see below** |
| 1419 | R7d stride constant | nothing — a constraint | `pick(avoid=(subscript reg[, limit reg]))` | constraint, not a class — but see "owed" |
| 1766 | store-address protection | nothing — a constraint | `value(..., avoid=("ac2",))` | constraint; ac2 is also marked live the line above, so the avoid is belt-and-braces |
| 1744, 1829, 1919, 1943, 1965, 1991, 2051, 2090 | various | nothing — constraints | `pick(avoid=(<live reg>,))` | constraints on registers currently holding needed values; correct |

`pick_fp` deserves a note: it is the one site where an exclusion would be
WRONG. R33 says the frame's default home is ac3, but P37 declined to encode
"the LDAFP target is always ac2" on §16 grounds, and FrameRelocation.md §4
forbids a hard class. A preference implemented as an ordering is exactly
right, and it should not be "fixed" into a class by a future sweep.

## The finding: line 1230 is the same bug, forty lines below the P40 fix

The DO-loop limit reload (R21e′, the D3 branch) was

    lr2 = self.regs.pick(avoid=(lr, "ac2"))

The surrounding comment states the class in the same words as R21c —
"ac2 stays free for addressing" — but the implementation excludes ac2 and
the loop register and leaves **{ac0, ac1, ac3} minus lr**. With `lr = ac0`
and ac1 live (cost 3), ac3 sits at `FP_COST = 2` and **wins**: the loop
limit is loaded into the frame pointer, exactly the failure P40 described.

P40 fixed the loop register at 1205 and did not look forty lines down at
its sibling. This is the second instance of the same defect, and it is the
one the audit was for.

**Fixed** to `pick(only=("ac0","ac1"), avoid=(lr,))`. The limit is a value,
so the value class is {ac0, ac1} — the same class R21c′ already enforces —
minus the loop register, which leaves exactly one candidate. This asserts
nothing new: it applies an already-derived class at a site that was missed.

**Not witnessed.** None of the four matched routines reaches line 1230
(OWNS' limit is a constant, and neither P35 loop reloads), so the fix is
verified only to not regress: 349/349 primary, 242/242 folded after.
Confidence in the *fix* is inherited from R21c′; confidence that this line
is ever reached with ac1 live is unestablished. LIST_PLAYERS.3, abandoned
in P39 for unrelated reasons, is the routine that would exercise it.

## Owed, not fixed

**Line 1419, the stride constant, can also reach ac3.** `pick(avoid=(r,))`
(plus the limit's register under `header_expr_limit`) ranges over all four.
A multiply constant in the frame register would be as wrong as a loop
counter there. Unlike 1230 this names no class — plain R7 genuinely applies,
and constants demonstrably do reach ac2 (FIRE.2 7016A48B `NLDAI 686,2`).
Whether a constant may reach ac3 has no witness in either direction, so
narrowing it would be a new claim and is left alone under "derive before
asserting". Recorded so the next sweep does not have to rediscover it.

## A separate finding: R41's stated SCOPE is too broad

R41 is written as covering "an **address or base load**", with ac0/ac1
"not candidates however cheap they are". But `bit_base_reg` (1982) loads a
record base by plain R7 over all four registers — it does not call
`pick_base()` — and its witnesses put the base in **ac1**:

    70166278  LWLDA 1,[0x70000210]
    7016628B  LWLDA 1,[0x70000210]

Both are genuine base loads of SD_PTR landing in a register R41 says is
never a candidate. So one of these is true:

- R41's scope is narrower than its wording — the class governs the
  **indexing base pointer and the static link**, not every load of a base
  address; or
- these two sites falsify R41.

The **implementation is already right on both readings** (`bit_base_reg`
uses R7 and produces ac1; `base_reg`/`link_reg` use the class and produce
ac2/ac3). It is the RULE TEXT that overclaims. Proposed narrowing, for the
planning session:

> **R41′** — a base pointer loaded *for indexing*, and the static link, come
> from the class {ac2, ac3}, ac2 preferred. This is a claim about the
> addressing role, not about every load of an address: a record base loaded
> for a bit reference (R28) is allocated by plain R7 and reaches ac1
> (70166278, 7016628B).

Recorded rather than edited — CODEGEN_RULES.md is a design of record.

### R28's own cited PCs are wrong, too

`bit_base_reg`'s docstring cites "70166283 LWLDA 1, 70166296 LWLDA 1,
70166046 LWLDA 2". All three of those PCs are `NLDAI 686 (0x02AE),1` —
the stride constant, not a base load. The real `LWLDA 1` sites are
70166278 and 7016628B, each exactly 11 words below the cited PC; 70166046
has no `LWLDA` before it at all (7016603D is the routine entry).

This is the **fourth and fifth** bad witness PC found this project, after
the three in `PARENT_FRAMES`. The substantive claim survives — bit-reference
record bases do land in ac1 — but the citations do not resolve. The
`check_frames()` tightening committed earlier only covers `PARENT_FRAMES`;
**witness PCs in comments and docstrings are unchecked everywhere else**,
and by this project's count roughly half of the ones spot-checked are
wrong. That is a tooling gap worth its own item.
