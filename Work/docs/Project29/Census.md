# Project 29 — strings, Phase A: idiom census (RESEARCH ONLY)

Session Sep 5 2026, solo. Research only: nothing outside
tools/string_sites.py and docs/Project29/ was touched; no emulator, no
lower.py, no IR.md, no artifact the emulator reads, no battery.

TREE VINTAGE: the Sep 5 Work.tgz (Work__79_) + Disassembled.tgz, handed
as tarballs (not a git checkout, so no main commit hash can be stated).
Verified against docs/Provenance.md: quest.dis 5c1db5fb…, quest.mem
1d44317d…, quest.tags 90659843…, quest.symbols 7fc5e4f7…, quest.blocks
772d21aa…, blocks.split 1d3baaf6… all match the Sep 5 table. P28 has NOT
landed in this tree (docs/Project28/ holds only PROMPT.md; no
rt_sites.py), so IR.md is ir 3 and the census style follows
docs/Project27/Census.md + tools/derr_clusters.py.

Tool: `c_src/tools/string_sites.py` (Python only; inputs quest.dis,
quest.blocks.split, quest.mem, quest.symbols). Run from Work/c_src/tools:

    python3 string_sites.py --dis ../../../Disassembled/quest.dis \
      --blocks ../quest.blocks.split --mem ../../../Disassembled/quest.mem \
      --symbols ../../../Disassembled/quest.symbols \
      --sites ../../docs/Project29/sites.txt \
      --strings ../../docs/Project29/quest.strings \
      --census ../../docs/Project29/census_raw.txt

**Runtime 1.8 s wall clock** (1-core box; under the 10 s flag).
Outputs: `sites.txt` (every site: window, four operand classes with the
symbolic value, idiom, destination), `quest.strings` (the literal table,
791 literals, quest.mem sha256 in the header), `census_raw.txt` (all the
tables this document summarises, plus the full lists).

## 0. How the tool reads the ground (so the numbers can be checked)

Forward symbolic evaluation of every block over linear-form values
(`fp`, `sp@pc`, constants, loads, byte pointers `bp(base, bytes)`), with
dependency tracking so each site's WINDOW is exactly the instructions that
produced ac0..ac3. Blocks inherit the end state of a unique predecessor;
blocks with several predecessors get a phi-merge (this is what resolves
the `WSLE 0,1 / WMOV 1,0` min() shape, whose skip splits the block).
Semantics read from the emulator source, not intent (METHOD §5):
NLDAI/WNADI/NADDI sign-extend 16 bits (EagleCompute.cpp:311,317);
WADI/WSBI use imm 1–4; WMOVR is an unsigned >>1 (:160); WSTB acs,acd stores
acd's byte at [acs] (EagleGeneral.cpp:120); after WRTN ac3 = caller's wfp
(EagleStack.cpp:488), so calls leave ac3 = fp; after SYSCALL ac3 is
treated as preserved because the compiler addresses `[ac3+d]` right after
one (7015BE5B) — an ASSUMPTION, flagged, not verified against the OS
dispatcher. Byte-EA conventions are Project25/ByteEA.md §2; the standing
LLEFB `@` rendering defect is compensated exactly as lower.py does
(raw = printed | bit31; docs/DISASSEMBLER_BYTE_OPERANDS.md).

Provenance classes (per the prompt): literal / frame / static / temp /
computed. Everything the classifier is not sure of stays `computed` with
the symbolic expression printed; nothing was rounded into a bucket.
`frame` byte offsets are reported relative to fp (negative = the
argument area).

## 1. Counts — reproduce the prompt's table exactly

| op | sites | note |
|---|---:|---|
| WCMV | 1,637 | 1,630 in blocks.split blocks + 7 in blocks whose successor line is a bare `n` (7016E020, 7017CA42, 7017CFEE) — a parser corner, not a listing defect |
| WCMP | 40 | all 40 followed by `LDAFP 3` then `WSEQ 1,1` (23) or `WSNE 1,1` (17) |
| WBLM | 12 | |
| WMSP | 57 | in 12 routines |
| STASP | 19 | every one restores wsp to the value before its group's FIRST claim |
| pass-by-ref WPSH+LDASP | 3 | LOCK_FILE 70169B77 / 7C / 7F |
| WSTB (not in the prompt) | 99 | 86 store a constant byte — the 1-char concatenation piece |

Windows: 819 of the 1,765 sites have a window that crosses a block
boundary (resolved through the predecessor chain); 103 still depend on
unresolved block-entry state (a register set in a block with several
predecessors whose values differ) — listed in census_raw.txt §windows.

## 2. What the four operands are (WCMV, 1,637 sites)

| operand | const | frame | static | literal | temp | computed |
|---|---:|---:|---:|---:|---:|---:|
| ac0 dst_count | 1,002 | 96 | 8 | – | – | 531 |
| ac1 src_count | 1,018 | 98 | 17 | – | – | 504 |
| ac2 dst | – | 1,499 | 19 | 0 | 88 | 31 |
| ac3 src | – | 548 | 40 | 901 | 46 | 102 |

(`frame` for a count = a frame length word; `computed` for a pointer =
one of the four dereference/record shapes below.)

Top signatures (dst_count/src_count/dst/src): const/const/frame/literal
843; computed/computed/frame/frame 305; const/const/frame/frame 114;
computed/computed/frame/computed 86; frame/frame/frame/frame 85;
computed/computed/temp/temp 38 (full table in census_raw.txt).

The `computed` counts decompose as: a frame length word ± a constant
(concatenation totals), a length word reached through a pointer slot
(`N[*(fp-12)]` — the length word of a by-reference string argument),
a `?UNSIGNED_TO_CHAR` result plus a length (112 sites tagged
CALLRESULT), a phi (min of two lengths, 85 sites), the residual
src count of the previous WCMV (17 sites), or a record-field length
`N[SD_PTR + 686*PLAYER_NUM + k]`.

The `computed` pointers decompose as: dereference of a frame pointer
slot (a by-reference argument or a based string: `bp(W[fp-12])+2`),
a record via a static pointer (`SD_PTR`/`OBJ_PTR`/`CAS_PTR` +
record_size*index + field — the shared-data records), a static table
indexed by a register (`LLEFB @[ac3+…]`, the 12-word varying-string
name tables at 0x70000266/0x70000932/…), or a block-entry register.

## 3. IDIOM CATALOGUE (WCMV)

Layout facts every idiom rests on: a PL/I VARYING string is one length
word followed by its data, so a local at word `w` has its length at
`[fp+w]` (`XNSTA 0,[ac3+w]`) and its data at byte `2w+2`
(`XLEFB 2,[ac3+2w+2]`). Static varying strings likewise:
`IN_BUFFER` = length at 0x7000021C, data at 0x7000021D:0.

| count | idiom | PL/I construct | shape | example |
|---:|---|---|---|---|
| 528 | ASSIGN-LIT-VARYING | `v = 'literal'` (v varying) | `NLDAI n,1 / WMOV 1,0 / XNSTA 0,[fp+w] / XLEFB 2,[fp+2w+2] / XLEFB 3,lit / WCMV` — length word stored, then an exact n-byte copy | 7015BE68 |
| 349 | CONCAT-PIECE | one operand of `a \|\| b \|\| …` | dst is the END pointer left in ac2 by the previous WCMV (ac2 is NOT reloaded); 30 of these take the piece length from `?UNSIGNED_TO_CHAR`; 4 carry a symbolic byte offset | 7015C91C |
| 225 | COPY-STR-EXACT | first piece of a concatenation, or `fixed = t` with equal lengths | dst_count == src_count, no length store, dst a frame scratch buffer or fixed field | 7015CE7D |
| 153 | COPY-LIT-EXACT | first piece of a concatenation from a literal | as above, src literal | 7015C902 |
| 136 | ASSIGN-STR-VARYING | `v = t` (both varying) | length word stored = len(t), exact copy | 7015CE8E |
| 112 | …+CALLRESULT (all idioms) | `… \|\| CHAR(n)` | the piece length is ac0 returned by `?UNSIGNED_TO_CHAR` (38 COPY-STR-EXACT, 33 ASSIGN-STR-VARYING, 30 CONCAT-PIECE, 5 TEMP-FIRST-PIECE, 6 others; counted in their own rows) | 7015C916 |
| 49 | TEMP-FIRST-PIECE | first piece into a WMSP temp | dst = `(LDASP+2)<<1`, i.e. the first free wide after the claim | 7016617A |
| 21 | ASSIGN-STR-VARYING-PAD | `v = t` where maxlen(v) < len(t) possible | dst_count = min(len(t), maxlen(v)) via `WSLE/WMOV`, src_count = len(t): WCMV truncates (c=1) when len(t) > maxlen; the length word stores the min | 7015D131 |
| 18 | ASSIGN-STR-FIXED | `fixed = t` | dst_count const, src_count a length: pads with blanks or truncates. The 17 const/const pad-fill sites are 12←8/9/6/1 (a 12-char name field padded), 80←1 (7017B811 row fill), 1←0 | 7016ED28 |
| 7 | ASSIGN-LIT-FIXED | `fixed = 'lit'` with a room check | dst_count = min(len(lit), room) | 7015D942 |
| 7 | ASSIGN-STR-MIN | `fixed = t`, min shape, no length store | | 701664F6 |
| 11 | ASSIGN-FROM-TEMP (5 + VARYING 1 + SUBSTR 1 + CALLRESULT 1 …) | the result of a dynamic concatenation assigned out of its temp | src temp | 70166EE9 |
| 23 | …+SUBSTR (all idioms) | `SUBSTR(s,i[,n])` on either side | a byte pointer carrying a non-constant byte term: 10 COPY-LIT-EXACT (a blank-run literal sliced), 5 COPY-STR-EXACT, 4 CONCAT-PIECE, 2 ASSIGN-STR-FIXED (INIT_OBJ_TBL, dst = record + index), 1 ASSIGN-LIT-FIXED (7017B811: `row(i) = ' '`, 80 ← 1: a blank FILL by padding) | 70167EE1 |

Two concatenation machineries, chosen by the compiler on whether the
total length is a compile-time constant:

1. **Frame scratch buffer** (the common one, ≈500 pieces): pieces are
   WCMV'd back-to-back into a fixed CHAR(n) local at some `[fp+k]`
   (ac2 simply carries on), then one ASSIGN-*-VARYING copies the buffer
   into the varying local with its length word, then `XPEF [fp+w] /
   LPEF OUT_CHAN / LCALL ?WRITE_SCREEN`. HELP 7016D5E3..7016D602 is a
   clean 3-piece example (164 + 150 → 314).
2. **WMSP temps** (57 claims, 12 routines): when a piece length comes
   from a length word, the compiler claims `ceil((len+k)/4)` wides
   (`WADI 3,ac / WMOVR / WMOVR / WMSP`), saves the temp base
   (`LDASP; WADI 2,ac` → first free wide, stored in a frame slot, later
   re-read and `WLSI 1` to a byte pointer) and concatenates into it.
   Each result that feeds the next expression gets a NEW claim (temp1 =
   a‖b, temp2 = temp1‖c, temp3 = the varying result with room for its
   length word: the `+5` size), and ONE `XWLDA 1,[slot]; WSBI 2,1;
   STASP 1` releases the whole group. Groups: DIED 3+3, DISPLAY_MAGIC 4,
   DISPLAY_CAVE 1, DISPLAY_SCREEN 3+3+3, DISPLAY_INVENTORY 2, GET_QUEST
   3+2, HELP 1, INIT_OBJ_TBL 2, LIST_PLAYERS 5, OBSERVE 1+1, OP_EDIT
   5+5+5, STORE 5 — sum 57 = every claim is released by an STASP, none
   is left to WRTN, none is nested across groups, none escapes its
   block-group (all the temp pointers are dead after the STASP).

Related small idioms found alongside (outside the prompt's op list):

- **WSTB const**: 86 single-byte appends (`NLDAI 0x0A,1 / WSTB 2,1 /
  WINC 2,2`) — the 1-character piece of a concatenation; bytes: 0x0A×23,
  0x0B×20, ' '×8, '|'×8, 0x0D×5, ':'×4 … (census_raw.txt §WSTB).
- **Tail split** (17 sites, ATTACK/DISPLAY_SCREEN …): the residual
  src count of the previous WCMV (`wcmv.src_left`) becomes the next
  piece's src_count — the compiler tracks the remaining room of a fixed
  target in the src-count register (so the first WCMV never pads) and
  the LAST piece's length is `80 − remaining`.
- **`?UNSIGNED_TO_CHAR` pieces**: number → CHAR conversion returns the
  length in ac0 and is followed by `XNLDA 1,[fp+w]; WADD…` — 112 sites.

## 4. WCMP (40 sites) and what consumes the result

Every WCMP is followed by `LDAFP 3` and a `WSEQ 1,1` (23) / `WSNE 1,1`
(17): the program only ever tests ac1 == 0. The −1/+1 ordering results
are never distinguished. Shapes:

- 11 CAST: `SUBSTR(spellname,…) = 'lit'` — dst literal (6/7 bytes),
  src the string reached through the by-ref argument at fp−6.
- 12 against `IN_BUFFER` (the typed command): 8 with a record field via
  SD_PTR/OBJ_PTR/CAS_PTR (name matching), 3 with a literal, 1 with a
  frame string.
- 10 with **dst_count = 0 and a literal dst pointer** (7015C09C, 70167281,
  70168188, 7016ECE2, 70172DA8/E7A, 7017CE4D/84/B0…): `IF s = ''` — WCMP
  supplies blanks for the exhausted side, so this is "s is all blanks".
  These are the 4 literals recorded in quest.strings with a length-0 use.
- 3 DISPLAY_SCREEN cache checks (`OLD_SCREEN_STR`/`OLD_NAME`/
  `OLD_LAND_MASS_STR` vs the new text) gating a redraw; 1 DISPLAY_CAVE.
- 2 phi-length compares (70162D72 uses `WUSGE` to pick the shorter).

Only the blank-padding rule of WCMP matters to these consumers, never the
sign of the result.

## 5. WBLM (12 sites) — word-block moves

| kind | sites | shape |
|---|---:|---|
| record copy, 90 words | 5 | src/dst = `SD_PTR + 686*PLAYER_NUM − 86 (+k)`: a player record ↔ working copy (QUEST 7015C2E4 is 10 words; DIED 7016635B, GET_QUEST 7016BCD8, START_TURN 70178A31, STORE 7017A597 90 words) — destination class (b) |
| self-overlapping zero fill | 6 | `WSUB 0,0 / XWSTA 0,[fp+k] / NLDAI n,1 / XLEF 2,[fp+k] / XLEF 3,[ac2+2] / WBLM` — src = dst − 2 so the zeroed wide smears forward: `struct = 0` for 8 words (BARGAIN ×2, LOGON ×2 at 10), 3,040 words (DISPLAY_MAP 70165205) and 5,758 words (TERRITORY_MAP 7017B7A4). The emulator's forward copy order is what makes this a fill — a direction-semantics point for the manual check |
| record copy, 90 words, count from a call result | 1 | CAST 7016357B (`unk(LCALL:DISPLAY_SCREEN)` in the index: the block starts after the call, so the multiplier is the call's ac1 — listed as unresolved) |

## 6. Pass-by-reference WPSH temps (3)

LOCK_FILE 70169B75..7F: `NLDAI 0xEB9,0 / WPSH 0,0 / LDASP 0 /
WLDAI 1,0xC00 / WPSH 1,1 / LDASP 1 / WSUB 2,2 / WPSH 2,2 / LDASP 2` —
three constants pushed and their stack addresses taken, i.e. by-reference
arguments made from constants for the following system call (the queue
lock). They are the only WPSH+LDASP pairs in the program.

## 7. DESTINATIONS (the user's cut)

Direct class of each WCMV/WBLM write, and the ULTIMATE class after
following scratch buffers through the WCMV that copies them out:

| class | direct | ultimate | meaning |
|---|---:|---:|---|
| (a) | 785 | **1,334** | buffer that feeds `?WRITE_SCREEN` (8 of them `?WRITE`) — ephemeral, a wrong byte is a wrong message |
| (b) | 46 | **88** | game state: statics, or a record through SD_PTR/OBJ_PTR/CAS_PTR |
| (c) | 750 | 135 | scratch/temp consumed only by another WCMV or WCMP |
| (d) | 31 | 46 | frame local passed to some other call (DIED ×~30, ?OPEN_FILE ×6, RETURN_MESSAGE ×4, ?OPEN_SHARED_IO_FILE/?GET_SHARED_PAGE — file names!, ?UNSIGNED_TO_CHAR, OP_EDIT's unnamed XCALL target 7016F556) |
| (p) | 9 | 18 | written through a by-reference argument (CAST, KILL_PLAYER, OP_EDIT, TERRAIN, TERRITORY): the caller's variable, class decided per caller |
| ? | 28 | 28 | unresolved (below) |

### 7(b) — the fidelity-critical list, in full (88 sites)

- **Shared-data player record** (`SD_PTR + 686·PLAYER_NUM`, persisted in
  the shared file): ATTACK 7015F75E (byte +48); DIED 70166067 (byte
  −1248), 70166084 (−1214); LOGON 70175F9B, 70175FBD, 70175FF7 (−1248),
  70176054, 70176076 (−1214); DISPLAY_INVENTORY 7016817F (byte −122,
  record index from the fp−12 argument); WBLM record copies QUEST
  7015C2E4, DIED 7016635B, GET_QUEST 7016BCD8, START_TURN 70178A31,
  STORE 7017A597.
- **Shared-data object table** (`SD_PTR + 0x1EFBA…0x1F5A1`, INIT_OBJ_TBL):
  7016DF66, 7016DFD6, 7016E005, 7016E033, 7016E048, 7016E05D, 7016E072,
  7016E087 — the name tables written at startup (the 5 `LLEFB @[…]`
  literal sources at 0x602A2AB4 etc. are the text).
- **OBJ_PTR records** (OP_EDIT 7017426E byte −28, 701744EC, 701749FC byte
  +3974) and **CAS_PTR records** (START_TURN 70178E30, TAKE_OVER_CASTLE
  7017C380, byte −32 — castle names).
- **Statics**: `CAVE_STR` (DISPLAY_CAVE: 56 sites, 7 concatenation chains
  ending at 7016675A, 70166794, 701667F5, 70166850, 701668B1, 7016690C,
  701669E1, 70166A1B plus 7016698B), `OLD_CAVE_STR` 70166703,
  `OLD_SCREEN_STR` 7016708E, `OLD_NAME` 7016715F, `OLD_LAND_MASS_STR`
  70167278, `LAND_MASS_STR` (TERRITORY 7017D004), `IN_BUFFER`
  (DISPLAY_INVENTORY 701680A4, 701680BC, 701680D4, 701680EB, 7016812B,
  70168136, 70168143 — the inventory routine REWRITES the input buffer).
  The `OLD_*`/`CAVE_STR` statics are redraw caches: silent until the
  next WCMP against them decides whether to redraw.

Full per-site lines: census_raw.txt §"(b) game-state writes".

### 7(?) — unresolved (28), listed, not rounded

- 22 WCMV into frame locals with no consumer found within 12 successor
  blocks (BACKPACK fp+13 ×2, BARGAIN fp+18 ×2, DISPLAY_SCREEN fp+11/239/239,
  DISPLAY_INVENTORY fp+6 ×2/fp+11 ×2, SEIGE fp+8/9 ×3, START_TURN fp+30,
  ALLY_PLAYER/KILL_PLAYER fp+517, TERRITORY_MAP fp+16, OBSERVE fp+19,
  BOAT fp+19…): the varying local is written on several branches and
  consumed after a join further than the scan reaches. All frame locals,
  so (a)/(d), never (b).
- 3 TERRITORY_MAP writes at fp−24/−25 (7017B811, 7017B866, 7017BD95):
  NEGATIVE offsets = the argument area, with ac3 set in a
  multi-predecessor block — almost certainly a by-reference map ARRAY
  argument (`row(i) = ' '` fill and `SUBSTR(row(i),j,1) = c`), i.e.
  class (p). Needs a look at TERRITORY_MAP's callers.
- 6 WBLM zero-fills of frame structures (BARGAIN, LOGON, DISPLAY_MAP,
  TERRITORY_MAP) and CAST 7016357B (count from a call result).

## 8. quest.strings — the literal table

791 literals decoded from quest.mem (sha256 in the file header); all
resolved (none outside the dump). Literals used with more than one
length (the prompt's SUBSTR/shared-tail finding):

- **7015155A** — a run of ≥39 blanks used with lengths 19, 20, 22, 32, 39
  and with 9 non-constant lengths: the compiler's shared BLANK SOURCE
  for `(n)' '` / blank padding pieces (`COPY-LIT-EXACT+SUBSTR` sites
  slice it with a computed byte offset).
- **7015BF5F** ('Two characters please.', 23) and **7016EA67** (the
  159-byte player-list header) also appear with length 0: those are the
  `s = ''` WCMP tests above, which point the dst register at whatever
  literal is nearest — the 0-length use is a compare, not a copy.
- **7017804D** (15 / 0 / non-const): a WCMP dst in START_TURN whose
  count is READ_IN's result.

No literal is used with two different NON-ZERO constant lengths except
the blank run, i.e. there is no "shared tail" in the program.

## 9. FINDINGS (stop-and-report items for the design session)

F1. **WCMV's carry is never read.** No WCMV or WCMP is followed by a
carry test (`SNC/SZC`, `WSKBO/WSKBZ`, …) within the next 3 instructions
(0 of 1,677). The program never tests for truncation; `c` matters only
as lockstep residue.

F2. **WCMP results are only tested for equality** (40/40 `WSEQ/WSNE 1,1`).
The manual's ordering semantics matter only if the emulator's −1/0/+1
could disagree with the hardware on EQUALITY — i.e. the padding rule.

F3. **The compiler pre-computes min() rather than relying on WCMV
truncation** in 85 sites, but 21 of them still let WCMV truncate
(ASSIGN-STR-VARYING-PAD: src_count = len(t), dst_count = min). Padding
by WCMV (src exhausted → blanks) is live in the 17 const/const pad-fill
sites (12←8/9/6/1, 80←1, 1←0) and wherever a fixed field takes a shorter
varying value (ASSIGN-STR-FIXED, 18). No const/const site TRUNCATES
(dst_count < src_count never occurs with both constant).

F4. **Every WMSP claim is released by an STASP** (57/57, 19 groups, no
claim reaches WRTN, groups never interleave). The claim size is always
`ceil((bytes + k)/4)` wides with k ∈ {3, 5, 6, 29, 31, 82, 84, 189, …}
where the +5/+6 groups hold the varying result's length word.

F5. **ac2 carries between WCMVs.** 349 concatenation pieces (plus 17
tail splits) use the END pointer the previous WCMV left in ac2/ac3
without reloading — the IR must model the residue values of WCMV
(ac0 = 0, ac1 = src remaining, ac2/ac3 = end pointers), not just its
memory effect.

F6. **WBLM is used as a fill by overlapping** (src = dst − 2, 6 sites):
the emulator's word-at-a-time forward loop is load-bearing; a memmove
would silently break DISPLAY_MAP/TERRITORY_MAP initialisation.

F7. **SYSCALL preserves ac3** is an assumption taken from the compiler's
code shape (7015BE5B, 70176099…), not from the emulator source; if wrong,
4 INIT_SHARED_DATA/LOGON/UPDATE_USER_DATA_FILE destinations become
unresolved. Worth one glance at OSTask.cpp's return path.

F8. Two sites (701678C8 and the second `value differs` hit in sites.txt)
store a length word whose symbolic form differs from dst_count while the
counts are equal: the same quantity reached through another register
path, most likely; left flagged rather than assumed.

F9. Parser corner: three blocks.split blocks end in a bare `n` line (no
successors); any tool that expects `n A B` misses their 7 WCMVs.

## 10. Manual vs emulator (item 5) — all five pages read (WCMV, WCMP, WBLM, WMSP, STASP)

Source: the ECLIPSE MV/Family "Wide Character Move" page (user's scan,
Sep 5 2026), diffed against EagleSpecial.cpp:42–74 in the HWFindings
table format. Emulator = law for the IR (METHOD §5); a disagreement is
an emulator FINDING, not something this project touches.

### WCMV — agreements

| point | manual | emulator (EagleSpecial.cpp) | verdict |
|---|---|---|---|
| operands | AC0 dst #bytes (+asc/−desc), AC1 src #bytes (+asc/−desc), AC2 dst byte ptr, AC3 src byte ptr | same roles; `direction()` maps sign → ±1 step | agree |
| AC0 after | 0 | loop runs until `dst_count==0`, `ac[0]=dst_count` | agree |
| AC1 after | 0 or #bytes left unmoved (two's complement when descending) | `ac[1]=src_count`, decremented toward 0 from either sign — negative residue for descending | agree |
| AC2 after | address of the byte FOLLOWING the destination string | `dst -= dst_direction` per byte written, one step past the last | agree |
| AC3 after | address following the last byte FETCHED | `src` advances only on a fetch, not while padding | agree |
| pad | remainder of destination filled with spaces when source shorter | `copy=' '` (0x20) once `src_count==0` | agree |
| CARRY | 1 iff source #bytes > destination #bytes, else 0 | `c = (src_count != 0)` at exit — equivalent: bytes remain in the source exactly when the source was longer; AC0=0 initially → c=1 iff AC1≠0 (source > 0 = destination); AC1=0 initially → destination space-filled, c=0. Mixed directions fall out the same way (magnitudes) | agree |
| AC0 = 0 initially | no bytes fetched, none stored | loop body never runs | agree |
| AC1 = 0 initially | destination filled with spaces | every iteration pads | agree |
| overlap | strings may overlap in any way; overlap does not affect execution | byte-at-a-time read/write in program order, no buffering — the overlapping-fill behaviour is the manual's | agree |
| PSR / stack / PC | unchanged / unchanged / PC+1 | untouched / untouched / `address+1` | agree |

### WCMV — findings

| # | point | manual | emulator | note |
|---|---|---|---|---|
| **W1** | **Overflow** | **0 after execution** | **`ovr` not written** (no `machine.ovr` in the WCMV arm; other Eagle ops in EagleStack.cpp clear it explicitly at :128,:204,:274,:466) | a flag-residue disagreement of the kind lockstep cannot see (METHOD §2). Whether it is ever observable in Quest depends on OVR being set before a WCMV and read after it without an intervening clear; the census did not look for OVR readers. Recorded as a finding for the emulator owner; not fixed here. |
| W2 | invalid byte pointers | protection fault MAY occur even if no bytes are moved | segment check only per byte actually read/written; a 0-count WCMV with garbage pointers proceeds silently | emulator is more permissive; no Quest site has a 0 count with a computed pointer, so no known live exposure |
| W3 | segment/ring crossing | a backward move that would cross a ring inward faults BEFORE execution; nothing forbids a string spanning a segment | throws "crossing segments not allowed" mid-copy on any segment change | an emulator restriction stated loudly (METHOD §8), not a semantics disagreement; no site crosses (all sites are within the frame/static/code segment of ring 7) |
| W4 | interruptibility | interruptible, PC decremented so the instruction restarts from the updated ACs | atomic | irrelevant to a single-task emulation; noted for completeness |

Aside from the page: its example `LLEFB 2,DEST*2` confirms LLEFB's
displacement is a BYTE displacement (`*2`), consistent with
Project25/ByteEA.md §2 and the standing `@` rendering defect.

### WCMP (EagleSpecial.cpp:76–109) — agreements

| point | manual | emulator | verdict |
|---|---|---|---|
| operands | AC0 = string 2 #bytes (±), AC1 = string 1 #bytes (±), AC2 = string 2 bp, AC3 = string 1 bp | `dst`=ac2/ac0 is string 2, `src`=ac3/ac1 is string 1 | agree |
| result in AC1 | −1 str1 < str2, 0 equal, +1 str1 > str2 | `src_byte<dst_byte → -1`, `>` → `+1`, else 0 | agree |
| bytes | unsigned 8-bit 0–255 | `read_byte` returns `uint32_t` (Memory.hpp:38), compared as ints | agree |
| shorter string exhausted | comparison continues against spaces (040₈) for the rest of the longer string | `while(dst_count!=0 \|\| src_count!=0)`, exhausted side supplies `' '` | agree |
| both lengths 0 | no comparisons, result 0 | loop body never runs, `result=0` | agree |
| halt | at first mismatch, or when both strings are completed | `break` on the first unequal pair | agree |
| CARRY | unchanged | untouched | agree |
| strings unchanged / overlap allowed | yes | read-only, byte at a time | agree |
| PSR / stack / PC | unchanged / unchanged / PC+1 | untouched / untouched / `address+1` | agree |

### WCMP — findings

| # | point | manual | emulator | note |
|---|---|---|---|---|
| **P1** | **AC2/AC3 after a mismatch** | **address OF the failing byte** (of the byte following the string only when both strings compare equal) | pointers are advanced when the byte is fetched, BEFORE the compare, so after a mismatch they point ONE PAST the failing byte (`src-=src_direction` / `dst-=dst_direction` at :90/:99, compare at :101) | residue disagreement on the 17 WSNE-consumed sites that can mismatch (equal-compare exit agrees). Quest never reads AC2/AC3 after WCMP (every site does `LDAFP 3` then tests only ac1 — §4), so no game-visible effect; lockstep cannot see it (both engines share the arm). Finding for the emulator owner. |
| **P2** | **AC0 after** | number of bytes (or two's complement) LEFT TO COMPARE in string 2 | `dst_count` already decremented for the failing byte, so on a mismatch it is one less (in magnitude) than the manual's count; on an equal exit both are 0 | same root cause as P1; same non-exposure (ac0 is reloaded before use at every site — the next instruction after `LDAFP 3; WSEQ/WSNE 1,1` writes ac0 or branches to code that does). |
| **P3** | Overflow | 0 after execution | `ovr` not written | same as W1 |
| P4 | invalid pointer | result code **4** in AC1 (protection fault error) | throws on a segment crossing; otherwise no check | emulator loud vs hardware soft-fail; no Quest site can reach it (all pointers are frame/static/literal/record) |
| P5 | interruptibility | interruptible, restartable | atomic | as W4 |

The page's example (`XJMP TABLE,2` on −1/0/+1) shows the intended
three-way use; Quest uses only the zero test (§4, F2), so P1/P2 are the
only WCMP points with a behavioural difference and neither is live.

### WBLM (EagleSpecial.cpp:20–40) — agreements

| point | manual | emulator | verdict |
|---|---|---|---|
| operands | AC1 = #words (+asc/−desc), AC2 = source, AC3 = destination; AC0 unused | `src_count=ac[1]`, `src=ac[2]`, `dst=ac[3]`; ac0 untouched | agree |
| AC1 after | 0 | `ac[1]=0` | agree |
| AC2 / AC3 after | pointer to the next word after the last word moved | `src -= dir`, `dst -= dir` per word, one past the last | agree |
| AC1 = 0 initially | no words moved | loop body never runs | agree |
| order | words moved one at a time in consecutive (ascending or descending) order, addresses updated after each word stored | `read_word` then `write_word` per iteration, then step | agree — and this is exactly what makes the 6 self-overlapping fills (F6) well-defined on hardware: an interruptible, restartable instruction that updates addresses after each store MUST be sequential, so `src = dst − 2` smears the first two words forward |
| data | unsigned 16-bit words, untouched | `read_word`/`write_word` | agree |
| CARRY | unchanged | untouched | agree |
| PSR / stack / PC | unchanged / unchanged / PC+1 | untouched / untouched / `address+1` | agree |

### WBLM — findings

| # | point | manual | emulator | note |
|---|---|---|---|---|
| **B1** | **indirect bit in AC2/AC3** | not a fault: when updating the addresses WBLM **forces bit 0 to 0**, i.e. the addresses are used direct | `throw "WBLM instruction with indirection!"` if bit 31 is set in either pointer before starting | emulator loud where hardware is lenient. No Quest site is exposed: all 12 sites take AC2/AC3 from `XLEF` (frame words) or from `LWADD SD_PTR` record arithmetic, never with bit 31 set |
| **B2** | Overflow | 0 after execution | `ovr` not written | same as W1/P3 — the whole WCMV/WCMP/WBLM family fails to clear OVR |
| B3 | ring crossing in descending mode | protection trap before execution, AC1 := 4 | throws on any segment change mid-copy | as W3; no descending WBLM exists in Quest (all 12 counts are positive) |
| B4 | interruptibility | interruptible, restartable | atomic | as W4 |

### WMSP (EagleStack.cpp:567–576) — agreements

| point | manual | emulator | verdict |
|---|---|---|---|
| function | wsp + 2·ac → wsp (ac = signed count of double words; computed as a 1-bit left shift, added to wsp) | `machine.wsp=machine.wsp+2*machine.ac[AA]` | agree |
| ac after | unchanged | not written | agree |
| CARRY | unchanged | untouched | agree |
| PSR / PC | unchanged / PC+1 | untouched / `address+1` | agree |
| stack | wide stack pointer updated | wsp written | agree |

### WMSP — findings

| # | point | manual | emulator | note |
|---|---|---|---|---|
| **M1** | **the fault condition** | the ONLY test described is a **fixed-point overflow of the shift** (2·ac not representable): then the stack is NOT modified, a return block is pushed against the ORIGINAL wsp, the processor jumps to the stack-fault handler, and on return WMSP is re-executed. No comparison against the stack limit (WSL) or base (WSB) appears on this page — on the MV, limit detection belongs to the push-class instructions | `if(wsp>wsl) throw "Stack fault - upper limit"; if(wsp<wsb) throw "Stack fault - lower limit"`; no fixed-point-overflow test (int32 wrap) | two-sided disagreement. (a) The emulator's WSL/WSB checks are an emulator invention at this instruction — hardware would let wsp pass the limit here and fault on the NEXT push. They are inert in Quest: every run has been clean (METHOD §3), so no live WMSP ever exceeded a limit; all 57 claims are positive and small (`ceil((len+k)/4)` wides). (b) The fixed-point overflow the manual does test cannot arise in Quest (|ac| ≪ 2³⁰). Net: no game-visible difference, but design question (ii) should not lean on "WMSP's limit checks" as hardware behaviour — they are the emulator's loud guard (METHOD §8), fine to keep, wrong to cite as semantics. Recorded for the emulator owner. |
| **M2** | Overflow | 0 after execution | `ovr` not written | same as W1/P3/B2 — fourth member of the family |
| M3 | ruling-8 `zero_claim` | (no hardware equivalent) | instrumentation: a positive delta zeroes the claimed words when `zero_claims` is on | emulator-only convention, already ruled (P26/ruling 8); listed so the design session sees it is not hardware |

Mechanically the page also settles that WMSP's delta is `2·ac` WORDS
(4·ac bytes), matching the census's reading of the claim sizes
(`>>2` of a byte count = double words).

### STASP (EagleStack.cpp:516–524) — confirmed from the STASP page

(The user also supplied the sibling STAFP page, which matches the
emulator's STAFP arm `machine.wfp=machine.ac[AA]`, :531, apart from S2.)

| point | manual | emulator | verdict |
|---|---|---|---|
| function | ac → wsp | `machine.wsp=machine.ac[AA]` | agree |
| exceptions | none | none (no checks in either direction) | agree |
| ac / CARRY / PSR / PC | unchanged / unchanged / unchanged / PC+1 | untouched / untouched / untouched / `address+1` | agree |
| Overflow | 0 | `ovr` not written | **S2**: same as W1/P3/B2/M2 — the fifth member of the family |
| ruling-8 zero_claim on a RAISE | (no hardware equivalent) | zeroes exposed words when wsp goes up and `zero_claims` is on | emulator-only convention; all 19 Quest STASPs LOWER wsp (§3), so it never fires here |

Item 5 is complete: all five pages read.

### Summary of the manual review (item 5)

Every instruction in the family agrees with the emulator on the
semantics Quest depends on: WCMV pad/direction/carry/residues, WCMP
equality and padding, WBLM sequential word order (the fill idiom), WMSP
arithmetic, STASP store. The disagreements, all residue/guard-class and
none game-visible:

| id | what | live in Quest? |
|---|---|---|
| W1/P3/B2/M2/S2 | manual clears **Overflow** after WCMV, WCMP, WBLM, WMSP, STASP (and STAFP); emulator leaves `ovr` untouched | only if OVR is set before one of these and read after without an intervening clear — not surveyed; flag-residue class |
| P1/P2 | WCMP mismatch leaves AC2/AC3/AC0 one step further along than the manual | never read (all 40 sites test ac1 only) |
| M1 | WMSP: emulator adds WSL/WSB limit throws the manual does not describe, and lacks the manual's fixed-point-overflow fault | neither can fire on Quest's claim sizes; the throws are a loud guard, not semantics |
| B1 | WBLM throws on an indirect bit the hardware would simply clear | no site sets it |
| W2/W3/B3/P4 | emulator is loud (segment throw / no 0-count pointer check) where hardware protection-faults or soft-fails (code 4) | unreachable from Quest's pointer shapes |
| W4/P5/B4 | atomic vs interruptible-restartable | irrelevant single-task |

## 11. OPEN QUESTIONS for StringsDesign.md (questions, not decisions)

(i) These are the first IR statements that write MEMORY in bulk, and
lockstep compares registers + wsp + block ordinal, not memory. What gate
proves a `wcmv(...)` wrote the same bytes? Candidates seen in the census:
(a) the 1,334 write-screen destinations are read back by `?WRITE_SCREEN`
within a few blocks — a syscall-argument capture (the bytes the write
syscall sees) would cover 80 % of sites with no new mechanism; (b) the 88
game-state sites are static/record addresses known at lowering time — a
QUEST_CAPTURE_DEST-style range compare per site; (c) the 135 scratch-only
sites are covered transitively by whatever consumes them. Is a
footprint diff of the destination range at the NEXT rendezvous enough,
or does the design want a per-statement byte compare?

(ii) `wmsp` is a wsp WRITE. Since every claim group is released by an
STASP in the same routine and never reaches a call boundary with the
claim live (57/57), can the whole group be ONE IR construct
(`claim n wides … release`) so that "no wsp arithmetic outside helpers"
holds by construction, with the helper owning the guard? Note (M1)
that the WSL/WSB checks in EagleStack.cpp's WMSP are the emulator's
guard, not hardware semantics — the manual's WMSP only faults on
fixed-point overflow of 2·ac — so the helper is free to keep or drop
them as a loudness choice. Does the ruling-8 zero-claim still apply to a
claim whose every byte is written before it is read?

(iii) The grammar has `bp(0xW:b)`; the sites need `bp(fp+2w+2)`,
`bp(fp)+len` (continuation), `bp(W[fp-12])+2` (through a pointer slot),
`bp(SD_PTR + 686*PLAYER_NUM - 1248)` (record field), `bp(0x70000267 +
12*i)` (indexed table), and the END pointer of the previous wcmv. Is the
end pointer an expression (`bp + count`) or a residue register read?

(iv) Yes for `s = 'lit'`: all 528 ASSIGN-LIT-VARYING sites are the one
5-instruction shape (length word store + exact copy, counts equal), so
`s = 'lit'` can be one IR statement carrying (word slot, literal, n).
`s = t` splits three ways (exact 136 / truncating min 21 / padded fixed
18) and 71 sites take the length from a call result — one statement with
an explicit length expression covers all of them.

(v) Things the census made me want to ask: does the IR need the WCMV
residue registers at all if it can re-express the continuation pointers
as `dst + n` (constant pieces) — but 17 tail-split sites and 30
CALLRESULT pieces need `src_left`, so which is cheaper: model the four
residues, or emit the concatenation chain as one statement? Should the
6 overlapping WBLM fills be recognised as `fill(words, value)` rather
than emulated as moves (F6)? Is `c` after WCMV to be set exactly (F1
says never read, lockstep says compared)? How should the 3 negative-
offset TERRITORY_MAP writes and the 22 unresolved frame destinations be
closed — by a wider forward scan, or by hand?
