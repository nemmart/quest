# RETURN_MESSAGE — analysis, and why it is staged

Project 37, routine 3.  Status: **STAGED, refusing.**  No match number is
quoted for it (P36's discipline, kept).

## 1. What the routine is (DERIVED, and cross-confirmed)

`RETURN_MESSAGE @70176FDD` is the game's fatal-error exit.  Three independent
sources agree:

- the tail is `@7017700F SYSCALL 0310` — **?RETURN**, the AOS/VS process-exit
  call, which never returns (`emulation/os/OSTask.cpp:145`,
  `docs/HeapSignalPlan.md`, `docs/Layering.md`);
- `docs/ERROR_PROCESSING.md` names RETURN_MESSAGE as the terminal path for the
  two fatal LOGON handlers and for INIT_SHARED_DATA's `SYSCALL 044` failure;
- the routine has no `ret` on any path.

## 2. The optional argument, and a self-confirming literal

The addrbook flags it `mixed:3/6`.  The first statement is

    ac2 = M32[wp(ac3, -16)]        XWLDA 2,[ac3+0x7FF0]
    goto [70176FE2, 70176FE3] (ac2 == 0)

— note `wp(ac3, -16)`, the argument SLOT, not `R[ac3 + -16]`, the value through
it.  So the routine tests **whether argument 3 was supplied at all**, by
comparing its pointer against zero.

This is a *different* mixed-arity mechanism from R12.  REFRESH_SCREEN reads a
frame MARKER word at `wp(ac3, -9)` and tests it with the Nova SZR form; here
there is no marker read at all — the absent argument's slot is simply null.
Both routines are `mixed` in the addrbook, so the compiler has two ways of
spelling optionality and the choice between them is not yet explained.

When argument 3 is absent the routine substitutes a literal at `0x70000CCD`
with the constant length 16.  Reading that address out of `Disassembled/quest.mem`
gives **"Unexpected error"** — sixteen characters.  The string and the length
constant were recovered independently and agree, which is what confirms the
reading of the whole diamond.

When it is present, the two halves of the VARYING are taken apart directly:
`bp(ac2, 2)` is the data (skipping the length word) and
`sx16(M16[R[ac3 + -16]])` is the length.

## 3. The rest

    packed = len | (*code | 0x8000)         WIORI 0,0x8000 ; WIOR 0,1
    if (*severity != 0) packed |= 0x1000    R13b; WIORI 1,0x1000
    ac1 = text ; ac2 = packed ; SYSCALL 0310

Three constructs the subset does not have: a raw SYSCALL statement, 32-bit
bitwise OR (`WIOR`/`WIORI`), and the VARYING-parameter accessors.

## 4. An oddity worth recording: the self-move at 70176FF5

Block 70176FF5 is the join of the two arms of the message diamond, and its
FIRST instruction is

    ac0 = ac0                      WMOV 0,0

a copy of a register to itself.  It is not dead-code noise the disassembler
invented — it is in the instruction stream.  Both arms leave the length in ac0
(the present arm by `XNLDA`, the absent arm by `NLDAI 16`), so the value is
already where the join wants it, and the compiler emits the move anyway.

The natural reading is that at a join the code generator materialises the
incoming value into an R7 pick without checking whether the source and
destination coincide — the same "copy to ac2" step that R5/R23 describe
(`WMOV r,2`), with r == the destination.  That would make it a general fact
about joins rather than a one-off.  **One instance is not enough**, and this
one cannot be tested until the routine translates, so it is recorded, not
ruled.  A second instance would most likely appear at any if/else join whose
two arms compute the same quantity.

## 5. Why it is staged rather than finished

The routine needs four things at once: a raw SYSCALL statement, 32-bit bitwise
OR, VARYING-parameter accessors (`DATA`/`LEN`), and the null-slot optional
argument.  Three of the four are cheap, but the fourth interacts with R12 — the
compiler demonstrably has two mixed-arity spellings and nothing yet says which
one it picks.  Building the null-slot form now would mean either guessing that
rule or hard-coding this routine's shape, and per §3 of the project boundaries
the translator refuses rather than guesses.

### The census, run

I ran it rather than deferring it, and the result is worth more than the rule
would have been: **the whole program contains exactly TWO `mixed:` routines** —
REFRESH_SCREEN (`mixed:0/1`, marker word at `wp(ac3, -9)`, no null test) and
RETURN_MESSAGE (`mixed:3/6`, null slot test at `wp(ac3, -16)`, no marker read).
So the two spellings cannot be separated by further instances: there are no
further instances.  Any binary hypothesis fits two data points with one bit of
difference between them, which makes the discriminator **unfalsifiable within
this program**.  That is a better thing to record than a rule at confidence C.

### Where the 3-argument call comes from — and it is not PL/I

RETURN_MESSAGE has five call sites.  Four pass six arguments.  The single
3-argument site is at **70169B82 — inside LOCK_FILE**, and it builds its
arguments by hand on the stack:

    70169B78  LDASP 0
    70169B79  WLDAI 1,0x00000C00 ; WPSH 1,1 ; LDASP 1
    70169B7E  WSUB 2,2 ; WPSH 2,2 ; LDASP 2
    70169B81  WPSH 0,2
    70169B82  LCALL [0x70176FDD],3

LOCK_FILE is the hand-written assembly routine identified at the P37 plan gate
(plan-gate finding (a): it and UNLOCK_FILE branch into each other's ranges,
which no pair of PL/I procedures can do).  This region, 70169B78..B82, is
exactly the shared tail those two routines jump into.

So RETURN_MESSAGE's `mixed:3/6` is **not evidence about PL/I optional
arguments at all** — it is the hand-assembly caller using a shorter calling
sequence into a compiled six-argument procedure.  The addrbook's arity flag is
recording an artifact of the assembly, not a language feature.

This is the plan-gate finding paying a dividend: knowing that some addrbook
entries are hand-assembly changes how their *callees* must be read.  Anything
inferred about the PL/I compiler from an arity, a convention or a register
usage seen only at a LOCK_FILE/UNLOCK_FILE call site is inadmissible evidence.
The reconstruction should treat those two routines as a contaminated source
throughout, not merely as out of scope for translation.
