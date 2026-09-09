# QUEST.1 @7015C5E1 — ABANDONED, with the reason

**16/86 statements MATCH.** No match number is quoted for it elsewhere and it
is not counted as a finished routine. It is kept as the witness for R36′,
R36a, R21e′ and R21c′ (CODEGEN_RULES §10) and as the site of the R7d finding.

Its C is `game/routines/QUEST.1.c`. The pre-registered divergences are in
`QUEST1_PREDICTIONS.md`, written and committed before `translate.py` was
touched.

## 1. What closed

The **loop header matches the book instruction for instruction** — the entry
block, the init block and the skip block, all three:

```
  ac0 = sx16(M16[0x70000216])          the PLAYER_NUM subscript
  assert(!(ac0 >s 0xA) && (ac0 >s 0))  DERR 17
  ac2 = 0x2AE ; ac0 = mul(ac0, ac2)    the hoist: stride multiply       R7d
  ac0 = add(ac0, M32[0x70000210])      ...and the base, because the        R36'
                                       whole reference is invariant
  ac2 = M32[0x70000212]                the limit's base            R41
  ac1 = sx16(M16[wp(ac2, 911504)])     the limit                   R36a
  M16[wp(ac3, 4)] = trunc16(ac1)       R21e's temp
  ac2 = 0x1 ; M16[wp(ac3, 2)] = ...    the control variable        R21c'
  M32[wp(ac3, 6)] = ac0                the hoist store             R36
  goto [...] (ac2 <=s ac1)             the entry test on cr, not lr    R21c'
  ---- skip block ----
  ac0 = ac2                            the copy into the loop register R21c'
```

Four predictions, D1–D3 and the scoped R7d, all came out together. The slot
bijection is the identity (w2→2, w4→4, w6→6), so the frame layout — R1, R2,
R3 and R21e's temp — is right without adjustment.

## 2. What did not

The **body**. All 70 remaining DIFFs are downstream of one thing, and it is
the same thing R7d is:

Book, statement 1 of the body (7015C607..7015C60B) — both operand ADDRESSES
are built, and only then are the two fields loaded:

```
  assert DERR i, 1000
  ac1 = 0x17 ; ac0 = mul(ac0, ac1)     CASTLE[i]: scale
  ac0 = add(ac0, M32[0x70000214])      ...and base        -> the address
  ac2 = M32[wp(ac3, 6)]                PLAYER's hoisted address
  ac1 = sx16(M16[wp(ac2, -589)])       PLAYER's field
  ac2 = ac0 ; ac2 = sx16(M16[wp(ac2, -23)])   CASTLE's field
  goto [...] (ac1 == ac2)
```

The translator instead completes the left operand entirely (address, then
load) before starting the right one. That is not merely a different order: it
loads PLAYER's field into ac0, **destroying the loop register**, so the
subscript `i` has to be reloaded from its frame slot and every register
downstream permutes.

## 3. The reason for abandonment

The body's divergence and R7d are the same claim: **the compiler allocates
registers over a whole statement's expression tree** — building both operands'
addresses before either load, and protecting a register a later operand will
occupy. Under it, QUEST.1 7015C621's stride constant takes ac2 to leave ac1
for `i`, and 7015C650's takes ac1 because nothing follows; both are otherwise
unexplained.

I have not implemented it, and QUEST.1 is abandoned rather than fitted to it.
The reasons, in order:

1. It is a claim about **when** the register allocator runs, not about what it
   prefers. Every rule in CODEGEN_RULES so far describes a preference inside a
   fixed instruction-at-a-time emission order. This one replaces that order.
2. The evidence for the general form is **one routine's body**. R7d in the
   loop header earns its confidence from three routines with permuted
   registers and a negative control; the general form has nothing comparable.
   Promoting it on QUEST.1's body alone would be exactly the single-routine
   fitting that §10.1 shows produced four of the rules P40 had to amend.
3. Restructuring the expression emitter would touch every emit site and put
   349 matched statements at risk to test one hypothesis. Boundary 3 forbids
   leaving the four regressed; the honest sequence is to establish the claim
   on the book first and rebuild once, not to iterate the emitter against a
   comparator until QUEST.1 goes green.

**What would settle it.** A routine whose statement has two element operands
where the two orders are distinguishable, translated from a model built for
the claim rather than patched toward it. MOVE_PLAYER.1 (P40 routine 4) has
two XNDO loops and bit work in the same shape; FIRE.1's `COM.#` field tests
are single-operand and will not separate them. The claim is also directly
testable against the book with no translator work at all: enumerate the
program's two-element-operand statements and check whether both addresses are
always built before either load.

## 4. What this says about the model

The P40 prompt asked whether the model can close a routine when nothing is in
the way. On this routine the answer is: **it closes the parts that had two
witnesses and fails the parts that had one.** Nothing here was a missing
construct — QUEST.1 translated without a single refusal on the first attempt.
Four rules needed amendment and all four had been fitted to one routine
apiece; the fifth thing, the emission order, has never had a witness at all
because no matched routine has two element operands in one statement.

That is a better result than a match would have been, and it is the diagnosis
P39 predicted: the bottleneck is expression-level modelling. But it is
narrower than "the model is wrong about ordinary expressions". The model is
right about frames, slots, temps, conversions, control flow and bit
operations — QUEST.1's identity slot bijection and its two first-try bit
statements say so. What it is wrong about is **the order in which a statement's
operands are emitted**, and that is one question, not a class of them.

## 5. The book check, run — and it does NOT confirm the claim

§3 said the general claim could be tested against the book with no translator
work, so it was run. Over the 242 blocks containing two element-address
constructions (`add(acN, M32[<table base>])`):

| | blocks |
|---|---|
| both addresses built before either field load | 163 |
| a field load sits between the two addresses | **79** |

**This is not the clean answer the claim needs.** Had it come back 242/0 the
general form would have had program-wide support and would have been worth
building; a third of the population going the other way means it does not.

The sweep is crude in one specific way and the number should not be read as a
refutation either: **a block is not a statement**. R13's one-word THEN, R8c's
surviving edges and ordinary straight-line code all put several PL/I
statements in one block, so many of the 79 are very likely two separate
statements, each with one element operand, which the claim says nothing about.
Separating them needs statement boundaries, which is what the translator
computes and this sweep does not have — the marker-level blindness of METHOD
§16's fourth caution, met again one level down.

What this settles: the general form of R7d cannot be established or dismissed
by a cheap sweep, and QUEST.1's body alone is not enough to carry it. The
decision to record it rather than fit it stands, and it now rests on a
measurement rather than on caution alone.
