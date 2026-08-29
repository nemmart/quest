# Project 24 — Carry-consumption census (Part 1 deliverable)

Census date: Aug 29 2026. Inputs: current regenerated listings
(quest.dis sha256 1f9153c0…, verified; quest-rt.dis; quest.blocks.split,
18,009 blocks) and the current emulator source. The prior session's
census was marked unrecorded/off-rails and NOTHING from it was reused;
the user's 4-site candidate list was treated as a hypothesis to test.
Scripts: p24/census.py (extraction), p24/classify.py (classification) —
both mechanical, both re-runnable.

## Headline

**Zero wide-reached and zero ambiguous carry consumers exist anywhere in
the game + runtime range.** Every reachable consumer's carry producer is
fix-invariant (Nova ALC / DIVX / CRYTO — none touch the wide helpers).
The four candidate sites from the P23-era list DO read carry, but their
reaching producer is the immediately preceding `ADD.O x,x,SBN` — Nova,
fix-invariant — so they are not exceptions after all. **The wide-carry
fix is unobservable to gameplay**, and the WADC routing question with it
(§6).

## 1. Method

1. **Writer/reader sets extracted from emulator source, not hand lists**
   (P23 skip-table lesson). Every `machine.c` access in the tree was
   enumerated (`grep -rn "machine\.c\b"` over hw/, os/, runtime/,
   emulator dirs) and attributed to its instruction `case` label
   programmatically. os/ contains no carry access at all; syscalls
   signal errors via the skip-return convention, never via carry.
2. **Reader model derived from hw/NovaCompute.cpp**, the single true
   consumer in the instruction set: incoming `machine.c` enters an ALC
   computation only when the carry field is blank (base = c) or C
   (base = ~c); Z/O force 0/1. The 17-bit datapath then determines
   whether the incoming bit can reach (a) the skip decision, (b) the
   loaded result, (c) the new carry:
   - shift none/S: new c = f(c_in); result independent of c_in
   - shift L: new c = bit15 (independent); result bit0 = f(c_in)
   - shift R: new c = bit0 (independent); result bit15 = f(c_in)
   - `#` (no-load): neither c nor ac written — NovaCompute gates both
     on N==0.
   A site **consumes** c_in iff a c-testing skip sees f(c_in), or a
   zero-testing skip sees a result containing a c_in-derived bit, or a
   c_in-derived bit is loaded into an accumulator. This prunes the
   dominant `MOV.L# x,x,SNC` bit-15-test idiom (708 occurrences), which
   mechanically reads c but discards it on every path.
3. **Classification**: backward walk from each consumer over
   quest.blocks.split (game) / a branch-target-aware linear scan
   (rt), with per-instruction transfer functions: wide-helper writers
   KILL_WIDE; narrow helpers, ALC(N==0, non-pass), DIV/DIVX/CRYTO/
   CRYTZ/WCMV KILL_FIXINV; ALC forms with c_out = f(c_in) are
   pass-through links; WRTN/WPOPB restore. Register DATA is identical
   under old/new carry formulas (the fix changes only `machine.c`), so
   fix-invariance of a consumed value depends only on its c-producer
   chain.
4. **Hidden code (METHOD §4)**: the two XCT sites (7017E9F6, 7017ECF4)
   execute register-built `ENQH`/`ENQT` (0xC7E9/0xC7F9 per the listing
   and I_ALLOC.md) — the queue instructions do not touch carry
   (source-verified), and the E9F6 path aborts at the unimplemented XCT
   before any enqueue regardless. All four heap-region holes were
   recovered and recorded in I_ALLOC.md: contents are XWADD/WSBI/XWSTA/
   ISZTS/WRTN + the coalescer — one wide WRITER, zero carry consumers.

## 2. Writer set (source-extracted)

| Class | Instructions | Under the fix |
|---|---|---|
| Wide helpers `add()/sub()` (>>31 bug) | WADD WSUB WNEG WADC WINC WADI WSBI WNADI WADDI XWADD LWADD XWSUB LWSUB XWADI XWSBI **XWDO LWDO** | **changes** |
| Narrow helpers (>>16, correct) | NADD NSUB NNEG NADI NSBI NADDI XNADD LNADD XNSUB LNSUB XNADI LNADI XNSBI LNSBI XNDO LNDO | unchanged |
| Nova ALC (N==0 only) | all COM/NEG/MOV/INC/ADC/SUB/ADD/AND load forms | unchanged |
| Explicit | DIV DIVX CRYTO CRYTZ WCMV | unchanged |
| Transport restores | WRTN WPOPB (frame/stack bit 31), RTBridge save/restore, EagleIntegration wrtn/wrtn_void | carries the producer's class |
| Native runtime (clone only) | frames.cpp emu_add/emu_sub (**verbatim replicas of the buggy helpers** — Finding F2), lib_error.cpp staged c (124/172/331/350/414), o_signal.cpp 266, p_defon.cpp 142, def_on.cpp 312 | Part-3 audit scope |

**Finding F1 — writer-list delta:** `XWDO`/`LWDO` (wide do-loop
increment) call `add()` and are wide carry writers; WideCarry.md's
15-instruction list omits them (33 occurrences across the listings).
This widens the fix's write surface but, per §4, changes no consumer.

IR note: `#+`/`#-` ops call the same helpers (P23 ruling), so every
classification below covers emulated master, emulated clone, and IR
clone identically.

## 3. Reader set and consumer census

Mechanical ALC readers (carry field blank/C): 956 game + 99 rt.
After datapath dependence analysis: **47 game + 10 rt consumers.**

### Game — all 47 → NOVA (fix-invariant)

| Sites | Idiom | Producer |
|---|---|---|
| 70160E64/65, 70160E73/74, 7016E75B/5C, 7016E76A/6B (8) | `ADD.O x,x,SBN` → `ADC.C x,x,SNC` → `SUB.CL x,x` (rounding/sign fixup after XNSUB) | `ADD.O x,x,SBN` — Nova, carry base forced to O; the `LWADD` earlier in-block writes wide carry that is DEAD (overwritten by XNSUB then ADD.O before any read) |
| 19 pairs (38): 7015E701, 701644C5, 70168107, 7016A5E5, 7016B852, 7016DB9A, 7016DCD0, 7016DCEF, 70171734, 70171984, 70171E2B, 7017200B, 701723F2, 701727AB, 70177336, 701785C8, 7017860D, 70178738, 70179BFC | `DIVX` → `MOV.L 2,1` → `ADD.# 0,0,SEZ` (16-bit divide + remainder-sign handling) | `DIVX` (first site), `MOV.L 2,1`'s own Nova write (second) |
| 70169B56+5 (1) | `CRYTO` … `MOV.# 0,0,SNC` | `CRYTO` |

No game consumer's backward walk ever crossed a call or routine-entry
boundary — every producer is in-block or in the immediate predecessor,
so the interprocedural machinery below is corroboration, not
load-bearing.

### Runtime region — 7 → NOVA, 3 → unreachable

| Sites | Verdict | Evidence |
|---|---|---|
| 7017DB90, DCC1, DD0F, DDA7, DE0D, DECA, E26C (`MOV.R 0,0`) | NOVA | post-syscall status idiom: `MOV.O 3,3,SKP` / `MOV.Z 3,3` — both paths Nova; rotates the flag into the result |
| 7017FDCE/FDCF (`AND.CS# 3,3,SBN`, entry of `?URTB`) | UNREACHABLE | zero references in quest.code, quest.targets, addrbook, either listing — unreferenced library stub |
| 7017E4E3 (`SUB.S# 3,0,SNC`) | UNREACHABLE, **provably never executed** | inside the SWAT debugger trap handler (0x7017E4DB, installed into vector [0x8] by SWAT.NIN); (a) nothing in the emulator dispatches through [0x8]; (b) the installer is on the ?FATAL-only path (OSContextTask.cpp analysis, METHOD §13); (c) straight-line upstream at 7017E4E0/E4E2 sit two Nova-form `LEF` instructions that decode to the empty class / `oper=-1` — the base `Instruction` whose execute() throws. METHOD §3: millions of clean pairs prove no entry has ever reached E4E3. |

Note: the bare `LEF` rendering at E4E0/E4E2 is the disassembler showing
an unimplemented-decode entry (`LEF*`, empty operand spec), not an
omitted-field defect — §14 checked, not triggered.

## 4. Transport verification (WSAVS/WRTN are transport, not writes)

- All **124** distinct call ('c'-terminator) targets begin with
  WSAVS/WSAVR/WSSVS/WSSVR; returns restore c from the saved word's bit
  31 → framed calls are carry-transparent.
- Frame-word edits: the only [wfp+0] rewrite in either listing
  (7017E579–7F, retargeting a return) explicitly preserves bit 31
  (`WANDI 2,0x80000000; WIOR`). The `XWISZ [ac3+0x0]` skip-return bumps
  cannot carry into bit 31. All `WIORI x,0x80000000` sites are
  indirection-bit address building, not carry signalling.
- Push-jump convention (`XPSHJ`/`LPSHJ` ↔ `WPOPJ`) neither saves nor
  restores c — such calls are carry-transparent only if the callee is;
  `WPOPB` pairs with WSSVS-style frames and restores saved c. **Game
  code contains zero push-jump/pop instructions** (rt-internal only),
  and no verdict in §3 depends on call transparency at all.
- The stack-fault handler push (EagleStack:126) saves c; fault paths
  restore it — transport, no game consumer downstream.

## 5. Empirical status

Static result: old-vs-new carry is **identical at every reachable
consumer** by construction (all producers fix-invariant), so there is
no game-observable difference to capture. All 8 idiom-cluster blocks
are lowered in quest.ir2.book (embedded instructions), so the battery's
book-IR leg exercises them through the same fixed helpers; the landing
bar's coverage evidence (exception-site blocks demonstrably executed)
remains the right empirical demonstration and needs no new tooling.
No pre-fix capture is required: there is nothing the fix can change at
these sites.

## 6. WADC — plan-gate ruling input

The census proves no wide-produced carry (WADC's included) is consumed
on any reachable path. Per the P24 prompt, the routing question is
therefore **unobservable to gameplay**. Options:

- **(a) Fix WADC with the rest** (carry = literal carry-out of
  dst + ~src; `WADC x,x` → c=0). Pro: one uniform rule, no special
  case to document/maintain; unobservable either way. Con: if real-MV
  evidence later shows c=1, the emulator is "wrong" on a
  never-consumed flag until corrected.
- **(b) Keep WideCarry.md's conservative routing** (WADC keeps today's
  c=1 for x,x). Pro: no behavior change without hardware evidence
  (METHOD §8's spirit). Con: permanent special case; the c=1 it
  preserves is itself only the >>31 coincidence, not evidence.
- Either way, Part-3 re-derivation treats WADC carry per the ruling
  (p_defon.cpp:142 currently bakes in "WADC sets c=1"; Project2
  DERIVATION ~310 likewise).

The user rules; the session has no stake beyond noting that (a) is now
evidence-safe by unobservability.

## 7. Findings register (report, don't fold in silently)

- **F1**: XWDO/LWDO are wide carry writers missing from WideCarry.md's
  affected-instruction list (§2).
- **F2**: runtime/frames.cpp re-implements the buggy helpers verbatim
  (emu_add/emu_sub, >>31) — a fourth Part-3 landing site beyond the
  three lib_error stages named in WideCarry.md. The "sweep ALL
  runtime/" scope in the prompt is confirmed necessary.
- **F3**: the user's 4-site exception list is real but NOT
  wide-reached — each site's producer is the adjacent `ADD.O x,x,SBN`.
  Delta reported per the prompt: the census expected these as the
  wide/ambiguous residue; there is none.
- **F4**: `?URTB` branches on caller carry at entry but is dead code
  (unreferenced); recorded in case future work ever links it.
- **F5**: METHOD §5's "WSUB x,x clears carry" correction note is owed
  at Part-2 landing (already scheduled in the prompt), and §2's writer
  list here supersedes WideCarry.md's when the two differ.
