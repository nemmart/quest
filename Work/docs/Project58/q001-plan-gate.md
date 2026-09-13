# P58 q001 — plan gate (Sep 13 2026)

Worker: Project 58. Prompt: `docs/Project58/PROMPT.md`. STOPPED here; waiting
for `a001`.

Everything below was measured by `compiler/countflow.py` (new file, the Stage A
tool) over `emulation/quest.ir2.book`; the per-operand ledger it writes is
`docs/Project58/sites.tsv` (3,366 rows, one per count operand of the 1,689
sites) and its full console output is `docs/Project58/countflow.out`. Nothing
executed, nothing outside `docs/Project58/` and `compiler/countflow.py` was
touched.

---

## 1. `emulation/tools/dataflow.py`, and the Stage A tool

**What `dataflow.py` does.** 306 lines. It parses the RAW basic-block format
(`quest.blocks.split`, not the IR), builds a CFG from the `n`/`c`/`s`/`j`/`u`
terminators, and provides one generic forward-dataflow worklist with a single
client: a boolean "is ac3 still the frame pointer" analysis
(`ac3_kills_frame` from `ir_convert.py`). It knows nothing about string
statements, registers other than ac3, or the IR. Its worklist has a hard cap
of `3 × blocks` iterations (`max_iterations`) and returns whatever it has when
the cap is hit — it can stop before the fixpoint and say nothing about it.

**Decision: REPLACE**, keeping only the idea. The proof needs expression trees
(the count operands), the IR's own def/use model, exact per-kind string
residues, and frame slots as tracked locations. `compiler/irparse.py` (P56)
parses the book at statement level but deliberately treats every string
statement as clobbering all four registers and falls through raw terminators
in listing order; not enough either, so `countflow.py` has its own parser and
cites irparse for the shared conventions.

**What `countflow.py` is** (built; runs the whole book in ~15 s):

1. **Parser** — every statement to a tree; `[@a, n]`, `[@a, n varying]`,
   `[@a, varying]`, literal pieces (byte count = unescaped length), `cmp`,
   `words`, `claim`/`release`, `goto`, `call`, `rt_call`, raw `@pc` lines.
2. **CFG** — `goto` labels; `call ret=`; `rt_call` → `site+4`; a block ending
   in a raw instruction takes the successor list of the `quest.blocks.split`
   block containing that pc. Loose ends (all reported, none affect a site):
   the excluded block 7015BD6B; 70169B44 (target of two SYSCALL blocks, not
   in the book); three predecessor-less blocks that do not open with WSAVS
   (70169B22, 70169B56 — after SYSCALLs; 7016D707 — a DERR sink).
   134 predecessor-less blocks: 100 addrbook entries, 31 nested-routine
   entries the addrbook keeps commented out, plus the three above.
3. **Register model** (stated, not inferred): `acN = e` defines acN; string
   statements define exactly what the library writes — WCMV ac0..ac3 (+c),
   WCMP ac0..ac3, WBLM ac1..ac3 (`EagleString.cpp:152-163, :199-202,
   :206-210`); **calls preserve ac0..ac2 and set ac3 = wfp** — verified in
   `EagleStack.cpp` (WRTN pops ac2, ac1, ac0, then `ac[3] = wfp`) and
   `RTBridge.cpp:88-90` (native runtime bodies restore `saved_ac[0..2]`, then
   `ac[3] = wfp`); WSAVS opens every routine with ac3 = wfp; float ops, WPSH,
   DERR write no accumulator; any other raw instruction (DIVX, WLOB, SYSCALL,
   raw LCALL's arguments) is opaque, so a trace through it is UNKNOWN, never
   "proven".
4. **Reaching definitions** — union meet, worklist to a true fixpoint, over
   ac0..ac3 program-wide and, per routine, over FRAME SLOTS
   `M32/M16[wp(ac3, k)]` as pseudo-registers. Slot effects: stores; varying
   assignments (length word + data range when the count is constant);
   `words()` fills; runtime callees' writes-through from
   `docs/Project28/RTConventions.md` §64-80 (`?UNSIGNED_TO_CHAR` writes at
   most 17 words at ac2 — `runtime/unsigned_to_char.cpp:136`
   "`k<=width<=32 always`"); game callees clobber UPWARD from every
   by-reference argument (`M32[argslot] = wp(ac3, j)` stores before the
   `call`) — a policy knob, `--call-policy upward|wide`; raw instructions
   clobber everything. A bounded effect is DEFINITE; an open-ended one is a
   MAY. Two RD passes (all effects / definite only) so that "no write on
   some path" can be bracketed rather than guessed.
5. **Tracer** — replaces registers (and frame slots) by their reaching
   definitions, through copies, effectful arithmetic and the min diamonds
   (a join becomes a union of values), down to ROOTS: constants, literal
   byte counts, `sx16(M16[A])` length-word reads, `M32` reads, string
   residues, argument cells, routine-entry values, opaque.
6. **Judge** — each root against the closure lemma (§2); a union is as weak
   as its weakest member; `add` of non-negatives is non-negative;
   subtraction, products of unknowns, `&` masks, entry/argument/opaque are
   UNKNOWN; WCMP's ac1 is NO.
7. **Base class** — `[@A, varying]` by form (the lifter's own rule,
   `string_sites.py:1696/2177`: count == N[A] and ptr == bp(A)+2), plus
   register operands whose traced count is `sx16(M16[A])` with the traced
   pointer `bp(base, 2k+2)` for `A = wp(base, k)`.
8. **§b pass** — for every varying read of the routine's own frame, what
   reaches its length word (definite / may / nothing).
9. **P6** — residue liveness, with a value-independent read (`acN = acN`,
   `acN = sub(acN, acN)`) distinguished from an observing one.

**How I check it is right** (ruling 1 applies to the tool):

- `--selftest`: a synthetic 7-block book with hand-computed answers (loop,
  min diamond, closure through a tail split, cmp result feeding a count,
  liveness, dominators, tokenizer edges), plus program-wide invariants: every
  def in an IN set is a def of that key; ac3 traces to `wfp` at every entry.
- **The closure lemma is checked by exhaustion** over every pair of counts in
  [−6, 6]: a Python transcription of `copy()` :60-90 and the WCMP loop
  :167-202 against `residues_after_copy` :152-163 — 169 cases, 0 disagreements.
  Corroboration (it is a transcription), stated as such.
- The site count is the book's exactly: 1,637 WCMV + 40 WCMP + 12 WBLM = 1,689.
- The known counterexample lands where the prompt says it should:
  `7016816B` has both operands `[@…, varying]`, is base class on both sides,
  verdict `cond` (assert), and tracing its length word gives a shared-data
  record field — outside any induction.
- Two ledgers cross-check each other: every `cond` verdict names the length
  words it depends on; every register operand names its root kinds; the
  ledger can be re-derived by anyone from the book with one command.
- Sensitivity is explicit: the callee-write policy is a switch, and the
  "traced-through-length-words" verdict is reported next to the base-class
  verdict so that the induction's reach is a measured number, not an
  argument.

What it does NOT do (so nobody over-reads it): no path sensitivity (a guard
`goto … (ac1 >s 0)` dominating a site is not used), no interprocedural
tracing (argument cells and callee writes are roots), no aliasing between a
pointer loaded from memory and the routine's own frame (assumed disjoint),
no 16-bit overflow model (`sx16(trunc16(x))` is taken as `x`).

---

## 2. The closure lemma — verified against `EagleString.cpp`

Read from the code, not the prompt; corroborated by the exhaustive check above.

**WCMV** (`copy` :60-90, residues :152-163). With `t = min(|n|, |len|)`:

    ac0 = 0                        always — the loop runs dst_count to 0 in both directions
    ac1 = len − sgn(len)·t
    ac2 = dst + n
    ac3 = src + sgn(len)·t
    c   = (ac1 != 0)

- `len > 0`: `ac1 = len − t ≥ len − len = 0`.
- `len = 0`: `sgn = 0`, `t = 0`, `ac1 = 0`.
- `t = 0` (n = 0 or len = 0): `ac1 = len`.
- `len < 0`: `ac1 = len + t ≤ 0` — stays non-positive, i.e. the sign is PRESERVED
  in both directions.

**So the lemma is stronger than stated:** after WCMV, `ac0 ≥ 0` unconditionally
and `ac1 ≥ 0 ⇔ len ≥ 0`. **The destination count's sign never enters.** A
negative `n` is a self-contained event at its own site (blank-fills backwards,
then leaves `ac0 = 0`).

**WCMP** (:167-204): `ac0 = dst_count` at the stop, which moves from `n`
toward 0 by ±1 per byte and never crosses it, so `n ≥ 0 ⇒ 0 ≤ ac0 ≤ n`;
`ac1 = result ∈ {−1, 0, +1}` — **closure FAILS for cmp's ac1**: a count fed by a
compare result is NO, not conditional. Measured: it never happens (§P6: all 40
cmp results are consumed by their `goto` test, nothing else). `ac2/ac3` are
pointers.

**WBLM** (:111-131, :206-210): `ac1 = 0` always; `ac2 = src + k`, `ac3 = dst + k`.

**Consequence for the chains.** A residue-fed `ac1` is non-negative iff the
source length at its HEAD site was; the 17 tail splits inherit their head's
obligation and add none. The tool returns `cond` for such an operand and
records the head site it depends on.

---

## 3. Chain heads versus residue-fed — the number that sizes the project

432 register count operands at 229 sites (the prompt's 154 was a regex
under-count; §4).

| register operands | count |
|---|---:|
| fed ONLY by string residues (no other root) | **18** — 17 tail-split `ac1` (WCMV ac1 → next WCMV ac1) + 1 WCMP s2 count fed by a WCMV ac0 (= 0) |
| chain heads (at least one non-residue root) | **414** |

Root kinds of the 414 (an operand can have several roots — the min diamonds):

| roots | operands | what it is |
|---|---:|---|
| constant + length word | 196 | the compiler's precomputed concatenation total (`len + k`), stored to a frame slot and reloaded; traced through the slot |
| length word alone | 94 | a varying's length in a register |
| opaque (alone, or with a constant / length word) | 69 | the slot trace hit a conservative clobber (an unbounded scratch-buffer copy, `M8[ac2]` cursor stores, a callee) |
| constant + `M16[…]` (not sign-extended), ± a length word | 26 | `nsub(cap, position)` — SUBSTR remaining-room arithmetic |
| residue on one arm of a diamond, a head on the other | 17 | |
| `M32[…]` through a pointer (± constant / length word) | 11 | INIT_OBJ_TBL's `M32[wp(arg 2, 1)]` and the like — an argument cell, interprocedural |
| constant only | 2 | |
| a `t1` scratch | 3 | CAST's WXCH-swapped count |

So the chain structure is SHALLOW: closure removes 18 operands' own
obligations, not hundreds. The project is not "prove the heads and induct";
it is "the heads are nearly everything, and most of them are length words".

---

## 4. Corrected classification

Method: parse every site's count operands from the IR text; classes per
operand: `const` | `literal` (byte count) | `register` | `varying` (a
`[@A, varying]` read) | `lenload-expr` (an expression containing `sx16(M16[…])`)
| `wide-expr` (`M32[…]`) | `expr`. Per site, the prompt's buckets:

| bucket | prompt | measured | note |
|---|---:|---:|---|
| both counts constant | 5 | **113** | 111 WCMV + 2 WCMP; the prompt's 5 presumably excluded literal pieces |
| at least one register | 154 | **229** | 432 operands |
| a `varying` length (source read) | 593 | **188** sites, 195 with any `[@A, varying]`, 206 operands | the word `varying` appears on 860 lines, 650+ of them DESTINATIONS (`[@a, n varying]`), whose count is `n`, not a length read. I could not reproduce 593 by any rule; treat it as retired |
| other | 463 | **1,147** | 870 literal assignments with a constant count, 200 `expr+expr`, 40 `wide+wide`, 35 `lenload+lenload`, 2 … |
| WBLM | 12 | 12 | |

Per operand (3,366): const 1,158 · literal 882 · register 432 · varying 206 ·
lenload-expr 207 · wide-expr 80 · expr 401.

---

## 5. Approach to the varying sites, and whether the induction is sound

**The base class is discharged by assertion; the induction is measured, not
relied on.** For each of the 206 `[@A, varying]` operands (+ 21 register
operands the dataflow recognises as the same layout), the deliverable is

    assert((sx16(M16[<A>]) >=s 0), "P58 varying length @<pc>")

emitted before the statement (§3 grammar: `>=s`, no message quotes inside).
For an operand whose count is a sum `sx16(M16[A]) + k` (k ≥ 0) the same assert
on `A` discharges it; for a register operand the assert names the traced
length word(s).

**Does the induction ("every write is non-negative, so every read is") hold?**
Measured by tracing each length word through its writers (the
`verdict_through_len` column):

| base-class operand (206) | result |
|---|---:|
| length word traced to a provable non-negative write on every path | **7** |
| length word is not in the routine's frame (statics such as `IN_BUFFER` 0x7000021C ×29, SD_PTR/OBJ_PTR record fields ×18, by-reference arguments ×3, computed record addresses) — OUTSIDE any intraprocedural induction | **78** |
| length word in the frame, but a writer is a callee (`?READ`, `?UNSIGNED_TO_CHAR`, a game routine through a by-ref argument) or an unbounded copy | **121** |

So the induction, done honestly, proves 7 of 206. It is SOUND where it
applies and it applies almost nowhere, because a varying's length word is
written by `?UNSIGNED_TO_CHAR`, by `?READ`, by another routine, or is a record
field — exactly what the prompt predicted for SD_PTR/file cases, and it turns
out to be the norm, not the exception. **The assert is the mechanism; the
induction is a footnote.** I do not plan to spend Part 2 extending it.

---

## 6. The pattern-match count

| where | operands | sites |
|---|---:|---:|
| `[@A, varying]` by form — WCMV src 160, WCMP s1 31, WCMP s2 15 | **206** | 195 |
| register operand recognised by dataflow (count = `sx16(M16[wp(b,k)])`, pointer = `bp(b, 2k+2)`) — WCMV src 17, WCMP s1 3, s2 1 | **21** | 18 |
| **total base class** | **227** | **213** |

Address shapes of the 206: frame slot `wp(ac3, k)` 128 · static word 36
(`IN_BUFFER` 29) · SD_PTR / OBJ_PTR / PLAYER-cache record fields 18 (+10
`M32[wp(ac3,−6)] + 0x12` — a record via an argument pointer) · by-reference
`R[…]` 3 · other computed 11. `7016816B` is in the record-field 18.

---

## 7. First pass at the residue, and the uninitialised-varying question

**Verdicts per site** (worst operand; 1,689):

| tier | sites | what |
|---|---:|---|
| **proven by construction** (`yes`) | **996** | all counts constant or literal byte counts, or `ac0` after a copy (= 0), or `ac1` after WBLM |
| **conditional on named length words** (`cond`) — the assert class | **617** | 213 base-class sites + 404 sites whose count is `len + k`, a register carrying a length word, a min over lengths, or a residue-fed `ac1` |
| **unknown** | **76** | in 27 routines; see below |
| **no** (a count that can be negative by construction) | **0** | no cmp result ever feeds a count |

**The 76 unknown sites, by cause** (from the ledger; complete list in
`countflow.out` §7):

| cause | sites (approx.) | what would establish it |
|---|---:|---|
| a total slot clobbered by an UNBOUNDED scratch-buffer copy (`[@bp(ac3, b), <expr>]` with a non-constant count, or an `M8[ac2]` cursor store whose cursor traced to a call/loop) | ~45 | the scratch buffer's EXTENT (its declared capacity). The p32 chain ledger names each buffer; a capacity per buffer would bound the clobber and, I expect, resolve most of these to `const + length word` (= assert class). Frame layout knowledge, which P35's `gen_declarations.py` derives for translated routines only |
| **SUBSTR remaining-room**: `min(k, cap − position)` (`nsub(0x1E, M16[…])`, `0 − len + 30`) — DISPLAY_INVENTORY ×6, DISPLAY_MAGIC ×3, DISPLAY_SCREEN, HELP, INIT_OBJ_TBL, DISPLAY_MAP | ~16 | path sensitivity: the diamond that computes the min is guarded by a compare on the same quantity; a dominance-guard rule (`goto [...] (x >=s 0)` reaching the site on the true edge with x unchanged) would settle them. This IS StringsDesign's tail-split claim in its only surviving form |
| a product `len × 2 + k` (TAKE_OVER_CASTLE ×2), an argument cell (INIT_OBJ_TBL ×3), a `t1` scratch (CAST ×1), argument-of-callee arithmetic | ~10 | interprocedural or arithmetic reasoning; probably left as assert |
| a 16-bit narrow read used as a count (`M16[…]` without `sx16`, 20 operands) | (inside the above) | these are the `nsub` positions |

None of the 76 is a `[@A, varying]` read that the pattern failed to recognise —
the prompt's guess that the residue would be "sites that read a length from
somewhere the pattern does not recognise" is not what the data shows. The
residue is (a) frame-layout blindness and (b) path-insensitive min arithmetic.

**§b — varying reads whose length word may be unwritten.** Of the 128
`[@wp(ac3, k), varying]` reads of the routine's own frame:

| status | reads |
|---|---:|
| a DEFINITE write of word k reaches on every path (store / varying assignment / bounded callee write) | **114** |
| a definite write is missing on some path but a MAY-write covers it — `?READ`'s buffer (ALLY_PLAYER), `?UNSIGNED_TO_CHAR` (SEIGE, DISPLAY_INVENTORY ×3 — the `?UNSIGNED_TO_CHAR` at 70167C96 is before the reads; the `write16`s are the min shape's stores), a game callee through a by-ref argument (DISPLAY_SCREEN via 7017CD71), an unbounded copy (BOAT, SEIGE ×5, BARGAIN.2) | **13** |
| **NO write of any kind on some path** | **1** — `MOVE_IN_CAVE` 70170BCA, `wp(ac3, 10)` |

The MOVE_IN_CAVE case is the shape the prompt asked for: the length word is
written on two arms (`"decreased by "` @70170B76, `"increased by "` @70170B84)
of a three-way test on the strength delta, and there is a CFG path from the
routine entry that takes neither arm and still reaches the read (72 blocks,
found by BFS avoiding both writers). Whether that path is FEASIBLE (delta = 0)
is a path-condition question the tool cannot answer; if it is, the original
program read a previous call's leftover (a stack slot in 1986, a persistent
0x74 area under M4a — **the M4a hazard the prompt asked to be named, with a
concrete site**). Reported, not chased; I recommend it as the one place Part 2
does chase.

Static varyings: the tool does not read `quest.mem` yet (the base case "written
at load" of §b); the 36 static reads are all `IN_BUFFER`-class buffers written
by `?READ` before use in the routines I inspected — a claim I will check
against `quest.mem` and the `?READ` sites in Part 2, tier "provable by
provenance".

**P6 — dead residues.** The prompt's cmp claim holds: after all 40 WCMPs
only `ac1` is observed (by the `goto` test, 40/40); `ac0` and `ac2` are READ at
3 sites but only by `x = sub(x, x)` / `x = x`, whose result does not depend on
the value. After WBLM: `ac2/ac3` dead at all 12, `ac1` read once by an
`add(ac1, ~ac1)` (the −1 idiom; value-independent in effect, counted as a read).
**After WCMV — a finding the prompt did not have:** `ac0` (= 0) is LIVE at
**328** sites — the compiler stores it as a zero (`M16[wp(ac3, k)] =
trunc16(ac0)` for `?WRITE_SCREEN`'s row/col arguments, 250+ times), across an
intervening `rt_call` (registers preserved); `ac1` is live at 44 (19 `goto`
tests "was the source exhausted?", 17 `ac0 = sub(ac0, ac1)` remaining-room
computations, 4 pushed as an argument); `ac2` is live at 455 (383 as the next
piece's destination cursor); `ac3` is dead at all 1,637. So a rewrite may drop
`ac3` and, at 1,309 sites, `ac0` — but the `ac0 = 0` residue is a real
dependency at one site in five, which is the answer to "how much binding
precision is recoverable".

**P4 — WBLM.** All 12 counts are constants (8, 10, 90, 0xBE0, 0x167E), all
positive ⇒ every fill is FORWARD by construction; all 12 are the
self-overlapping smears the book annotates (`src = dst − 1` ×5, `− 2` ×7).
Proven by construction; nothing to assert.

**P5 — segment containment.** Not provable by this tool and I do not think
provable statically without frame/record bounds: every address is
frame-, shared-page-, or arena-relative, and "never crosses a 2^28 segment" is
a property of the loader's placement plus the counts' magnitudes. The library
throws; lockstep catches. Tier: **unproven, runtime-checked** — I propose to
state it as an obligation with that tier and move on, unless a001 wants a
bound argument (frame ≤ 0xD89 words, counts ≤ 32 K constant, shared page
sizes).

---

## 8. Pre-registered predictions (scoreable in REPORT.md)

1. Of the 76 unknown sites, giving the slot model per-buffer extents will
   resolve **at least 40** to `cond` (assert class) and none to `yes`.
2. A dominance-guard rule will resolve **all ~16** SUBSTR remaining-room sites
   to `cond`, and StringsDesign's "positive when the WCMV runs" claim will be
   re-derived mechanically for the 17 tail splits — as `cond` on the head, not
   as `yes`.
3. At least **5** sites will stay `unknown` at the end: INIT_OBJ_TBL's
   argument-cell counts (3) and TAKE_OVER_CASTLE's products (2).
4. The MOVE_IN_CAVE path will turn out FEASIBLE (delta = 0 is a legal
   outcome of a strength change) — i.e. a genuine 1986 uninitialised read —
   and it will be the only one.
5. `quest.mem` will show every static varying read by the book with a
   length word of 0 or a small positive value at load; no static will be the
   "written at load with garbage" case.
6. **The induction will not be extended**: no further base-class operand will
   move from `cond` to `yes` without interprocedural work I am not proposing.
7. `7016816B` stays exactly as classified: base class both sides, `cond`, the
   expected assert-tripper, and no other site in the record-field class will
   look different from it structurally.

---

## Decisions needed

**Q1 — Base class = "either side".** The prompt writes the pattern as
`len → ac0, chars → ac2` (destination side) but its own example is
`XNLDA 1` / `XLEFB 3` (source side). I have taken it as *any* operand laid out
as a varying, on whichever side — 227 operands, including WCMP's second string.
*Recommendation:* confirm; it is the only reading under which the counterexample
is a member.

**Q2 — Buffer extents.** Resolving most of the 76 needs per-buffer capacities
for the frame scratch chains. Options: (a) take them from the p32 chain ledger
+ the largest constant count ever copied into each buffer (a lower bound on the
declared size — sound for a MAY-write only if the buffer never receives an
unbounded copy, which is the case being resolved, so circular in ~10 sites);
(b) take them from `game/declarations.json` where a routine is translated
(7 routines); (c) leave them `unknown` and let the assert list carry them.
*Recommendation:* (a) with the circular cases marked, and (c) for what remains;
(b) is too narrow. This stays within the prompt's boundaries (read-only use of
those artifacts).

**Q3 — Path sensitivity.** A dominance-guard rule is ~60 lines in the tool and
is the only way to re-derive the tail-split/SUBSTR claim mechanically.
*Recommendation:* do it in Part 2; report the rule and the sites it touches
separately so the two mechanisms stay distinguishable.

**Q4 — Scope of the P6 finding.** The `ac0 = 0` residue being live at 328
sites belongs in `CountProof.md` §P6 and in the REPORT as a fact for DESIGN
§5.2; it changes nothing in this project's proof. *Recommendation:* record it,
do not act on it here.

**Q5 — The MOVE_IN_CAVE candidate.** Chasing feasibility means reading the
three-way test's conditions (path conditions, not dataflow). Small, bounded,
and it is the prompt's §b question with an actual site. *Recommendation:* chase
it in Part 2, one page, and name it in `quest.assumptions`-style form
("FALSIFIED BY: a run in which the delta is 0 and the message is well-formed").

**Q6 — `quest.assumptions` rows.** I expect to recommend three: (i) every
`[@A, varying]` operand is non-negative — checkable by the assert at the site;
(ii) no WCMP result feeds a count — checkable by `countflow.py`'s `no` count;
(iii) `?UNSIGNED_TO_CHAR` writes at most 17 words at ac2 — checkable against
`runtime/unsigned_to_char.cpp`. *Recommendation:* agree the shape now so the
REPORT can write them in the file's format.

---

Not started: `CountProof.md`, `REPORT.md`, the assert site list, Part 2's tool
changes. Waiting for `a001`.
