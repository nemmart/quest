# FIRE.1 @7016A3BD — abandoned, with the reason

Project 38, stage 1.  **89 statements.  No match number, and none should be
quoted** (boundary 5: no match number for a partial translation).

P38's prompt named FIRE.1 as the validator for R33/R34, on the grounds that it
was the small case the rule was derived from, and said that FIRE.1 failing to
close would be the rule being wrong and a STOP-and-report.  It did not close.
The rule is not wrong: **R33/R34 were implemented and the four matched routines
held at 349/349 primary, 242/242 folded**, and REFRESH_SCREEN's five `LDAFP`s
now come through the new path, so the implementation is live code rather than a
no-op refactor.  What FIRE.1 needs is three constructs nobody has modelled,
each of which would have to be fitted from this one routine.

## 1. Why it was abandoned

### 1.1 It is a nested procedure — the static link (BLOCKING, see §2)

`wp(fp, -6)` is the **enclosing procedure's frame pointer**.  Four of FIRE.1's
references are uplevel variable accesses through it:

    ac2 = ac1                     entry: the link arrives in ac1
    ac0 = M32[wp(ac2, 10)]        ... a variable of the enclosing procedure
    ac2 = M32[wp(ac3, -6)]        later: reloaded from where WSAVS saved it
    ac0 = M32[wp(ac2, 14)]

The frame-layout fact is **not** fitted from FIRE.1: `M4aDesign.md` §"restore
image" gives `ac1 @ wfp-6 (= static link for nested)` from the EagleStack
WSAVS/WRTN implementation, and `ON_ERROR_CATALOG.md` §B independently reads
`XWLDA 0,[ac3+0x7FFA]` as "load enclosing frame pointer".  What is missing is
not the layout but the *source spelling* — see §2.

### 1.2 `COM.# 1,1,SZR` — a test against −1

    t1 = (((ac1 & 0xFFFF) | lsh(c, 16)) ^ 0xFFFF)
    goto [7016A418, 7016A419] ((t1 & 0xFFFF) == 0)

R14 covers the Nova `MOV# r,r,SZR/SNR` form (a 16-bit value against 0) and
R14b the `MOV.L#` sign test.  This is the `COM#` form — complement, skip if
zero — which tests a 16-bit value against −1.  One witness.

### 1.3 `WADC 0,0` as the constant −1

    ac0 = add(ac0, ~ac0)          WADC 0,0
    M16[wp(ac2, 11495)] = trunc16(ac0)

`x + ~x` is −1 for every x, so this is a two-byte way to materialise −1 in a
register that already holds something.  R11 and R5's spelling list have no
entry for it.  One witness.

### 1.4 The ac3 destination is not an R7 pick

At `ac3 = M32[wp(ac3, -6)]` the register costs are ac0 = 3 (live, holding the
value about to be stored), ac1 = 0 (the constant 0 from the previous statement,
cost 0 by R7a), ac2 = 0 (an element address whose last read has happened),
ac3 = 2 (the frame).  R7 picks ac1.  The book picks ac3.

This one is **not** a reason to abandon — it is a finding, and it generalises.
See CODEGEN_RULES §9.3 (R41, the base-register class), which was derived from
this site together with INIT_OBJ_TBL's, and which amends R33's mechanism.

## 2. The static link is a BLOCKING CONSTRUCT for the next project

This is the part of FIRE.1 that outlives it, and it is flagged here rather than
buried as a FIRE.1 detail.

**There is no way to write "a variable of the enclosing procedure" in the C
subset.**  `game/quest_rt.h` has no notation for it, `game/declarations.json`
describes statics and tables but not another procedure's frame, and
`compiler/gen_declarations.py` therefore cannot generate one.  Until it has a
spelling, every nested procedure that touches an uplevel variable is
untranslatable.

**How many routines this blocks is not measured**, but the addrbook's `.N@`
entries — nested pieces of a parent procedure — run all through the program
(FIRE alone has `.1`, `.2`, `.3`), so the answer is not small.  Two things
would need designing:

1. a source spelling, and a way for `declarations.json` to carry the
   enclosing procedure's frame layout as a named record;
2. the codegen rule for *when* the link is reloaded from `wp(fp, -6)` versus
   kept in a register — FIRE.1 reloads it at the head of every block that uses
   it, which is consistent with R8 but has only this one routine as evidence.

Nothing here was derived far enough to be a rule, and no rule was recorded for
it.  It is named as the next blocking construct, not solved.

## 3. What replaced it as stage 1's validator

**INIT_OBJ_TBL @7016DF39** (173 statements), which was already stage 2's
routine.  It exercises frame displacement 8 times and contains the `WPSH 3,3` /
`WPOP 3,3` site, so it tests R33/R34 and R41 directly — and it is ordinary
compiled code with no nested-procedure construct to confuse the evidence.
