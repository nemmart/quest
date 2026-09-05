# Project 28 — `rt_call`: decorate the 987 runtime call sites; plus the P26 leftovers

GOAL (user design, Sep 5): every game → runtime call (`LCALL … # ?NAME`,
987 sites, all `?`-prefixed callees) becomes ONE statement

    rt_call ?NAME(e1, e2, …, eN)

with the argument pushes (2,876 XPEF/LPEF/XPEFB/LPEFB + 8 WPSH) folded
into argument EXPRESSIONS — t-places or inline — so the IR reads as the
PL/I call it was. This is P25's decoration applied to the runtime edge:
P25 covered the 566 game→game sites (book slots); this covers the 987
game→runtime sites (REAL stack — the runtime reads its args there).
Plus the small P26 leftovers, sized at the plan gate: LNDO (1 embed;
register now visible after the Sep 5 disassembler fix), the LDSP pair
(2; `goto [labels] e` with a range assert — the two DERR 17s P27 left
out), and the 67 Nova LOAD forms (high half is UNDEFINED per the
manual — HWFindings_Sep5.md §3 — so they lower matching the emulator's
zero-fill, spec marks it a don't-care). Expected: embeds 8,529 → ≈4,600.

Hi Claude! Solo implementation session; user reviews at the plan gate
and at the landing. Read docs/METHOD.md first. Context of record:
docs/IR.md (ir 3; `call`/`argpush` §… is the P25 shape you are NOT
reusing — see "Why real stack"), Project25/{PROMPT,REPORT}.md (the
decoration discipline: per-site census, argc must match, refuse on any
mismatch), docs/Project26/REPORT.md §7 (the RT-call adjacency it
declined, with counts), docs/HWFindings_Sep5.md, docs/Provenance.md
(verify your tree FIRST — quest.dis 5c1db5fb…), docs/WPSH_WPOP.md (the
stack-push semantics and the load-bearing overflow gate). TREE VINTAGE:
main after P27 merges — state the commit. Runner: godspeed, 4 cores,
no Java, JOBS=3; tasks use bin/task_source.sh (never `git checkout
<branch> -- Work` in the queue tree).

## Design of record (user rulings, Sep 5)

- **`rt_call ?NAME(e1, …, eN)`** is a STATEMENT (never a terminator; the
  LCALL's fall-through continues in the same block, exactly as the
  embedded LCALL does today).  Semantics = the Eagle sequence it
  replaces, via the SAME helpers: evaluate and `wide_push` each argument
  — **RIGHT TO LEFT: eN first, e1 last** (arg 1 lands nearest the frame,
  arg N deepest; the M4a layout `arg N at wfp-10-2N` and the pushmap's
  `# XPEF arg2` before `# LPEFB arg1` are the evidence) — then perform
  the LCALL through the emulator's own LCALL path (EagleStack; hoist
  into a callable helper if it is not one — a K=1 stock gate of its
  own).  The runtime then runs exactly as today (emulated, or a native
  span for translated routines); lockstep behaviour is unchanged.
- **Real stack in BOTH modes.** No book slots, no argpush machinery: the
  runtime reads the stack.  `wide_push` carries the wsp==wsl overflow
  fault — that is why the pushes go through the helper and never
  through `M32[wsp+2] = …; wsp = wsp+2` spelled out (stack WRITES remain
  refused in ir 3 grammar; rt_call is the one statement that moves wsp,
  and it does so inside the helper).
- **Arguments are expressions.** XPEF/LPEF → a word-pointer expression
  (`wp(...)`/the address form the P25 grammar already renders for
  argpush); XPEFB/LPEFB → a byte-pointer expression (`bp(...)`); WPSH →
  the register's value.  t-places are single-assignment and block-local
  (P26), so `t0 = …; rt_call ?X(t0, wp(ac3, 0x14))` and the fully inline
  form are both legal; the emitter picks inline when the push's operand
  is a pure expression of the state at the call, t-places when an
  intervening statement (XWSTA, XLEF) changes something the expression
  reads.  The census decides per site; refuse when unsure.
- **Register arguments are ordinary preceding statements.** 100 windows
  carry an `XLEF 2,…` (ac2 = pointer for `?UNSIGNED_TO_CHAR`-class
  callees) or an `XWSTA` between the pushes.  Those lower as the
  statements they are, in program order; rt_call does not declare them.
  The per-callee table (Part 1) records which registers each runtime
  routine reads on entry and writes on return (result register, e.g.
  `?RANDOM_NUMBER`), read from the runtime source (c_src/runtime/*.cpp
  native bodies, RTStubs registrations) or the disassembly of the
  emulated routine — cited.  Nothing in the IR asserts them; they are
  documentation and a check that no site depends on an unmodelled one.
- **argc discipline (P25):** the LCALL's argument-count field must equal
  the number of pushes captured (Sep 5 census: 987/987 match).  Any
  site where the window is not [pushes + at most XLEF/XWSTA] → refuse,
  stays embedded, censused.
- `rt_call` is stock-legal AND book-legal; the loader accepts it in
  both.

## The work

### Phase A — census + design (plan gate; may run while P27 is in flight —
###           touches no file P27 edits)

1. **Site census** (`tools/rt_sites.py`, text-only, over quest.dis +
   quest.blocks.split): all 987 sites — callee, argc, push mnemonics in
   order, interleaved instructions (expected: XLEF 72, XLEF+XWSTA 15,
   XWSTA 11, XWSTA+XLEF+XWSTA 2, contiguous 887), block of the LCALL,
   whether the window crosses a block boundary (refuse if so).
   Reproduce the Sep 5 numbers or report the difference.
2. **Per-callee convention table** (docs/Project28/RTConventions.md):
   for each of the ~15 callees — stack argc (must be constant per
   callee; a callee called with two different argcs is a FINDING),
   registers read on entry, registers written on return, whether it is
   native or emulated today, source citation.
3. **Grammar draft for IR.md**: the `rt_call` production, the
   right-to-left evaluation rule stated in words AND with one worked
   example from the census, the "no wsp arithmetic outside the helper"
   rule, the refuse list (rt_call to a non-`?` target; argc mismatch;
   argument that is not a pure expression at the call).
4. **Leftovers census**: LNDO 7015C0C7 (now `LNDO 0,25,[0x70000216]`),
   the LDSP pair 701604D4 / 7016D707 (table bounds from quest.tags —
   Follow resolved them — and the DERR 17 out-of-range exit → `assert(lo
   <=s idx && idx <=s hi, "DERR 17 @pc"); goto [t1,…,tN] idx - lo`), the
   67 Nova load forms by (op, CC, SS, KKK) shape with the emulator's
   ac+c write (NovaCompute.cpp:63–66) as the reference; propose a
   `nova(...)` effectful helper or a per-shape lowering; recommend.
5. **Battery plan**: 034 template via bin/task_source.sh, JOBS=3, 13
   legs; verdict lines for rt_call sites emitted / refused, embeds, and
   rt_call coverage (sites executed / sites emitted, by callee) from the
   IRExec first-execution lines.  Expected count after landing.

STOP AND REPORT.  Phase B waits for BOTH the user's rulings AND P27 on
main (lower.py, IRExec, IR.md are P27's files until then).

### Phase B — implementation

- EagleStack: make the LCALL body a callable helper (same-helpers), K=1
  stock gate before anything else.
- lower.py: the rt_call emitter (window capture → argument expressions,
  right-to-left push order preserved in the statement's argument order
  e1…eN = arg1…argN, so the READER sees PL/I order and the EXECUTOR
  pushes N→1); LNDO as XNDO with pc+4; LDSP per the census; Nova loads
  per the ruling.  Land in slices behind K=1 book + stock gates: (i)
  contiguous single-callee `?WRITE_SCREEN` sites (723 — the bulk), (ii)
  the remaining contiguous sites, (iii) the 100 interleaved sites, (iv)
  leftovers.
- IRExec: parse `rt_call`, evaluate args, push right-to-left via
  `wide_push`, invoke the LCALL helper; loader refusals per the grammar
  draft.  The ovk/ovr check and block-ordinal accounting are unchanged
  — the callee's blocks are not IR blocks and the runtime is not
  lowered.
- IR.md: ir 3 → ir 4 IF the grammar gains a production (it does:
  rt_call); version-history entry; the evaluation-order rule.
- Artifacts regenerated with headers; Provenance.md updated.

### Phase C — validation

- Local K=1 book/stock/play legs per slice; then task 041 (13 legs +
  verdict lines).  Landing bar: 13/13 green, 0 div, every censused site
  emitted or listed as refused with reason, embeds ≤ the Phase A
  prediction, `?WRITE_SCREEN` and `?RANDOM_NUMBER` rt_call sites LIVE
  in the play legs (they are the whole game's I/O — they will be).

## Boundaries — BINDING

1. **Scope**: rt_call at `?` sites + the three leftovers.  NOT: game→game
   calls (P25 owns them), WMSP/STASP dynamic allocation, the 3
   pass-by-reference WPSH+LDASP temps at 70169B77, string ops (WCMV),
   any checker change, any runtime translation.
2. **Phase A before Phase B; Phase B after P27 merges.**  Do not branch
   lower.py while P27 is open.
3. **Same helpers.**  Pushes and the call go through the emulator's
   functions.  No wsp arithmetic in the IR or in IRExec.
4. **Refuse, don't guess.**  A window that isn't [pushes + XLEF/XWSTA],
   an argc mismatch, a callee with inconsistent argc, an argument
   expression that reads state an intervening statement wrote — refuse,
   census, report.
5. **Design-vs-reality: STOP AND REPORT** — a runtime routine that reads
   an argument other than from the stack/known registers; a site whose
   pushes are not right-to-left arg order; a battery leg that moves.
6. Deliverables: tools/rt_sites.py, docs/Project28/{Census,
   RTConventions}.md, the emitter + executor + IR.md (ir 4), regenerated
   artifacts + Provenance.md, task 041 result, REPORT.md + worklog,
   CURRENT_STATE/NextSession, TREE VINTAGE, tool runtimes (flag > 10 s).
