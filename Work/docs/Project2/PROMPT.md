# Project 2 — The Signal Sources: ?LIB_ERROR trio (196 words)

Hi Claude! You are one of three parallel sessions translating the PL/1
condition system to native C++ (Quest reconstruction, Milestone 3).

Read IN ORDER before any work: docs/METHOD.md, docs/SharedProtocol.md
(binding merge rules + frozen interfaces), docs/M3Plan.md, then this.
Background as needed: docs/ERROR_PROCESSING.md, docs/I_ALLOC.md (your
heap callees, native), docs/Run.md.

## Your routines

| Addr | Symbol | Words | Notes |
|---|---|---|---|
| 0x7017E33A | ?LIB_ERROR | 152 | LIVE per signal; the library error raiser |
| 0x7017DE25 | ?LIB_ERROR_CODE | 14 | 1 game caller (LOGON 0x70175EE4) |
| 0x7017E3D2 | ?DEFAULT_ERROR_HANDLER | 30 | LIVE per signal; installed at [area+0x1E], called indirectly |

Disassembly: Disassembled/quest-rt.dis. NOTE the corrected entry:
?LIB_ERROR is 0x7017E33A (an older doc said 0x7017E2E0 — wrong; also
note rtcalls once logged a ?LIB_ERROR return address of 7017DDB2, which
is worth understanding — find who at ~7017DDAF calls it).

## Facts already established this project (verify, don't re-litigate)

- ?LIB_ERROR structure (partially read at 0x7017E33A–E3D0): T?AREA ×9;
  installs ?DEFAULT_ERROR_HANDLER into [area+0x1E] when empty (zeroing
  [area+0x20]); sets a bit via WBTO 2,0; stores the error code at
  [area+0x1]; **free-then-alloc message buffer** at 0x7017E371–E39C:
  load [area+0x3], skip I.FREEW when zero (first signal), I.ALLOC
  (class 3, size from the message length via WMOVR — derive exactly),
  store back to [area+0x3]; copies the message (WCMV) with a
  length-clamp (WSGE 1,0; WMOV 1,0); tail at 0x7017E3C9–E3CD loads
  [area+0x20]→ac1, [area+0x1E]→ac2, then **`XCALL 0,0,[ac2+0x0]` — the
  indirect handler dispatch**. That call's target is runtime data; end
  your native version with `RTBridge::native_transfer` to the resolved
  target after replicating the XCALL's stack/register setup exactly
  (derive XCALL's pushes from hw/EagleStack.cpp — argument marshaling,
  frame word, ac3).
- Heap callees are NATIVE: call `rt::i_alloc`/`rt::i_freew`-equivalents
  as plain C++ — read runtime/i_alloc.{hpp,cpp} for the actual exposed
  signatures and their locking expectations (they wrap I.LOCK/I.UNLOCK;
  understand whether the rt:: layer or your caller owns that).
- `rt::t_area` is frozen and implemented (runtime/t_area.hpp) — use it.
  The area field map in SharedProtocol.md matches what your routines
  read/write; extend it in your DERIVATION with anything new, and
  resolve what the ubiquitous `XLEF 2,[ac2+0x8]` base (area+0x8) is.
- ?DEFAULT_ERROR_HANDLER (0x7017E3D2–E3F0): T?AREA, then LCALLs O?SIGNAL
  (0x7017E3EB, ret 7017E3EF observed live). O?SIGNAL is **Project 1's**
  routine: code against an `rt::` signature you propose in REPORT.md
  ("Interfaces I consume") — SharedProtocol.md interface rules. Until
  integration, your validation stops at that boundary (see below).
- The LIST_PLAYERS unhandled second-signal path exists behind your
  routines (UNIMPLEMENTED.md §9 area); anything of yours heading to
  DEF?ON/?FATAL: native_transfer to that entry — terminal machinery
  detaches (TerminalDetach.md).

## Deliverables

- `runtime/lib_error.{cpp,hpp}` (+ siblings at your discretion)
- `docs/Project2/DERIVATION.md` — I_ALLOC.md quality bar: instruction
  semantics, flag side effects from the emulator source, frame images,
  residue maps (?LIB_ERROR has a real frame — WSAVS 0x0002 — plus the
  message-buffer heap writes: your footprint includes BOTH).
- `docs/Project2/REPORT.md` per SharedProtocol.md.

## Validation you can do alone

Fault-inject (`QUEST_FAIL_OPEN=USER_DATA_FILE`, login, L→P) drives
?LIB_ERROR live. With a LOCAL temporary registration (revert before
REPORT): capture-validate ?LIB_ERROR to the XCALL boundary — the master
runs the emulated body; under the range-exit rule the pair rendezvouses
where control leaves the RT range, which for your routine is NOT
immediate (the dispatch target ?DEFAULT_ERROR_HANDLER is RT-range!).
So your ?LIB_ERROR validation strategy must be: native ?LIB_ERROR
transfers to ?DEFAULT_ERROR_HANDLER's ENTRY (RT-internal transfer to a
still-emulated sibling is exactly the forbidden case) — therefore
either (a) translate the trio as one unit whose native path runs
LIB_ERROR + DEFAULT_ERROR_HANDLER as plain C++ and transfers onward at
the O?SIGNAL boundary via the agreed rt:: signature — blocked at
integration, capture-validate everything before it; or (b) validate
?LIB_ERROR_CODE and ?DEFAULT_ERROR_HANDLER separately where their
boundaries allow. Reason it out, pick, and record the choice + evidence
in REPORT.md. Note the indirect target is only ?DEFAULT_ERROR_HANDLER
when the game hasn't installed its own — check whether ON ERROR units
replace [area+0x1E] (O_ON.md / Project 1's chain layout) and handle
both cases.

Work plan-first; one routine at a time; honest statuses.
