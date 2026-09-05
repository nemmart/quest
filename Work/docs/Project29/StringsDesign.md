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
    terminator, never in memory. Every concatenation piece and every
    intermediate of a WMSP group (`temp1 = A‖B`, `temp2 = temp1‖C`) is an
    sN.
  - `pN` — one STATIC ARENA VARIABLE per block that contains a WMSP claim
    group (19 in Quest; the census's "claims on path" table). It holds
    the group's RESULT — the one value that leaves the block through the
    consuming rt_call — as a varying image at a FIXED arena address with a
    FIXED capacity computed at lowering time from the operands' declared
    lengths. Re-executing the block reassigns it; nothing is ever freed
    (the arena is bounded by the program: ≤ 19 live strings). In C++ a
    `static std::string` per site until addresses stop mattering.

Primitives (statement level unless noted):

```
sN = "literal"                 ; from quest.strings (address + length recorded)
sN = [@a, n] | [@a, n varying] ; read a located string (expression)
sN = sM + <piece>              ; concat; a piece is a literal, a located read,
                               ;   another sK, char(x) (one byte: the WSTB idiom),
                               ;   or substr(<string>, i, n)
append([@a, n], <piece>)       ; a piece WCMV'd into a located scratch buffer
                               ;   (frame scratch chains — §5); residues per §4
[@a, n] = <string>             ; PL/I assignment: truncate or blank-pad to n
[@a, n varying] = <string>     ; ... and write the length word = min(len, n)
<string> == <string>           ; expression; WCMP semantics: blank-padded
                               ;   equality (the ONLY relation Quest uses — F2)
words(@dst, n) = words(@src, n); WBLM; the 6 self-overlapping forms render as
fill(@dst, n, 0)               ;   fill (F6) — same helper, sequential order
pN = <string>                  ; the group result, materialised at pN's fixed arena address
rt_call ?X(pN, …)              ; pushes pN's arena address (§5)
rt_call ?X([@a, n varying], …) ; a located string: its address is pushed, as today
```

`?UNSIGNED_TO_CHAR` is a constructor (`sN = char(v)`; native today,
writes to the string sink). `?READ`/`?READ_SCREEN` fill a located
string (`IN_BUFFER`); unchanged rt_calls.

## 2. Scope — RULED (Sep 5)

`sN` is block-local, single-assignment, like the t-places; the loader
refuses a read after the block's terminator. Every concatenation closes
inside one PL/I statement, and the Sep 5 check against blocks.split
shows that in all 19 WMSP groups the claims and pieces are STRAIGHT-LINE
(zero block boundaries between first claim and last piece). Our CFG cuts
a group only at its tail: the `min()` skip of the copy-out and the
consuming `?WRITE_SCREEN` rt_call (2–5 listed entries; HELP's group is
one block). Across those cuts only the STACK CLAIM is live, and it is
real memory at the same address on both sides (§5), so nothing differs
at any rendezvous. The 528 `s = 'lit'` sites need no temporary at all:
`[@a, n varying] = "lit"` is one statement. The emitter refuses any
group it cannot close within the census's claim…release bracket
(totality: the sites stay embedded `@WCMV`, which works today).

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
splits use ac1), so they are not optional. ovr is untouched (B-1 of
EmulatorDivergences.md). No deadness argument is needed anywhere.

## 5. Where dynamic-length results live — RULED (Sep 5, revised)

- **Frame scratch chains** (≈500 pieces): the master WCMVs pieces into a
  fixed CHAR(n) frame local. That buffer IS a located string, so the
  chain is `append([@fp+k, n], piece)` per piece — same bytes, same
  address, same time (§3) — with the final copy-out where the compiler
  emits one. Conditional pieces (the `IF … THEN MSG = MSG ‖ …` shape)
  live here, as separate statements on separate blocks.
- **WMSP temporaries** (57 claims, 19 groups): in the IR text the group
  is `s0 = A + B; s1 = s0 + C; p3 = s1; rt_call ?WRITE_SCREEN(p3, …)` — no
  claim, no release, no metadata (RULED, Sep 5). `p3` is the block's
  static arena variable (§1): fixed address in the otherwise unused
  emulated segment 0x75000000, fixed capacity, reassigned each time the
  block runs, never freed. The runtime reads the same bytes through the
  arena address. wsp and the AC residues differ from the master's (its
  temps are on its stack) and are compared through the translation of
  §6. Lifetime needs no management: the value is overwritten on the next
  execution of its block and otherwise simply persists — which is also
  what makes condition-system unwinds (the master's wsp reset past its
  claims) a non-event on the clone.
- The master is not changed. Stock mode is the only mode for strings.
  The same arena + translation later serves the frames project when the
  stack itself becomes virtual.

## 6. Verification — the translated compare — RULED (shape); OPEN (details)


A **triple table** `(clone_addr, master_addr, length)`, one row per pN:
- the clone half is STATIC (the arena layout artifact: pN → address,
  capacity, from the census bounds); the master half is dynamic;
- clone→master by binary search (hot path: every AC compare);
  master→clone by scan (memory oracle, diagnostics only);
- `master_addr == 0` = not currently bound.
- **Binding**: ONE hook per group at its FIRST WMSP pc (19 hooks) records
  `base = wsp_before`; the group is live iff `base < master_wsp`, its
  total claim is `master_wsp − base` while live, and the pushed temp's
  master address is the last claim's `wsp_before+2` (recorded by the same
  hook when the group's last WMSP fires, or computed from base and the
  census claim sizes — plan-gate choice). A group whose base is no longer
  below wsp is unbound (STASP, WRTN, or an unwind — all the same to the
  rule). No release hooks exist.
- **compare_pair** (after the sweep): an AC value inside the arena segment
  translates through the table before comparing; anything else compares
  raw. wsp: `master_wsp − clone_wsp` must equal EXACTLY `Σ over live groups of
  (master_wsp − base)` — normally one term or none; any other divergence
  breaks the equality. A value hitting an
  unbound triple, or a master hook with no unbound clone variable, is a
  MISMATCH (catches emitter ordering bugs).
- **Memory oracle**: at rendezvous, each live pN's arena bytes are compared
  with its master range (footprint-capture shape).

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
  change; ships dark): `hw/strings/`: Arena (a STATIC layout over the 0x75
  segment inside Memory: pN → fixed address + capacity; no allocator),
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
- **P33 — WMSP temporaries**: the 19 groups as `pN = …; rt_call ?X(pN)`;
  the static arena layout, the 19 first-claim hooks, the
  Lockstep translation layer (§6) and memory oracle. 57 claims. The
  checker work lands LAST, for the smallest population, with everything
  else green. Battery + a leg that forces an unbound-triple mismatch to
  prove the checks fire.
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

- Arena layout: capacity per pN from the census's declared-length bounds;
  overflow of a capacity = loud fault.
- Where the memory oracle runs (§6 OPEN).
- The 28 unresolved destinations (all frame/argument; 3 TERRITORY_MAP
  by-reference array writes need the callers read).
- `substr` with computed byte offsets: grammar form for `bp + expr`.
- How `char(v)` (native ?UNSIGNED_TO_CHAR) reports its length to the IR
  (ac0 today; keep — it is the residue rule again).
