# The frame relocation, derived — and the pragma is not needed

Project 37, routine 4 (FIRE.1 @7016A3BD) plus DIED's 70166376..701663B3.
**Verdict: no `#pragma fp ac2`.  Pragma count stays 0.**

This is the question P37 was chartered to answer, so the derivation is written
out in full even though the translator change it implies is not yet built.

## 1. P36's finding, restated more weakly — and correctly

P36 finding 1 described DIED 70166376..701663B3 as *"a register reassignment of
the frame pointer, not a borrow"* and called it *"a structural change [that]
touches every emit site"*, which is why stage 2 went unbuilt.  Reading FIRE.1
first — the small case — and only then re-reading DIED makes a simpler story
available, and the simpler story fits both.

**There is no relocation.**  The frame pointer is not a thing that moves.  It is
an ordinary VALUE in the register model, materialised on demand by `LDAFP` into
whatever register R7 leaves free, and living exactly as long as any other cached
value would under R7/R8.  `ac3` is its default holder and is otherwise an
ordinary allocatable register.

## 2. FIRE.1 — the mechanism at a size where it can be read

Block 7016A3C7, in order:

    ac3 = M32[wp(ac3, -6)]     XWLDA 3,[ac3+0x7FFA]   ac3 STOPS holding the frame
    M32[wp(ac3, 14)] = ac0                            ... and is used as a base
    ac2 = wfp                  LDAFP 2                the frame is needed -> ac2
    ac2 = M32[wp(ac2, 2)]      XWLDA 2,[ac2+0x2]      ... so a frame ref spells wp(ac2, d)
    ac2 = add(ac2, M32[0x70000212])
    ac3 = wfp                  LDAFP 3                ac3 free again -> the frame goes back
    ...
    t1 = ac3                   WPSH 3,3               ac3 holds a live element address
    ac3 = wfp                  LDAFP 3                ... but the frame is needed in ac3
    M32[wp(ac3, 2)] = ac2                             ... for one store
    ac3 = t1                   WPOP 3,3               ... and the value comes back

Every `ac2 = wfp` in the routine is immediately preceded by ac3 holding
something else.  Nothing here is a "relocation": it is R7 picking a register for
a value, and R24's laziness deciding when.

## 3. DIED — the same rule, with ac2 simply never clobbered

DIED's stretch looked like a different phenomenon because the frame stays in ac2
for ~10 blocks and 22 references spell `wp(ac2, 8)`.  It is not.  Look at what
occupies the registers through the run of R29a bit assignments:

    ac3 = M32[0x70000210]      LWLDA 3   the record base, held for the whole run
    ac1 = M32[wp(ac2, 8)]      the bit OFFSET, reloaded from the R27 temp each time
    ac0 = ac2 / WMOV           the bit VALUE being assigned

R28 already says it: *"the record base ... stays PROTECTED while the `16*scaled`
temp is alive — one `LWLDA` serves the whole run."*  With ac3 on the base, ac1
on the offset and ac0 on the value, **the only register left for the frame is
ac2** — and nothing in those ten blocks ever wants ac2 for anything else, so the
`LDAFP 2` is never repeated.  The long lifetime is an absence of pressure, not a
policy.

FIRE.1 and DIED differ only in how quickly ac2 gets reused.

## 4. The rules

**R33** — The frame pointer is not pinned to a register.  `ac3` holds it by
default and is otherwise ordinarily allocatable; when a statement needs a base
or an address and ac3 is the R7 pick, ac3 takes it and the frame becomes "not
in a register".  The next frame reference emits `LDAFP` into an R7 pick and
spells `wp(acN, d)`.  R24's lazy-emission rule already governs *when*; R7
governs *where*.
*Witnesses:* FIRE.1 (3 × `ac2 = wfp`, 3 × `ac3 = wfp`), DIED (1 × ac2, 12 × ac3),
INIT_OBJ_TBL (8 × ac3), DISTANCE_TO_PLAYER (1 × ac3).  **Confidence B.**
In every instance where ac3 was occupied the LDAFP target was ac2 — but that is
also the only register R7 could have picked in each case, so "always ac2" is NOT
an independent claim and must not be coded as one.

**R34** — When a live value in ac3 must survive an `LDAFP 3`, it is preserved
across it by `WPSH 3,3` / `WPOP 3,3`.
*Witnesses:* FIRE.1 7016A3C7, INIT_OBJ_TBL 7016DF68.  **Confidence B.**

## 5. Why no pragma

The plan gate said a pragma would be justified *"if the trigger for ac3 being
taken over cannot be stated as an R7 consequence — i.e. if there are sites where
ac3 takes a base while a cheaper register is free, with no discriminating
variable."*  No such site was found.  In every instance ac3 is taken by the
ordinary cost model, and the frame reappears by the ordinary cost model.  The
construct needs no directive, only an existing rule applied to one more
register.

## 6. What is NOT done, stated plainly

The **derivation** is complete; the **implementation** is not.  `Regs` models
ac0–ac2 with ac3 pinned, and making ac3 allocatable plus threading a
"where does the frame live" state touches every emit site — P36's assessment of
the size of the change was right even though its description of the phenomenon
was not.  That work was deliberately not started at the end of a long session:
a structural change to the register model, made in a hurry against a routine as
intricate as FIRE.1, is exactly how a fitted rule gets into the ledger.

**FIRE.1 therefore has no match number, and none should be quoted for it.**
What P37 delivers here is the rule and the verdict on the pragma, not the
routine.
