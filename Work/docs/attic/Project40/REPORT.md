# Project 40 — REPORT

Branch `p40-routines`. Tree vintage: `main` @ `f82b566` (the P39 REPORT the
integrator added at merge), on top of `1985901` (the P40 prompt) and `7e8f9cc`
(the P39 merge). `docs/Provenance.md` verified unchanged; nothing in
`emulation/` or `Disassembled/` was touched, no battery was run.

## The number

**Routines finished this project: 0. Running total: 4 of ~130** (PICK_X_Y,
UPDATE_SCREENS, REFRESH_SCREEN, OWNS — unchanged).

QUEST.1 is **abandoned at 16/86** with the reason
(`QUEST1_ABANDONED.md`). Routines 2–6 were not reached. Pragma count: **0**.

Gate re-baseline and final state both green: four routines **349/349 primary,
242/242 folded**, `ircmp.py --selftest` **PASS**, `gen_declarations.py` clean,
`crossings.py` showing only the three known pairs (LOCK_FILE↔UNLOCK_FILE
mutual, the two R40 one-way pairs) and no new crossings.

## What the project was for, and what it found

P40 asked whether the model can close a routine when no construct is in the
way. QUEST.1 was chosen as the diagnostic: 70 statements, no float, twin,
divide, syscall, string, or call of any kind, and — verified in the book, not
assumed from the census — zero uplevel references despite being flagged
`nested`.

**It translated on the first attempt with no refusal at all.** Every construct
it uses was already in the subset. So all 93 first-run DIFFs were
expression-level, which is exactly the branch the prompt planned for.

The answer is more specific than "the model closes ordinary routines" or "the
model needs an expression-level overhaul":

> **The model closes the parts that had two witnesses and fails the parts that
> had one.**

Four rules needed amendment, and all four had been fitted to a single routine
apiece. In each case the amendment claims LESS and explains MORE:

| rule | was | is |
|---|---|---|
| **R36′** | the loop-invariant hoist lifts the scaled subscript (OWNS) | it lifts the invariant PART of the reference; how much is invariant falls out of the reference. OWNS' inner subscript varies with the loop variable so only the scale lifts; QUEST.1's reference is invariant entire so the whole element ADDRESS lifts |
| **R36a** | "before the loop's own initialisation" (OWNS, constant limit) | before the WHOLE loop header, the limit expression included |
| **R21e′** | the DO limit is evaluated once into a temp AND reloaded for the entry test (LIST_PLAYERS.3) | it is an ORDINARY LIVE VALUE, reloaded only if its register was taken. LIST_PLAYERS.3 reloads because its initial constant took the limit's register; QUEST.1 does not |
| **R21c′** | the loop register comes from ac0/ac1 | ...and that is now ENFORCED. It was implemented as `avoid=("ac2",)`, which fell through to **ac3, the frame register**, once ac0 and ac1 were both live. Also: the copy into the loop register is emitted at the first point that register is free, and the entry test compares the initial value's register |

None of these could have been asked before. R36 had one witness whose limit is
a **constant**; R21e had one witness with **no hoist**. QUEST.1 is the first
routine with both in one loop, and every amendment above is a question only
such a loop can ask. That is the finding in one line: the model's
single-witness rules were not wrong so much as **under-determined**, and the
missing evidence was an ordinary routine, not a new construct.

The R21c′ bug is worth calling out separately: a rule that named a register
CLASS was implemented as an exclusion, so it could silently leave the class
and put a loop counter in the frame pointer. `Regs.pick` now takes an `only=`
class. **R41's {ac2, ac3} should be audited the same way and was not.**

## R7d — the one genuinely new thing, and why it was not promoted

At QUEST.1 7015C5EA, R7 predicts ac1 for the hoist's stride constant and the
book loads ac2.

| routine | live | limit | R7 says | book says |
|---|---|---|---|---|
| QUEST.1 7015C5EA | ac0 = subscript | expression | ac1 | **ac2** |
| QUEST 7015c344 | ac1 = subscript | expression | ac0 | **ac2** |
| OWNS 70175CC7 | ac0 = subscript | **constant** | ac1 | **ac1** |

**R7d (scoped to the loop header):** the stride constant is transient — dead
at the WMUL — while the limit that follows it needs a value register, so the
constant takes ac2 and leaves the value register for the limit.

The parent **QUEST @7015C337**, read as evidence at the user's instruction and
not translated, is the discriminator and it could have failed: its registers
are permuted against QUEST.1's, so "avoid ac1" and "prefer ac2 whenever free"
both predict wrongly there while R7d comes out right in both. OWNS is the
negative control — no limit to evaluate, nothing to avoid, plain R7 stands.

Implementing R7d in that scope made QUEST.1's entry block, init block and skip
block match the book **instruction for instruction**, and the slot bijection
is the identity.

**The general form was not implemented and is not a rule.** It says the
allocator works over a whole statement's expression tree, building both
operands' addresses before either load and protecting a register a later
operand will occupy. It would explain QUEST.1's body — where the translator
completes the left operand entirely, loading its field into ac0 and destroying
the loop register. But it is a claim about *when* the allocator runs, not what
it prefers, and its evidence is one routine's body.

The abandonment doc proposed a cheap test, so it was run. Over the book's 242
blocks with two element-address constructions: **163 build both addresses
before either field load, 79 do not.** Not the clean answer the claim needs.
The sweep is block-level rather than statement-level so many of the 79 are
probably two one-operand statements — METHOD §16's fourth caution met one
level down — but either way the general form cannot be settled cheaply, and
recording rather than fitting it now rests on a measurement.

## Corrections (METHOD §11)

**1. The plan gate's "unmodelled constant bit assignment" was wrong.** I
reported that QUEST.1's bare WBTO/WBTZ had no C spelling, citing a refusal at
translate.py:1891. That line types `BIT_PUT`'s third argument; it is not a
general refusal. `BIT_SET` / `BIT_CLR` have existed since P36, are dispatched
at translate.py:1016, and emit exactly the required instructions — they had
simply never been exercised, because DIED used the `BIT_PUT` diamond. Both of
QUEST.1's bit statements matched on the first translation.

The user ruled a spelling (`BIT(w,n) = -32768` / `= 0`) on the strength of
that report. **It was withdrawn rather than implemented**: with the statement
forms already in the language, adding an assignment form would give one PL/I
value two spellings, which is the inconsistency the ruling was made to
prevent. The ruling's principle is what kept it out. Recorded in
`quest_rt.h` beside the UPLINK note, with the rejected alternative and the
reason, as instructed.

The claim came from reading the code instead of running it — METHOD §10, and
the reason the plan gate should have translated something before reporting.

**2. The census's READ_IN row was wrong**, and struck at the gate. Its "17
statements, one rt_call" is the union of two addrbook ranges: READ_IN's own
body is 7 statements ending in a `WBR` that jumps over the embedded `nocall`
ON-unit `READ_IN.1@701766FE` and lands past it. It uses `I.PROLOG`, `O.ON`,
`I.GOTO` (a non-local goto out of the ON-unit) and `I.EPILOG` — four
unmodelled constructs, none on the census's marker list. METHOD §16's second
and fourth cautions firing on one routine.

## The FIRE mutual check — NOT PERFORMED

**P39's material gap is still open, and P40 did not close it.** FIRE.2 and
FIRE.1 were routines 2 and 3 and were not reached; the parent-frame layout in
`declarations.json` still rests on single witnesses per slot and remains
FITTING, not derivation. This is the second project in a row to leave it, and
it should lead the next one — it is now the oldest outstanding item in the
compiler line.

## Predicted arm-path defects

**None tripped.** The three sites recorded in §8.8 (R9's scaled-subscript
temp, R10's element-address temp, R3's temp-free-from-last-use, all counting
uses in mutually exclusive branches) were not exercised: QUEST.1's two element
references sit in sequence, not in arms. LIST_PLAYERS.3, the routine P37
predicted would trip one, was routine 6 and conditional on QUEST.1 closing
first; it did not, so LIST_PLAYERS.3 was correctly not attempted.

## Files

`compiler/translate.py` (R36′, R36a, R21e′, R21c′, R7d scoped, `pick(only=)`,
`subscript_vars`), `compiler/CODEGEN_RULES.md` §10, `compiler/
gen_declarations.py` (the CASTLE table, `OBJ_PTR->f911504`, an empty QUEST
parent frame), `game/quest_rt.h` (the bit-assignment spelling of record),
`game/routines/QUEST.1.c`, `docs/Project40/`. Nothing else.

## Honest read

The model does not need an expression-level overhaul. It is right about
frames, slots, temps, conversions, control flow, calls and bit operations —
QUEST.1's identity slot bijection, its exact loop header and its two first-try
bit statements say so, and the four matched routines never moved.

What it is wrong about is **the order in which one statement's operands are
emitted**, and that is one question rather than a class of them. It has never
had a witness because no matched routine has two element operands in a single
statement, and it will not be settled by patching the emitter against the
comparator. Establish it on the book first — with statement boundaries, not
blocks — and rebuild the expression emitter once.

The other thing P40 shows is procedural. Four of the five things that went
wrong here were rules fitted to one routine, and the fifth was a rule that
named a register class but did not enforce it. The confidence letters were
doing their job — every one of these was marked B or C — but nothing forced a
single-witness rule to be **re-asked** when a second witness finally arrived.
A routine that combines two constructs each having one witness is worth more
than a routine that exercises a new one.
