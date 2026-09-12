# Project 46 — q001: PLAN GATE (Part 1 report, Sep 12 2026)

No code and no existing doc changed. Everything below was measured on the
tree as handed over (Work__99_.tgz). Rulings needed are marked **RULING**.

---

## 1. Proposed ir 7 grammar additions (IR.md notation)

### 1.1 The `v` declaration — a file-level line, outside any block

    v <ENTRY>.v<digits> <vtype>
    vtype := i16 | u16 | i32 | u32          ; 1, 1, 2, 2 words
           | char <n>                       ; fixed CHAR(n): ceil(n/2) words (§5.8 [@a, n])
           | varying <n>                    ; CHAR(n) VARYING: 1 + ceil(n/2) words (length word + data)
           | words <n>                      ; an aggregate the source addresses by offset
                                            ;   (array, record, bit string): n words, uninterpreted

`char` and `words` are not in DESIGN §4.1 (see §6 below): a fixed-string
local and an array local are both PL/I locals the compiler will meet on its
first routine, and both have a fixed size, which is the only property the
loader consumes. The four scalar names are kept for the compiler's benefit
and for a width tripwire (below); at the IR level i16/u16 are the same
one-word cell, because IR.md puts signedness in the operator (`sx16`/`zx16`),
never in the storage.

### 1.2 How a `v` is referenced

**A `v` name is an ADDRESS CONSTANT**, exactly as `s@<block>.<k>` already is
(§5.9: "an ordinary constant in expressions"). It is a `primary`:

    primary += <ENTRY>.v<digits>            ; the word address the loader placed it at

so reads, writes, byte access and located strings use the existing forms:

    ac0 = sx16(M16[QUEST.v0])               ; an i16 read
    M16[QUEST.v0] = trunc16(ac0)            ; an i16 write
    M32[QUEST.v1] = add(M32[QUEST.v1], 1)   ; an effectful store (ruling R6 shape)
    ac2 = bp(QUEST.v2, 0)                   ; a byte pointer into a char/varying v  (bp(s@b.k, o) precedent)
    [@QUEST.v3, 27 varying] = [@s@70166144.3, varying]   ; twin -> v: the §5.8 min(len, 27) truncates
    M16[QUEST.v4 + 5] = 0                   ; word 5 of a `words` aggregate

There is deliberately **no bare-name lvalue** (`QUEST.v0 = e`) and no
bare-name rvalue. The rejected alternative — a typed value form with
implicit width and sign — would put signedness into storage, which IR.md
§5.1 forbids ("never implicit — it is in the operator"), and would make
`ircmp` expand names before comparing. The address form also makes
placement literally "an address and nothing else" (carried-in ruling 7):
after placement the loader substitutes a 0x74 constant for a 0x76 constant
and the statement text is otherwise identical.

Loader rules for `v`:
- declared before first use, in file order (single pass, like t-places);
  duplicate declaration REFUSES; a reference with no declaration REFUSES.
- cross-entry references are LEGAL (`QUEST.1.b0` reading `QUEST.v3` is a
  nested procedure reading its parent's local — DESIGN §4.1's "globally
  scoped").
- width tripwire (cheap, proposed): `M32[<v>]` on an i16/u16 REFUSES;
  `M16[<v>]`/`M32[<v>]` on a `char`/`varying`/`words` v is allowed only in
  the `+ offset` form or as the length word — **RULING**: land the
  tripwire, or size-only?
- a literal hex constant in [0x76000000, 0x78000000) REFUSES anywhere: these
  addresses are loader-assigned and not authorable.

### 1.3 Symbolic blocks

    block <ENTRY>.b<digits>                  ; header: no `seg` (the space fixes it: 0x70000000)
    goto [<label>, ...] <expr>               ; label := <hex8> | <ENTRY>.b<digits>, mixed lists allowed
    goto <label>                             ; sugar as before

- A symbolic label may be a FORWARD reference (loops need both directions);
  the loader assigns the address on first sight (header or label) and at
  end of file REFUSES any label never defined by a header. Duplicate
  header REFUSES.
- hex8 labels keep the ir 6 rule (must be a listed sync-list start); a
  hex8 in 0x77 REFUSES (only names denote symbolic blocks).
- Inside a symbolic block: statements, string statements, `assert`,
  `goto`, `ret` are legal. `@addr` instructions, `call`, `rt_call` REFUSE in
  ir 7 (§5 below: a naive program has no site to embed; the calling
  bridge is P48).
- `ret` from a symbolic block runs WRTN with the synthetic pc = the 0x77
  address, as today (abort-message use only).

### 1.4 Header

    ir 7
    mode <stock|book>
    source/blocks/pushmap/argmap/strings*/arena lines as ir 6,
    with `blocks <path> sha256=` REQUIRED only when the file contains a
    numeric block or a numeric goto label (a symbolic-only program has no
    CFG to bind to).

Names: `<ENTRY>` is an addrbook entry name, uppercase, `@ADDR` suffix
dropped; parsing splits on the LAST dotted component (`.v<d>`/`.b<d>`),
everything before it is the entry. Lowercase entry, missing digits, an
unknown entry: REFUSE. Verified against the tree: after suffix-drop the
130 entry names are unique.

---

## 2. Where declarations live — in the IR file, per routine

Inline `v` lines in the same file as the blocks, emitted by the compiler
before that routine's blocks. Reasons:

- The qualified name already carries the namespace, so file position is
  irrelevant beyond "before first use"; concatenating per-routine
  fragments is a valid file with no merge step — that IS separate
  compilation.
- A sidecar keyed like the addrbook would be a second artifact the loader
  must provenance-check, and it would be the natural place for
  *placement* to creep in. Placement (the future oracle) stays a separate
  input; the declaration file is the program.
- A header section would force every fragment to be spliced into one
  header, which is exactly the merge step separate compilation must avoid.

---

## 3. Segment mechanics for 0x76 and 0x77 — measured, not estimated

**0x76 is covered by existing machinery; 0x77 needs one loader rule and
nothing in the executor or Machine.** Details, with the source lines:

*Memory.* `hw::Memory` is a flat page table keyed by `address>>10` with
per-page permissions (Memory.cpp:31–40); nothing is segment-aware below
that. `Arena::map_pages` (Arena.cpp:135–143) and `AddressBook::map_pages`
(AddressBook.cpp:224) already map RW/no-exec ArrayPages at 0x75/0x74 from
`os/OSProcess.cpp:127,134`. 0x76 = a third `map_pages`, called at the same
site, mapping only the pages the placed `v`s touch. **New work: ~40 lines,
one precedent to copy.**

*The index wrap.* IR.md §5.2's uniform wrap is `(e & 0x0FFFFFFF) | seg`
(IRExec.cpp:894; `Machine::copy_segment` Machine.hpp:121 is the same
arithmetic). It replaces only the top NIBBLE (the ring), so 0x74/0x75/0x76
addresses survive it unchanged from any ring-7 block — this is the actual
reason ring 7 is right and 0x4 was wrong, and it holds for 0x77 too:
`seg` of a 0x77 block is `0x77000000 & 0xF0000000 = 0x70000000`. **No
executor change.**

*Dispatch.* `Machine::run` (Machine.cpp:276–278) asks
`IRExec::instance->has(pc)` BEFORE any memory fetch, and `run_block`
(IRExec.cpp:992) reads no memory at the block's own address; `find` is a
binary search on block start (IRExec.cpp:872). A 0x77 pc is therefore a
block identity by construction: nothing is ever fetched there, no page
need be mapped, and `goto` returning a 0x77 label re-enters the same path.
`ret` decodes WRTN through `segments[(start>>28)&7]` = segment 7, as for
0x70 (IRExec.cpp:1303). **No executor or Machine change.** The one gate is
`lockstep_role == CLONE` (Machine.cpp:276): 0x77 blocks run only on the
clone, as every IR block does today.

*Loader.* Currently REFUSES a block header not in the sync list
(IRExec.cpp:462) and a goto label not in it (:581/:596), requires the
`blocks` provenance line (:820) and parses only hex8 block/label tokens.
Changes: the name parser for `<ENTRY>.v/b`, the two address allocators,
the forward-reference table for labels, the relaxed provenance rule, the
refusals of §1. **This is where the work is: ~250 lines in IRExec.cpp,
all in `load()` and `Parser::primary()` (:181), plus a `load_file(path)`
entry so the self-test can load without QUEST_IR/-lockstep.**

*Address assignment.* Entry index = the addrbook FILE order over all 130
lines including the 28 `#` (unmigrated) ones — `AddressBook::instance`
holds only the 102 migrated entries (AddressBook.cpp:36 skips `#`), so the
IR loader reads QUEST_ADDRESS_BOOK itself for names. Per-entry reserved
range 0x10000 words in each space (130 × 0x10000 = 0x820000 < 0x1000000;
the whole 0x74 area is 40,428 words, so 65,536 words per routine is ample).

- block `<E>.b<k>` → `0x77000000 + idx(E)·0x10000 + k` — a pure function
  of the name (k ≥ 0x10000 refuses); deterministic, no loader state,
  invertible for diagnostics.
- `v` → sequential within `0x76000000 + idx(E)·0x10000`, in declaration
  order, by size; overflow of the range refuses.
- "Two `v`s at one address": with no placement input in P46 there is no
  way to ask for it. The allocator asserts disjointness (loud), and the
  refusal will sit on the placement input when that exists — **this is
  the one hard-error of the prompt with no reachable trigger in P46.**

*Lockstep side effects to record in the spec (not exercised here).*
A 0x77 arrival is not a listed sync-list pc, so `block_ordinal` does not
tick (Machine.cpp:306); a 0x76 pointer in a register at a rendezvous
decodes as Mapper form `None` and would MISMATCH against a master stack
pointer (Mapper.hpp:105–108). Both are P48 (substitution) facts; ir 7
states them and does nothing about them.

---

## 4. The rename's blast radius — measured

`grep -rc 't@'` over the tree excluding `docs/attic` (49 files; the
`emulation/emulator` binary and `docs/*` prose omitted below):

| file | `t@` lines | twin tokens | role |
|---|---|---|---|
| emulation/quest.ir2.book | **198** | **236** | 160 lines carry one twin, 38 carry two |
| emulation/quest.ir2.stock | 198 | 236 | same |
| emulation/quest.arena | 59 | 57 | 57 twins + 2 comment lines |
| emulation/quest.strhooks | 1 | 0 | a comment line |
| **docs/Project33/p33.tsv** | 229 | 503 | **lower.py's INPUT** (`--strings-sites33`) |
| docs/Project33/p33.ledger | 199 | — | string_sites.py output |
| docs/Project31/strings.ledger | 198 | 236 | **lower.py's OUTPUT** (`--strings-census`) |
| docs/Project32/p32.ledger | 111 | — | frozen P32 artifact (comments) |
| emulation/tools/string_sites.py | 9 | — | **mints the token**: `p33_name()` :455 |
| emulation/tools/arena.py | 5 | — | writes `quest.arena` names |
| emulation/tools/strhooks.py | 2 | — | comment lines |
| emulation/tools/lower.py | 1 | — | a comment only — it copies the `ir` column of p33.tsv verbatim |
| emulation/hw/IRExec.cpp | 12 | — | atom parser :213, claim/release :611–619, version message :399 |
| emulation/hw/strings/Arena.cpp/.hpp | 5 | — | quest.arena parser :47 |
| emulation/hw/strings/StrHooks.cpp/.hpp, Lockstep.cpp, Mapper.hpp | 7 | — | diagnostics / comments |
| emulation/tests/strhooks_selftest.cpp | 8 | — | synthetic arena fixtures |
| compiler/ircmp.py | 8 | — | TWIN_RE/CLAIM_RE/RELEASE_RE :49–52, re-minting :298; its `--selftest` reads the book |
| compiler/readable.py | **0** | — | see finding F1 |
| docs/IR.md | 11 | — | §5.9 |

**Corrections to the prompt:**

- **The count is 198 LINES (statements) and 236 tokens.** Both artifacts.
  The token-only proof will be stated on tokens.
- **lower.py does not own the spelling.** The chain is `arena.py` →
  `string_sites.py --p33` (writes p33.tsv, whose header carries the
  arena's sha256) → `lower.py` (which DIES if p33.tsv's arena sha ≠
  `--arena`, lower.py:1187). Regenerating `quest.arena` therefore FORCES
  regenerating `docs/Project33/p33.tsv` and `p33.ledger`, and lower.py's
  `--strings-census` rewrites `docs/Project31/strings.ledger`. All three
  are outside the writable boundary. **RULING R1:** extend the boundary to
  `docs/Project33/p33.{tsv,ledger}` and `docs/Project31/strings.ledger`
  (regenerated by their own tools, provenance headers intact), or rule
  that lower.py renames at ingest (which I would call the `sed` the prompt
  forbids, in a different coat).
- **`compiler/ircmp.py` is outside the boundary and breaks.** Its regexes
  hard-code `t@` and its selftest reads the book; after the rename the
  selftest goes red. **RULING R2:** add ircmp.py to the writable set for
  the three regex lines and :298, or accept a known-red ircmp selftest
  until P48.
- **`compiler/readable.py` has nothing to rename** (finding F1, §6): the
  `p@b ≡ t@b.last` convention is documented in IR.md §5.9 and
  CURRENT_STATE.md but readable.py never implemented it. Its touch in
  Stage A is a no-op; the spec line changes.
- The "ONLY textual change" cannot be literally true: the version line
  (`ir 6` → `ir 7`) and the `strings33`/`arena` provenance sha lines
  change because their inputs change. The proof will be: regenerate; then
  `diff <(sed 's/t@/s@/g' old) new` on a SCRATCH copy must show exactly
  the header lines named above and nothing else; plus token census before
  (`t@`: 236, `s@`: 0) and after (`t@`: 0, `s@`: 236). The `sed` is the
  yardstick, never the mechanism. Regeneration uses the Provenance.md
  P33-B command chain verbatim (strhooks.py → arena.py → string_sites.py
  --p33 → lower.py), and slice-6 output is checked byte-identical to the
  P32 artifact except the header, as the ir 6 bump was.

**Stage A version bump.** DESIGN §4.3 ties the rename to the ir 7 bump.
Stage A lands `ir 7` = "ir 6 with `s@`"; the loader refuses `ir 6`; Stage
B extends the same version with `v`/blocks. Two dialects under one number
is the alternative and is worse.

---

## 5. How far execution goes — the self-test

Nothing in the book jumps to 0x77 and nothing can call out of a symbolic
block yet, so the proof is a hand-written program run on the executor
directly. `tests/vform_selftest.cpp` + `tests/run_vform_selftest.sh`,
built like `run_strhooks_selftest.sh` (links the emulator objects; scratch
`Machine`+`Memory` rigs; a synthetic 4-entry addrbook and the program
written to /tmp by the test itself, so the test is self-contained).

**Positive leg — `tests/vform_selftest.ir`, hand-written, covers:**
1. every `vtype` declared, placed, and read/written through M16/M32/M8 with
   the explicit `sx16`/`zx16`/`trunc16` conventions;
2. an effectful op storing to a `v` (`M32[A.v1] = add(M32[A.v1], k)`) — the
   flag path and the ovk check attributed to a 0x77 block;
3. a loop over a `v` counter with a BACKWARD symbolic edge and a FORWARD
   reference used before its header (`goto [A.b2, A.b5] (…)` where b5 is
   defined later);
4. a three-way `goto [A.b6, A.b7, A.b8] idx` table with an out-of-range
   index checked to FAULT (loud);
5. a cross-entry reference (`A.1.b0` reads `A.v0`);
6. a varying `v` assigned from a literal longer than its capacity
   (truncation), a `cmp` against it, and a `bp(A.v3, 0)` byte read;
7. `ret` from a symbolic block over a rig-built WSAVS frame (the strhooks
   selftest already executes WRTN this way);
8. the block-name → 0x77 address function and the `v` → 0x76 allocation
   checked by query (`IRExec::v_address`, `block_address`), every `v` at
   0x76, every block at 0x77, all 0x76 pages RW/no-exec, NO 0x77 page
   mapped;
9. **dispatch through `Machine::run` itself** (role = CLONE,
   `IRExec::instance` set): the rig calls `machine.run(A.b0's address)`
   and asserts the block-entry trace and final ac0–ac3 / `v` memory
   contents against hand-computed expectations — so the Machine.cpp:276
   path is under test, not only `run_block`.

**Teeth leg — each expected to REFUSE at load with the named message:**
lowercase entry; `QUEST.x0`; `QUEST.v` (no digits); unknown entry; `v`
read before declaration; duplicate `v`; duplicate block header; `goto` to
a never-defined block; hex8 label in 0x77; literal `0x76000010`; `@addr`
inside a symbolic block; `rt_call` inside a symbolic block; `ir 6` header;
`v` line inside a block; and (if ruled in) `M32[<i16 v>]`.

**Strict-surface leg:** the three existing self-tests, and one K=1 play
leg on the regenerated book (checker on) — the ir 6-with-rename program
must run identically. Budget per METHOD §15: the login-fast leg.

**Not covered, said plainly:** the Mapper/checker's view of 0x76 pointers
at a rendezvous; ordinal counting for 0x77 arrivals; any call, `rt_call`
or `@addr` from a symbolic block; placement to 0x74 (no placement input
exists); the compiler's emission shape (P47); substitution (P48).

---

## 6. DESIGN §4–§6 vs IR.md — what does not survive contact

**F1. "readable layer's `p@b ≡ t@b.last`" (DESIGN §4.3, prompt) —
readable.py does not implement it.** The identity exists only in prose
(IR.md §5.9, CURRENT_STATE.md) and one comment in strhooks_selftest.cpp.
The spec line will become `s@b ≡ s@b.last`; there is no code to rename.

**F2. Types (DESIGN §4.1) have no consumer in the IR.** IR.md carries
width and sign in the operator; a `v`'s "type" reduces to a size for the
allocator. i16 and u16 are indistinguishable at the IR level. Kept as
declaration vocabulary (the compiler and the future merge/binding need
them) with an optional width tripwire — §1.2 RULING.

**F3. DESIGN §4.1's type list is incomplete for PL/I locals.** Fixed
`CHAR(n)` (IR.md §5.8's `[@a, n]` form, 7 book sites are fixed-string
assignments) and arrays/records are missing. Proposed `char n` and `words n`
(§1.1). Without `words`, the first routine with a local array is
unexpressible.

**F4. DESIGN §6 "goto runs unresolved" vs IR.md §4's sync-list rule.**
Every goto label today must be a SHIPPED sync-list pc, and every listed
arrival ticks the block ordinal. Symbolic labels cannot be listed. ir 7
scopes the listing rule to numeric labels and states that 0x77 arrivals
are uncounted — the pairing consequence belongs to P48, which will already
be delisting the replaced routine's blocks.

**F5. DESIGN §6's last paragraph contradicts its own ruling 9.** "Block
ordering is observable: order changes fall-through, and branch polarity
flips with it" describes the BOOK's lowering of machine skips
(`[no-skip, skip]` ascending, IR.md §6), not a property of the IR: IR.md
§4 has no fall-through (every terminator explicit), and `goto [X,Y] c` vs
`goto [Y,X] !c` is a polarity choice with no ordering behind it. Carried-in
ruling 9 (numeric order carries no meaning) is the one the spec keeps; the
"entangled with if/else reversal" sentence is an oracle/`ircmp` concern
(P48), not a loader one.

**F6. "Placement changes an address and nothing else" (DESIGN §5.2) is
true of the address and false of the TEXT the book uses.** The book never
spells a local as an absolute 0x74 constant; it spells `wp(ac3, d)`
(register-relative, IR.md §5.2, "no wp(0,d) is ever emitted"). A placed
`v` is an absolute constant. Equivalence between `M16[0x74001A04]` and
`M16[wp(ac3, 6)]` is a fact `ircmp`'s slot bijection must supply — it is
not in the IR, and it is not free. Recording it now so P48 does not
discover it as a match failure.

**F7. A naive program cannot call anything.** `rt_call ... site=` and
`call ... site=` are validated against LCALL words at a real site
(IR.md §6); a compiled routine calling `?WRITE_SCREEN` has no site. DESIGN
§9.3 calls runtime calls "free"; in the IR they are free only from a real
block. ir 7 refuses them in symbolic blocks; the calling bridge is P48's
keystone and the biggest thing between "loads and runs" and "substitutable".

**F8. `#` addrbook entries.** DESIGN §4.1 says names are qualified by
addrbook entry; the runtime's `AddressBook` drops the 28 `#` entries. The
namespace must be all 130 (a `nocall` ON-unit is still a routine the
compiler may emit), so the IR loader reads the file for names and indexes
by file order — stable as long as lines are never reordered, which the
addrbook's own header already demands ("keep the columns").

**F9. The `blocks` provenance line is mandatory today** (IRExec.cpp:820).
A symbolic-only program has no CFG. ir 7 makes the line conditional (§1.4);
the strict surface is unchanged because any numeric block still forces it.

---

## Rulings requested

| # | question | my recommendation |
|---|---|---|
| R1 | boundary: regenerate `docs/Project33/p33.{tsv,ledger}`, `docs/Project31/strings.ledger` | yes — their own tools, headers intact |
| R2 | boundary: `compiler/ircmp.py` (4 lines) | yes, or accept its selftest red |
| R3 | `v` as address constant, no bare-name lvalue (§1.2) | address constant |
| R4 | `char n` and `words n` types (F3) | add both |
| R5 | width tripwire (`M32` on a 16-bit `v` refuses) | land it; cheap, catches the compiler early |
| R6 | symbolic block header without `seg` | omit `seg` |
| R7 | refuse `@addr` / `call` / `rt_call` in symbolic blocks (F7) | refuse in ir 7 |
| R8 | Stage A ships `ir 7` header | yes |

STOP — awaiting `a001-plan-gate.md` before Stage A. (Verified on main 7279d22: every file cited above is byte-identical to the tree analysed.)
