> **CAVEAT (added Sep 9 2026, P37/R40)**: this census used per-routine
> statement counts derived from addrbook ranges. Those counts MIS-SIZE a
> PL/I multiple-ENTRY pair, because the shared body falls in whichever
> range contains it — CREATE_MAP 7 vs DISPLAY_MAP 902, TRANSPORT_TERRAK 2
> vs TRANSPORT_SUNDAR 138. Any figure here that is per-routine and
> size-dependent should be re-checked against the union of an R40 pair's
> ranges. See METHOD §16 and compiler/crossings.py.

# Project 34 — the readable layer: census

Sep 6 2026, branch `p34-readable` (from main 4837d5c).  Book read:
`Work/emulation/quest.ir2.book` from `origin/p31-located-strings` @
b4384b177bed8229c50b43235b2c08d62a954fd8, sha256
281fd5010ad4e8b35502646d4d8759dd7db53274a92f3e91792eeb2f9409051c (ir 5,
13,507 blocks).  Other inputs unchanged from main: quest.addrbook (e6fde2c2…),
quest.blocks.split (1d3baaf6…), Disassembled/quest.symbols (7fc5e4f7…).
Tool: `compiler/readable.py`; machine census: `census_raw.txt`; renderings:
`readable/*.txt` (every header carries the provenance and the switch set).

Everything here is a RENDERING census — nothing was executed.  Every count
below is `compiler/readable.py --census` output (whole book, all 130 addrbook
entries), quoted, not re-derived.

## 0. What was built

Six switchable rewrites over the ir 5 statements (plus three helper
rewrites that fell out of making them provable), each counting what it
resolved and what it refused, with the reason:

| switch | rewrite | resolved | refused / left explicit |
|---|---|---|---|
| frame | `wp(ac3, d)` → `local_d`, `R[ac3 + -k]` → `arg_N`, image words | 13,130 locals, 976 args, 819 image words, 35 via a register holding `&local_d`, 13 frame arrays | 356 refs while ac3 is not fp (§1), 53 odd offsets, 2 beyond the frame, 1 beyond argc |
| static | constant address → symbol / `static_<addr>`; `wp(i, K)` with K wrapping into the data segment → `NAME[i]` | 3,378 named, 542 array sites | 13 generated names (statics quest.symbols does not list, 96 distinct addresses over the whole census, §2) |
| callargs | book-mode arg-slot stores folded back into `call NAME(args)` | 558 calls | 7 calls (value not stable to the call / slots missing), 28 slot stores left explicit |
| record | base + i*stride + K → `BASE_rec<stride>[i].f<K>` | 1,292 indexed, 300 through an element-pointer local, 239 direct fields | 1,152 base + unknown term (§3c), 193 locals with defs of mixed shape (§3d) |
| literal | `[@local, n varying] = "text"; rt_call F(&local)` → `F("text")` | 78 | 428 unproven (§4), 18 whose destination is not a frame local, 6 not varying |
| regfold | single-use register values folded into the use | 14,397 pure + 3,519 adjacent effectful (nested) | 6,792 kept because the register is live at the terminator; 3,639 blocked (crossing rules, §5); 2,860 multi-use; 305 effectful not at root; 252 unused in block |
| simplify | Nova 17-bit identities (`(e&0xFFFF \| lsh(c,16)) & 0xFFFF` → `e & 0xFFFF`; bit k of it → `lsh(e,-k)&1`; carry → `c`; the COM form) | 171 + 666 + 1 + 32 | — |
| sketch | `if/else goto`, `; fall through`, `; loop back`, `bounds_check … ; DERR nn`, `return` | all terminators | — |

Three DECLARED BELIEFS, each a switch, each counted (§6): heapdisjoint
(76,628 alias decisions), argdisjoint (24,533), envinherit (9,696 blocks).
Runtime: 15.5 s for the whole book on the runner-class box — **over the
10 s flag** (profiled once: the in-block symbolic env re-walk in
analyse_fp / analyse_shapes / the liveness scans; not optimised further, a
research tool).

## 1. Frame naming (rewrite 1)

- Layout from quest.addrbook: locals `ac3+1 .. ac3+2*frame`, image words
  0/-2/-4/-6/-8/-10 (`frame.ac3_c`, `prev_wfp`, `ac2_img`, `ac1_img`,
  `ac0_img`, `frame.word`), arg N at `wfp-10-2N`.  A write to
  `frame.ac0_img` is the return value (WRTN reloads ac0 from it,
  EagleStack.cpp:471–495).
- ac3 is tracked statement by statement: `ac3 = wfp` (LDAFP) and WSAVS set
  it; any other write, and the string-library residues of a located-string
  statement (§5.8: the library writes ac0–ac3), clear it; the state
  propagates along CFG edges (a predecessor leaving ac3 repurposed makes
  the successor's entry unknown until its own LDAFP).  356 frame-form
  references were rendered raw for this reason; the dominant shape is
  `ac3 = add(ac3, local_50)` — **ac3 as fp + index into a frame array**,
  then a DERR guard splits the block before the LDAFP that restores it.
  Where the in-block env still knows `ac3 = fp + E` the reference renders
  as `local_<K>[E]` (13 sites, ALCHEMIST_HOME `local_9[local_50.w]`).
- **Finding — nested procedures are ON-unit bodies inlined between the
  parent's statements.**  The addrbook's stacked `#` entries
  (`LOGON.1@70175EB0`, `LOGON.2@70175EE2`, `ALLY_PLAYER.1@7015D068`, …:
  `nested,nocall`) each begin with their own WSAVS (own frame: LOGON.1/.2
  frame 0x06, ALLY_PLAYER.1 frame 0x09) and are registered, not called:
  the parent does `ac2 = 0x70175EB0; LJSR [O.ON]` and branches over the
  body (`WBR` in the blocks file).  The body ends in `LJSR [I.GOTO]`
  (PL/I GOTO out of the unit, 26 sites) — which never returns, but the
  blocks file records a fall-through edge after it.  Routine attribution
  had to stop at those edges (`NONRETURNING_LJSR = {I.GOTO, I.STOP}`);
  with them, 4,312 blocks were mis-owned and the parent's locals rendered
  as `local_534?` beyond the unit's 18-word frame.  With the edge cut:
  24 blocks unreached by any entry (fallback: nearest preceding entry).
  For 5b this means: a routine's body is NOT its pc range; the ON-unit
  bodies are separate frames living inside it; `I.GOTO` is a non-local
  exit.  LOGON proper is 82 blocks; its two units are 2 and 3 blocks.
- Belief: the LJSR helpers I.PROLOG / I.EPILOG / O.ON / O.REVERT return
  with ac3 == fp (the code after them uses ac3 as fp with no LDAFP —
  LOGON 70175EA8..70175F10).  Without it LOGON's whole body renders with
  ac3 unknown.

## 2. Static naming and usage (rewrite 2)

129 distinct static addresses referenced by the whole book; 33 named by
quest.symbols, 96 not (rendered `static_<addr>`; table in
`census_raw.txt` §"static usage census" with per-width read/write counts,
routine counts and address-taken counts).  Highlights (reads are M32/M16
counts over the book):

| static | routines | reads | writes | &taken | note |
|---|---|---|---|---|---|
| SD_PTR 70000210 | 73 | 1,040 | 0 | 3 | shared-data base pointer (player table, §3) |
| OBJ_PTR 70000212 | 49 | 546 | 0 | 44 | object-file base pointer (region/object tables) |
| CAS_PTR 70000214 | 22 | 121 | 0 | 0 | castle-file base pointer |
| PLAYER_NUM 70000216 | 40 | 1,426 (M16) | 4 | 112 | the index into SD_PTR_rec686 nearly everywhere |
| OUT_CHAN 70000260 | 70 | 1 | 0 | 721 | passed to ?WRITE_SCREEN |
| IN_BUFFER 7000021C | 29 | 54 | 3 | 54 | |
| MOVES_LEFT 70000270 | 2 | 7 | 15 | 0 | |

Unnamed statics: 90 of the 96 are reached only through the `NAME[i]`
array form (`M16[i]`/`M32[i]` in the table): lookup tables in the data
segment at 7000058D…70000D92 (small, 1–7 routines), 7001EE8B…7001F6CF and
700DE87A…700E6987 (the last group written by INIT_* routines and read
through register indices — the map/terrain tables).  Nothing was named
by hand; the census gives the C++ world the list.

The "compared-with constants" column is empty: the tool records
comparisons only when a static read is compared directly against a
literal in a goto/assert, and the compiler always loads to a register
first (the constant then sits in the register-folded rendering, e.g.
`if (sx16(PLAYER_NUM.h) >s 10)` in the DERR guards).  Worth adding to the
census tool: comparisons through the fold.

## 3. Records (rewrite 3)

Base = a pointer static read `M32[0x7000021x]`; recognised through the
in-block env (register values), through frame locals whose EVERY reaching
definition has one shape (`i*stride` → index local, `base + i*stride (+K)`
→ element-pointer local), and through single-predecessor / same-value
joins (envinherit).  K is the raw displacement; origin note = min K seen
+ stride, valid only if the smallest K seen is field 0.

| table | stride | sites | routines | guarded index sites | fields seen (raw K) |
|---|---|---|---|---|---|
| SD_PTR_rec686 (player) | 686 | 807 | 66 | 567 | 86 distinct K, −642 … (list in census_raw.txt); index almost always `sx16(PLAYER_NUM.h)` |
| SD_PTR_rec6 | 6 | 34 | 8 | 16 | 6898, 6899, 6900 |
| OBJ_PTR_rec9 (region) | 9 | 515 | 42 | 245 | 11495 h, 11496 h, 11497 h, 11498 w, 11500 w, 11502 h, 11503 h → origin base+11504 if 11495 is field 0 (PICK_X_Y_reading.md agrees: region i at OBJ_PTR+11495+9i) |
| OBJ_PTR_rec19 | 19 | 23 | 8 | 16 | 1983… |
| OBJ_PTR_rec20 (object) | 20 | 51 | 4 | 16 | −19 … −15 → origin base+1 (1-based) |
| CAS_PTR_rec23 (castle) | 23 | 111 | 21 | 47 | −23 … −17 → origin base+0 |
| direct fields (no index) | — | 239 | | | `OBJ_PTR->f11502` (region count), `SD_PTR->f40` (the ?RANDOM_NUMBER seed), `SD_PTR->f42/43`, … |

"Guarded" = the routine asserts `index >s 0` (DERR 17 fold) on the same
closed index expression: 567/807 player sites, 245/515 region sites —
the 1-based origin is corroborated by the guard where it is present.

(c) **1,152 sites are `BASE + <unknown term>`** (rendered
`(SD_PTR + lsh(add(mul(mul(i, 686), 16), -9456), -4))->f0.h`): the term
is `lsh(16*(i*686) + c, -4)` — a BIT address.  These are the WSZB/WBTZ/
WBTO bit tests and the `M8[…]` byte forms: bit fields of the player
record (SD_PTR: 1,023 of them), and 58/78 for OBJ_PTR/CAS_PTR.  The
tool classifies them as heap for aliasing but does not name the field;
the decomposition needed is `word = base + i*686 + c/16, bit = c%16`.

(d) 193 frame locals have reaching definitions of mixed shape
(PICK_X_Y `local_4` is `i*9` in one region and a ?RANDOM_NUMBER argument
in another; DIED `local_8` is `i*686`, `i*9` and `i*16` at different
points).  Naming is per USE (the reaching set at that point), so the
mixed locals still render as records where one shape reaches — the
compiler reuses frame slots across statements, so a local is not a
variable.

## 4. Literal folding (rewrite 4)

541 literal pieces in the book; 78 folded
(`?WRITE_SCREEN(&OUT_CHAN, "text")` etc.), 428 left as
`vstr(&local_d, n) = "text"` with the reason, by shape:

| reason (census key `literal.unproven_kind`) | sites |
|---|---|
| live after the call: an undecorated `LCALL`/`XCALL`/`LJSR` instruction line downstream reads memory opaquely | 97 + 44 + 69 = 210 |
| live after: a partial overwrite of the range (the compiler reuses the varying's words as a plain word — frame slot reuse) | 87 |
| live after: a plain read of some word of the range (unknown pointer) | 45 |
| live after: a `WCMV` instruction line (P32's append chains, opaque) | 22 |
| live after: another string statement reads it | 12 |
| the block has no consuming rt_call before its exit (call in a later block through a non-trivial edge) | 31 |
| the local is read again before the call | 19 |
| read through an indirect pointer | 2 |

Proof used: intra-routine may-live fixpoint over the folded blocks; a
range is dead when every path reaches a full overwrite (or a varying
assignment to the same base, which rewrites the length word) before any
read; unknown-address reads, string statements on unknown addresses and
instruction lines count as reads.  The two large buckets are the honest
statement of what the flat-graph world must decide first: **(i) what an
undecorated game→game call may read (the P28 decoration answers it for
rt_calls only; the 159 LCALL / 37 XCALL instruction lines are the
game→game sites the book still leaves as instructions), and (ii) frame
slot reuse — a "local" is a slot, and the varying temp at `local_32`
is later a 32-bit word at `local_32`.**

## 5. Register folding and the strict surface (rewrite 5)

Rules: a def folds into its single use in the block when nothing between
writes an input register/flag, no possibly-aliasing store intervenes
(classification: local / static / slot / heap / argderef; unknown = may
alias), effectful ops only replace a bare-register copy or nest into the
immediately following statement (side-effect order unchanged), and never
when the register is live at the terminator.  Liveness across blocks
uses the RT conventions as declared beliefs (RTConventions.md: no rt
callee reads ac0/ac1 on entry, ?UNSIGNED_TO_CHAR reads ac2, the four
value-returning callees write ac0, ac1/ac2 preserved) and WRTN's reload
of ac0–ac2 from the frame image (ret discards register values).

Registers live at the block terminator (defined in the block, reaching
the exit explicitly, read by some successor before being written):

| live regs | blocks |
|---|---|
| 0 | 7,372 |
| 1 | 4,551 |
| 2 | 1,130 |
| 3 | 367 |
| 4 | 53 |

Explicit register defs reaching the exit whether live or not: 0/1/2/3/4 →
6,732 / 4,152 / 1,631 / 674 / 284 blocks.  6,792 defs were kept only
because of liveness; the typical carriers are the rt_call result in ac0
(`ac0 = cvwn(ac0)` in the continuation), loop indices kept in ac1 across
a `goto`, and the `t1`-carrying Nova tests.  Blocked folds (3,639):
"memory read cannot cross a possibly-aliasing store" (a record read
across a store through a register the env does not know) and
"effectful op cannot cross a flag reader/writer" (two `add`s before two
stores — WNADI pairs) are the two shapes.

## 6. Declared beliefs (the seeds of 5c)

| switch | belief | evidence | uses counted |
|---|---|---|---|
| heapdisjoint | an address that decomposes to `pointer-static + …` lands in the shared data pages, never in the frame, the statics or the arg-slot area | SD_PTR/OBJ_PTR/CAS_PTR are set by ?GET_SHARED_PAGE/?OPEN_SHARED_IO_FILE (RTConventions.md); no instruction in the book computes a frame address from them | 76,628 alias decisions |
| argdisjoint | `R[ac3 + -k]` (k ≥ 12), a by-reference argument pointer, was computed by the caller before this frame existed, so it does not point into this frame | frame allocated by WSAVS at entry; recursion would need the callee to pass its own local — not observed | 24,533 |
| envinherit | a block's entry register values are what every goto-predecessor leaves (identical closed text, entry-value markers `r#in@pc` included) | pure dataflow; no hardware belief, but it assumes the blocks file's goto edges are complete (they are the emitter's own) | 9,696 blocks |
| (fixed) fp-preserving helpers | I.PROLOG, I.EPILOG, O.ON, O.REVERT return with ac3 == fp | LOGON 70175EA8..70175F10 uses ac3 as fp after them without LDAFP | — |
| (fixed) non-returning helpers | I.GOTO, I.STOP never return to pc+… | ON-unit bodies (§1) | 4,312 blocks re-attributed |
| (fixed) RT register conventions, WRTN reload | as in RTConventions.md / EagleStack.cpp:471–495 | — | every live-out |

Switching any of the first three off (`--no-heapdisjoint`, …) re-runs
the census without it; the counts are the price.

## 7. Assessment — what the flat-graph world must decide first

1. **Frame slots are not variables.**  193 locals carry defs of mixed
   shape, 87 literal folds are blocked by partial overwrites, and the
   compiler's WSAVS frame is reused slot by slot (PICK_X_Y's `local_4`
   is an index, then an argument buffer).  A per-USE naming (reaching
   shapes) works; a per-ROUTINE variable table will not.  The C++ world
   needs either SSA-style renaming of slots or a frame-slot type map
   built from reaching definitions — decide which before writing any
   `struct Frame`.
2. **Undecorated game→game calls are opaque.**  210 literal folds and
   most "unknown exit" liveness answers stop at LCALL/XCALL/LJSR
   instruction lines.  P28's rt_call decoration answered the runtime
   side; the same declaration (what each callee reads/writes through
   its argument pointers and in registers) is needed for the 159 + 37
   game sites, or every fold across a call is refused.
3. **ON-units are separate frames inside the parent's pc range**, exited
   by `I.GOTO` (§1).  Routine = reachable set from its WSAVS, cut at
   the non-returning helpers; the flat graph must carry that ownership.
4. **Bit-addressed fields** (1,152 sites) need one more decomposition
   step (`lsh(16*i*stride + c, -4)`) before the player record's bit
   flags can be named; the 90 unnamed static arrays need names only the
   C++ world can give (index origin, element type).
5. **Table origins are inferable but not proven from the book**: the
   `>s 0` guard proves 1-based indexing where present (567/807 player
   sites), the min-K note gives a candidate origin; the region table's
   `OBJ_PTR+11495` agrees with the hand reading, the object table's
   `base+1` and the castle table's `base+0` are candidates.

## 8. Corrections to declared beliefs (METHOD §6)

- Earlier in this project the addrbook's `n`/`c` edge parser ignored
  `j <target> n <ret>` lines (the LJSR/XJMP form) — 4,312 blocks were
  attributed by the nearest-preceding fallback; corrected in the tool,
  recorded here.
- Rendering `ac0#in` (entry-value marker) inside a record index is
  honest but not a variable name; the rt_call result in PICK_X_Y's
  `OBJ_PTR_rec9[ac0#in]` is the `r` of the hand reading.
