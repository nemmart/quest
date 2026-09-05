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
