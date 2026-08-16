# Project 15 — ArgWindows: the call-site arg-push census (REPORT)

New tool `Tools/ArgWindows.java` (Tools family, beside StartStop/Follow;
reuses Memory, Instruction decode, SymbolTable, Follow's wordLength and
target-set computation). Analysis only — no emulator, book, or
doc-of-record edits. Invocation added to Tools/README.md:

```
java -cp tools.jar ArgWindows ../QUEST QUEST quest.addrs quest.targets \
     ../Work/c_src/quest.addrbook quest.dis quest.argmap quest.callsites
```

## The census answer for M4b (headline)

**The "never" bucket is EMPTY. The CLEAN bucket is 100%.**

| class                    | sites |
|--------------------------|------:|
| CLEAN                    |  566  |
| CLEAN-EMPTY (argc==0)    |  188  |
| CLEAN-WITH-INNER-CALLS   |    0  |
| PROBLEMATIC              |    0  |
| **total game-target**    | **754** |

Every one of the 754 game-target call sites has a straight-line,
mechanically-emittable arg window (or none, argc==0). quest.argmap holds
1352 arg lines covering all 566 non-empty sites. Zero sites needed the
PROBLEMATIC escape hatch, zero windows contain inner calls, zero windows
contain a branch, skip, assert, dispatch, mid-window target, or any
stack-state-touching instruction. M4bNotes open question 3 is answered:
the dataflow-ambiguous residual class does not exist in QUEST.

## Why the result is this clean (1988 codegen findings)

1. **The compiler evaluates call-arguments into temporaries before the
   push run starts.** Nested-call shapes `F(G(x))` compile as
   `t := G(x); push-run; call F` — never as pushes interleaved with an
   inner call. Hence zero CLEAN-WITH-INNER-CALLS: the accounting for
   `f1(f2(x), y)` was implemented and unit-verified but no real site
   exercises it.
2. **Window interiors are almost sterile.** Across all 566 windows the
   only non-push instructions inside are: XWLDA (45), WMOV (35),
   LDAFP (14), XNSTA (7), XWSTA (3) — address formation and the XCALL
   static link, all stack-neutral. Bounds-check asserts (skip+DERR) and
   string ops (WCMV etc.) consistently sit *before* the first push.
3. **Push vocabulary is tiny**: XPEF/LPEF (1 wide), XPEFB/LPEFB (1 wide),
   WPSH s,d (multi-wide). No WFPSH, no WMSP, nothing exotic feeds an arg.
4. **Mixed arity is per-site and classifies normally**, as M4bNotes
   predicted: REFRESH_SCREEN appears as 10×argc0 + 3×argc1,
   RETURN_MESSAGE as 1×argc3 + 4×argc6, all CLEAN — the M4a book flag
   was indeed conservatism.
5. **Generalization probe**: rerunning with the book augmented by all 23
   RT call targets classifies **all 1749 call sites in the program**
   CLEAN/CLEAN-EMPTY (4243 arg lines). The property is program-wide, not
   a game-code accident — relevant if the M4b goal question ever pulls
   RT arg pushes into scope.

## Arg slot numbering (user gate ruling + empirical settlement)

Slots are by DEPTH, not push order: arg N at [wsp−2N] at the call ==
[wfp−10−2N] in the callee. Settled empirically against
`c_src/hw/EagleIntegration.cpp` (`arg_addr(n) = read_wide(fp-10-2n)`)
and `c_src/quest/return_message.hpp` (arg3 = the message): in the
RETURN_MESSAGE,6 window at 7015BE74, slot 3 is the push of the local the
caller just WCMV'd the message into. **So arg1 = LAST-pushed wide,
arg argc = first-pushed (= the window start).** Note the PROMPT.md
example's pc ordering (arg1 at the earliest pc) is misleading under this
convention; quest.argmap follows the formula. A WPSH spanning several
wides yields several slots at the same pc (e.g. RETURN_MESSAGE arg4 and
arg5 both at 7015BE6B).

## Cross-checks (all pass; tool verifies before exiting)

- Tool game-target sites 754 == quest.dis-derived count 754; tool total
  call edges 1749 == quest.dis call lines 1749 (the known census).
- All 63 XCALL sites present; every one has its WMOV/XWLDA static link
  immediately before the call; no link sits inside another site's window.
- Independently computed target set (via Follow.process, covering skip
  fall-throughs by construction) MATCHES the quest.targets file exactly
  (17982 entries).
- argmap/callsites agreement: 1352 arg lines == Σ argc over CLEAN sites,
  each such site has exactly argc lines; enforced by the tool at exit.
- The single "INDIRECT INDEXING" caution during target computation is
  the pre-existing LJSR @… to .LIERR at 7015BD6F (the one 'u' tag in
  quest.tags) — not a call edge.

## Verification of the machinery (because zero PROBLEMATIC must be earned)

A result this clean demanded proof the disqualifiers can fire:

1. **Independent re-derivation**: a Python implementation working from
   quest.dis TEXT (different decode path, different traversal) recomputed
   all 691 LCALL game sites — identical clean/empty classification and
   identical window starts on all 540 argc>0 sites, zero mismatches.
2. **Synthetic firing tests** (12 cases, /tmp only, not deliverables):
   skip-in-window, stack-op-in-window (WPOP), flow-in-window (WBR),
   push-straddles-window-start, depth-never-closed, target-lands-in-window,
   discontinuous-code each fire with the named reason; window-start-as-target
   is exempt (user ruling) and stays CLEAN; WPSH 0,2 multi-slot attribution
   correct; inner-call debt accounting yields CLEAN-WITH-INNER-CALLS with
   correct slots, and a second source on the inner return word fires
   inner-return-has-other-source.

## Rulings applied (from the plan gate)

- Anything touching stack state between the first push and the call is
  never handled automatically → PROBLEMATIC (the tolerated set inside a
  window is exactly: the attributed pushes, inner returning calls, and
  stack-neutral compute).
- The window-start pc being a branch target is exempt (routinely a call
  return); strictly-interior targets disqualify.
- Inner calls to RT routines would count the same as game inner calls
  (identical WRTN net accounting); non-returning targets (RETURN_MESSAGE
  is [[noreturn]] per its C++ port) would be PROBLEMATIC. Neither case
  occurs in the data.
- Book = Work/c_src/quest.addrbook, all 130 entries including
  #-commented ones (they are game routines whether or not migrated).

## Deliverables

- Tools/ArgWindows.java (+ README.md invocation line)
- Disassembled/quest.argmap (1352 lines)
- Disassembled/quest.callsites (754 lines + summary census)
- this report

## For M4b planning

The push_map's direct ancestor now exists and covers every site. With an
empty "never" bucket and no inner-call windows, the open-window shadow
accounting (M4bNotes issue 1 / Mapper §3b) never has to survive an inner
call in practice, and the arg-build re-entrancy tripwire (issue 2) has no
statically-visible trigger — both remain worth keeping as fail-loud
runtime checks, but the census says they should never fire.

---
*Review (session of record, Aug 16): reproduced end-to-end (JDK build,
run against QUEST.PR: 754/566/188/1352/63, target set 17982 — exact);
RETURN_MESSAGE,6 window at 7015BE74 hand-verified against quest.dis
(WPSH multi-slot, depth numbering, tolerated LDAFP interior, WCMV
outside). APPROVED. The PROMPT.md output example's arg1-at-earliest-pc
ordering was wrong; the report's empirical settlement is correct and
quest.argmap follows the formula.*
