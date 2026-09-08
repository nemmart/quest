# Project 35 — Part 1: plan (STOP at this gate)

Sep 8 2026. Nothing built yet; nothing executed. This document is the
plan-gate report the PROMPT asks for, with the six items of Part 1, the
things I found while planning that need a ruling, and the scope note.

## 0. Tree vintage and provenance

The session was handed three tarballs (Work, Disassembled, QUEST), not a
git checkout, so **the commit cannot be stated from the tree** (no `.git`;
the PROMPT asks for 625d968 or later — the layout rename IS present:
`compiler/readable.py`, `game/quest_rt.h`, `compiler/README.md` dated
Sep 8, `docs/Project35/PROMPT.md` present). Verified against
docs/Provenance.md (Sep 6, P33-B table):

| file | sha256 (first 16) | table |
|---|---|---|
| emulation/quest.ir2.book | e2f18f144c195da4 | e2f18f144c195da4 ✓ (ir 6, header carries dis 5c1db5fb… / blocks / pushmap / argmap / strings / arena) |
| emulation/quest.addrbook | e6fde2c246630e0e | e6fde2c246630e0e ✓ |

The git commit should be filled in by whoever runs this from the repo.

## 1. Routine set (four, from the addrbook)

Instruction counts are dis lines between the routine's addrbook entry and
the next entry (script in this session; the same count for the book gives
blocks / statement lines).

| routine | entry | argc | frame | flags | dis instr | book blocks / lines | why |
|---|---|---|---|---|---|---|---|
| PICK_X_Y | 701761E7 | 2 | 0x05 | – | **73** | 18 / 64 | the P34 hand reading; 3 rt_calls, 1 DERR fold, by-ref args, a loop, `mul/div/cvwn/add` |
| UPDATE_SCREENS | 7017D635 | 3 | 0x05 | – | 70 | 18 / 72 | pure arithmetic: a do-loop, 4 DERR folds, `nadd`/`sub`/abs diamond, a 2-D record store; NO calls, NO strings. Called by DIED. |
| REFRESH_SCREEN | 70176A93 | 1 | 0x17 | mixed:0/1 | 96 | 24 / 89 | THE two-arity shape: first block reads the marker word (`sx16(M16[wp(ac3, -9)])`) and branches; `?WRITE_SCREEN` at arity 2 and 5; 5 located-string statements (varying literal assigns); XNDO loop. Called by DIED at arity 0 → the `REFRESH_SCREEN$0` call site. |
| DIED | 7016603D | 2 | 0x07 | dyn,push | **529** | 73 / 495 | two WMSP claim groups (STASP at 701661AA and 70166215), 21 string ops incl. a WBLM, 2 rt_calls, 6 game→game decorated `call`s (UPDATE_SCREENS, HIT_ANY_CHAR, REPOSITION, REFRESH_SCREEN, DISPLAY_SCREEN, DISPLAY_INVENTORY), 20 DERRs, WSZB bit tests |

Total: **768 instructions / 720 book lines**, not ~400. The PROMPT's
"PICK_X_Y (96 instructions)" is not what the dis gives (73; 96 is
REFRESH_SCREEN's count — possibly a transposition). DIED is the SMALLEST
routine in the program with two claim groups (next: OBSERVE at 991), so
if two groups are the point, DIED is the only choice. See §7 for the
staging proposal.

## 2. C subset and its ir 6 mapping

Everything the four routines need, and nothing else. Each row names the
ir 6 production (docs/IR.md §) the translator emits; anything not in this
table is REFUSED with the construct, the C line, and the reason.

| C construct | ir 6 output | notes |
|---|---|---|
| `int32_t x;` / `int16_t x;` local | frame slot `wp(ac3, d)`; reads `M32[wp(ac3,d)]` / `sx16(M16[wp(ac3,d)])`; stores `M32[…] = acN` / `M16[…] = trunc16(acN)` | slot rule §3 |
| `VARYING(n) s;` local | length word at `wp(ac3,d)`, data at `bp(ac3, 2d+2)`; only as operand of the string functions | |
| `ARRAY1(T,N) NAME` at a static address (declarations.h) | `M32[0x7000XXXX]` base load (`LWLDA`), then `wp(acB, K)` with K = the compiler's folded constant | §5.2 wp; static addresses are NOT equivalences |
| record field `NAME[i].f` (1-based) | `mul(i, stride)`, `add(ac, M32[base])`, `sx16(M16[wp(ac, K)])` with K = raw displacement (P34 origin rule: origin = K + stride) | fold `(i-1)*stride + origin` → `mul(i,stride) + (origin-stride)` |
| by-ref parameter `T *arg_N` | `M16[R[ac3 + -(10+2N)]]` / `M32[R[…]]`; store through it `M16[R[ac3 + -k]] = trunc16(acN)` | argc from the declaration |
| `int arg_count` leading parameter (two-arity routines) | `ac0 = sx16(M16[wp(ac3, -9)])` (the marker word); never pushed | README says `M16[fp+0x7FF7]`; the book spells it `wp(ac3, -9)` |
| `+ - *` on ints | `add()/sub()/mul()` effectful, root-only; 16-bit destination → `nadd/nsub`; `x + 1` → `add(x, 1)` (WINC spelling), `x + k` → `add(x, 0x%08X)` (WNADI spelling, sign-extended constant) | constant spelling follows the instruction the selector picks (§5 below) |
| `/` on ints | `div(a, b)` then `cvwn()` when the result is narrowed | PICK_X_Y 70176217 |
| `(int16_t)e` | `cvwn(acN)`; a store of it `trunc16(…)` | |
| `-e` / abs diamond | `sub(0, ac)` after a `>=s 0` skip | UPDATE_SCREENS |
| `if (a OP b)` | `goto [fall, skip] (acX OPs const)` with the emulator's skip signedness (WSEQI → `==`, WSGTI → `>s`, WSLEI → `<=s`, WSGE → `>=s`); test-against-zero of a 16-bit value → the Nova form `t1 = ((acN & 0xFFFF) \| lsh(c, 16)); goto [f, s] ((t1 & 0xFFFF) != 0)` | the Nova 17-bit shape is emitted verbatim (§5.6); `if (x) …` on a 16-bit load selects it |
| `while` / `do … while` / backward `goto` | `goto [L] 0` blocks (WBR / XJMP); XNDO loop `t1 = nadd(M16[…], 1); M16[…] = t1; ac = t1; goto [fall, loop] (t1 >s limit)` | canonical block order = DFS from entry, ascending successor order |
| `SUB(i, n)` in an index | `assert(!(acN >u n) && (acN >s 0), "DERR 17 @<pc>")` then `goto [K] 0` in the guard block | see ruling request R1 (the pc) |
| `NAME$N(a1, …, aN)` runtime call | `rt_call ?NAME(e1, …, eN) site=<pc>` block-final; each `&expr` argument materialises into a frame temp first (`M32[wp(ac3,d)] = acN`) and passes `wp(ac3, d)`; a static `&X` passes the constant or `wp(acB, K)` | arity checked = N; `const` params refuse a write-through; `site=` ignored by the comparator |
| game routine call `NAME(...)` / `NAME$N(...)` | `call <tgt> args=<n> marker=… site=… ret=…` with the book-mode arg-slot stores `M32[<alloc_base slot>] = <ea>` before it | slots from quest.addrbook (static addresses, must match) |
| `return` | `ret` | a value-returning routine writes `M32[wp(ac3, -8)]` (ac0 image) first |
| `assign_varying(&s, "lit")` / `assign_varying(&s, &t)` | `[@wp(ac3, d), n varying] = [@0xW:b, "text"]` / `= [@src, m]` | StringsDesign §2; literal address from quest.strings/mem (provenance, compared by text) |
| `assign_fixed(buf, n, piece)`, `append(...)` | `[@bp(ac3, k), n] = piece`; continuation pieces `[@ac2, n] = piece` | the scratch-buffer machinery |
| `pad_equal(a, b)` / compare | `ac1 = cmp(piece, piece)` then a skip on ac1 | |
| `words_copy(&d, &s, k)` | `words(@d, k) = words(@s, k)` | DIED's WBLM |
| claim group (DIED) | `acN = t@<block>.k`, `claim t@<block>.k, acN`, `release t@<block>, acN` | needs the arena names → the translator takes `--arena quest.arena` and the group's block pc is unknowable → RENAME class via the twin equivalence (#3) |

**Refused (loudly, with the line):** floats, pointers not of the declared
forms, `switch` (LDSP tables — none in the four), `&&`/`||` in C conditions
(the compiler emits skip chains; write nested `if`), function pointers,
recursion, `static` locals, unsigned arithmetic other than the `>u`
bounds test, string ops outside the header's functions, any runtime
callee not in RTConventions' argc set, `arg_count` used other than in an
`if`, a write through a `const` pointer, any declaration whose address is
not in declarations.h.

## 3. Frame layout rule (hypothesis, tested by the bijection)

From the addrbook layout (P34 Census §1) and the four routines:

1. Locals occupy even offsets `2, 4, 6, …` up to `2*frame`, one wide slot
   each **including 16-bit locals** (UPDATE_SCREENS: `local_2.h`,
   `local_4.h`, `local_6.w`, `local_8.w`).
2. Declared locals in declaration order, first.
3. Compiler temporaries (an `&expr` argument, a spilled index, a string
   scratch buffer) take the **lowest free even slot at the point of
   creation; a temp dies at its last use, so slots are reused**. PICK_X_Y:
   call 1 uses 4,6 (r*9 lands in 4 and stays live across call 2, so call
   2 uses 6,8; call 3 reuses 4,6). This is what the P34 "local_4 / local_6
   is reused" notes are.
4. The translator therefore does declaration-order allocation + a
   liveness-based temp pool; the comparator's bijection absorbs the rest
   and reports it. A slot mapped to two offsets is a DIFF (rule 2 of the
   PROMPT) — that is where rule 3 is wrong.

## 4. Comparator design — `compiler/ircmp.py A.ir B.ir [--folded]`

- **Parser**: readable.py's `tokenize`/`Parser`/`parse_stmt`/`load_book`
  (import, not copy); routines attributed by addrbook entry → address
  range as readable.py does. The translator's output uses synthetic
  labels `L0, L1, …`; the parser accepts both.
- **Canonicalisation** (the seven equivalences, no others):
  1. block order: DFS from entry, successors in the order the `goto` lists
     them; `goto` targets compared by canonical position; `site=`,
     `marker=`, `ret=` dropped.
  2. frame slots: collect `wp(ac3, d)`/`[ac3+d]` offsets in first-use order
     per side; the bijection is built greedily as statements are paired
     and reported; a conflicting second mapping → DIFF(slot).
  3. `tN` renamed by first appearance; `t@<block>.k` ↔ `t@<canon>.k`.
  4. `[@0xW:b, "text"]` → compare `"text"` only.
  5. strip from `;` to end of line.
  6. static addresses compared verbatim.
  7. registers verbatim in the primary run; `--folded` applies
     readable.py's `fold_block` (the in-block register-folding pass) to
     BOTH sides first, then the same comparison.
- **Statement pairing**: within a block, sequence alignment (difflib
  SequenceMatcher on canonical strings) so one inserted spill does not
  shift every later line into DIFF.
- **Report** per routine: `statements total / MATCH / RENAME / DIFF`, the
  slot bijection, block-order map, then the first N DIFFs with BOTH texts
  and a class tag: `reg` (identical after register renaming), `slot`,
  `border` (block order), `expr` (same operands, different shape),
  `const-spelling` (same value, different spelling), `missing`/`extra`
  (unpaired statement), `unknown`.
- **Self-test** (`ircmp.py --selftest`): the book against itself = 100%
  MATCH; the book with every `wp(ac3, d)` in a routine permuted by a fixed
  bijection = 100% RENAME; a copy with one register swapped = exactly one
  DIFF(reg).

## 5. Translator architecture (why it is more than a pretty-printer)

The primary target is register- and spelling-exact (`ac2 = ac0 ; WMOV`
then `ac2 = sx16(M16[wp(ac2, 11495)])`; `add(ac0, 1)` for WINC vs
`add(ac0, 0x00000014)` for WNADI; `ac1 = 0x00000064` before `div`). So
`translate.py` is three stages:

1. pycparser → a checked subset AST (refusals here).
2. AST → **DG-shaped micro-ops**: (mnemonic class, registers, frame slot,
   constant) — a deterministic model of the DG PL/I code generator whose
   rules are written down in `compiler/CODEGEN_RULES.md`, each rule with
   the instruction pair that motivated it. Initial rules read off the
   four routines: ac3 = fp always; ac2 = the address register for record
   access; a load-immediate before `mul`/`div` goes to ac1; short-lived
   values rotate ac0 → ac1 → ac2 (REFRESH_SCREEN 70176ABA/70176ADF show
   the rotation restarting at different registers — a finding already);
   a value needed after a call is spilled to a temp before the pushes.
3. micro-ops → ir 6 text through lower.py's own spelling functions
   (`hexc`, the effop/skip tables) imported from `emulation/tools/lower.py`
   (read-only import; nothing in emulation/ is touched), so a spelling
   difference can only be a selection difference.

Register choice is where "refuse-don't-guess" needs one clarification:
the semantics of the C are the same whichever register the model picks,
so a register rule is a **declared belief** (IR.md §6's word) that the
comparator falsifies, not a guess about meaning. The translator applies
the written rule and the comparator reports `DIFF(reg)`; it never
refuses over a register. Refusal is for constructs and conventions.

## 6. Native check

`gcc -std=c99 -Wall -Wextra -Werror -c game/routines/*.c` and
`g++ -std=c++17 -Wall -Wextra -Werror -c` on the same files, with
`quest_rt.h` filled in (C view: typedefs + prototypes; C++ view:
`array1<T,N>::operator[]` with the −1 and bounds check, `varying<n>`).
Compile only. `$` in identifiers needs `-fdollars-in-identifiers` —
default on for gcc/g++ on GNU targets, stated explicitly in the build
line. Two scaffold corrections found while planning, to make in Part 2:

- `RANDOM_NUMBER$3`'s parameter ORDER in quest_rt.h is wrong. The IR's
  argument 1 is the constant/lo, 2 the hi, **3 the seed**
  (`rt_call ?RANDOM_NUMBER(wp(ac3, 4), wp(ac3, 6), wp(ac2, 40))`, and
  RTConventions: "writes through arg3 (the seed)"). Correct prototype:
  `int32_t RANDOM_NUMBER$3(const int32_t *lo, const int32_t *hi, uint32_t *seed);`
  — and it returns in ac0.
- `SUB(i, n)` must carry the DERR number and, per R1 below, possibly the
  pc.

## 7. Success criteria (stated in advance) and scope

- PICK_X_Y primary: **≥ 90% MATCH+RENAME**; `--folded`: ≥ 95%.
- UPDATE_SCREENS primary ≥ 85%; REFRESH_SCREEN ≥ 80% (string statements
  and the register rotation); DIED: **report whatever it reaches** — it
  is the stretch routine (§1). Every DIFF classified (`reg / slot /
  border / expr / const-spelling / missing / extra / unknown`);
  `unknown` count reported, target 0.
- Order of work in Part 2: PICK_X_Y → UPDATE_SCREENS → REFRESH_SCREEN →
  DIED, each translated/compared/fixed before the next, so a partial
  DIED still leaves three complete reports.
- Runtimes recorded (translate + compare per routine, whole-book
  self-test).

**Scope decision needed**: keep DIED (768 instructions total, the only
two-group routine under 900) or substitute **INIT_OBJ_TBL** (7016DF39,
195 instructions, `dyn`, argc 2, ONE claim group, 6 DERRs, no calls) for
a ~430-instruction total that still covers claims + strings + arg_count
— at the cost of the second claim group and the game→game `call` form.
My recommendation: keep DIED as the staged stretch; the `call` decoration
and two groups are exactly where the compiler's regularity is most
likely to run out, which is the project's question.

## 8. Rulings requested before Part 2

- **R1 — the DERR message pc.** `assert(…, "DERR 17 @70176207")` embeds a
  pc the C cannot know. The PROMPT says the DERR assert text must match
  textually AND that `site=` addresses are provenance. Proposed: compare
  the message as `DERR nn` with `@pc` treated like `site=` (equivalence
  1 extended to the message's address), otherwise every bounds check is
  a guaranteed DIFF and the 90% target is unreachable on principle.
- **R2 — claim-group twin names.** `t@<block>.k` names the claim block's
  pc; PROMPT equivalence 3 already maps twins by canonical block, so the
  translator will emit `t@<canonical label>.k` — confirming that reading.
- **R3 — register rule = declared belief** (§5), never a refusal.
- **R4 — scope** (§7): DIED kept as staged stretch, or INIT_OBJ_TBL.
- **R5 — commit.** The tree came as tarballs; the TREE VINTAGE line in
  the report will say "main, post-625d968 layout, commit unrecorded"
  unless the runner supplies it.

## 9. Rulings received (Sep 8 2026, user) — Part 2 went ahead on these

- R1 accepted: the `@pc` in a DERR message is provenance like `site=`; the
  message compares as `DERR nn` (equivalence 1 extended).
- R2 accepted: twins are emitted `t@<canonical>.k` and mapped by equivalence 3.
- R3 accepted: a register rule is a declared belief the comparator
  falsifies (`DIFF(reg)`); refusal is for constructs and conventions.
- R4 accepted: DIED stays the staged stretch, order PICK_X_Y →
  UPDATE_SCREENS → REFRESH_SCREEN → DIED.
- R5: the commit is **35859fc** (main at hand-off: the P35 prompt commit on
  the 625d968 layout).
- Corrections accepted for Part 2: `RANDOM_NUMBER$3(lo, hi, seed)` returning
  in ac0; `SUB()` carries the DERR number; PICK_X_Y has 73 instructions
  (the PROMPT's 96 was a transposition with REFRESH_SCREEN).
- Request: the frame-layout hypothesis (§3) is numbered rules in
  compiler/CODEGEN_RULES.md (R1–R3, R3b) alongside the register rules.
- Two extensions made during Part 2 and recorded in ircmp.py's header:
  equivalence 4 compares a literal by its byte length (the book shows no
  text for data-segment literals: REFRESH_SCREEN's 0x70000E07/0x70000DDF);
  a block is compared by canonical DFS position, the entry block's WSAVS
  fall-through included.
