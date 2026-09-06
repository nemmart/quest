# Project 31 — located strings (ir 5), Phase A: census + grammar (PLAN GATE)

Session Sep 6 2026, solo. Phase A only: `tools/string_sites.py` extended
(`--p31`, `--p31-tsv`; three tool corrections below), `docs/Project31/`
written. No lower.py, no IRExec, no IR.md, no artifact the emulator
reads, no battery. Branch `p31-located-strings`.

TREE VINTAGE: main @ `00f641c` (P28 merged 309585d; StringsDesign.md of
record). Verified against docs/Provenance.md before starting: quest.dis
5c1db5fb…, blocks.split 1d3baaf6…, quest.ir2.book 294f81d3…,
quest.ir2.stock 1cf12f33…, synclist.p27 af1be42f…. `hw/strings/` is not
on main (P30 not landed) — Phase B waits, as the prompt says; Phase A
shares no file with P30.

Tool (from Work/c_src/tools; runtime **2.1–2.7 s** on this box):

    python3 string_sites.py --dis ../../../Disassembled/quest.dis --blocks ../quest.blocks.split \
      --mem ../../../Disassembled/quest.mem --symbols ../../../Disassembled/quest.symbols \
      --sites ../../docs/Project31/sites.txt --strings /tmp/quest.strings \
      --census ../../docs/Project31/census_raw.txt \
      --p31 ../../docs/Project31/p31.ledger --p31-tsv ../../docs/Project31/p31.tsv

Outputs of record: `p31.ledger` (one record per candidate site: window,
fold/keep split, the four operands as IR expressions, `n` and where it
came from, destination class, the diamond if any, the exact IR line,
EMIT / REFUSE + category + reason; summary tables at the end),
`p31.tsv` (one line per site for the battery's verdict lines),
`sites.txt` / `census_raw.txt` (the P29 census regenerated with the
corrected tool — §1.2 says what changed). `quest.strings` regenerates
byte-identical to docs/Project29/quest.strings (not copied).

Rulings already taken (user, Sep 6, before Phase A): F7 → finding; min
diamond → no-delist form (statement in the join, synclist delta 0,
diamonds censused); WCMP → `ac1 = cmp(piece, piece)` root statement;
literal verification → lazy, at the first execution of each
literal-bearing statement, through the normal read path.

---

## 1. Findings

### 1.1 F7 — VERIFIED, and stronger than "preserved": after every SYSCALL, ac3 = wfp

`SYSCALL nnn` is the disassembler's name for `XJSR @[6]` + the call word
(Tools/Disassemble.java:96: opcode C619 8006). Word 6 of the segment
(quest.mem 0x70000006 = 7017FDEC) is the runtime's ?SYSTEM stub:

    7017FDEC WPSH 3,3                   ; push XJSR's return (ac3 = the call word's address)
    7017FDED LCALL [0x30000000],0       ; ?G.SYSCA — the OS; ac3 = 7017FDF1
    7017FDF1 WBR 2 (7017FDF3)           ; error return (dispatcher returns return_address)
    7017FDF2 ISZTS                      ; hidden word C7C9 (METHOD §4): success return (+1)
    7017FDF3 ISZTS                      ; TOS = call word + 1 (error) / + 2 (success)
    7017FDF4 LDAFP 3                    ; ac3 = wfp   <-- F7
    7017FDF5 WPOPJ

The dispatcher (os/OSTask.cpp:117–188) reads ac3 only as its return
address and writes back ac0..ac2; the stub then executes `LDAFP 3` on
BOTH paths. So the compiler's `XWSTA 0,[ac3+6]` right after a syscall
(7015BE5B) is fp-relative by construction — not because ac3 was
preserved but because it was reloaded. The tool now sets ac3 = fp after
SYSCALL (string_sites.py, the SYSCALL arm). **Effect on the census: 0
lines of sites.txt / census_raw.txt change** — every syscall in a window
already had ac3 = fp symbolically before it. No site's window moves; the
4 INIT_SHARED_DATA / LOGON / UPDATE_USER_DATA_FILE destinations the
census had flagged stand.

### 1.2 Two P29 tool defects found while rendering (fixed; census regenerated)

**T1 — byte-EA windows stopped one instruction short.** `byte_ea()` built
X-form and L-indexed byte pointers with `deps=[idx]`, dropping the base
register's producers, so the window of every `XLEFB n,[acX+d]` whose acX
was not fp began at the XLEFB itself (the `WMOV 2,3` feeding
`XLEFB 3,[ac3+0x24]` at 7015D201/7015D205 was outside the window and
marked `.`). Fixed (deps = base.deps | {idx}). 744 of 1,768 records have
a larger window (typically now reaching the LDAFP/WSAVS that produced
fp); "crossing a block boundary" 819 → 1,446. Classification of the four
operands, idioms and destinations: unchanged for all but the T2 sites.

**T2 — WUSGT / WUSGE / WULEI were treated as register writers.** They are
skips (EagleCompute.cpp:203–211, :365) and write no AC; the P29 generic
op arm made them produce an opaque `(a WUSGE b)` value. Effect: 51
records change classification, all in the DERR-cluster / min-shape
family — `dst_count=min()` 85 → 83, `src_count=phi` 28 → 25, three
`computed/computed/temp/temp` signatures become `const/const/temp/temp`,
70162D72's WCMP counts are now plainly `N[IN_BUFFER]` both sides, and
**the WMSP claim sizes of DISPLAY_MAGIC / DISPLAY_INVENTORY / HELP /
LIST_PLAYERS / STORE are now clean** (`size(wides) = 8`,
`>>2(N[fp+64]+21)`, … instead of expressions polluted with `WULEI(...)`
terms). Idiom totals are unchanged. P33 should take its claim sizes from
this regenerated census_raw.txt, not P29's.

Both corrections are recorded as corrections (METHOD §11); the P29
files stay as P29's record.

### 1.3 Correction to Census §5: none of the 12 WBLMs is a record copy — all 12 are fills

P29 read the six 90-word (and one 10-word) WBLMs as "player record ↔
working copy". The rendered operands say otherwise: in every one of them
`dst − src` is **1 word** (DIED 7016635B, GET_QUEST 7016BCD8, START_TURN
70178A31, STORE 7017A597, CAST 7016357B: `XLEF 2,[ac2+0x7FA9]` (−87),
`XNSTA 0,[ac2+0]`, `XLEF 3,[ac2+1]`, `NLDAI 90,1`, WBLM — the word just
stored is smeared through the following 90 words of the record) or **2
words** (QUEST 7015C2E4, the six frame-structure fills). Word-at-a-time
sequential order (Census §10, WBLM "order") is what makes all twelve
well-defined; the `words()` statement inherits it from the library. The
ledger prints `; fill: src = dst - k` on each. No design change: §7's
"WBLM copy 6 / fill 6" becomes "fill 12 (6 wide-smears, 6 word-smears)".

### 1.4 F8 closed for this population

`find_len_store` flags two sites whose stored length-word value is not
syntactically the dst_count: 701678C8 (ASSIGN-STR-VARYING+CALLRESULT,
outside P31) and 7016F730 (ASSIGN-STR-VARYING, refused here as
P32-COPYOUT). Every EMIT with a length-word store has `value ==
dst_count` (the `same` flag; a mismatch would refuse as
LENSTORE-MISMATCH — 0 such).

### 1.5 StringsDesign §1.3–§1.5 re-checked on the regenerated census (T2 changed the claim sizes)

Script over the regenerated census_raw.txt claim/release tables +
quest.dis + blocks.split (19 groups, 57 claims):

- **§1.3 straight-line claims — HOLDS, 19/19.** Between a group's first
  WMSP and its last append into a temp there is no listed block
  boundary in any group. Every boundary inside the [first WMSP, STASP]
  bracket is a TAIL cut, of exactly four kinds: the min skip of the
  copy-out into the varying-result temp (`WSGE/WSLE + WMOV`, the 11
  ?WRITE_SCREEN groups: 2 entries each), the consumer's return block
  (11), the two P27 DERR-cluster continuations K inside INIT_OBJ_TBL's
  tail (the SUBSTR bounds checks before its record copy-out), and the
  tail-split conditional byte after DISPLAY_SCREEN's third copy-out
  (70167049: `WSGE 1,1 / WBR / WSTB 2,0 / WSBI 1,1` — a conditional
  piece on the LOCATED chain, as ddf4187 says). HELP is one block.
- **§1.4 no call inside a group but the consumer — HOLDS, 19/19.** The
  only calls inside any bracket are `?WRITE_SCREEN` (11 groups). Minor
  correction to the design's count: it is **11 consumed by
  ?WRITE_SCREEN and 8 copied out to a located target** (DISPLAY_CAVE,
  DISPLAY_SCREEN ×3, HELP, INIT_OBJ_TBL, OBSERVE ×2), not 12 + 7 — HELP's
  ?WRITE_SCREEN is after its STASP and its copy-out (7016DC29) before.
- **§1.5 every claim released — HOLDS, 57/57.** Each STASP is in the
  claiming routine and restores `sp@<block>`, the wsp at entry of the
  group's block, i.e. before the FIRST claim; groups never nest; the
  claim sizes are now the clean `⌈(len + k)/4⌉` forms (k ∈ {3, 4, 5, 6,
  16, 20, 21, 22, 24, 35, 37, …}) F4 described.

---

## 2. Site list (Phase A.1) — docs/Project31/p31.ledger

Candidates: every WCMV in the idioms ASSIGN-LIT-VARYING(-PAD),
ASSIGN-STR-VARYING(-PAD), ASSIGN-STR-FIXED, ASSIGN-LIT-FIXED,
ASSIGN-STR-MIN, ASSIGN-FROM-TEMP(-VARYING); every WCMP; every WBLM.
**776 sites** (724 WCMV + 40 + 12; the prompt's ≈780). Census idiom
counts reproduced exactly (528 / 1 / 136 / 21 / 18 / 7 / 7 / 5+1).

| op | idiom | sites | EMIT | REFUSE |
|---|---|---:|---:|---:|
| WCMV | ASSIGN-LIT-VARYING | 528 | **528** | 0 |
| WCMV | ASSIGN-LIT-VARYING-PAD | 1 | 0 | 1 (P32-CAPACITY) |
| WCMV | ASSIGN-STR-VARYING | 136 | 48 | 88 (85 P32-COPYOUT, 3 COMPUTED-OPERAND) |
| WCMV | ASSIGN-STR-VARYING-PAD | 21 | 10 | 11 (9 P32-SUBSTR, 1 P32-CAPACITY, 1 P32-COPYOUT) |
| WCMV | ASSIGN-STR-FIXED | 18 | 16 | 2 (1 P32-SUBSTR, 1 COMPUTED-OPERAND) |
| WCMV | ASSIGN-LIT-FIXED | 7 | **7** | 0 |
| WCMV | ASSIGN-STR-MIN | 7 | 0 | 7 (P32-CAPACITY) |
| WCMV | ASSIGN-FROM-TEMP(-VARYING) | 6 | 0 | 6 (P33-TEMP) |
| WCMP | | 40 | 31 | 9 (2 P32-SUBSTR, 3 SUBSTR-of-another-length, 2 OPERAND-DEAD, 1 P32-RESIDUE, 1 P32-CALLRESULT) |
| WBLM | | 12 | **12** | 0 |
| **all** | | **776** | **652** | **124** |

Refusal categories (each site's reason in the ledger; the category is
the bucket, stable across sites):

- **P32-COPYOUT (86)**: `v = scratch` where the source count is a sum of
  length words plus a constant — the copy-out of a frame scratch chain.
  The count is what the chain appended; P32's emitter tracks it
  (StringsDesign §5.1). Not expressible with a constant `n`.
- **P32-SUBSTR (15)**: the source count is a min (`SUBSTR(IN_BUFFER, 1,
  min(32, len))`, the ALLY_PLAYER shape 7015D131 and the 9 DISPLAY_CAVE
  copies) or the length word of a *different* string (70162D72,
  OBSERVE 70172DA8/E7A: `SUBSTR(x, 1, len(y)) = y`). `substr` pieces
  are P32.
- **P32-CAPACITY (9)**: `dst_count = min(chain total, const)` — a fixed
  field or a varying receiving `SUBSTR(scratch, 1, min(total, cap))`;
  the capacity is itself a chain total (DISPLAY_MAGIC 701664F6 …). P32.
- **P33-TEMP (6)**: the source is a WMSP temp (`p@b`).
- **COMPUTED-OPERAND (4)**: the address expression contains `cvwn` /
  `ash` / `WNEG` (record index arithmetic through effectful ops, which
  the grammar keeps at statement root) and the destination is varying,
  so neither the expression form nor the register form applies. Needs
  the t-place form (P32's tail-split machinery) — listed, embedded.
- **OPERAND-DEAD (2)**: CAST 70162AAF/70162B53 — a leaf register the
  expression reads is rewritten by a kept instruction before the
  statement and the operand is varying. t-place form; P32.
- **P32-RESIDUE (1)**, **P32-CALLRESULT (1)**: DISPLAY_SCREEN 7016713B
  compares against the residue of a WCMV; START_TURN 70178DBE's count is
  READ_IN's result.

Every refused site stays an embedded `@WCMV`/`@WCMP`, block emitted as
today. Nothing is rounded.

Destinations of the 652 EMITs (Census §7 ultimate class): (a) 543
write-screen buffers, (b) 28 game state (the shared-data record fields
in ATTACK/DIED/LOGON, the CAVE_STR/OLD_* redraw caches, the 5
record-smears), (c) 9, (d) 23 (DIED, ?OPEN_FILE names …), (p) 6, (?) 12,
(−) 31 (the WCMPs). 52 routines. **498 distinct (address, length)
literals** are referenced; all are in quest.strings and their bytes were
read from quest.mem when the IR line was rendered.

### 2.1 How a site is rendered (the emitter's rules, mechanised in the tool)

The statement replaces the WCMV **and** the contiguous run of pure
operand producers immediately before it in the same block (`fold`: NLDAI
/ WLDAI / WMOV / XNLDA / XWLDA / LNLDA / LWLDA / XLEF / LLEF / XLEFB /
LLEFB / XLDB / LLDB / WLDB / ZEX / SEX, plus the absorbed `XNSTA` length
store). The emitter drops them and echoes them after `<-` in the
statement's comment, exactly as `rt_call` folds its pushes. The run
stops at the first non-pure producer (WNADI, WMUL, LWADD, WINC … write
c/ovr, which WCMP/WBLM do not overwrite; stores; calls) — everything
before it stays a statement in program order. `LDAFP` inside the run is
KEPT (it is `ac3 = wfp`, the leaf).

Operands are rendered from the census's symbolic values: `fp` → `ac3`;
`acN@entry` → `acN`; `N[a]`/`W[a]` → `sx16(M16[a])`/`M32[a]`; `fp+k` →
`wp(ac3, k)`; byte pointers → `bp(ac3, 2k+off)` / `0xW:b` (+ a symbolic
byte term); linear forms → flat `+ - *` chains. Three checks decide EMIT:

1. **Register leaves.** For each register the expression reads, the last
   non-folded in-block writer before the statement must leave the leaf
   value (for ac3: none, or LDAFP / WSAVS / WSAVR / SYSCALL (F7); for
   any other register: none) and the block-entry value must be the leaf
   (the census's `entry_ac`). Otherwise OPERAND-DEAD.
2. **Memory leaves.** No store in the window chain (predecessor blocks
   included) to an address the statement reads, the absorbed length
   store excepted. (0 self-assignments `v = v` in the population.)
3. **Fold consistency.** No kept instruction inside the run reads a
   register a folded instruction wrote (the clone never computes it).

When 1 fails and every pointer operand is a *fixed* piece (or a
literal), the site is re-rendered in **register form**: nothing is
folded, the setup executes as statements, and the operands are the
registers themselves — `[@ac2, 13] = "…"@0x7015D725:0` (5 ASSIGN-LIT-
FIXED sites whose loads sit in the min diamond's guard block) and
`words(@ac3, 90) = words(@ac2, 90)` (CAST, STORE). Exact by
construction: the master's operand IS the register's value at the
instruction, and the clone executed the same producers.

Counts are never operands. A varying piece carries its length in its
length word; a fixed piece and a literal carry `n`. The tool checks
that the census's count registers equal what the piece implies
(`ac1 == N[w]` with `ac3 == bp(w)+2` for a varying source; `ac0 == n`
or `min(n, ac1)` for the destination; the length-word store value
`== ac0`).

### 2.2 The capacity `n` and where it comes from (ledger `n` line)

| shape | EMITs | `n` |
|---|---:|---|
| varying ← literal | 528 | the literal length (`dst_count == src_count == len`) |
| varying ← fixed, both counts constant (constant-length chain copy-out) | 48 | that constant |
| varying ← varying/fixed, min shape `WSGE/WSLE + WMOV` | 10 | the guard's constant (`phi(c, len)`), or `min(c1, c2)` when both arms are constants (DISPLAY_CAVE 7016675A: min(80, 7) = 7) |
| fixed ← fixed, both constant (the 12←8/9/6/1, 80←1 pad-fills) | 15 | `dst_count` |
| fixed ← varying | 2 | `dst_count` (12) |
| fixed ← literal | 7 | `dst_count`, incl. the 5 `min(13, 30)` truncations (ATTACK/CAVE_ATTACK/DEFEND: the compiler emitted a constant-vs-constant min diamond, always the same arm) |

Consequence for the executor: for every varying destination in this
population `dst_count = min(n, len(piece))` — the transferred count never
exceeds the source, so a varying assignment **never pads**; a fixed
assignment transfers exactly `n` bytes (pad with 0x20 when `len < n`,
truncate when `len > n`).

---

## 3. Grammar draft — IR.md ir 5 (Phase A.3)

Additions to §5.1 (everything else unchanged; ir 4 files are refused by
the ir 5 loader, as ir 4 refused ir 3):

    located := [@<addr>, <n>]                  ; fixed CHAR(n): n bytes at BYTE address <addr>
             | [@<addr>, <n> varying]          ; CHAR(n) VARYING: length word at WORD address <addr>,
                                               ;   data at <addr>+1 word, capacity n
             | [@<addr>, varying]              ; a varying READ whose declared capacity is not
                                               ;   derivable at the site (sources only; never an lvalue)
    piece   := "<text>"@0xW:b                  ; literal: text = the bytes (escaped), 0xW:b their
                                               ;   byte address in the image; len = byte count
             | located                         ; a located read (fixed: n bytes; varying: len = M16[addr])
    <addr>  := any pure expr (§5.1 primary/chain); for a fixed string a byte-pointer VALUE
               (bp(...), 0xW:b, acN holding one, or such a value + a byte count); for a varying
               string a word address, wrapped into the block's segment exactly like an M16 index
    <n>     := a non-negative decimal/hex constant (32 K limit: n < 32768)

    stmt    += located = piece                 ; PL/I assignment (§2.4 semantics; residues §4)
             | ac1 = cmp(piece, piece)         ; WCMP: -1/0/+1 in ac1; ac0/ac2/ac3 as §4 (ruled)
             | words(@<addr>, <k>) = words(@<addr>, <k>)   ; WBLM: k words, sequential; <k> a
                                               ;   constant or a pure expr (acN holding the count)

    literal escaping: printable 0x20..0x7E except `"` `\` `;` are literal; everything else is
    \xHH (two upper-case hex digits). `;` MUST be escaped (comments strip first, §2). The loader
    unescapes and takes the byte count as the literal's length.

Loader REFUSES: a piece with `n` ≥ 32768; a located whose `n` is not a
constant; a literal without `@0xW:b` or whose address is not in the
code segment of the block; `[@a, varying]` as an lvalue; a `cmp` whose
lvalue is not `ac1`; `words()` with different `k` on the two sides; any
of these statements in an `ir 4` file (version line); anything else
unrecognised (as always).

Executor (IRExec, calling the P30 library, never string semantics of
its own):

- `located = piece`: evaluate the piece FIRST (address; for a varying
  piece read its length word through Memory), then, for a varying
  destination, store `min(n, len)` in the length word and copy
  `min(n, len)` bytes; for a fixed destination copy `n` bytes with the
  library's pad/truncate. Then set the residues (§4). Order matters only
  for `v = v` (0 sites here; recorded so P32 keeps it).
- `ac1 = cmp(a, b)`: library compare of a (string 1 = the master's
  ac3/ac1) against b (string 2 = ac2/ac0), blank-padded; ac1 = −1/0/+1;
  residues §4 including B-4.
- `words(@d, k) = words(@s, k)`: library block move, word by word,
  ascending; residues §4. (12/12 sites are self-overlapping fills — the
  sequential order is the semantics.)
- **Literal verification (ruled, lazy):** at the FIRST execution of each
  literal-bearing statement the executor reads the literal's bytes from
  the image through `Memory::read_byte` (the normal path, demand-paging
  untouched) and compares them with the statement's text; a mismatch
  throws (`IR literal mismatch [block, stmt] at 0xW:b`). Every later
  execution uses the address. Cost: once per statement.
- Faults (loud): a segment crossing inside a copy/compare/move throws
  from the library exactly as EagleSpecial does (G-3); a `k`/`n` that
  is negative after evaluation throws (no descending forms in this
  population); a `[@a, varying]` whose length word is ≥ 32 K throws.

Not in ir 5 (P32/P33): `substr(piece, i, n)`, `char(x)`, `append()`,
`p@b`, `release`, `len()`, and the pure `==` on pieces — the readable
layer (ruled). `fill()` — see open question O2.

Two worked lines from the ledger (final form; the `<-` echo lists the
folded pcs):

    [@wp(ac3, 12), 27 varying] = "Cannot set shared partition"@0x7015BD9B:0   ; WCMV <- 7015BE5F 7015BE61 7015BE62 7015BE64 7015BE66
    [@((sx16(M16[0x70000216]) * 686) + M32[0x70000210] + 23), 30 varying] = [@0x7000021C, varying]   ; ATTACK 7015F75E, min shape, join of diamond 7015F746
    ac1 = cmp([@0x7000021C, varying], "GOD"@0x7015BF5D:0)                    ; WCMP 7015C0AD <- 7015C0A3 7015C0A5 7015C0A8 7015C0AA
    words(@wp(ac3, 18), 10) = words(@wp(ac3, 16), 10)                         ; WBLM 70175F1A, fill: src = dst - 2

---

## 4. Residue plan (Phase A.4) — every statement sets ac0–ac3 and c as the instruction would

Library functions (P30's names per its plan gate; signatures here are
what P31 will call, to be reconciled at P30's landing):

| statement | transferred `t` | ac0 | ac1 | ac2 | ac3 | c | ovr |
|---|---|---|---|---|---|---|---|
| `[@a, n] = piece` (fixed) | n (pad/trunc) | 0 | `len − min(n, len)` | `a + n` | `src + min(n, len)` | `len > n` | untouched (B-1) |
| `[@a, n varying] = piece` | min(n, len) | 0 | `len − t` | `data(a) + t` | `src + t` | `len > n` | untouched |
| `ac1 = cmp(s1, s2)` | — | s2 bytes left at the stop point (one fewer on a mismatch — P2/B-4) | −1/0/+1 | s2 pointer at the stop (one PAST the differing byte — P1/B-4) | s1 pointer at the stop | unchanged | untouched |
| `words(@d, k) = words(@s, k)` | k | unchanged | 0 | `s + k` | `d + k` | unchanged | untouched |

`residues_after_copy(machine, dst_bp, dst_count, src_bp, src_count)` is
called with `dst_count = n` (fixed) or `min(n, len)` (varying) — the
exact ac0 the master had — so the library's formula is the emulator's
loop invariant (`ac0 = 0; ac1 = src_count remaining; ac2/ac3 one past
the last byte written/fetched; c = src_count != 0`) and nothing in
IRExec computes a residue. The carry after a copy is set although never
read (F1); `c` is UNCHANGED after cmp/words (the emulator's arms do not
touch it — and the kept, non-folded producers before those statements
keep their own flag writes, which is why the fold run excludes them).
B-4 is reproduced by calling the library's compare (P30 tests it against
EagleSpecial); the 17 `WSNE`-consumed sites that can mismatch are where
it is live on the strict surface — never read by the game, always
compared by lockstep.

---

## 5. The min diamonds (Phase A.2) — censused, kept, synclist delta 0

`find_diamond` locates, for every phi-count site, the last two-successor
block in its window whose one successor is a single-`WMOV` block that
falls into the other (the join). **40 diamonds, 40 sites, 40/40 with
`preds(join) == {guard, interior}` and `preds(interior) == {guard}`** —
no outside predecessor anywhere (the prompt's STOP condition never
fires). Per diamond: guard pc, skip mnemonic (WSGE ×N / WSLE ×N),
interior pc + its WMOV, join pc, the site(s), verdicts — the table is in
the ledger's summary.

Ruled form: guard and interior lower as today (pure: `ac0 = c; goto
[join, interior] (ac1 >=s ac0)` / `ac0 = ac1; goto [join] 0` — 0 embeds
already), the statement lives in the join with `n` = the guard's
constant and computes the min itself; at the join entry both engines
hold `ac0 = min` (the master computed it, the clone computed it the same
way), and after the statement the residues agree. **Sync list: unchanged
(quest.synclist.p27, 13,510).** For the record the flat-graph world will
want: option-B absorption (interior ∪ join into the guard) would delist
**80** entries (2 per diamond) → 13,430; the predecessor evidence for it
is in the ledger now. (15 of the 40 diamonds feed EMIT sites — the 10
-PAD joins and the 5 `min(13, 30)` LIT-FIXED joins; 25 feed sites P32
will take.)

Note for the emitter: in a diamond whose loads are in the guard block
(the 5 ASSIGN-LIT-FIXED `min(13, 30)` sites) the join block is
`WCMV` + continuation, so the statement takes the register form (§2.1).

---

## 6. Liveness and battery plan (Phase A.6)

Coverage of the 652 EMIT sites' blocks by the 042 battery's IRExec
first-execution lines (10 IR legs; the union of live blocks):

| kind | EMIT | blocks live in ≥1 042 leg |
|---|---:|---:|
| `[@v, n varying] = "lit"` | 528 | 39 (every message set-up on the login/first-turn paths; QUEST, LOGON, START_TURN, DISPLAY_*) |
| `[@v, n varying] = [@fixed, n]` (constant copy-out) | 48 | 5 |
| min shape (varying ← varying) | 10 | 3 |
| `[@fixed, n] = …` | 23 | 1 |
| `ac1 = cmp(…)` | 31 | 10 (QUEST 3 — the `''` blank tests and "GOD"; DISPLAY_SCREEN 2, DISPLAY_INVENTORY 2, TERRITORY 3) |
| `words()` | 12 | 5 (LOGON ×2 and QUEST: wide smears; GET_QUEST and START_TURN: word smears) |
| total | 652 | **63** |

Every statement kind and both smear widths are live; ASSIGN-STR-FIXED
(LIST_PLAYERS name padding, INIT_OBJ_TBL) is not reached by the scripted
legs and is carried on census classification, as P28 carried LNDO/LDSP.
The `abort`/`derr` legs reach QUEST's 3 compares and the LOGON fills, so
even the short legs exercise cmp and words. The k1fo leg (K=1 strict)
reaches 1,346 blocks: the K=1 gates in Phase B see every residue at
every block entry on that path.

Task 044 (034 template via bin/task_source.sh, JOBS=3, source
p31-located-strings): 040/042's 15 legs unchanged + verdict lines:

- `embeds_book == 1670`, `embeds_stock == 3549` (2,322 − 652 / 4,201 −
  652: all 652 EMIT pcs are `@` lines in both current artifacts, checked)
  — the landing bar's "embeds ≤ prediction";
- statement counts in each artifact: `] = "` (literal assignments) 534
  — 528 varying + 6 fixed; the seventh LIT-FIXED, START_TURN 70178D66,
  is `[@bp(ac3, 60), 1] = [@0x7017804D:0, 0]`, the `1 ← ''` blank fill —
  `varying] =` 586, `ac1 = cmp(` 31, `words(@` 12 — book and stock alike
  (string statements are mode-independent);
- ledger: `p31.ledger` EMIT 652 / REFUSE 124, and lower.py's
  `--strings-census` ledger must list the same 652 pcs as emitted with
  the same IR text (byte compare of the IR lines — the census tool is
  the oracle for the emitter);
- coverage: string statements first-executed (from the IRExec lines,
  block → statement kinds via p31.tsv) with `cmp`, `words`, lit→varying
  and the min shape REQUIRED live in the book and stock play legs;
- `IR literal mismatch` must not appear; 0 div; 15/15 green; sync list
  13,510 unchanged; the P27/P28 lines as in 042.

Landing bar (prompt A.6): 15/15 green, 0 div, embeds ≤ 1,670/3,549.

---

## 7. Open questions for the gate (small; defaults stated)

- **O1 — `[@a, varying]` (capacity-less read).** 50 EMIT operands are
  varying reads whose declared capacity the site does not reveal
  (IN_BUFFER, by-reference arguments, record fields). Proposed: the
  read form carries no capacity (it is irrelevant to a read); the
  design's `[@a, n varying]` stays the write form. Default: as
  proposed.
- **O2 — `fill()`.** All 12 WBLMs are smears. Proposed: emit
  `words(@d, k) = words(@s, k)` for all 12 with the `; fill: src = dst -
  k` comment; `fill(@d, k, v)` joins the readable layer with `==`.
  Default: as proposed (one statement kind, one library call, no
  emitter proof obligation about the wide below `d`).
- **O3 — `substr` / `bp + expr`.** Not needed in this population: the
  4 P31 SUBSTR sites are refused for other reasons (P32-SUBSTR /
  P32-CAPACITY) and every emitted address is a plain expression. The
  byte-offset form is therefore P32's question, with one data point:
  `(bp(M32[0x70000210], 1) + (sx16(M16[wp(ac3, 2)]) + 0x3DF76))`
  (QUEST 7015C259) renders fine as a pure chain, so `substr` can be
  sugar over `bp + expr` rather than a primitive.
- **O4 — register form.** 7 sites emit with the setup kept and the
  operand registers named (`[@ac2, 13]`, `words(@ac3, 90) =
  words(@ac2, 90)`). Alternative: refuse them (645 EMIT). Default: emit.
- **O5 — the 32 K limit** is a grammar check on constants only (largest
  here: 5,758 words for `words()`, 2,092 bytes for a copy (DEFEND
  70165EF8), 268 bytes for a literal); a runtime length word ≥ 32 K in a
  varying read throws. Default: yes.

---

## 8. Slices for Phase B (after P30 on main)

`--strings-slice 1`: the 528 lit→varying (+ 7 lit→fixed). `2`: the
str→varying/fixed shapes incl. the 10 min-shape joins (74). `3`: cmp (31)
and words (12). Slice 0 = the P28 artifacts byte-for-byte (regression
check, as `--rt-slice 0` was). `--strings-census` writes lower.py's
per-site ledger in p31.tsv's columns for the byte compare. Each slice
behind K=1 book + stock gates locally (k1fo, k1play), then 044.

---

## 9. Rulings at the gate (user, Sep 6) and the StringsDesign wording they imply

O1 yes (`[@a, varying]` is the capacity-less READ form; capacity stays
mandatory on the write side); O2 yes (12 fills as `words()` + comment;
`fill()` is the readable layer's rewrite); O3 noted (no `substr` in
P31); O4 emit the register form (§2.1 of the design: an address is any
expression the IR has, and a register is one); O5 keep 32 K as a refuse
threshold. Refusal buckets and the 1,670 / 3,549 bar accepted.

Proposed StringsDesign edits (for the integrator to fold in):

- **§2.1** add after the two located forms: "`[@a, varying]` — a located
  varying READ whose declared capacity the site does not reveal (length
  from the length word); read side only, never an lvalue. An address
  `a` may be a register holding the pointer (`[@ac2, 13]`) when the
  setup is kept rather than folded."
- **§2.3** replace `words(@dst, k) = words(@src, k)` / `fill(...)` lines
  with: "`words(@dst, k) = words(@src, k)` — WBLM, k words in sequential
  ascending order (the semantics the 12 self-overlapping sites rely on);
  the emitter notes `src = dst − 1` / `− 2` in the comment. `fill(@dst,
  k, v)` is the readable layer's rewrite of that shape, not an IR
  statement." Add: "`ac1 = cmp(<piece>, <piece>)` — WCMP as a root
  statement: −1/0/+1 in ac1, the §3 residues in ac0/ac2/ac3 (B-4
  reproduced). The pure `<piece> == <piece>` of the expressions list is
  the readable layer's rewrite of `cmp(...) == 0`."
- **§7** `WBLM copy / fill | 6 / 6` → `WBLM fill | 12 (6 wide smears, 6
  word smears; none is a record copy — P31 Census §1.3)`;
  `WCMP equality | 40 | if (a == b)` → `ac1 = cmp(a, b); goto … (ac1 ==
  0)`.
- **§1.3/§1.4** counts: 11 groups consumed by ?WRITE_SCREEN, 8 copied
  out (§1.5 above).
