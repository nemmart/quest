# Project 3 — Frames and the Unwind: T?AREA, I.PROLOG, I.EPILOG, I.GOTO (124 words)

Hi Claude! You are one of three parallel sessions translating the PL/1
condition system to native C++ (Quest reconstruction, Milestone 3).

Read IN ORDER before any work: docs/METHOD.md, docs/SharedProtocol.md
(binding merge rules + frozen interfaces), docs/M3Plan.md, then this.
Background as needed: docs/O_ON.md (the SS/LJSR convention precedent —
your closest relative), docs/ERROR_PROCESSING.md, docs/Run.md.

## Your routines

| Addr | Symbol | Words | Notes |
|---|---|---|---|
| 0x7017ED93 | T?AREA | 8 | body derived (SharedProtocol.md); YOU own the emu_rt wrapper + validation |
| 0x7017E733 | I.PROLOG | 29 | LIVE every session (READ_IN bracket, 10+/session); **LJSR entry, returns pc+7** — a continuation, not WRTN |
| 0x7017E77D | I.EPILOG | 7 | LIVE; LJSR; never "returns" conventionally |
| 0x7017EC7C | I.GOTO | 80 | the unwind; LIVE per handled signal; 26 game call sites (ON-units GOTO out); ends by transferring to the resume pc |

Disassembly: Disassembled/quest-rt.dis. I.GOTO ends at 0x7017ECCC
(I.FFALT follows — NOT yours, dead in emulation; the old "XCT in
I.GOTO" claim was wrong, there is no XCT hazard in your set).

## Facts already established this project (verify, don't re-litigate)

- T?AREA: `WSAVS 0; LDASB 2; XLEF 0,[sb-0x29]; XWSTA 0,[fp-8]; WRTN` —
  the store patches a WSAVS-saved-register slot so WRTN returns the area
  address in ac0 (?UDIV32 precedent: RTBridge::set_return_ac). Confirm
  WHICH slot fp-8 is against RTBridge's frame layout and captures; the
  frozen `rt::t_area` (implemented) must agree with your captures — if
  it does not, that is a REPORT-level finding, not a silent edit.
- I.PROLOG/I.EPILOG are LJSR routines with continuation returns
  (I.PROLOG resumes at entry-ac3-relative pc+7 — the dotted-helper
  analysis in RTWorklist.md). Use `RTBridge::native_transfer` for the
  continuation; the SS-convention notes in O_ON.md and RTBridge.hpp are
  your template, but DERIVE this pair's exact frame/stack behavior —
  I.PROLOG builds the condition frame the whole chain walks. Your
  DERIVATION.md must document that frame layout precisely: Project 1
  reads it (O.SET's walk) and O_ON.md documents O.ON's writes into it —
  the three must agree.
- I.GOTO: 26 game call sites; observed live ret=7016EC74-style (called
  FROM handler game code). It unwinds — cuts the stack back to the
  target frame and transfers to the resume pc. Your native version ends
  with `native_transfer(resume_pc)` after setting wsp/wfp/registers per
  the derivation. The master pairs via the range-exit rule
  (SharedProtocol.md) — I.GOTO is the poster child. Note
  `Machine::rt_pending_return` clearing is handled by that rule; you do
  not touch it.
- The non-local unwind produces benign shadow call-stack notices
  ("call return address; X, stack return address: Y") —
  UNIMPLEMENTED.md §8. Decide and document what your native unwind does
  to debug::CallStack so backtraces stay sane (see
  CallStack::native_return for the precedent; you may need a
  native_unwind sibling — that is a hw/ file edit, so PROPOSE it in
  REPORT.md with the exact diff rather than editing, per merge rules).

## Deliverables

- `runtime/t_area_wrapper.cpp` or fold into `runtime/frames.{cpp,hpp}` —
  your call
- `docs/Project3/DERIVATION.md` — I_ALLOC.md quality bar; the condition
  FRAME LAYOUT section is the centerpiece (Project 1 depends on it)
- `docs/Project3/REPORT.md` per SharedProtocol.md (include the proposed
  CallStack diff if needed)

## Validation you can do alone

Best liveness of the three projects: T?AREA fires on every signal
(both triggers), I.PROLOG/I.EPILOG on every session's READ_IN bracket
with NO fault injection, and I.GOTO on every handled signal. All four
have boundaries that don't cross into the other projects (T?AREA is a
leaf; PROLOG/EPILOG are self-contained frame ops; I.GOTO is called from
game code and transfers to game code). With LOCAL temporary
registration (revert before REPORT; list entries instead): full
capture + lockstep validation is achievable for your entire set this
session. The store-"ABC" trigger gives you a handled-signal I.GOTO;
LIST_PLAYERS fault injection gives a second shape.

Work plan-first; one routine at a time (suggested order: T?AREA wrapper →
I.EPILOG → I.PROLOG → I.GOTO); honest statuses.
