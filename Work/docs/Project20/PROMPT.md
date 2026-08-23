# Project 20 — FINAL M4b tranche: redirect the WPSH/WPOP frame-borrows off-stack

**Goal.** Redirect the 23 WPSH/WPOP frame-borrow brackets off the stack
into a reserved slot block at the area base, using the M4b caller_map
machinery. This is the LAST bit of game-code stack residue. When it lands
GREEN, **M4 de-stackification is DONE**: frames (M4a) + args (M4b arg
tranches) + frame-borrows (this) are all off the stack; the program is
flat and live-analyzable, everything still executing the original
instructions so the checker stays green. (WMSP/STASP string buffers stay
put — purely local, lockstep-transparent, they dissolve at translation;
see M4cNotes.md. ON-handler control flow is an M5 matter.)

**This is mostly REUSE. The only genuinely new code is one hook (the
decorated WPOP load) plus small tool/loader additions. Do NOT invent new
mechanism. If a design-vs-reality gap appears, STOP AND REPORT** (the P16
mid-window / P17 timing / P18 XPEFB-gap precedent).

---

## What a borrow bracket is (verified)

23 brackets, uniform shape, span exactly 3 instructions, no call inside:

    WPSH r,r            ; save AC[r] to the stack (r = AC3 in 22, AC2 in 1)
    LDAFP 3            ; load frame pointer into AC3 (clobbers it)
    X?STA ..,[ac3+off] ; one frame-relative store
    WPOP r,r           ; restore AC[r] from the stack

It borrows a register for exactly one frame-relative store. Purely local.
`WPOP` is used NOWHERE ELSE in the program — all 23 WPOPs are bracket
closes (verified: every WPOP pairs backward to a WPSH r,r, span 3, LDAFP
present). The register is 22×AC3 + 1×AC2 (pair @ WPSH 7016E7D5).

A cross-check pairing (coordinator scan, NOT authoritative — see step 1)
is in `Work/docs/Project20/borrowmap.crosscheck.txt`: 23 pairs, block
92 bytes (0x5C), frames would shift base→0x7400005C. The AUTHORITATIVE
pairing must come from the tool's basic-block proof (step 1), and the two
should agree (like the two push_map generators cross-validated in P18).

---

## The scheme

Reserve N 32-bit slots at the area base (0x74000000) BEFORE the code
frames. Number the pairs 0..N-1. Slot n is at `base + 4*n` (0-indexed,
flat — a DIFFERENT indexing/equation than args, hence a different term:
see below). Decorate the WPSH to STORE AC[r] to its slot instead of
pushing (`stack_offset += 2`); decorate the WPOP to LOAD AC[r] from its
slot instead of popping (`stack_offset -= 2`). Net zero across the
bracket; opcode drives store-vs-load.

### Terminology / indexing (deliberate)
- Real routines keep `NAME argN at PC` — **1-indexed**, slot at
  `wfp_base - 10 - 2N` (unchanged; do not perturb the 566-site pipeline).
- Borrows use `_PAIRS slotN at PC` — **0-indexed**, address `base + 4*N`,
  flat reserved block. **Different word ⇒ different equation**, so a
  reader/loader never conflates the two formulas.
- First argmap line records the count for up-front allocation:
  `_PAIRS count 23` (three tokens, trivial parse).

Example argmap addition:

    _PAIRS count 23
    _PAIRS slot0 at 7015F7BA      # WPSH — opcode stores
    _PAIRS slot0 at 7015F7BE      # WPOP — opcode loads
    _PAIRS slot1 at 7015F7C5
    _PAIRS slot1 at 7015F7C9
    ...
    <real routines unchanged: NAME argN at PC>

The map is opcode-agnostic: `NAME slotN at PC` just says "pc ↔ this slot";
the OPCODE at that pc (WPSH vs WPOP) decides store vs load. Same as the
existing map where XPEF/LPEF store and LCALL writes-a-marker at their
mapped slots.

---

## Steps

### Step 1 — BB-PROVE + count + number the pairs (in ArgWindows.java)
Adjacency does NOT prove a single basic block. There are **130 unresolved
indirect jumps**; one could target a bracket interior, desyncing the store
from the load. Each bracket must be PROVEN single-block (no branch target
in the interior, no flow instruction inside).

**The tool that builds quest.argmap is `Tools/ArgWindows.java`** (in the
`Tools/` tree — NOT the `Work/` tarball; the implementing session needs
Tools/ checked out). It ALREADY does exactly this proof for arg windows:
it takes a **targets file** (all branch/jump destinations, incl. the
indirect ones — cross-checked against its own `Follow.targets`) and, for
each window, asserts `targets.subSet(windowStart+1, call.pc)` is empty
(no target lands inside), plus disqualifies any FLOW/STACK instruction in
the span. `Tools/DisassembleBlocks.java` produces quest.blocks (the CFG)
separately; the argmap proof uses the targets set, not quest.blocks.

Add a borrow pass to ArgWindows using the SAME `targets` set:
- Detect brackets by WPOP→back-scan (WPOP is only ever a bracket close;
  all 23 verified). Each is `WPSH r,r / LDAFP / store / WPOP r,r`.
- PROVE single-block: `targets.subSet(wpsh+1, wpop+1)` must be EMPTY (no
  branch target in the interior or on the WPOP), and no FLOW instruction
  inside (there's only LDAFP + a store — neither is flow). This is the
  arg-window proof minus the debt/attribution accounting.
- Count provable pairs (N=23), number 0..N-1, emit into quest.argmap: the
  `_PAIRS count N` header + two `_PAIRS slotN at <pc>` lines per pair
  (WPSH pc, WPOP pc, same slotN). ArgWindows already emits `NAME argN at
  PC` lines — this is a sibling emission.
- Any bracket that fails the proof: FLAG, do NOT decorate, report. (None
  expected — coordinator's independent quest.blocks check found 23/23
  single-block; the ArgWindows targets-set proof should agree.)
- Cross-check emitted pairs vs borrowmap.crosscheck.txt (23, AC3x22 +
  AC2x1).

### Step 2 — book: reserve the block, shift frames (build_address_book.py)
Read `_PAIRS count N` → reserve N slots (N*4 bytes) at base 0x74000000,
BEFORE laying out code frames → frames start at `base + 4*N` (shift is
DERIVED from N, nothing hardcoded). QUEST's base record and every frame
move up by 4*N. Regenerate quest.addrbook. (Cross-check: N=23 ⇒ frames at
0x7400005C.)

### Step 3 — generator: resolve slotN → ABSOLUTE address, emit borrow lines
In the pushmap generator (extend gen_pushmap*/add a sibling): resolve
`_PAIRS slotN` → `base + 4*N` to an ABSOLUTE 32-bit address, against the
FINAL (post-shift) book, and emit `push`-style borrow lines carrying the
absolute slot. **Resolution happens ONCE at generation — the map stores
absolute addresses, never an index or an offset formula. NO runtime
arithmetic.** (Same discipline as argN → wfp-10-2N today: the generator
freezes absolutes; the loader/hooks only look up.)

### Step 4 — loader: accept the borrow entries, validate the region
In AddressBook.cpp: accept the borrow lines; validate each slot address
against the reserved region `[base, base + 4*N)` — the new validation arm,
mirroring the existing arg-region (push) and marker-slot (call) checks.
Store flat `caller_map[pc] = absolute_addr`. The reserved-block allocation
(step 2's N) is read before frame placement.

### Step 5 — WPSH store: REUSE (no code)
The existing P18 WPSH hook (EagleStack.cpp `case WPSH`, the
`caller_write(address)` branch) already handles `WPSH r,r` (wides=1):
stores AC[XX] to the mapped slot + `note_arg_write(machine,1)` (=
stack_offset += 2). It just needs the WPSH pc in the map with wides=1.
Verify the wides=1 single-register path is correct here; no new code
expected.

### Step 6 — WPOP load: THE ONE NEW HOOK
Add a decorated path to `case WPOP` in EagleStack.cpp: if
`caller_write(pc)` hits, LOAD AC[r] from that slot (r from the WPOP
instruction's own operand — NOT the map), decrement stack_offset by 2
(mirror of note_arg_write; add a note_arg_pop or inline it), and do NOT
move wsp / do NOT pop. Otherwise fall through to the stock WPOP. This is
the first real decorated POP (M4b only decorated pushes + consumed at
WSAVS).

### Step 7 — battery
- div=0 on all legs (m/fo/inj/abort/play + any targeted borrow leg).
- **AC[r] round-trip**: the value stored at each fired WPSH == the value
  loaded at its WPOP (a mismatch is silent like the P18 WPSH-ordering
  case — verify VALUES, not just div=0).
- Confirm stack_offset returns to its pre-bracket value across each
  bracket (net-zero), including any mid-bracket checkpoint (offset=2
  between store and load, exactly a 1-word window).
- Book-shift sanity: no hardcoded 0x7400xxxx literal elsewhere assumed a
  pre-shift address (push_maps are regenerated; scan for stragglers).

---

## Landing criterion
All provable borrow brackets decorated, div=0, AC round-trip verified,
book shift clean. Then **M4 de-stackification is COMPLETE** — write the
REPORT (mechanism reused, what was new = WPOP hook + tool/loader/book
additions, coverage, any unprovable brackets left on-stack), update
CURRENT_STATE, and note M4 done in M4Roadmap.md.

## Notes / deferrals (not for this project)
- We considered having the loader read quest.argmap DIRECTLY (resolving
  argN/slotN from the book at load) instead of the argmap→generator→
  pushmap hop. Deliberately DEFERRED — the current chain is green and
  load-bearing; not worth re-validating the 566-site path to save a hop.
- `_PAIRS` slots being flat base+4N (not a fake routine's wfp-10-2N arg
  region) keeps them a structurally-distinct region below all frames —
  never confusable with an arg/marker address.
