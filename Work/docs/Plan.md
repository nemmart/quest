# Quest Reconstruction — Plan

*Created: August 2026. Follows on from PortingPlan.md (Phase 1 complete;
Phase 2 lockstep harness complete and validated). This plan covers the path
from "harness ready" to reconstructed source.*

## Milestones (Big Picture)

- **Milestone 1**: Java code ported to C++, game runs fine: **DONE**
- **Milestone 2**: dual emulation — master + clone against one server,
  same input, execution compared at sync points; the server reads and
  writes separate shared memory systems and ensures they match:
  **DONE**
- **Milestone 3a**: code broken into categories — L1 game/runtime
  wrapper code, L2 the error handler, L3 terminal, plus L0 (syscalls
  and utilities; Layering.md for specifics). All L2 lifted to native
  code; all inputs and outputs of every handler entry point
  understood (proven by continuous lockstep execution + the Project
  1–5 derivations): **DONE**
- **Milestone 3b**: a contract completely describing the L2 surface —
  any native implementation honoring it can replace the MV/8000
  stack-based solution. Then the FIRST conforming implementation,
  STAGED: frames/args/locals stay on the MV stack, but ALL handler
  state is tracked natively and NOTHING walks the MV stack to find an
  ON handler (the native chain records each establisher's wsp level,
  since I.GOTO still cuts the stack on unwind until M4): **WIP** —
  contract DELIVERED and approved (Project 6, Aug 13 2026); the
  crossings-only checker it requires is LIVE (CrossingsChecker.md /
  CheckerHistory.md, Aug 13 2026); the conforming implementation
  (Phase 2) is the remaining work
- **Milestone 4a**: the WSAVS-hijack translation (design:
  PostM3b-FlatGraph.md, M4 entry design) — locals and args move to
  per-routine global areas; the stack shrinks to pure call linkage
  (ten words per depth, 0 bytes for locals beyond the call
  store/restore); game code unchanged, one instruction hijacked,
  probe-class checking + pointer-normalized mediation. B1-class
  dies by construction. Further transformations (analysis-driven
  rewrites, handler lowering, expression recovery) are LATER 4x
  stages, layered on the running 4a system.
- **Milestone 4 (b+)**: the flat-graph analyses and rewrites over
  the 4a system — the routines are not re-entrant (4a's live-flag
  tripwire verifies dynamically; the call-graph census verifies
  statically), so everything runs from static/global memory. **Verification-regime premise (ratified Aug
  2026): de-stackify the STORAGE, keep the ACCOUNTING.** The clone
  maintains shadow stack arithmetic — wsp/wfp/frame-address values
  computed exactly as the master's, ~a dozen counter updates per call
  boundary — with no memory behind them, so the full-register-file
  compare (incl. the pinned dispatch token, NativeDesign §2 / hazard
  H2) survives M4 UNCHANGED: same rendezvous, same oracle, exactly
  when the mechanical rewrite of the game needs it most. Sound
  because de-stackified code never dereferences stack addresses (all
  data at fixed locations); self-enforcing because a stray
  dereference reads unbacked memory and diverges immediately. The
  scan-3 abstraction license is deliberately never spent — matching
  the master beats diverging from it.
- **Milestone 5**: live-range analysis to split global slots reused
  by different variables

## Key Insight: Non-Reentrancy Enables De-Stackification

The game is not reentrant (verified in a prior session; confidence ~99%,
not provable with certainty — watch for recursion or the ctrl-A/ctrl-C
task touching shared routines). Therefore each routine's WSAVS frame and
stack variables exist in exactly one copy, at stable addresses. Stack
slots can be treated as fixed storage locations per function.

This dissolves a wall from earlier efforts: the compiler reuses stack
slots within a function (stack+4 holds A early, B later), which was
tricky to unpack by hand. With fixed addresses, slot reuse becomes a
standard **live-range analysis** problem: mechanically de-stackify the
whole game, then split each slot's distinct live ranges into separate
variables.

## The Obstacle: PL/1 Condition Handling Walks the Stack

The game uses PL/1 ON-condition machinery (try/catch equivalent; the
catch logic is simple everywhere, but present). The runtime walks stack
frames to locate handlers — it reads the stack *structurally*, not just
as slot storage. De-stackification breaks that.

Hence the ordering below: replace the runtime first, so that afterward
the stack is touched only by compiled game code, which we can rewrite
mechanically.

Two Aug-2026 decisions shrink this obstacle (detail: M3Plan.md,
TerminalDetach.md):

- **DERR = terminal-with-ABORT** (Layering ruling 7; supersedes an older
  hard-abort-instead-of-vectoring decision): faithful vectoring to
  DERR.TRP, one verified pair on both engines, then hard stop naming pc +
  DERR number, no write-back — never recovery into the
  condition system — so the only non-local exit left is a signal, which
  native I.GOTO makes an explicit event.
- **Terminal detach**: at I.STOP / ?FATAL (DEF?ON until the Aug 12 lift
  made it verified L2) the clone stops after one final verified pair and
  the master runs to completion unverified; DERR.TRP instead ABORTS the
  world after its verified pair (ruling 7); terminal syscall 0310
  RETIRES the pair at dispatch.
  ?FATAL and the traceback machinery are permanently out of translation
  scope, and the mixed-unwind worry for Step 2 reduces to: the native
  condition system owns the wsp reset when abandoning emulated leaf
  frames (state of de-stackified callers lives in globals, so the unwind
  is "reset wsp to the establisher's level and jump" — no frame walking).

## Steps

### Step 1 — native condition system (scope narrowed Aug 2026: M3Plan.md)

Get the condition system running with **no emulated PL/1 dispatch**:
DONE for the 616-word minimal lift (M3Plan.md, validated under
lockstep), including the DEF?ON cluster (translated Project 4, lifted
Project 5). Remaining: contract-fidelity per Layering.md — the L2
contract is DELIVERED (Project 6) and the crossings-only checker is
LIVE (Aug 13 2026); the staged M3b implementation (Phase 2) remains. Leaf routines follow opportunistically; they are not on M3's
critical path.

- Translate one routine at a time; validate by gameplay under the checker.
- **Opus-era runtime translations are reference only.** Re-derive each
  routine fresh from the disassembly; consult the old sources for PL/1
  convention insight, never trust them as-is. (Same policy PortingPlan.md
  set for the old game translations.)
- Reinvent the calling-convention bridge (EagleIntegration's old role)
  carefully as part of the first translation.
- More runtime input material to come from the user before starting.

### Step 2 — De-stackify the game

Mechanically rewrite game code so stack slots become fixed per-function
storage locations. Requires Step 1 (no structural stack readers remain).

**Mechanism — synthetic Instructions (agreed design sketch):**

The emulator today has no address→Instruction map: execution re-fetches
the 16-bit word at pc each step and decodes via opcode-indexed tables
(Instruction objects are per-opcode singletons). De-stackification adds
an address-keyed override layer consulted before fetch+decode: an
address carrying a synthetic Instruction executes it; all other
addresses fall through to normal decode. Properties:

- Synthetic Instructions escape the 16-bit encoding entirely — e.g.
  "load flattened-local #12 of DISPLAY_INVENTORY into ac0" as a
  first-class object with arbitrary operands, occupying an address
  without occupying memory words. `execute` already receives the
  address and returns the next pc explicitly, so synthetic
  instructions have no encoding-length constraint.
- Hot-loop cost is contained with the same range-gating trick as the
  RT coverage (cheap range test before any map lookup).
- De-stackification becomes **incremental and testable per-function**:
  rewrite one function's slots to synthetic instructions in the clone,
  validate under lockstep, move on — not a whole-binary transform.

**Argument passing:** function args are passed on the stack (arg
pointers pushed at the call site; PL/1 passes by reference, so the
pushed values are addresses; LCALL pushes the arg count; the callee
reads args at negative offsets from the frame). Call sites stay
unchanged so de-stackified and original functions interoperate. First
version: a synthetic WSAVS at the function entry copies the incoming
arg pointers (and count) from the stack into the function's flat
storage, then the body runs against flat slots. By-reference semantics
are preserved automatically since what's copied are the pointers.

### Step 3 — Live-range analysis

On the de-stackified code, compute live ranges per storage slot and split
multi-use slots into separate variables. Output feeds source
reconstruction with honest, per-variable names.

### Step 4 — Source reconstruction

(Outline; detail later.) Lift the de-stackified, slot-split code to
idiomatic C++17 function by function, validated continuously under the
lockstep harness per the existing equivalence definition (same OS-layer
calls, same shared-data changes).

## Tooling Status (Tools/)

Built clean under OpenJDK 21; `tools.jar` in `Tools/`. Pipeline per
`Tools/README.md`.

- **Game artifacts (trustworthy, complete):** `quest.symbols`,
  `quest.addrs`, `quest.targets`/`quest.tags`, `quest.blocks` (basic
  blocks, symbol-labeled, ASSERT patterns absorbed), `quest.dis`,
  `quest.code`, `quest.mem`.
- **Runtime artifacts (best-effort):** `quest-rt.addrs`, `quest-rt.dis`
  only. Follow/DisassembleBlocks are not run on the runtime — it contains
  register-indirect XCALLs (`XCALL 0,0,[ac2+0x0]` etc.) that static
  reachability can't resolve.

### Known limitations (fix-on-demand policy)

1. **Dead code in the game binary** (e.g. THIEF: `WSAVS; WRTN;` then an
   unreachable body — developer stubbed a broken routine with RETURN;
   also dead loop-ends after always-taken cases). Reachability handles
   this correctly: dead bodies classify as `mem` and vanish from blocks.
   Translate what runs (THIEF ⇒ `return;`). Remember dead code lurks in
   `quest.mem` dumps looking like data.
2. **Hidden live code in the runtime**: some `mem` ranges in
   `quest-rt.addrs` are actually executed via paths StartStop can't see.
   As runtime work reveals real paths, hand-patch the addrs or teach
   StartStop the pattern.
3. **Possible future enrichment**: de-stackification may want the block
   tools to emit per-instruction stack-slot read/write facts. Design that
   when Step 2 starts, not before.
4. **Dynamic cross-check idea**: log every executed pc in the RT range
   during play; diff against static `code` ranges to enumerate hidden
   entry points empirically.

## Snapshot Practice

Milestone snapshots live on the user's side in a HISTORY/ tree
(milestone-1/, milestone-2/, ... plus a Tools archive); docs are
tracked latest-only in the live tree. At each milestone completion,
a snapshot of c_src is cut for HISTORY; between milestones, the
end-of-session Work tarball is the safety copy. Claude's filesystem
resets between sessions — upload the latest Work state at session
start.

## Working Agreements

- No code/doc changes without an agreed plan and explicit go-ahead.
- Every translation validated by gameplay under the lockstep checker
  before the next one starts.
- Prior-generation translations (game *and* runtime) are reference
  material only.
