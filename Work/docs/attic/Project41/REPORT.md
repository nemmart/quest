# Project 41 — REPORT

*Assembled at landing (Sep 9 2026) from the branch's four commits and its
two working documents. The P41 session ended at its budget without a
Part 3 report; this is the P39 precedent (f82b566), so that later
projects have one place to read.*

**Result: Item 1 DONE and the answer is stronger than either outcome the
prompt anticipated. Item 2 DONE and it found a real bug. Items 3–4 (two
routines if budget remained) NOT attempted. 0 routines finished; total
still 4 of ~130.**

---

## Item 1 — the FIRE mutual check: VACUOUS

The deliverable, outstanding since P38 and unperformed by two projects.

> **FIRE.1 and FIRE.2 reach DISJOINT sets of slots in their parent's
> frame. They cannot check each other, and never could have.**

`FIRE_DERIVATION_FROM_1.md` and `FIRE_DERIVATION_FROM_2.md` were written
from the book independently, before `declarations.json` was opened. The
intersection of the two slot sets is empty:

| parent slot | FIRE.1 | FIRE.2 |
|---|---|---|
| −12 (arg 1) | — | 16-bit ×3 |
| +10 | 32-bit ×3 | — |
| +12 | — | 32-bit ×2 |
| +14 | 32-bit ×3 | — |

So P39's guard — "sibling nested procedures must agree on their common
parent's layout independently" — **passed vacuously and read as
confirmation**. Every entry in the table was single-witness, and no
amount of further work on those two routines could have raised any entry
above one witness.

This is the same shape as the P40 `quest_rt.h` incident and METHOD §10:
*the check passed because it was not the check anyone thought it was.*

### The check was then discharged properly

FIRE has **three** nested children. FIRE.3@7016A4F8 was unused by P39,
is ordinary compiled code (R39 does not bite), and is by far the heaviest
user of the parent frame — 18 link loads against FIRE.1's 4 and FIRE.2's
4. It overlaps both siblings. And FIRE's own body reads its own frame
directly, which is *direct* evidence of that frame's layout and needs no
sibling at all — P39 used none of it.

Performed against the family rather than the named pair, the check
**passes with zero disagreements** on three independent strands: FIRE.3
vs FIRE.1 on +10 and +14; FIRE.3 vs FIRE.2 on −12; and FIRE's own body
corroborating −14, −12, +8, +10 and +12. Every slot two witnesses reach
is read at the same displacement and the same width by both. Nothing
already in the table had to move: **P39's readings were all correct.**

+14 is the interesting slot — FIRE never touches it. It is declared in
FIRE and used only by FIRE.1 and FIRE.3, which is exactly why it needed
two sibling witnesses and why the parent could not supply one.

### The guard, restated so that it can fail

> A parent slot is DERIVED when at least two independent witnesses reach
> it — any two of {a sibling, another sibling, the parent's own body} —
> and agree on displacement and width. A slot with one witness is
> confidence C and named as such. **A pair of procedures with no shared
> slot is not a check.**

### Two slots were missing

Added *after* Item 2's audit, so the audit ran against a neutral table:

| entry | slot | width | witness | corroboration |
|---|---|---|---|---|
| `args.2` | −14 | 16 | `FIRE.3@7016A6C7` | FIRE's own body ×2 |
| `locals.w8` | +8 | 16 | `FIRE.3@7016A557` | FIRE.3 ×8 + XPEF, FIRE's body ×11 |

Both are better witnessed than anything already in the table. arg 2's
DERR 17 bound is **100**, not arg 1's 10 — a different subscript domain,
which is independent evidence that the two argument slots are distinct
variables rather than one slot read twice.

Every comment now records its corroborating witness counts, so
single-witness entries are visibly distinguishable. `w12` is the only
slot with no sibling second witness.

---

## Item 2 — the R41 exclusion audit

**R41 itself is ENFORCED.** `Regs.pick_base()` is a closed enumeration of
{ac2, ac3}, not a filtered pick over all four; ac0/ac1 are unreachable by
construction. Both call sites check the class members and then **refuse
by name** rather than leaving the class. R41 does not have R21c's defect.

### The finding: the same bug, forty lines below P40's fix

The DO-loop limit reload (R21e′, line 1230) was
`pick(avoid=(lr, "ac2"))`. Its comment states the class in R21c's words —
"ac2 stays free for addressing" — but the implementation leaves
**{ac0, ac1, ac3} minus lr**. With `lr = ac0` and ac1 live, ac3 sits at
`FP_COST = 2` and **wins**: the loop limit is loaded into the frame
pointer, exactly the failure P40 described.

P40 fixed the loop register at 1205 and did not look forty lines down at
its sibling. Fixed to `pick(only=("ac0","ac1"), avoid=(lr,))`. **This
asserts nothing new** — it applies an already-derived class at a site
that was missed.

**Not witnessed.** No matched routine reaches line 1230, so the fix is
verified only not to regress. Confidence in the fix is inherited from
R21c′; confidence that the line is ever reached with ac1 live is
unestablished. LIST_PLAYERS.3 is the routine that would exercise it.

### `pick_fp` is correct as a preference — do not "fix" it

R33's frame preference is an ordering over all four registers, not a
class. FrameRelocation.md §4 explicitly forbids coding it as a class. A
future sweep should leave it alone.

### Owed, not fixed

**Line 1419, the stride constant, can also reach ac3.** Unlike 1230 this
names no class — plain R7 genuinely applies, and constants demonstrably
reach ac2 (FIRE.2 7016A48B `NLDAI 686,2`). Whether a constant may reach
ac3 has no witness either way, so narrowing it would be a new claim.
Left alone under "derive before asserting", recorded so the next sweep
need not rediscover it.

### R41's stated SCOPE is too broad — NEEDS A RULING

`bit_base_reg` loads a record base by plain R7 over all four registers and
its witnesses put the base in **ac1** (70166278, 7016628B) — a register
R41 says is never a candidate. Either R41's scope is narrower than its
wording, or those two sites falsify it.

The **implementation is already right on both readings**. It is the rule
text that overclaims. Proposed:

> **R41′** — a base pointer loaded *for indexing*, and the static link,
> come from the class {ac2, ac3}, ac2 preferred. This is a claim about
> the addressing role, not about every load of an address: a record base
> loaded for a bit reference (R28) is allocated by plain R7 and reaches
> ac1 (70166278, 7016628B).

**CODEGEN_RULES.md is unedited.** It is a design of record and this is a
change to a rule's meaning, so it is carried to the user as a ruling
request rather than taken by the session or by the integrator.

### A caveat inside the class, recorded not changed

R41's text says ac3 is taken "when ac2 is occupied by a live address";
`pick_base` implements "whichever is cheaper, ties to ac2". These agree
on every witnessed case and diverge only where ac2 is expensive for some
*other* reason while ac3 is cheaper. No witness either way; the cost form
is the weaker claim, so it stands.

---

## The witness-PC defect — five bad citations, and the check that missed them

The P39 witness rule was implemented as "a witness **string** exists",
not "the PC names the instruction the comment quotes". Three of the five
`PARENT_FRAMES` entries were wrong:

| entry | was | is | why |
|---|---|---|---|
| `FIRE.w12` | 7016A483 | **7016A485** | inside the three-word `LWADD 1,[0x70000210]` at 7016A482 |
| `FIRE.w14` | 7016A3DA | **7016A3DC** | the link LOAD `XWLDA 3,[ac3+0x7FFA]`, not the `XWSTA 0,[ac3+0xE]` its comment cites |
| `LIST_PLAYERS.w13` | 7016F59F | **7016F5A2** | inside the two-word `XNLDA 0,[ac2+0x7D8B]` at 7016F59E |

All three are 2–3 words short of the real instruction, consistent with
being computed from assumed instruction lengths rather than read off the
listing. **No width or slot changed** — the layout data was right, only
the citations were unusable.

Two more were found in `translate.py`'s `bit_base_reg` docstring: R28's
cited PCs (70166283, 70166296, 70166046) are all `NLDAI 686,1` — the
stride constant, not a base load. The substantive claim survives; the
citations do not resolve.

**Five bad PCs, roughly half of those spot-checked.**

### `check_frames()` now checks the citation, not just its existence

Tightened in two steps. The branch added a boundary check (is the PC an
instruction start?). That catches only two of the three: **7016A3DA is a
perfectly good instruction boundary** — it is simply the wrong
instruction. So the check now also compares the comment's leading text
against the disassembly at that PC.

Teeth, all five REJECTED:

| case | caught by |
|---|---|
| `w12` → 7016A483, mid-instruction | boundary |
| `w13` → 7016F59F, mid-instruction | boundary |
| `w14` → 7016A3DA, real instruction, wrong one | **text** |
| `w10` comment altered, PC left correct | **text** |
| `arg2` → 70153715, a data-dump address | **boundary**, after requiring a mnemonic |

The last is its own small hole: the data areas' hex-dump lines have the
same 8-hex-then-space shape as instruction lines, so a witness PC landing
in the data dump used to pass. Requiring a mnemonic excludes them.

**STILL OWED: witness PCs in comments and docstrings outside
`PARENT_FRAMES` are unchecked everywhere.** The R28 pair above is the
evidence that this matters. It is a tooling gap worth its own item.

---

## Verification at landing

Branch rebased onto main (which had added only the P42 and P43 prompts —
doc-only, no conflicts).

| check | result |
|---|---|
| `crossings.py` | clean — only the known LOCK_FILE/UNLOCK_FILE mutual pair |
| `ircmp.py --selftest` | PASS |
| `gen_declarations.py` | table `363b753584dd1b8c`, `.h`/`.json` regenerate byte-identical to the committed files |
| PICK_X_Y | 64/64 primary, 41/41 folded, DIFF 0 |
| UPDATE_SCREENS | 72/72 primary, 44/44 folded, DIFF 0 |
| REFRESH_SCREEN | 61/61 primary, 43/43 folded, DIFF 0 |
| OWNS | 152/152 primary, 114/114 folded, DIFF 0 |
| **totals** | **349/349 primary, 242/242 folded** |

All four routines were translated end to end, not merely selftested —
INTEGRATOR §5, the P40 `quest_rt.h` lesson.

---

## What P41 leaves

**Carried to the user as a ruling request:** R41′, the scope narrowing.
A change to a design of record.

**Owed, recorded:**
1. Witness PCs outside `PARENT_FRAMES` are unchecked; two known-bad in
   `bit_base_reg`'s docstring.
2. Line 1419's stride constant can reach ac3; no witness either way.
3. R21e′'s fix at line 1230 is unwitnessed; LIST_PLAYERS.3 would
   exercise it.

**Not attempted:** Items 3–4, the two routines. The session spent its
budget on Items 1–2.

**The lesson, and it is the third instance:** a check that cannot fail
reads exactly like a check that passed. P39's sibling guard, the P40
`quest_rt.h` comment, and `check_frames()`'s witness rule are all the
same defect — the mechanism was in place and verified the wrong
proposition. METHOD §10 and §16 already say this; P41 is the evidence
that saying it is not enough, and that the remedy is to test that the
check **bites**.
