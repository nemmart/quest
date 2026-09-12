# GET_INPUT — what was derived, and the one thing that was not

Project 37, routine 2.  Status: **STAGED, refusing.**  No match number is
quoted for it (P36's discipline, kept).

## 1. X.CB is the PL/I BIT-literal constructor (DERIVED, two witnesses)

`X.CB @7017E708` converts a CHARACTER literal into a BIT string.  Both of its
two direct game call sites agree on the register convention:

| | ac2 | ac0 | ac1 |
|---|---|---|---|
| | destination WORD address | byte pointer to the character form | its length |

    GET_INPUT 7016AA41   ac2 = XLEF [ac3+0x4C]   ac0 = 0x7016A9B9:0 "001"   ac1 = 3
    <unnamed> 701703A6   ac2 = XLEF [ac3+0x16]   ac0 = 0x7017024D:0 "1"     ac1 = 1

followed in both cases by an embedded, undecorated `LCALL [0x7017E708],0`.
The literal bytes were read out of `Disassembled/quest.mem`: `30 30 31` = "001"
and `31` = "1".

**The 1986 compiler does not constant-fold a bit literal.**  `'001'B` is not a
compile-time constant in the object code; it is rebuilt by a runtime call at
every evaluation.  The second site is worth noting on its own: it builds
`'1'B`, reads the result back with `XNLDA 0,[ac3+0x16]` and stores it to
`wp(ac3, -7)` — i.e. it *returns* `'1'B`, which is R35 (the slotpatch value
return) with a third witness.

`BITS("001")` is the form in the C subset (`game/quest_rt.h`); the translator
refuses anything but a string literal of `'0'`/`'1'`.

## 2. R2a — array locals span real words (DERIVED)

R1/R2 gave every declared local exactly one wide slot.  An ARRAY local takes as
many words as it needs, rounded up to a whole even slot: a `CHAR(n)` buffer
takes `ceil(n/2)` words.  GET_INPUT's buffer therefore runs from slot 4 to slot
75 and the frame's first temp is slot 76 — which is where the book puts it
(`XLEF 2,[ac3+0x4C]`).  The frame is `WSAVS 0x0029` = 41 wide words = 82 slots,
and 76/78/80/82 are exactly the four temps the routine uses, so the buffer is
144 bytes.  This is a genuine confirmation of the reconstruction: the frame
size, the buffer size and the temp slots are consistent, and would not have
been if the local layout were wrong.

## 3. NOT DERIVED — the temp ALLOCATION order for this call

    ac2 = bp(ac3, 8)        XLEFB 2,[ac3+0x8]     the buffer's byte pointer
    M32[wp(ac3, 78)] = ac2  XWSTA 2,[ac3+0x4E]    ... into slot 78  = argument 2
    ac2 = wp(ac3, 76)       XLEF  2,[ac3+0x4C]    the BIT temp, slot 76 = argument 6
    ac0 = 0x7016A9B9:0      XLEFB 0
    ac1 = 3                 NLDAI 3,1
                            LCALL [0x7017E708],0  # X.CB
    ac0 = 1  ; M16[wp(ac3, 80)] = trunc16(ac0)    slot 80 = argument 3
    ac1 = 0x1000 ; M16[wp(ac3, 82)] = trunc16(ac1) slot 82 = argument 5
    rt_call ?READ(0x70000262, wp(ac3,78), wp(ac3,80), wp(ac3,2), wp(ac3,82), wp(ac3,76))

The slots run 76, 78, 80, 82 in allocation order (R3: lowest free even slot),
so **argument 6's temp is allocated before argument 2's** — yet argument 2's
store is EMITTED first and the X.CB call second, with arguments 3 and 5
following.  Neither candidate rule explains both facts:

- *"left to right" (R18)* predicts allocation 78→arg2, 80→arg3, 82→arg5,
  76→arg6 — wrong allocation, and it also predicts X.CB emitted last;
- *"an argument needing a runtime call is allocated first"* gets the allocation
  right (76 to the BIT literal) but predicts X.CB emitted first, not second.

A rule fitted to this single instance would be a guess, and a guess here is
cheap to make and expensive to carry — the same shape governs every multi-dummy
call in the game.  **Left refusing.**  What would settle it: a second routine
whose call mixes a call-materialised argument with plain dummies.  The other
X.CB site (701703A6) does not qualify — its X.CB result is the whole
expression, with no competing dummies.

## 4. Also derived along the way

- `char` (8-bit) is now a subset type; `bp()` byte addressing of a CHAR local
  follows from the slot as `bp(ac3, 2*slot)`.
- `?READ$6(chan, buffer, one, count&, options, flags)` — the argument list read
  off the six `XPEF`s at 7016AA5A.
- The tail is ordinary: `*c = buf[1]`, then `if (*c > 128) *c = (*c - 128) &
  0xFF` — a re-read (`WLDB`) after the store, because byte values are not
  tracked across a statement boundary (R8b), and a `WANDI` byte mask.

## 5. A correction the native check caught (METHOD §11)

The first draft declared the buffer and the parameter as `char`.  Both native
compilers rejected `if (*c > 128)` as *"comparison is always false due to
limited range of data type"* — `char` is signed here, so the test can never
fire.  The book settles it: the byte is loaded with `WLDB` (`ac2 = M8[ac0]`,
ZERO-extending) and compared `>s 128`, which is only meaningful for an unsigned
datum.  **PL/I CHARACTER is an unsigned byte**, and the subset spells it
`unsigned char`.

This is the native compile check earning its place: it is not a formality
alongside the comparator, because the comparator would never have seen it —
the routine refuses before reaching that statement, so nothing would have been
compared.  A wrong signedness would have sat in the source of record until some
later routine tripped over it.
