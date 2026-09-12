> **SCOPE WIDENED (Sep 12 2026, P51).** This file was P28's table of the
> **18 `?` routines the game LCALLs**. It is now the **home of record for
> ANY runtime routine's convention**, including the internal `X.*`, `I.*`,
> `O.*` and `D.*` helpers.
>
> **Standing rule: if you have to work out what a runtime routine does and
> what its inputs and outputs are, that finding goes in THIS FILE** — not
> only in a C file's header or a project report. Otherwise the next project
> re-derives it. `X.CB` is the cautionary case: it was worked out because
> GET_INPUT forced it, recorded as `docs/Salvage.md` F12, and would have been
> re-derived by the next session to meet a BIT literal.
>
> Give each entry: entry address, inputs (which register or slot holds what),
> outputs and which registers survive, the call form (decorated `rt_call` or
> an undecorated `LCALL`), the game call sites, and the evidence. Mark
> confidence the way `Salvage.md` does — `Verified` / `High` / `Single` /
> `Reported`.
>
> Cross-references: `docs/Salvage.md` (verified program facts — F12 `X.CB`,
> F13 unsigned CHARACTER, F14 GET_INPUT's frame and `?READ$6`),
> `docs/RTWorklist.md` (which runtime routines play actually reaches, with
> call counts).

> NOTE (Sep 6, P32 gate): this table was RIGHT about `?UNSIGNED_TO_CHAR`
> (it writes a CHAR VARYING at the ac2 word address and returns ac0–ac2
> unchanged; "returns in" blank). The "length in ac0" error lived in
> StringsDesign (§1.4/§1.8/§2.2/§7) and the P32 prompt, corrected Sep 6;
> this banner records that the convention table was not the source.

# Project 28 — runtime call conventions, per callee (the 18 `?` routines the game LCALLs)

Evidence: `tools/rt_sites.py` Part 2 (rt_sites.out) — a mechanical
scan of each routine's body in Disassembled/quest-rt.dis (entry to the
next routine header) — then read by hand for the rows below. Frame
layout used to read the scan (LCALL marker pushed by EagleStack.cpp:239–
244; WSAVS pushes ac0 ac1 ac2 wfp ac3|c above it, :421–425; a wide at A
occupies words A, A+1):

| slot, from the callee's ac3 (= wfp) | holds |
|---|---|
| [ac3+0x7FF6] / low word [ac3+0x7FF7] | LCALL marker `(psr<<16)|argc` — **the argc word** |
| [ac3+0x7FF8] | saved ac0 → WRTN restores it: **write here = return value in ac0** |
| [ac3+0x7FFA] | saved ac1 |
| [ac3+0x7FFC] | saved ac2 → **read here = register argument ac2** |
| [ac3+0x7FFE] | caller's wfp |
| [ac3+0x7FF4] (= wfp−12), `@` form 0xFFF4 | arg 1 pointer (value through it) |
| [ac3+0x7FF4 − 2(n−1)] | arg n pointer — `RTBridge::arg_pointer(n) = M32[wsp−2n]`, hw/RTBridge.cpp:111 |

Every stack argument is a POINTER (PL/I by-reference: the game pushes
XPEF/LPEF addresses; the 3 WPSH push ac0's value, which is itself a
pointer — see ?OPEN_SHARED_IO_FILE). "writes through" = the callee
stores through an argument pointer (an output argument). "argc-gated"
= the body reads the argc word and branches before touching higher
arguments; positions beyond the site's argc are never dereferenced on
that path (the native ?UNSIGNED_TO_CHAR is the worked example:
runtime/unsigned_to_char.cpp:61–77).

**Registers**: no callee reads ac0 or ac1 on entry; one reads ac2; four
return a value in ac0; none writes ac1/ac2 back; ac3 is the frame
pointer for all (WRTN restores the caller's). Nothing else is passed in
registers — every window's interleaved instruction is either the ac2
argument of ?UNSIGNED_TO_CHAR or a compiler spill (Census.md §1).

| callee | argc set (sites) | frame | argc word read | entry regs read | returns in | args dereferenced (by position) | writes through | nested calls | native today (RTStubs.cpp translation_table) |
|---|---|---|---|---|---|---|---|---|---|
| ?WRITE_SCREEN | {2 (436), 5 (287)} | WSAVS 0x1E | 7017E27C | – | – | 1,2,3,4,5 (argc-gated: 4,5 only in the 5-arg form; rt/write_screen.hpp: channel, text, row&, col&, options) | arg3, arg4 (7017E319, 7017E315: row/col written back) | ?FILL_WORDS, ?LIB_ERROR | no (stub) |
| ?RANDOM_NUMBER | {3 (111)} | WSAVS 0x05 | – | – | **ac0** (7017DE5B) | 1 (7017DE4C), 2 (7017DE4F), 3 (7017DE35/DE54) | arg3 (7017DE4A: the seed) | D.MOD/F.MOD (LCALL 7017E722) | no |
| ?UNSIGNED_TO_CHAR | {1 (89)} | WSAVS 0x19 | 7017DA77 | **ac2** (7017DB13 `XWLDA 2,[ac3+0x7FFC]`; native `entry_ac(2)` = destination word address) | – | 1 (7017DAB5), 2, 3 (argc-gated; native :69–74) | – (writes the destination via ac2, not via an argument) | ?UDIV32 | **yes** (emu_rt::unsigned_to_char; argc 1..3 accepted, else fallback) |
| ?DELAY | {1 (18)} | WSAVS 0x02 | – | – | – | 1 (7017DC65) | – | – (SYSCALL 0263) | no |
| ?READ | {4 (5), 6 (1), 7 (5)} | WSAVS 0x0E | 7017DE71 | – | – | 1..7 (argc-gated) | arg3 (7017DEE4), arg4 (7017DEBB/DED9) | ?FILL_WORDS, ?LIB_ERROR | no |
| ?CHAR_TO_UNSIGNED | {1 (10)} | WSAVS 0x07 | 7017D99D | – | **ac0** (7017DA13, DA39, DA69) | 1 (7017D9B3/D9BA/DA3C, pointer 7017D9BC/D9D9/DA42), 2 (7017D9A2, argc-gated) | – | ?LIB_ERROR, ?UMUL32, C.INDEX | no |
| ?OPEN_FILE | {2 (6)} | WSAVS 0x10 | 7017DD66 | – | – | 1, 2, 3 (argc-gated) | arg1 (7017DDB3/DDB8) | ?LIB_ERROR | no |
| ?CLOSE_FILE | {1 (4)} | WSAVS 0x0D | – | – | – | 1 (7017DB7C) | – | ?FILL_WORDS, ?LIB_ERROR | no |
| ?OPEN_SHARED_IO_FILE | {5 (3)} | WSAVS 0x07 | – | – | – | 1, 2, 3 (args 4, 5 — the WPSH'd ac0 pointer and the pc-relative constant — are not loaded by any frame-relative instruction in the body; recorded) | arg1 (7017DE1B/DE21) | ?LIB_ERROR | no |
| ?GET_SHARED_PAGE | {4 (3)} | WSAVS 0x09 | 7017DC8F | – | – | 1, 2, 3, 4 (argc-gated) | – | ?FILL_WORDS, ?LIB_ERROR | no |
| ?WRITE | {3 (1), 6 (1)} | WSAVS 0x0E | 7017E20E | – | – | 1..6 (argc-gated) | arg3 (7017E277) | ?FILL_WORDS, ?LIB_ERROR | no |
| ?CREATE_TASK | {2 (1)} | WSAVS 0x06 | 7017DBD3 | – | – | 1 (pointer, 7017DBB5/DC0A), 2, 3, 4, 5 (argc-gated) | – | ?LIB_ERROR, MT?TASK, LJSR 7017E866 / 7017E949 | no |
| ?AWAIT_CONSOLE_INTERRUPT | {0 (1)} | WSAVS 0x01 | – | – | – | – | – | ?LIB_ERROR (SYSCALL 016) | no |
| ?LOOKUP_PORT | {3 (1)} | WSAVS 0x48 | – | – | – | 1, 2 (arg3 unreferenced in the body) | arg2 (7017DD24) | ?LIB_ERROR | no |
| ?LIB_ERROR_CODE | {0 (1)} | WSAVS 0x00 | – | – | **ac0** (7017DE30; native `set_return_ac(0, code)`, runtime/lib_error.cpp:473) | – | – | T?AREA | **yes** (emu_rt::lib_error_code) |
| ?CONNECT | {1 (1)} | WSAVS 0x01 | – | – | – | 1 (7017DB9D) | – | ?LIB_ERROR (SYSCALL 0167) | no |
| ?CURRENT_PID | {0 (1)} | WSAVS 0x02 | – | – | **ac0** (7017E146 error path, 7017E14A: `CVWN 1` result via `XWSTA 1,[ac3+0x7FF8]`) | – | – | ?LIB_ERROR (SYSCALL 0116) | no |
| ?READ_SCREEN | {3 (1)} | WSAVS 0x23 | 7017DEFC | – | – | 1..7, 10, 11 (argc-gated; the body handles up to 11 arguments) | arg4, arg5, arg10, arg11 | ?FILL_WORDS, ?LIB_ERROR | no |

Notes for the emitter and the executor:

1. **argc set** is what lower.py enforces per site (ruling F2, Census.md
   §2): a site whose LCALL argc is not in the set refuses. The set is
   the census's, not the body's capacity (?READ_SCREEN accepts up to
   11; the game passes 3).
2. Nothing here changes what the runtime does — rt_call leaves the
   callee emulated (or native for the two translated ones) and its
   registry lookup inside the LCALL body (EagleStack.cpp:284–303). The
   "returns in ac0" column matters only to a READER of the IR: the
   block at site+4 re-reads ac0 from the machine, as it does today.
3. Design-vs-reality check (boundary 5) passed: no callee reads an
   argument from anywhere but the stack pointers and (for one) ac2;
   no site's window carries anything other than pushes, the ac2
   XLEF, and XWSTA spills.
4. `?LOOKUP_PORT`'s third argument is pushed and never dereferenced by
   the body — recorded, not a problem (the frame teardown discards it).

---

## Additions from Project 51 (Sep 12 2026) — candidate C for seven routines

Recorded as met, per the standing rule in the banner.  Evidence is
`Disassembled/quest.dis` / `quest-rt.dis` / `quest.mem` at the addresses
given; confidence in Salvage.md's vocabulary.

### Reading convention (not a routine): byte-form X displacements are BYTES

`XLEFB / XPEFB / XLDB r,[ac3+d]` address byte `d`, i.e. word slot `d/2`.
Witnesses: GET_INPUT 7016AA37 `XLEFB 2,[ac3+0x8]` = its buffer at slot 4
(F14); HIT_ANY_CHAR 7016DE98 `XLEFB 2,[ac3+0xA]` = the data word after the
length word at slot 4, and 7016DEA7 `XPEFB [ac3+0x4]` = its CHAR(1) at slot
2 (the frame closes only under this reading).  A CHAR argument is therefore
passed as a **byte pointer**; GET_INPUT stores through arg 1 with `WSTB`
(7016AA60..62).  **Verified** (3 sites, frame sums).

### X.CB @7017E708 — build a BIT literal at run time (Salvage F12)

| item | value |
|---|---|
| entry | 0x7017E708 |
| inputs | **ac2** = destination WORD address (a frame temp); **ac0** = BYTE pointer to the character form of the literal (`"001"`, `"1"`); **ac1** = its length |
| outputs | the bit string at [ac2]; ac0/ac1/ac2 not relied on afterwards by either caller |
| call form | undecorated `LCALL [0x7017E708],0` — no stack arguments, not an `rt_call` |
| game sites | GET_INPUT 7016AA41 (`"001"` at 0x7016A9B9, len 3, into temp 76 → arg 6 of ?READ); 701703A6 (`"1"` at 0x7017024D, len 1, into temp 22 → the routine's own return value, F4) |
| evidence | both sites' three register loads immediately precede the LCALL; literal bytes confirmed in `quest.mem` (P45 F12; re-seen P51) |
| confidence | **Verified** (2 sites) |

What it means for the C: a BIT literal is `BITS("001")` — a call, not a
constant; the lowering owns the temp and the three register loads.

### ?READ @7017DE5F — argument roles (refines the row above and Salvage F14)

From the body, 7017DE71..7017DEE6:

- **arg 3 is the byte count, IN/OUT.**  The count transferred is written
  back through it: `7017DEE2 XNLDA 0,[ac3+0xA]; 7017DEE4 XNSTA 0,@[ac3+0xFFF0]`
  (`0xFFF0` = arg 3).  GET_INPUT passes the constant 1 in a frame temp (slot
  80) — a dummy the callee overwrites, which PL/I permits.
- **arg 4 is a 16-bit end/error flag.**  `7017DECD..7017DED9`: on error
  code 24, if argc > 3, `NLDAI 0x8000,0; XNSTA 0,@[ac3+0xFFEE]` (`0xFFEE` =
  arg 4).  Otherwise untouched.  GET_INPUT's arg 4 is its slot 2, which
  nothing else writes and nothing reads.
- arg 2's datum is a **pointer**: GET_INPUT stores the byte pointer to its
  buffer in temp 78 and pushes `&temp78` (7016AA37..39, 7016AA55).  In the
  C: `TMP(buf)`, a dummy holding the buffer's address.

So F14's `?READ$6(chan, buffer, one, count&, options, flags)` reads better
as `?READ$6(chan, TMP(buf), count (in/out, =1), flag&, options=0x1000,
BITS("001"))`.  **Verified** (body + site).

### ?RANDOM_NUMBER @7017DE33 — caller-side facts

Value in ac0 (row above).  At PICK_X_Y's three sites (701761FD, 7017623D,
7017625E) all lo/hi arguments are **dummies in 32-bit temps** (`XWSTA`),
including the constant 1, `OBJ_PTR->region_count` (copied before its
address is pushed) and `field ± 20` with a 16-bit field: the parameters
are FIXED BIN(31).  The seed (`SD_PTR->seed`, K 40) is passed directly by
reference.  KNIGHT_ATTACK 7016E810 and MOVE_FAMILIAR 7016FE5E have the same shape
(32-bit temps at their slots 0x12/0x14 and 0x16/0x18).  **High** (3 sites read, others by grep).

### ?WRITE_SCREEN @7017E27A — the text argument at a CHAR-constant site

At HIT_ANY_CHAR 7016DE93..DEA3 the caller builds a **CHAR VARYING dummy**
(length word then data) in its frame for a constant text and pushes its
word address as arg 2; for a ≤ 2-byte constant the whole VARYING is one
`WLDAI` immediate + `XWSTA` (7016DEAD..DEB0, length 2 | bytes).  Consistent
with the row above and rt/write_screen.hpp.  **Verified** (2 sites).
