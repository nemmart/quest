# StringsDesign.md — the string family: design of record

*Ruled Sep 5–6 2026 (user + integrator) over docs/Project29/Census.md
(the Phase A research, Sep 5). Status: DESIGN OF RECORD for Projects
30–33. Every statement marked RULED is a user ruling; LEAN is the
integrator's recommendation awaiting confirmation at a plan gate; OPEN
is collected at the plan gate named. Implementation sessions transcribe
this document; they do not reinterpret it. Where the ground disagrees
with it, STOP AND REPORT (METHOD §5).*

---

## 0. Why this document, and what it decides

After P28 the IR embeds ≈2,300 Eagle instructions, of which the string /
dynamic-storage family is ≈1,865:

| op | sites | what it is |
|---|---:|---|
| WCMV | 1,637 | PL/I character assignment / concatenation piece |
| WSTB | 99 | one-byte append (86 constant, 13 computed) |
| WMSP | 57 | stack claim for a dynamic-length temporary |
| WCMP | 40 | string compare (only equality is ever consumed) |
| STASP | 19 | release of a claim group |
| WBLM | 12 | word-block move (6 copies, 6 self-overlapping fills) |
| WPSH+LDASP | 3 | by-reference constants (LOCK_FILE) |

These are the DG PL/I compiler's implementation of `CHAR(n)`,
`CHAR(n) VARYING`, `‖`, `SUBSTR`, `=` on strings, and its own expression
temporaries. This document gives the IR a **string type** that matches
what the source had, so that (a) the IR reads as the program and (b) the
translation to C++ is direct — located strings are spans over the memory
image, temporaries are `std::string`, `?WRITE_SCREEN` takes a string.

The design keeps the invariant every project since P22 has kept: **the
clone reproduces the master's strict surface (ac0–ac3, c, ovr, wsp,
block ordinal) at every listed block entry.** The representation
changes; the verification does not relax. Where the clone's memory
layout legitimately differs from the master's (§6), the difference is
confined to one address segment and handled by the existing Mapper's
rules, not by masking.

Retiring the family takes the IR to ≈460 embeds: frames, syscalls, the
float family, two divides.

---

## 1. The ground the design rests on (facts from the census, checked)

Everything below was measured on the object code, not assumed. A future
program that fails one of these checks needs a different design; the
census tool (`tools/string_sites.py`) is what says so.

1. **Layout.** A PL/I VARYING string is one 16-bit length word followed
   by its data: a local at frame word `w` has its length at `[fp+w]`
   (`XNSTA 0,[ac3+w]`) and its data at byte `2w+2` (`XLEFB 2,[ac3+2w+2]`).
   Statics likewise: `IN_BUFFER` = length at 0x7000021C, data at
   0x7000021D:0. A `CHAR(n)` fixed string is n bytes with no length
   word. Byte pointers are `word_address·2 + byte_index` (ByteEA.md §2).
2. **Two concatenation machineries**, chosen by the compiler on whether
   the total length is a compile-time constant:
   - **Frame scratch buffer** (≈500 pieces): pieces WCMV'd back-to-back
     into a fixed `CHAR(n)` local at `[fp+k]` — ac2 (the destination end
     pointer left by the previous WCMV) is NOT reloaded between pieces
     (349 CONCAT-PIECE sites) — then one copy-out into the varying local
     with its length word, then `XPEF [fp+w] / LPEF OUT_CHAN / LCALL
     ?WRITE_SCREEN`. HELP 7016D5E3..7016D602: 164 + 150 → 314.
   - **WMSP temporaries** (57 claims in 19 groups, 12 routines): when a
     piece length comes from a length word, the compiler claims
     `⌈(len+k)/4⌉` wides (`WADI 3,ac / WMOVR / WMOVR / WMSP`), saves the
     temp base (`LDASP; WADI 2,ac` → first free wide, kept in a frame
     slot, later re-read and `WLSI 1` to a byte pointer), and
     concatenates into it. Each result feeding the next expression gets
     a NEW claim (`temp1 = a‖b`, `temp2 = temp1‖c`, `temp3` = the varying
     result with room for its length word — the `+5/+6` size), and ONE
     `XWLDA 1,[slot]; WSBI 2,1; STASP 1` releases the whole group.
3. **Claim groups are straight-line.** In all 19 groups there are ZERO
   block boundaries between the first WMSP and the last piece (checked
   Sep 5 against blocks.split). Our CFG cuts a group only at its tail:
   the `min()` skip of the copy-out (`WSGE 1,0 / WMOV 1,0`) and the
   consuming `?WRITE_SCREEN` rt_call — 2–5 listed entries; HELP's group
   is entirely one block.
4. **No group is split by a call.** The only call inside any group is
   the consuming `?WRITE_SCREEN` (11 of 19; the other 8 copy out to a
   located target — HELP's write is after its STASP; corrected by P31's
   re-check on the regenerated census). Every `CHAR(n)` conversion (`?UNSIGNED_TO_CHAR`) is
   executed BEFORE the first claim — the compiler needs the length to
   size the claim — writing its digits to a frame scratch word buffer at
   the address passed in ac2 and returning the length in ac0 (112
   CALLRESULT pieces overall).
5. **Every claim is released** in its own routine by an STASP (57/57);
   groups never nest or interleave; no temp pointer is read after the
   STASP. Two or three groups may live in one routine (DIED 3+3,
   DISPLAY_SCREEN 3+3+3, OP_EDIT 5+5+5) — sequentially, at (typically)
   the same stack base.
6. **PL/I semantics that make 3–5 true by construction**: the language
   has no conditional expression, so a temporary never lives across the
   program's own control flow (`IF … THEN MSG = MSG ‖ 'a'; ELSE …` is two
   statements, each with its own claim group or its own located
   assignment); and every named string has a declared capacity
   (`CHAR(n) [VARYING]`; there is no unbounded named string, only
   `CHAR(*)` parameters and BASED/CONTROLLED storage, unused here), so
   anything that survives control flow is located, with a capacity we
   can read from the code (the `min()` bounds) or from the allocation
   spacing.
7. **Where strings go** (Census §7): 1,334 sites end at
   `?WRITE_SCREEN`/`?WRITE` (ephemeral: a wrong byte is a wrong message,
   visible); 88 write game state (records, names, redraw caches — the
   fidelity-critical list, in full in Census §7b); 135 feed a compare or
   another piece only; 46 pass a frame varying string by reference to a
   call (DIED, RETURN_MESSAGE, `?OPEN_FILE` names); 18 write through a
   by-reference argument; 28 unresolved (all frame/argument, none game
   state).
8. **Runtime routines that take strings** (P28 RTConventions + Census):
   `?WRITE_SCREEN` (723; arg 2 = a varying string, pointer pushed),
   `?WRITE` (2), `?OPEN_FILE`/`?OPEN_SHARED_IO_FILE`/`?GET_SHARED_PAGE`
   (names), `?CHAR_TO_UNSIGNED` (parses `IN_BUFFER` words),
   `?LOOKUP_PORT`/`?CONNECT`/`?CREATE_TASK`. Producers: `?UNSIGNED_TO_CHAR`
   (writes raw digits at ac2, length in ac0) and `?READ`/`?READ_SCREEN`
   (fill `IN_BUFFER`, a located varying string).
9. **Manual vs emulator** (Census §10, EmulatorDivergences.md): WCMV,
   WCMP, WBLM, WMSP, STASP agree with the emulator on every semantic Quest
   depends on. Residue-class differences exist and are catalogued (B-1
   OVR model, B-4 WCMP post-mismatch pointers, G-1 WMSP limit guards,
   G-2/G-3 indirect-bit and segment-crossing throws) — none observable.

---

## 2. The string type — RULED

### 2.1 Two kinds of string

**Located strings** — memory with a declared capacity:

```
[@a, n]            fixed CHAR(n):        n bytes at byte address a
[@a, n varying]    CHAR(n) VARYING:      length word at word address a,
                                          data at a+1 word, capacity n bytes
```

`a` is any address expression the IR already has (`wp(ac3, d)`,
`bp(...)`, a static constant, `R[...]`). Frame locals, statics, record
fields, `IN_BUFFER`, the frame scratch buffers, and the digits
`?UNSIGNED_TO_CHAR` wrote are all located.

**`p@<block>`** — exactly ONE kind of string variable (RULED, Sep 5–6):
a static arena variable per block that contains a WMSP claim group (19
in Quest — the census's "claims on path" table), IDENTIFIED BY THE BLOCK
ADDRESS. Every WMSP in a block refers to the same `p@b`. It holds the
group's RESULT — the one value that leaves the block through the
consuming rt_call — as a varying image at a FIXED arena address with a
FIXED capacity computed at lowering time from the operands' declared
lengths. Re-executing the block rebuilds it from empty; nothing is ever
freed; the arena holds at most 19 strings. In C++ this is a
`static std::string` per site until addresses stop mattering.

There are **no block-local string temporaries** (RULED, Sep 6): pieces
are expressions, and the master's intermediate temps (`temp1 = a‖b;
temp2 = temp1‖c`) are implicit in successive appends to `p@b` — their
residues are overwritten before any rendezvous.

### 2.2 Pieces (expressions)

```
<piece> := "literal"                       ; from quest.strings; address + length recorded
         | [@a, n] | [@a, n varying]       ; a located string read
         | [@a, varying]                    ; capacity-less varying READ (P31 O1):
                                           ;   length from the length word
         | [@a:b, "text"]                   ; a literal: located, contents known
                                           ;   (quest.strings; verified lazily vs memory)
         | substr(<piece>, i, n)           ; PL/I SUBSTR; i, n are word expressions
         | char(x)                         ; ONE BYTE from a word expression (the WSTB idiom)
         | p@b                             ; the block's arena variable (as a value)
```

`char(x)` is a single byte, not number-to-text. Number-to-text is
`?UNSIGNED_TO_CHAR`, an ordinary `rt_call` whose result is a located
piece `[@fp+k, ac0]` consumed in the following block (§1.4).

### 2.3 Statements

```
p@b = ""                          ; FIRST statement of a claim-group block (§4)
p@b = p@b + <piece>               ; append; one per WCMV/WSTB piece; residues §3
[@a, n] = <piece> | p@b           ; PL/I assignment into fixed: truncate or blank-pad to n
[@a, n varying] = <piece> | p@b   ; ... into varying: data padded/truncated to the
                                  ;     transferred count, length word = min(len, n)
append([@a, n], <piece>)          ; a piece WCMV'd into a located scratch buffer at its
                                  ;     current end (frame scratch chains, §5.1)
words(@dst, k) = words(@src, k)   ; WBLM, sequential word order
                                  ; NOTE (P31): all 12 WBLMs are self-overlapping fills;
                                  ;     ir 5 renders them as words() with the overlap noted,
                                  ;     fill() is the readable layer's rewrite
release                           ; the STASP: on the clone, assert(wsp == ac1) (§6.3)
rt_call ?X(p@b, …)                ; pushes p@b's arena address (§5.2)
rt_call ?X([@a, n varying], …)    ; pushes the located string's address, as today
```

Expressions:

```
<piece> == <piece>                ; WCMP semantics: blank-padded equality — the only
                                  ;     relation Quest consumes (Census F2); value 0/1
len(<piece>)                      ; the length word of a varying, or n of a fixed
```

### 2.4 Byte-level semantics (from EagleSpecial.cpp; manual-verified)

- **Copy** (`WCMV`; assignment and append): with `dst_count = n`,
  `src_count = len(piece)`: copy `min(n, len)` bytes forward; if the
  source is exhausted first, the remaining destination bytes are
  BLANKS (0x20); if the destination is exhausted first, the source is
  TRUNCATED. Counts are signed: a negative count runs backwards
  (`direction()`: count>0 → pointer +1 per byte; count<0 → −1); Quest
  uses only forward copies at string sites (a negative count appears
  only in the tail-split idiom as the compiler's remaining-room
  arithmetic, and is positive when the WCMV runs).
- **Compare** (`WCMP`): both strings are read until BOTH counts are
  exhausted, the shorter one padded with blanks; result −1/0/+1 by the
  first differing byte (unsigned byte order). Quest tests only `== 0`.
- **Varying assignment**: the compiler stores the length word BEFORE the
  copy (`XNSTA 0,[fp+w]`) with `min(len(src), capacity)` (the `WSLE/WMOV`
  or `WSGE/WMOV` min shape when the capacity can be exceeded; a plain
  constant when the source is a literal). The IR statement does both.
- **Segments**: a copy never crosses a 32-bit segment (the emulator
  throws; no site does).

---

## 3. Residues — RULED (the strict surface is unchanged)

Every statement that replaces a WCMV, WCMP, WBLM or WSTB sets ac0–ac3
and c EXACTLY as the instruction would:

| after | ac0 | ac1 | ac2 | ac3 | c |
|---|---|---|---|---|---|
| copy (assign/append) | 0 | source bytes NOT transferred (`len − min(n,len)`) | destination end pointer (`dst + n`) | source pointer advanced by `min(n, len)` | `ac1 ≠ 0` |
| compare | dst bytes left at the stop point | result −1/0/1 | dst pointer at stop (one past the differing byte — B-4) | src pointer at stop | unchanged |
| words()/fill() | unchanged | 0 | src end | dst end | unchanged |
| char()/WSTB | unchanged | unchanged | (the compiler's `WINC 2,2` follows as its own statement) | unchanged | unchanged |
| release | unchanged | unchanged (holds the restored base − 2) | unchanged | unchanged | unchanged |

The compiler READS these: 349 CONCAT-PIECE sites continue from ac2; 17
tail splits use ac1 as the next piece's count. They are not optional and
they need no deadness argument. The carry after a copy is never read in
Quest (Census F1) but is set anyway. `ovr` is untouched (B-1). Pointers
into the arena compare through the Mapper form (§6.2).

Residues are computed by the library from the statement's operands
(`EagleString::residues_after_copy(dst, n, src, len)` etc.), unit-tested
against `EagleSpecial`'s own WCMV/WCMP/WBLM over random operands (P30).

---

## 4. Timing and scope — RULED

- **Timing.** The clone performs each piece WHEN the master's WCMV runs
  — same block, same order — never deferred to the region end. The
  strict surface must agree at every listed block entry, so the residues
  (§3) and, for frame scratch chains, the written bytes, must exist at
  the same rendezvous on both sides. The abstraction is in the
  representation, never in the timing.
- **`p@b` scope.** The block is the unit. `p@b = ""` is the block's first
  statement; the loader refuses a read of `p@b` before its assignment in
  the block, and refuses `p@b` referenced from any block other than `b`
  (the consuming rt_call is in `b`; the post-call block sees only the
  residues, which are registers).
- **The 528 `v = 'lit'` sites need no variable**: `[@a, n varying] =
  "lit"` is one statement.
- **Totality**: a site the emitter cannot classify into a §7 idiom, or a
  group it cannot close within the census's claim…release bracket, stays
  an embedded `@WCMV` (which works today) and is listed with its window.
  Nothing is rounded into a bucket.

---

## 5. Where dynamic-length results live — RULED

### 5.1 Frame scratch chains (≈500 pieces; P32)

The master WCMVs pieces into a fixed `CHAR(n)` frame local. That buffer
IS a located string, so the chain is `append([@fp+k, n], piece)` per
piece — same bytes, same address, same time — with the compiler's
copy-out into the varying local where it emits one. Conditional pieces
(the `IF … THEN MSG = MSG ‖ …` shape) live here, as separate statements
on separate blocks. The emitter tracks the appended length from the
residues (ac2's advance), never by guessing. No arena, no mapping:
memory agrees byte for byte.

### 5.2 WMSP temporaries (19 groups; P33)

In the IR text a group is

```
p@70166144 = ""
… (loads, min shapes, ?UNSIGNED_TO_CHAR pieces as located reads)
p@70166144 = p@70166144 + [@fp+4, 30 varying]
p@70166144 = p@70166144 + " died."
rt_call ?WRITE_SCREEN(p@70166144, OUT_CHAN) site=701661A3
```

— no claim, no release-of-storage, no metadata (RULED, Sep 5/6). `p@b`
lives in the **arena**: the otherwise unused emulated segment
[0x75000000, 0x75800000) — sized to one byte-prefix per Mapper form
(P30) — laid out STATICALLY at lowering time (block → address,
capacity; capacity from the census's declared-length bounds; overflow of
a capacity is a loud fault). Because it is emulated memory, the runtime
reads a temp through a plain pointer — no materialisation at the call.
The master is NOT changed: its temps stay on its stack; the deviation
lives entirely in the checker's compare (§6). Stock mode is the only
mode for strings — there is one master behaviour.

Lifetime needs no management on the clone: the value is rebuilt on the
next execution of its block and otherwise persists. That is also what
makes condition-system unwinds — the master's wsp reset past its claims
by the dispatcher — a non-event on the clone.

### 5.3 The three LOCK_FILE by-reference constants (P33 tail item)

`WPSH const; LDASP` to pass a constant by reference: lower as a located
constant in the frame or leave embedded — plan-gate choice (3 sites).

---

## 6. Verification — the Mapper's arena form — RULED (Sep 6)

### 6.1 What changes and what does not

The strict surface compares at every listed block entry exactly as
before. The only addition is a FORWARD-ONLY address translation for
clone values that lie in the arena segment, implemented as a new FORM in
the existing Mapper module (docs/Mapper.md, the Aug 15 design of record
for the book-mode stack map). `equivalent(master_v, clone_v)` handles
arena rows like any other form:

```
RAW      if clone_v == master_v
MAPPED   if fwd(clone_v) == master_v
MISMATCH otherwise
```

Never translate master→clone in the compare path. **The arena form is
ONE-DIRECTIONAL: clone→master only** (RULED, Sep 6). master→clone is
ambiguous — two groups in one routine claim at the same stack base at
different times, and a released region is reused by ordinary frames — so
the form does not offer it; `clone_location()` (the Mapper's single
master→clone operation, for mediated dereference) REFUSES loudly on an
arena address: the runtime only reads temps, so a mediated write into
one is a finding, not a lookup.

### 6.2 The map

One row per `p@b`:

```
row = (arena_addr, length, wfp, master_addr)     ; master_addr == 0 ⇒ unmapped
```

- `arena_addr` and capacity are STATIC (the arena layout artifact);
  `length` is the value's current length; `wfp` and `master_addr` are
  bound dynamically.
- clone→master by binary search over the 19 rows sorted by arena
  address (hot path: every AC compare). NO master→clone lookup exists.
- Invariants asserted on bind: arena ranges disjoint and inside the
  segment; a row's `wfp` is the master's current wfp.

### 6.3 Events — the map has TWO states and changes at TWO events

1. **Block entry** (`p@b = ""`): the row is (re)bound. The master's
   first-WMSP hook in that block binds `master_addr = wsp_before + 2`
   (the address `LDASP r; WADI 2,r` yields) and `wfp` = the MASTER's
   wfp (rows are keyed on the master's frame — in book mode the clone's
   wfp is an area address; P33-A ruling); every later WMSP of the block
   REBINDS to its own `wsp_before + 2`, so by the time the clone runs
   the block the row holds the address the group actually pushes (the
   last claim's — verified 19/19; no rendezvous exists between a group's
   claims, so intermediate binds are unobservable). Nothing is computed
   from census sizes at runtime. The master queues bind/rebind/unmap
   events per ordinal; the clone drains them into its own mapper at the
   top of its next batch, before any statement of the block runs.
   Appends update `length` and extend the arena image in place.
2. **Frame exit** — WRTN (hook keyed on `pre_wfp`, unmapping every row
   with `wfp ≥ pre_wfp` — the ≥ rule also covers the emulated I.GOTO's
   single WRTN after `STAFP 2`, and the R?SIGNAL/?ERROR frame walk),
   and the ON-system pop (the I.GOTO landing stub's `STASP 0` at
   7017EC9E restoring the target's wsp: unmap and frame-exit every row
   with `wfp ≥ machine.wfp`). Every row so hit is UNMAPPED
   (`master_addr = 0`); Δ for those frames asserts 0 and is erased.
   Arena memory is never freed. (P33-A survey of frame-restoring paths:
   WRTN, I.GOTO, R?SIGNAL — DEF?ON is ordinary verified L2 since Aug 12,
   NOT terminal; T.INIT/I.SFALT/O.ON/WPOPB sites dismissed with
   reasons.)

**The STASP does NOT touch the map.** Rows stay translatable until frame
exit because, after the release, the compiler reloads only ac1 —
ac0/ac2/ac3 may carry the last piece's pointers (master: stack; clone:
arena) through many blocks as dead registers, and those must keep
comparing equal. On the clone the STASP lowers to `release`, whose only
semantics is `assert(wsp == ac1)`: the clone's wsp never moved, and this
proves it is exactly where the master's is about to be.

What the STASP DOES affect is kept OUTSIDE the map:

- **wsp equality** (refined at the P33-A gate, Sep 6): EACH engine keeps
  its own per-frame accumulator Δ on its own wfp — `+2·ac` at each WMSP,
  `−(old−new)` at each STASP, zeroed at frame exit — and the compare is
  `master_wsp − Δ_master == clone_wsp − Δ_clone` ("claim-free wsps
  agree"). Today (both engines claim) it is identical to the plain wsp
  check; after P33-B (only the master claims) it is `master_wsp −
  clone_wsp == Δ_master`; a group P33-B refuses (both claim again) stays
  exact instead of being silently accepted. Any other cause of
  divergence breaks the equality. The hooks live IN the WMSP/STASP/WRTN
  instruction arms (null-gated, the zero_claim precedent) so the clone's
  embedded WMSPs inside IR blocks are seen too.
- **memory oracle** (OPTIONAL, K-gated — plan gate): a mapped row is
  compared (arena bytes `[0, length)` vs its master range) only while
  its master range lies inside the master's live stack
  (`master_addr < master_wsp`) — a predicate at oracle time, not a
  stored state.

### 6.4 compare_pair

A clone AC in the arena segment translates through the mapped row that
contains it; a hit on an unmapped row, or on no row, is a MISMATCH (a
clone value pointing at a temp the master does not currently have).
Everything else compares raw.

### 6.5 Known edge (recorded, not designed around)

A loop re-executing a `p@b` block rebinds its row while a dead register
may still hold the previous iteration's pointer; if the stack depth
differs between iterations the dead register mismatches. If it ever
shows, it is a censused armed-pc drop at that entry — the existing
escape hatch — not a change to the rules.

### 6.6 Static inputs

The arena layout artifact (block → arena_addr, capacity) and the hook
table (block → first-WMSP pc; the 57 WMSP and 19 STASP pcs for Δ), both
with provenance headers, in the pushmap tradition. The Δ accumulator and
the two frame-exit hooks are keyed on wfp, not on pcs.

---

## 7. Idioms the emitter recognises — RULED as the target set

From Census §3 (counts are WCMV sites unless noted):

| idiom | count | IR |
|---|---:|---|
| ASSIGN-LIT-VARYING `v = 'lit'` | 528 | `[@v, n varying] = "lit"` |
| ASSIGN-STR-VARYING `v = t` | 136 | `[@v, n varying] = [@t, m varying]` |
| ASSIGN-STR-VARYING-PAD (min shape) | 21 | same; the min is the statement's own rule |
| ASSIGN-STR-FIXED / ASSIGN-LIT-FIXED / ASSIGN-STR-MIN | 18 / 7 / 7 | `[@v, n] = …` |
| COPY-STR-EXACT / COPY-LIT-EXACT (first piece) | 225 / 153 | `append([@scratch, n], …)` or the first `p@b + …` |
| CONCAT-PIECE (ac2 continuation) | 349 | `append(…)` / `p@b = p@b + …` |
| TEMP-FIRST-PIECE | 49 | the first `p@b = p@b + …` |
| …+CALLRESULT | 112 | piece = `[@fp+k, ac0]` after the `?UNSIGNED_TO_CHAR` rt_call |
| …+SUBSTR | 23 | `substr(…)` piece |
| tail split (ac1 as next count) | 17 | `append(…)` with the count from the residue |
| ASSIGN-FROM-TEMP | 11 | `[@v, n varying] = p@b` |
| WSTB const / computed | 86 / 13 | `… + char(0x0A)` / `… + char(x)` |
| WCMP equality | 40 | `ac1 = cmp(a, b)` root statement + the existing `goto [..] (ac1 == 0)` (P31 ruling) |
| WBLM (all 12 are fills — P31) | 12 | `words()` (fill() in the readable layer) |
| WMSP / STASP | 57 / 19 | `p@b = ""` (hooks) / `release` |
| LOCK_FILE by-ref constants | 3 | plan-gate choice |

Everything else: refused, listed with its window, embedded.

---

## 8. Projects — RULED (split), sequence LEAN

- **P30 — the C++ string library** (no IR, no emulator behaviour change;
  ships dark): `hw/strings/`: `EagleString` (construct from literal /
  located / substr / char; append; assign-to-located with pad/truncate
  and the varying length word; blank-padded equality; the residue
  computation for each replaced instruction, §3); the **arena form** in
  the Mapper module (§6: 19 static rows, `equivalent()` over them,
  `clone_location()` refusing) and the Δ accumulator; unit tests in the
  `tests/helpers_selftest.cpp` style — residues brute-forced against
  `EagleSpecial`'s own WCMV/WCMP/WBLM on random operands, assignment /
  compare against a reference, Mapper form invariants. One K=1 stock
  gate: the library's presence changes nothing.
- **P31 — located strings (ir 5)**: the grammar of §2 for located
  strings and pieces; IRExec on the library; `[@a,n] = piece`,
  `[@a,n varying] = piece`, `==`, `words()`/`fill()`; residues §3. That
  is `v = 'lit'`, `v = t` in every pad/truncate form, WCMP, WBLM — ≈780
  sites. NO arena, NO checker change. Proves the string type, the
  residue rule and the emitter's idiom recognition on the simplest
  population. Battery.
- **P32 — append chains**: `append()` to located scratch buffers,
  CALLRESULT / SUBSTR / WSTB pieces, tail splits, copy-outs — ≈900
  sites. Still no arena, no checker change. Battery.
- **P33 — `p@b` (19 blocks) + the checker**: the arena, `p@b = ""` /
  append / `rt_call(p@b)`, `release`, the hooks (19 first-WMSP; the 57/19
  Δ hooks; WRTN; ON-pop), the Mapper form wired into compare_pair, the
  optional memory oracle; the 3 LOCK_FILE constants. Smallest
  population, all the checker work, LAST, with everything else green.
  Battery + a leg that forces an unmapped-row mismatch to prove the
  checks fire.

Each project: census-first plan gate, K=1 book/stock gates per slice,
full battery (034 template via `bin/task_source.sh`, JOBS=3), REPORT.
P30 and P31's Phase A may run in parallel (no shared files); P31's Phase
B waits for P30. P33's checker half may be built dark alongside P32
(Lockstep/Mapper, not lower.py); its Phase B waits for P32.

---

## 9. The C++ end state (why the design has this shape)

| IR | C++ (translation) |
|---|---|
| `[@a, n]` / `[@a, n varying]` | a span / a `varying<n>` field of the reconstructed struct |
| `p@b` | a `std::string` (static per site while addresses matter; a local afterwards) |
| `p@b = p@b + "lit"` | `s += "lit"` |
| `[@v, n varying] = piece` | `v.assign_padded(piece)` |
| `a == b` | `pad_equal(a, b)` |
| `rt_call ?WRITE_SCREEN(p@b, chan)` | `write_screen(chan, s)` |
| `release` | nothing |
| residues | dropped |

When the runtime is native the arena and the map disappear with the
stack; the Mapper's arena form is then the same mechanism the frames
project uses when frames themselves move to the clone side.

---

## 10. Findings carried in (do not re-derive)

Census §10: WCMV/WCMP/WBLM/WMSP/STASP agree with the emulator on every
semantic Quest uses; the residue-class differences (B-1 OVR, B-4 WCMP
post-mismatch pointers, G-1 WMSP limit throws, G-2/G-3 throws) are in
EmulatorDivergences.md. F1: WCMV's carry is never read — set it anyway
(§3). F5: ac2 continuation is load-bearing (§3). F6: WBLM sequential
order is load-bearing (fill). F7: SYSCALL-preserves-ac3 is an ASSUMPTION
to verify in P31's census. Mapper.md: comparison clone→master only;
`clone_location()` is the single master→clone operation, for
dereference by ruling.

---

## 11. Open items (collect at the named plan gate)

- P30: exact library API; the residue functions' signatures; how the
  Mapper form is registered (Mapper.md §1's three-call surface must be
  preserved).
- P31: `substr` with computed byte offsets — grammar form for `bp +
  expr`; the F7 SYSCALL-ac3 check; whether the optional footprint-hash
  oracle is worth landing early.
- P32: the 28 unresolved destinations (3 TERRITORY_MAP by-reference
  array writes need the callers read); tail-split count expressions.
- P33: memory-oracle gating; the LOCK_FILE constants; final arena
  capacities per row (P33-A ships provisional 4 KiB columns in
  quest.strhooks; P33-B replaces them from lowering).

---

## 12. Glossary

- **located string** — a string with a memory address and a declared
  capacity; fixed (`CHAR(n)`) or varying (length word + data).
- **piece** — an expression yielding a string value: literal, located
  read, `substr`, `char`, or `p@b`.
- **`p@b`** — the arena variable of claim-group block `b`; the only
  string variable kind.
- **arena** — emulated segment 0x75000000, statically laid out, holding
  the 19 `p@b` images.
- **residues** — the values WCMV/WCMP/WBLM leave in ac0–ac3 and c; the
  IR statements set them identically (§3).
- **claim group** — the WMSPs of one PL/I statement and the STASP that
  releases them; one per `p@b` block.
- **Δ(frame)** — the per-frame accumulator of outstanding master claims;
  `master_wsp − clone_wsp` must equal it.
- **Mapper form** — a kind of row the Mapper's `equivalent()` knows how
  to translate; the arena form is clone→master only.
