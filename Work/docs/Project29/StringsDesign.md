# Strings design — rulings of record (user + integrator, Sep 5 2026)

Input: docs/Project29/Census.md (the ground: 1,637 WCMV, 40 WCMP, 12
WBLM, 57 WMSP, 19 STASP, 99 WSTB, 3 WPSH+LDASP; idiom catalogue;
destination classes; manual review) and the Sep 5 planning discussion.
Marked **RULED** = user ruling; **LEAN** = integrator recommendation
awaiting the ruling; **OPEN** = collect at the P30 plan gate.

## 0. Goal

Retire the string family from the IR — not by transcribing WCMV with
its four-AC calling convention, but by giving the IR the string type
the PL/I source had, so the C++ translation is direct: located strings
are spans over the memory image, unlimited temps are heap strings, the
runtime calls take a string. Every project in the family keeps the
standing invariant: the clone reproduces the master's STRICT SURFACE
(ac0–ac3, c, ovr, wsp, block ordinal) at every listed block entry. The
representation changes; the verification does not relax.

## 1. The string type — RULED

Two kinds of string value:

- **located**: `[@addr, n]` (fixed CHAR(n): n bytes at addr) and
  `[@addr, n varying]` (PL/I VARYING: length word at addr, data at
  addr+1 word, capacity n). Frame locals, statics, record fields,
  `IN_BUFFER`, the ?WRITE_SCREEN buffers.
- **unlimited**: a variable holding a byte string of any length, in two
  scopes (RULED, Sep 5):
  - `sN` — BLOCK-LOCAL temporaries, like the t-places: single-assignment,
    defined and consumed within one block, dead at the block's
    terminator. Never live at a listed block entry, so they need no
    arena and no mapping — they are pure notation for an expression that
    is stored or passed before the block ends.
  - `pN` — PERSISTENT (routine-scoped) strings with explicit lifetime:
    `pN = create(<size expr>)` at the WMSP claim pc (the arena allocation;
    the master-side binding hook fires at the same pc). The clone's value
    lives until the routine RETURNS: the executor frees a frame's pN at
    WRTN (keyed by wfp), so the C++ translation is a local std::string
    with RAII — there is NO destroy statement in the IR (RULED, Sep 5).
    A `pN = create(…)` on a pN that is already live FREES the previous
    value first (assignment semantics: `MSG = a‖b‖c; PUT MSG; MSG = d‖e;`
    gives MSG several lives, each its own claim group on the master; a
    create inside a loop must not grow the arena). Each life binds its
    own triple at its WMSP pc.
    The STASP that releases the master's claim group is checker
    bookkeeping only (§6): it unbinds the master half of the triple. These
    are the WMSP temporaries and ONLY those (57 creates) — see §5 for why
    the frame scratch chains do not need them.

Primitives (statement level unless noted):

```
sN = "literal"                 ; from quest.strings (address + length recorded)
sN = [@a, n] | [@a, n varying] ; read a located string (expression)
sN = sM + <piece>              ; concat; a piece is a literal, a located read,
                               ;   another sK/pK, char(x) (one byte: the WSTB idiom),
                               ;   or substr(<string>, i, n)
pN = create(<wides>)           ; WMSP claim: arena allocation, master binding
pN = pN + <piece>              ; append to a persistent string
                               ; (no destroy: pN dies at WRTN; STASP is checker-side)
append([@a, n], <piece>)       ; a piece WCMV'd into a located scratch buffer
                               ;   (frame scratch chains — §5); residues per §4
[@a, n] = <string>             ; PL/I assignment: truncate or blank-pad to n
[@a, n varying] = <string>     ; ... and write the length word = min(len, n)
<string> == <string>           ; expression; WCMP semantics: blank-padded
                               ;   equality (the ONLY relation Quest uses — F2)
words(@dst, n) = words(@src, n); WBLM; the 6 self-overlapping forms render as
fill(@dst, n, 0)               ;   fill (F6) — same helper, sequential order
rt_call ?X(sN, …)              ; an unlimited string passed to the runtime:
                               ;   the arena address of its varying image is pushed
```

`?UNSIGNED_TO_CHAR` is a constructor (`sN = char(v)`; native today,
writes to the string sink). `?READ`/`?READ_SCREEN` fill a located
string (`IN_BUFFER`); unchanged rt_calls.

## 2. Scope — RULED (Sep 5, after the census)

`sN` is block-scoped; `pN` is routine-scoped. The census forces the
second kind: 819 of 1,765 windows cross a block boundary; every WMSP
group and most scratch chains span several blocks with conditional
pieces. A `pN` is defined at its first piece or claim and dies at its
region end (the STASP, or the consumer that takes the value: an
assignment, a compare, an rt_call). Regions are single-entry (census:
every group is closed within its routine, no interleaving). The emitter
uses `sN` whenever the whole idiom closes inside one block (the 528
`s = 'lit'` sites need neither: `[@a, n varying] = "lit"` is one
statement), `pN` otherwise, and refuses any region it cannot close
(totality: the sites stay embedded `@WCMV`, which works today). The
loader checks: no `sN` read after its block's terminator; every `pN`
has one definition region and one release.

## 3. Timing — RULED

The clone performs each piece WHEN the master's WCMV runs (same block,
same order), not deferred to the region end: the strict surface must
agree at every listed block entry, so the residues (§4) and, for frame
scratch chains, the written bytes, exist at the same rendezvous on both
sides. The abstraction is in the representation, never in the timing.

## 4. Residues — RULED (strict surface unchanged)

Every string statement that replaces a WCMV/WCMP/WBLM sets ac0–ac3 and
c exactly as the instruction would (EagleSpecial.cpp semantics, manual-
verified in Census §10): after a copy ac0 = 0, ac1 = source bytes left
(c = ac1 ≠ 0), ac2 = destination end pointer, ac3 = source end pointer.
The compiler READS these (349 continuation pieces use ac2; 17 tail
splits use ac1), so they are not optional. Pointers into the arena are
compared through the translation (§6). ovr is untouched (B-1 of
EmulatorDivergences.md). No deadness argument is needed anywhere.

## 5. Where unlimited strings live — RULED

- **Frame scratch chains** (≈500 pieces): the master WCMVs pieces into a
  fixed CHAR(n) frame local. That buffer IS a located string, so the
  chain is `append([@fp+k, n], piece)` per piece — same bytes, same
  address, same time (§3) — with the final `[@v, m varying] =
  [@fp+k, len]` copy-out where the compiler emits one. No `pN`, no arena,
  no mapping: memory agrees byte for byte, and the region bookkeeping is
  just the ac2 continuation (§4). The appended length is tracked by the
  emitter from the residues, never guessed.
- **WMSP temporaries** (57 claims, 19 groups; all `pN`): the clone keeps them in
  the **arena** — a heap in an otherwise unused emulated segment
  (0x75000000), first-fit allocation, deterministic across runs, freed
  at the group's release. In emulated memory so the runtime reads a temp
  through a plain pointer; no materialisation at the call.
- The master is NOT changed. Its temps stay on its stack; the deviation
  lives entirely in the checker's compare (§6). Stock mode is the only
  mode for strings — there is one master behaviour.

## 6. Verification — the translated compare — RULED (shape); OPEN (details)

A **triple table** `(clone_addr, master_addr, length)` sorted by
clone_addr:
- clone→master by binary search (hot path: every AC compare);
  master→clone by scan (memory oracle, diagnostics only);
- `master_addr == 0` = allocated on the clone, not yet bound on the
  master; invariants asserted on insert: clone ranges disjoint and inside
  the arena, live master ranges disjoint.
- Binding is dynamic: the master's existing per-pc hooks fire at the
  mapped WMSP pcs and record `[wsp_before+2, wsp_after]`; the STASP hook
  UNBINDS the master half of every triple in the group (the master reuses
  that stack region afterwards) — the clone's arena value stays live until
  WRTN. A translation hitting an unbound-on-master triple after its STASP
  is a MISMATCH (the compiler never reads a released temp; a hit is an
  emitter bug). The clone reports its arena allocation per variable and
  the frame free at WRTN.
  Static input: a map artifact in the pushmap style (57 claim pcs, 19
  release pcs, variable ids) with provenance headers.
- **compare_pair**: an AC value inside the arena segment translates
  through the table before comparing; anything else compares raw. wsp
  compares as `clone_wsp + Σ live master claim sizes`. A value hitting an
  unbound triple, or a master hook with no unbound clone variable, is a
  MISMATCH (catches emitter ordering bugs).
- **Memory oracle**: at rendezvous, each live variable's arena bytes are
  compared with its master range (footprint-capture shape).

OPEN: whether the memory oracle runs at every rendezvous or K-gated;
whether the frame slots where the master stores temp base pointers
(not compared today) get a translated compare too (LEAN: no — dead
after release; the arena oracle covers the bytes).

## 7. Idioms the emitter recognises (from Census §3) — RULED as the target set

`s = 'lit'` (528), `s = t` exact/pad/truncate (136+21+18+7+7),
COPY-*-EXACT first pieces (378), CONCAT-PIECE (349), TEMP-FIRST-PIECE
(49), CALLRESULT pieces (112), SUBSTR forms (23), tail splits (17),
WSTB single-char pieces (86 const + 13), WCMP equality (40), WBLM copy
(6) and fill (6), the 3 LOCK_FILE by-reference constants (left embedded
or lowered as located constants — OPEN). Everything else: refused,
listed, embedded.

## 8. Projects — RULED (split), sequence LEAN

- **P30 — the C++ string library** (no IR, no emulator behaviour
  change; ships dark): `hw/strings/`: Arena (first-fit over the
  0x75 segment inside Memory; deterministic; free by variable),
  EagleString (value type; construct from literal/located/char; concat;
  assign-to-located with pad/truncate and the varying length word;
  blank-padded equality; residue computation for each replaced
  instruction), TripleTable (§6), and unit tests in the
  tests/helpers_selftest.cpp style: residues checked against
  EagleSpecial's WCMV/WCMP/WBLM on random operands; assignment/compare
  against a reference; table invariants. One K=1 stock gate to prove the
  library's presence changes nothing.
- **P31 — located slice**: ir 5 grammar for §1; IRExec on the library;
  `s = 'lit'` and `s = t` into fixed/varying targets (≈720 sites);
  residues per §4. NO arena, NO checker change. Battery.
- **P32 — frame scratch chains**: `append` to located scratch buffers, per-piece, conditional pieces, CALLRESULT/SUBSTR/WSTB pieces, tail
  splits (≈900 sites). Still no arena, no checker change. Battery.
- **P33 — WMSP temporaries**: `pN` with create (freed at WRTN), the arena live, the map artifact, the
  Lockstep translation layer and memory oracle (§6), `rt_call` with an
  arena argument. 57 claims. The checker work lands LAST, for the
  smallest population, with everything else green. Battery + a leg that
  forces an unbound-triple mismatch to prove the checks fire.
- **P34 — the rest**: WCMP, WBLM/fill, `?UNSIGNED_TO_CHAR` as a
  constructor, LOCK_FILE constants; then the string family is gone
  (embeds ≈460: frames, syscalls, float, divides).

Each project: census-first plan gate, K=1 book/stock gates per slice,
full battery, REPORT. P30 and P31's Phase A may run in parallel with
each other (no shared files); P31 Phase B waits for P30.

## 9. Findings carried in (do not re-derive)

Census §10: WCMV/WCMP/WBLM/WMSP/STASP agree with the emulator on every
semantic Quest uses; the residue-class differences (B-1 OVR, B-4 WCMP
post-mismatch pointers, G-1 WMSP limit throws) are in
EmulatorDivergences.md. F1: WCMV's carry is never read — set it anyway
(§4). F5: ac2 continuation is load-bearing (§4). F6: WBLM sequential
order is load-bearing (fill). F7: SYSCALL-preserves-ac3 is an
assumption to verify in P31's census.

## 10. Open items for the P30/P31 plan gates

- Arena size and allocator policy (first-fit ruled; block size,
  fragmentation guard, exhaustion = loud fault).
- Where the memory oracle runs (§6 OPEN).
- The 28 unresolved destinations (all frame/argument; 3 TERRITORY_MAP
  by-reference array writes need the callers read).
- `substr` with computed byte offsets: grammar form for `bp + expr`.
- How `char(v)` (native ?UNSIGNED_TO_CHAR) reports its length to the IR
  (ac0 today; keep — it is the residue rule again).
