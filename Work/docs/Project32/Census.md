# Project 32 — append chains, Phase A: census + grammar (PLAN GATE)

Session Sep 6 2026, solo. Phase A only: `tools/string_sites.py`
extended (`--p32`, `--p32-tsv`; the evaluator correction of §1), the
P31 artifacts regenerated under that correction (§1.1, landed first as
its own commit), `docs/Project32/` written. No lower.py, no IRExec, no
IR.md change yet. Branch `p32-append-chains`.

TREE VINTAGE: main @ `c88c0ff` (P31/P33-A/P34 merged at 6c3c28c; the
Follow.java LJSR-I.GOTO fix 09f6593 — blocks.split ba30a841…, tags
010ae3ea…; the StringsDesign/prompt ?UNSIGNED_TO_CHAR correction
08afd77/c88c0ff). Provenance verified before starting on 6c3c28c (all
12 P31 prefixes) and again after 09f6593.

Tool (from Work/emulation/tools; runtime **3.4 s** on this box, was 2.1–2.7):

    python3 string_sites.py --dis ../../../Disassembled/quest.dis --blocks ../quest.blocks.split \
      --mem ../../../Disassembled/quest.mem --symbols ../../../Disassembled/quest.symbols \
      --sites ../../docs/Project31/sites.txt --strings /tmp/quest.strings \
      --census ../../docs/Project31/census_raw.txt \
      --p31 ../../docs/Project31/p31.ledger --p31-tsv ../../docs/Project31/p31.tsv \
      --p32 ../../docs/Project32/p32.ledger --p32-tsv ../../docs/Project32/p32.tsv

Outputs of record: `p32.ledger` (one record per candidate site: window,
the four operands as the evaluator sees them, per-operand mode
expression/register, the fold set, every demotion with its reason, the
chain the site belongs to, the exact IR line, form, slice; then the
chain table and the summary), `p32.tsv` (lower.py's input: p31.tsv's
columns + `form`, `slice`, `chain`). `quest.strings` regenerates
byte-identical to docs/Project29/quest.strings.

---

## 1. Findings

### 1.1 Tool defect (P31): tracked frame slots survived calls that wrote them — 17 P31 statements were wrong

`string_sites.py`'s evaluator kept every tracked frame-slot value across
a call (`clobber_all` dropped statics only: "compiler temporaries are
never passed"). But `?UNSIGNED_TO_CHAR` takes its destination in ac2
(`XLEF 2,[ac3+d]`) and WRITES a CHAR VARYING there — length word +
digits (runtime/unsigned_to_char.cpp:141–146). Where the compiler had
stored a constant length word in that slot earlier (a previous message's
`NLDAI n; XNSTA`), the tool still "knew" the old value after the call and
P31 rendered a constant count where the master reads the digit count:

- ATTACK 7015E990, CAVE_ATTACK 7016437E, DEFEND 70165EF8: emitted
  `[@wp(ac3, 112), 71 varying] = [@bp(ac3, 148), 71]`; the master's count
  is `N[fp+14] + 41`, `N[fp+14]` being the digit count the call at
  7015E95A just wrote (7015E95E `XNLDA 0,[ac3+0xE]`).
- LIST_PLAYERS ×14 (7016EF0D … 7016F18C): emitted `[@bp(ac3, 48), 12] =
  [@bp(ac3, 1046), 6]` (also 8, 9); the src count is `N[fp+522]` reloaded
  after `?UNSIGNED_TO_CHAR` wrote fp+522.

Effect when executed: wrong bytes in the message / name column and wrong
ac1/ac3 residues → a divergence. Not caught by 044/045: none of the 17
blocks executes in any leg (0 first-execution lines). METHOD §14: stop,
fix the map, regenerate, diff-audit.

**Correction (user rulings, Sep 6).** (a) At a call the evaluator drops
every tracked slot whose address the callee was handed — every word EA
pushed by XPEF/LPEF/XPEFB/LPEFB since the last call, and every word
address held in ac0..ac2 at the call (the `XLEF 2` register argument);
such a slot is tagged *callee-written* so a later load of it is
recognised as the call's result. (b) A tracked value that is reloaded
after ANY call ran between its store and the load is *stale*; the P31
renderer refuses it, the P32 renderer spells the located read the master
performed instead. Every value now carries its memory provenance (`Src`:
address, load pc, store pc, stale, callee) for both renderers. (c) The
merge state (`Merged`) carries the same provenance where every
predecessor agrees.

**Effect on P31 (regenerated, this commit): 649 EMIT / 127 REFUSE (was
652 / 124).** The 3 varying copy-outs refuse (bucket P32-COPYOUT — they
are chain copy-outs; the reason names the callee-written slot). The 14
LIST_PLAYERS sites stay EMIT with the correct text
`[@bp(ac3, 48), 12] = [@wp(ac3, 522), varying]` — the located read of the
string the call wrote (ruling: refusing a correct statement would be
worse). Two OBSERVE WCMPs (70172DA8, 70172E7A) move from P32-SUBSTR to
P32-CALLRESULT (a stale slot value in their count). Nothing else
changes: sites.txt / census_raw.txt idiom totals identical; strings
ledger 649 emitted / 0 refused; artifacts 649 statements, embeds
**1,673 book / 3,552 stock** (the 17 sites' diff is the whole diff
against 6c3c28c's artifacts, plus the `blocks` provenance line moved by
09f6593). K=1 gate on the regenerated artifacts: book k1fo 0 div,
308,923 pairs, 0 block mismatch, end clean, 49 string first-executions,
0 literal mismatches; stock k1fo-st: see REPORT_worklog (run after the
book leg on this 1-core box).

Also regenerated in the same commit: docs/Project27/assumed-foldable.txt
(content identical, 2,271 clusters; its tags/blocks sha header follows
09f6593 — lower.py refuses the old header).

### 1.2 `?UNSIGNED_TO_CHAR` does not return in ac0 (design-vs-reality; corrected on main 08afd77/c88c0ff)

The call writes its varying at the ac2 word address and returns ac0–ac2
unchanged like every LCALL (WRTN restores the WSAVS image; native:
`RTBridge::native_return`). The 112 "CALLRESULT" counts are the caller's
own running total, kept live in a register across the call (ac1, or
ac0 — the OP_EDIT chains), plus the length word it reloads. So the
piece is a located varying read `[@wp(ac3, k), varying]` and the count's
leaf is the post-call block-entry register. RTConventions.md was right
all along ("none writes ac1/ac2 back"; ?UNSIGNED_TO_CHAR returns
nothing); the error was StringsDesign §1.4/§1.8/§2.2/§7 and the P32
prompt, both corrected. Prompt item 3 ("confirm ac0 is unmodified") is
therefore replaced by the leaf check the renderer performs on whichever
register carries the total (§2.1).

### 1.3 The compiler concatenates PAIRWISE — every scratch chain is 1 or 2 pieces, none crosses a block (fact of record)

`a ‖ b ‖ c ‖ d` is compiled as scratch1 = a ‖ b; scratch2 = scratch1 ‖ c;
scratch3 = scratch2 ‖ d; result = scratch3 — each stage a fresh
`CHAR(n)` frame local receiving an exact copy of the previous stage as
its first piece (the census's COPY-STR-EXACT sites with a sum count) and
one continuation piece at the `ac2` cursor. Over all 211 buffers / 446
sequences the longest sequence is **2 pieces** and **no sequence spans a
block**. HELP's "3-piece chain" (7016D5E3..7016D602) is two such stages:
164 → 164 ‖ 150 into fp+0x150, then the 314-byte copy-out into the
varying at fp+0x146. The partial totals are precomputed by the compiler
into frame slots and reloaded as each stage's count (ATTACK 7015D9A7:
`XWSTA 0,[ac3+0x2E]` … `[ac3+0x46]`).

### 1.4 Copy-out totals: 335 verified, 0 mismatch

For every sequence the appended total (the sum of the piece counts,
plus one per WSTB byte stored through the chain's cursor between and
after the pieces) is compared with each copy-out's count: **335 MATCH,
0 MISMATCH**. 19 of the matches need the ruling that runtime callees
preserve ac1/ac2 (and ac0 for the value-less `?UNSIGNED_TO_CHAR` /
`?WRITE_SCREEN`) — the copy-out's total carries a register across the
call. Capacity-bounded copy-outs (`SUBSTR(scratch, 1, min(total, cap))`:
dst_count = the min-diamond's value, src_count = the capacity) verify
against the phi's arm. Readers matched through a join (the min diamond
or an IF/ELSE merge) must reach the same last piece on every
predecessor path. The 8 readers left UNMATCHED are chains whose writer
is on one branch only (LIST_PLAYERS 7016EE58: the title piece written
only when the player has a title; the ELSE path reads stale scratch —
the master's behaviour, reproduced exactly by the register form). Not
copy-outs and not counted: 1 WCMP reader, 13 varying reads of in-place
builds, 6 SUBSTR reads, 2 reads of buffers a P31 assignment writes.

### 1.5 Tail splits are bounded concatenation: `src_count` carries the remaining room

The 17 "tail split" sites are `v = SUBSTR(lit ‖ digits, 1, 30)` into a
CHAR(30) VARYING: piece 1 is WCMV(dst_count = min(13, 30), src_count =
30) — the SOURCE count is the buffer capacity, not the literal's length
— leaving ac1 = 30 − 13 = the room; piece 2 is WCMV(min(len, ac1),
ac1); then `30 − ac1` is stored as the length word. So the "in-place
length store" is capacity − remaining room by construction (16 such,
none equal to a piece sum — expected). P31's five `min(13, 30)`
LIT-FIXED sites are exact for the same reason: the emitted literal is 30
bytes (the source count), the library copies 13 and leaves ac1 = 17 —
as the master. The register form `[@ac2, ac0] = [@ac3, ac1]` is the
literal truth of piece 2.

### 1.6 Two more tool corrections recorded

- Merged states now carry provenance; the chain matcher walks
  predecessors recursively (depth 4) so copy-outs in diamond joins and
  post-IF merges are matched.
- `bp(e, d)` displacements that fold a negative base constant are now
  printed signed (`-0x1FD60A04`, 38 lines); the loader parses `-0x…`
  (IRExec.cpp:221). Previously they would have printed as an unsigned
  wrap.

---

## 2. Site list (Phase A.1) — docs/Project32/p32.ledger

Population: every WCMV P31 does not emit whose operands are not WMSP
temps, plus the WCMPs P31 refused: **944 sites** in 275 blocks, 60
routines. P33-B's population (temp operand): 96 sites in 37 blocks,
listed at the end of the ledger. 637 (P31 WCMV/WCMP EMIT) + 944 + 96 =
1,677 = 1,637 WCMV + 40 WCMP. Census idiom counts reproduced exactly.

| idiom | sites | EMIT | slice |
|---|---:|---:|---|
| CONCAT-PIECE / +SUBSTR / +CALLRESULT | 319 / 3 / 30 | all | 4 / 5 / 6 |
| COPY-LIT-EXACT / +SUBSTR | 151 / 10 | all | 4 / 5 |
| COPY-STR-EXACT / +SUBSTR / +CALLRESULT | 225 / 5 / 38 | all | 4 / 5 / 6 |
| ASSIGN-STR-VARYING / +CALLRESULT / -PAD / -PAD+CALLRESULT / -PAD+SUBSTR | 91 / 33 / 11 / 2 / 1 | all | 5 / 6 / 5 / 6 / 5 |
| ASSIGN-STR-MIN / +CALLRESULT | 7 / 2 | all | 5 / 6 |
| ASSIGN-STR-FIXED / +SUBSTR, ASSIGN-LIT-FIXED+SUBSTR, ASSIGN-LIT-VARYING-PAD / +CALLRESULT | 2 / 2 / 1 / 1 / 1 | all | 5 / 5 / 5 / 5 / 6 |
| WCMP (P31's 9 refusals) | 9 | all | 6 |
| **all** | **944** | **944** | 4: 677, 5: 134, 6: 133 |

**No refusals.** The emitter is total: every operand is rendered either
as an expression over block-entry leaves (P31's rules extended, §2.1) or
as the operand register itself, and a register operand is exact by
construction. Forms: **448 pure expression, 415 mixed, 81 full register
form** (`[@ac2, ac0] = [@ac3, ac1]`). Ruling (user, Sep 6): no
readability floor — emit the ground form; readability is the P34
layer's job.

Per-operand modes (ptr/cnt): dst expr/expr 447, expr/reg 50, reg/expr
311, reg/reg 127; src expr/expr 765, expr/reg 68, reg/expr 20, reg/reg
82. 90 destinations take the varying form (length store absorbed); 323
sources are literals with text; 70 sources are `[@…, varying]` reads of
a callee-written string.

### 2.1 How a site is rendered (mechanised in the tool)

Each of the four operands starts in expression mode and is demoted to
register mode when its expression form fails, iterating to a fixpoint
(≤ 8 rounds; 385 demotions recorded with their reason in the ledger):

- **Expression form** = P31's rules (`fp` → `ac3`, `acN@entry` → `acN`,
  `N[a]`/`W[a]` → `sx16(M16[a])`/`M32[a]`, `bp(fp+k)` → `bp(ac3, 2k)`,
  literal text from quest.mem) extended by: a *stale* tracked value →
  the located read `sx16(M16[a])`/`M32[a]` (rule (b)); a call-preserved
  register (`unk(LCALL…)#acN`) → `acN` with the leaf requirement that
  acN still holds the block-entry value; a residue (`wcmv.src_left`,
  `dst_end`, `src_end`) → the register that carries it (ac1/ac2/ac3),
  valid when its last writer is that WCMV; a phi (the min diamond) is
  register mode by construction.
- **Continuation destinations are always `@ac2`** (the append cursor —
  the design's §5.1 `append` in the register spelling the prompt
  leaned to); the count keeps its own mode (`[@ac2, 150] = "…"`,
  `[@ac2, sx16(M16[wp(ac3, 10)])] = [@wp(ac3, 10), varying]`).
- **Fold set** = the contiguous run of pure producers of the
  expression-mode operands ending at the op, TRUNCATED at the first
  producer that also writes a register-mode operand (the truncated
  producers stay statements); the P31 fold-consistency check applies.
- **Leaf checks** (P32 versions): register leaves as P31, plus the
  residue/call-preserved cases; memory leaves: no store to the address
  AFTER the load the statement re-reads (P31 refused any store in the
  window, which is too strong once loads are re-read), and NO CALL
  between that load and the statement (rule (b) — the callee may write
  the slot). A failed leaf demotes the operand that reads it.
- **Varying destination** (`[@A, n varying]`, XNSTA absorbed) only when
  dst and count are expressions and `dst_count == src_count`
  syntactically — then the library's `min(len, n)` equals the master's
  stored length. Otherwise the XNSTA stays a statement and the copy is
  the fixed form at the data address (`[@bp(ac3, 0x166), ac0] =
  [@bp(ac3, 252), ac1]`), which is exact for pad, truncate and
  bounded copies alike.

Why operands end up in register form (the 385 demotions): ac3 no longer
holds fp at the statement (the source-pointer setup uses ac3 as a
scratch base — `XWLDA 3,[ac3+k]; LLEFB 3,@[ac3+…]` — and the producer
is kept because a store or a WMUL splits the run): 142; the count's
carrying register (ac1/ac0) overwritten by the previous piece's WCMV
before the statement: 25; unknown values (NNEG/NADI/MOV.# results,
CVWN of a call result not stored anywhere readable): 68; opaque ops in
an address: 54; a store after the load: 29; a call between load and
statement: 6; phi: 3. None of these is a guess — the register is what
the master computed.

### 2.2 Worked lines (final form; the ledger has every one)

    [@bp(ac3, 116), 26] = [@0x70170105:0, "\x0BYour maximum strength is "]          ; first piece
    [@ac2, sx16(M16[wp(ac3, 10)])] = [@wp(ac3, 10), varying]                          ; continuation, varying piece
    [@bp(ac3, 0x164), M32[wp(ac3, 24)]] = [@bp(ac3, 116), M32[wp(ac3, 24)]]           ; next stage: exact copy, precomputed total
    [@wp(ac3, 112), (sx16(M16[wp(ac3, 14)]) + 41) varying] = [@bp(ac3, 148), (sx16(M16[wp(ac3, 14)]) + 41)]   ; copy-out (ATTACK 7015E990, the ex-P31 site)
    [@bp(ac3, 0x166), ac0] = [@bp(ac3, 252), ac1]                                     ; copy-out whose total rides ac1 across ?UNSIGNED_TO_CHAR (XNSTA kept)
    [@ac2, ac0] = [@ac3, ac1]                                                         ; tail-split piece 2 (ATTACK 7015D94A)
    [@bp(M32[wp(ac3, -16)], 2), ac0] = [@0x7017CCE1:0, "Wilderness"]                  ; by-reference varying, min count
    ac1 = cmp([@bp(ac3, 38), ac1], [@0x70000C1E, varying])                            ; DISPLAY_SCREEN 7016713B: str1 count = the residue
    ac1 = cmp([@0x7000021C, varying], [@0x7017804D:0, ac1])                           ; START_TURN 70178DBE: READ_IN's result as the count

---

## 3. Grammar draft — IR.md ir 5 §5.8 (Phase A.6)

One production changes; no new statement kind; **no version bump** (no
production is added — `<n>` widens):

    <n>     := a constant 0..32767 | any pure expr (§5.1)      ; was: a constant only

Semantics unchanged: `[@d, n] = [@s, m]` IS WCMV(dst_count = n,
src_count = m, dst = d, src = s) through the library's `copy`; for a
fixed piece `m` is its byte count (constant or expression), for a
varying piece the length word, for a literal the text's byte count.
`n`/`m` may be a register (`ac0`, `ac1`), a length-word read
(`sx16(M16[a])`), a sum (`sx16(M16[a]) + sx16(M16[b]) + 24`), or a
32-bit word read (`M32[wp(ac3, 24)]` — the compiler's precomputed
totals are wide). A negative bp displacement is written `-0x…` (the
loader parses it, IRExec.cpp:221; 38 lines).

No `len()`, no `append()`, no `substr()`, no `min()`: `len(piece)` is
the count expression itself, `append` is the `@ac2` spelling, SUBSTR
pieces are a byte-offset address (`(bp(ac3, 40) + ac1)` — P31's O3 data
point holds: sugar over `bp + expr`), min shapes leave their value in
the register the diamond writes. All four are the readable layer's
rewrites (P34). The prompt's "`len()`, which ir 5 added" was an error.

Loader (IRExec): `<n>` parsed with `p.expr()` instead of `p.primary()`;
the 32 K refuse stays on constants; `check_treads` extended to the count
expressions; a count that evaluates negative is a fault only where the
library faults (a descending copy is legal WCMV — none in this
population, but the 32 K limit on a runtime count is NOT enforced, as
F-B1 ruled for length words). Executor: evaluate the source piece
(address, count), then the destination address and count, then
`copy`/`assign_varying`/`compare` exactly as ir 5 — nothing new in the
library.

Refuse list at the emitter: none in this population. lower.py
re-validates every fold pc as for P31 (in the site's block, before the
site, pure, contiguous but for LDAFP) and byte-compares its text with
p32.tsv (`--strings-census`).

---

## 4. Residue plan (Phase A.7)

Every P32 statement is `copy` or `compare` with the master's own four
operands, so `residues_after_copy(dst, n, src, m)` /
`residues_after_compare` set ac0–ac3 and c exactly as ir 5 does — no new
residue rule. The tail-split piece consumes ac1 from the previous
statement's residue: in the emitted text the previous statement precedes
the diamond (guard `goto … (ac1 >=s ac0)` / interior `ac0 = ac1`) and
the join holds `[@ac2, ac0] = [@ac3, ac1]`; ordering is the block order
the master has. The CALLRESULT counts read the post-call block-entry
register; the leaf check proves no kept statement rewrites it before the
copy.

---

## 5. Liveness and battery plan (Phase A.8)

Coverage of the 944 sites' blocks by the 044b/045b legs' first-execution
lines: **67 sites / 37 blocks of 275** (slice 4: 34, 5: 18, 6: 15; forms:
expr 32, mixed 29, register 6). Live kinds: first pieces (13 literal,
11 exact copies), continuations (14 + 4 CALLRESULT), copy-outs
(9 varying + 1 PAD + 1 MIN + 2 PAD+CALLRESULT + 1 MIN+CALLRESULT),
SUBSTR pieces (2 + 1 + 1 + 1), 2 of the 9 WCMPs. HELP's chain (69 sites)
is NOT reached by any leg. Proposed (gate decision): grow `play` per the
Project 13 ruling with `send("H", 5); send("1\r", 8); send("0\r", 6)` —
`H` dispatches HELP (70179 4B1), topic 1 executes the 164 ‖ 150 → 314
chain and its ?WRITE_SCREEN, 0 exits; ~20 s on the long pole.

Task 046 = 044's 15 legs + verdict lines: `embeds_book/stock` ≤ 1,673 −
944 = **729** / 3,552 − 944 = **2,608** (bar: "≤ prediction"); string
statements 649 + 944 = 1,593 in both artifacts; lower.py's strings
ledger == p31.tsv ∪ p32.tsv EMIT sets, text byte-equal; chains complete
(no site of a sequence refused while another is emitted — vacuous at
0 refusals, kept as a line); first-execution coverage by slice and form
from the IRExec lines with slice 4, 5 and 6 each > 0 and HELP's chain
block 7016D5E3 live if the driver step is accepted; `IR literal
mismatch` = 0; 0 div; 15/15; sync list 13,510 unchanged (no block added
or removed — the diamonds stay); the P27/P28/P31 lines as in 044.
K=1 book + stock gates locally per slice.

---

## 6. Open questions for the gate (defaults stated)

- **O1 — driver step for HELP.** As §5; default: add it to `play`.
- **O2 — the 8 conditional-writer chains.** The reader copies whatever
  the buffer holds (stale on the ELSE path — the master does the same);
  the register/expression form is exact. Default: emit, note in the
  ledger; no per-site treatment.
- **O3 — P32-CALLRESULT bucket.** The three ex-P31 varying copy-outs sit
  in P32-COPYOUT (structural bucket) with the callee-written slot in the
  reason; the user asked for P32-CALLRESULT. Default: keep the structural
  bucket (the P32 emitter takes both the same way); rename on request.
- **O4 — slices.** 4 = first pieces + continuations (677), 5 = copy-outs,
  bounded copies, SUBSTR pieces (134), 6 = CALLRESULT counts, residue
  counts (tail splits), the 9 WCMPs (133). Slice 3 = the P31 artifacts
  byte for byte. Default: as listed.
