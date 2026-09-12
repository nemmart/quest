# Project 46 — REPORT: ir 7 (`v` declarations, symbolic blocks, the `s@` rename)

Worker session, Sep 12 2026. Branch `p46-ir7`. Gate: `q001-plan-gate.md`,
rulings `a001` (R1–R8 all granted), `a002` (no action), `a003` (Stage A
green; task 050's `exit=1` pre-existing). Evidence files under
`docs/Project46/evidence/`.

**Outcome.** Both success criteria of the prompt met:

- `quest.ir2.book` / `quest.ir2.stock` regenerated with the only textual
  change being `t@` → `s@` — **236 tokens on 198 statements** per artifact
  (the prompt's "198 sites" was the line count), proven token-only by
  regeneration and a scratch `sed` yardstick; Provenance rows added.
- A hand-written v-form IR program **loads, is placed, and executes
  correctly** under `tests/run_vform_selftest.sh` — every `v` at 0x76……,
  every block at 0x77……, dispatched through `Machine::run_steps` itself;
  84 cases green, a 33-case teeth leg, and a broken-allocator build that
  must go red.

The strict surface is untouched: the renamed ir 6 program runs the full
16-leg battery (task 050) with 0 divergences, and the Stage C/D loader runs
k1fo (K=1, checker on) at 0 div / 307,197 pairs on the unchanged book.

---

## 1. Stage A — the rename (evidence/stageA_rename.txt)

**Method.** The P33-B chain of record (`strhooks.py → arena.py →
string_sites.py --p33 → lower.py`) was first run *unchanged*: all seven
artifacts reproduced byte-for-byte. Then the tools were renamed and the
chain re-run. Nothing was edited by hand.

**Who mints the token.** Not `lower.py` (prompt) — `string_sites.py:455`
(`p33_name`) writes it into `docs/Project33/p33.tsv`, whose header carries
`quest.arena`'s sha256, which `lower.py:1187` verifies against `--arena`.
Regenerating the arena therefore *forces* regenerating p33.tsv/p33.ledger,
and `--strings-census` rewrites `docs/Project31/strings.ledger`. R1 extended
the boundary to those three.

**Proof — token census (twin tokens `t@|s@` + hex8 [+ `.k`]):**

| artifact | old `t@` | old `s@` | new `t@` | new `s@` |
|---|---|---|---|---|
| quest.ir2.book | 236 | 0 | **0** | **236** |
| quest.ir2.stock | 236 | 0 | 0 | 236 |
| quest.arena | 57 | 0 | 0 | 57 |
| docs/Project33/p33.tsv | 503 | 0 | 0 | 503 |
| docs/Project33/p33.ledger | 236 | 0 | 0 | 236 |
| docs/Project31/strings.ledger | 236 | 0 | 0 | 236 |

**Proof — the yardstick.** `diff <(sed 's/t@/s@/g' OLD) NEW` on a scratch
copy of each old artifact. The `sed` was the *measuring stick only*; the
mechanism was regeneration. The diff is exactly:

- book/stock: the version line (`ir 6` → `ir 7`) and the `strings33` /
  `arena` provenance sha lines (their inputs changed) — 3 lines each;
- quest.arena: its `generated from quest.strhooks sha256` comment (1 line);
- p33.tsv / p33.ledger: their `# strhooks sha256` / `# arena sha256` header
  lines (2 lines each);
- quest.strhooks, strings.ledger: **empty**.

So "the ONLY textual change is `t@` → `s@`" is true of every non-header
line, and every header line that moved names the input whose sha moved.

**Code sites renamed.** `arena.py`, `strhooks.py`, `string_sites.py`,
`lower.py` (header → `ir 7`); `IRExec.cpp` (atom parser, claim/release,
version refusal), `Arena.cpp/.hpp`, `StrHooks.cpp/.hpp`, `Lockstep.cpp`,
`Mapper.hpp`, `tests/strhooks_selftest.cpp`; `compiler/ircmp.py` regexes
:49–52 and the re-mint :298 (R2). `readable.py` had nothing to rename (the
`p@b ≡ t@b.last` convention lived only in prose — gate F1). The only
residual `t@` in the binary is the refusal message telling an ir 6 file
what changed. ircmp.py's :284/:286 still render `claim t@`/`release t@` in
its *internal* canonical form (both sides rendered alike, selftest PASS);
left per R2's "nothing else" — worth a one-line follow-up in ircmp when P48
opens it.

**Provenance rows** (sha256 first 16, old → new): book e2f18f144c195da4 →
9d31d80967e63767; stock c4340b497ea16eda → 79fbf07abef83626; arena
64ab09d44e343c36 → bb779f499772125f; strhooks 9cc8c96cbad2fd28 →
f4251e4c8a7c1200; p33.tsv a9fb1bbb24784e8c → 1456de7dd146b46b; p33.ledger
a90dd01a8dbbfd2b → acb39a97cc041afe; strings.ledger 7aaf0cc8164ed78c →
a2cc104ad270d471. `quest.synclist.p27` unchanged. Table in
`docs/Provenance.md`.

**Play legs.** In-container k1fo: 0 div, 299,793 K=1 pairs, blk_equal all,
end clean, master hook counts identical to 047b's k1fo
(evidence/stageA_k1fo.txt). Runner task 050 (047b's 16-leg battery
verbatim + a zero-`t@` census verdict): **16/16 legs OK, 0 div on every
non-forced leg, ~6.6 M pairs, census exactly 0/236 0/236 0/57, `forced`
still names an unmapped twin** — the checker's teeth survived the rename.

**Task 050 exited 1 — say it plainly.** `results/050-p46-stageA/FAILED`
says `exit=1`. So does `results/047b-p33b-arena-temps/FAILED`, the accepted
reference run of the same battery from before P46. I diffed the two verdict
files (evidence/task050_exit1_preexisting.txt): the *only* differences are
the `ir 7` header line and the new census line; the four failing `want`
lines (`embeds_book` wants 729, is 557; `string_statements` wants 1593, is
1689; literal assignments 857 vs 871; the ledger `same pc set=NO`) are
byte-identical in both runs. They are 047b-era expectations never refreshed
after P33-A/B/C (the same file's P33 section wants 557 and passes). Not a
regression from the rename. The integrator recorded the fix as task 051
(a003): a battery whose FAILED marker is always set cannot detect a failure.

---

## 2. Stages B–D — ir 7 as landed (IR.md §2, §3, §4, §5.1, §5.10, §7, §8, §9)

### 2.1 Grammar

    v <ENTRY>.v<digits> <vtype>          vtype := i16 | u16 | i32 | u32 | char n | varying n | words n
    primary += <ENTRY>.v<digits>         the placed word ADDRESS, a constant (R3) — no bare-name lvalue/rvalue
    block <ENTRY>.b<digits>              no seg (R6)
    goto [<label>, ...] e                label := <hex8> | <ENTRY>.b<digits>; mixed; forward references legal

Names: uppercase addrbook entry, `@ADDR` dropped, split on the last dotted
component; the namespace is all 130 entry lines of the addrbook including
the 28 `#` unmigrated ones, `idx(E)` = file order (F8).

### 2.2 Placement (loader-owned)

- `addr(E.b<k>) = 0x77000000 + idx(E)·0x10000 + k`, k < 0x10000 — a pure
  function of the name, assigned on first sight (header or label); an
  undefined label refuses at end of file.
- `v`: sequential within `0x76000000 + idx(E)·0x10000`, declaration order,
  by size in words (1/1/2/2, ceil(n/2), 1+ceil(n/2), n); range overflow
  refuses.
- Disjointness is asserted loudly. **It cannot fire in ir 7** — there is no
  input that could ask for a shared address; the spec says so next to the
  assertion so nobody reads it as a check that passed. `run_vform_selftest.sh`
  proves the hook is live by compiling `-DP46_BROKEN_ALLOC` (the cursor never
  advances) and requiring that build to go red on exactly that message.
- Literal constants in [0x76000000, 0x78000000) and hex8 labels/headers in
  that range refuse anywhere: only a name denotes a synthetic address.

### 2.3 Refusals (the teeth leg, 33 cases, each with its message)

no addrbook; lowercase entry; `.x` local; missing digits; unknown entry;
reference before declaration; duplicate `v`; duplicate block; goto to an
undefined block; hex8 label in 0x77; literal 0x76 constant; literal 0x77
byte pointer; `@addr` / `rt_call` / `call` inside a symbolic block (R7);
`ir 6` header; `v` inside a block; the width tripwire (R5): `M32` on i16,
`M8` on u32, `M16` on char, `M32` on varying, a fixed piece on a raw `v`
(needs `bp`), a varying piece whose capacity ≠ the declaration, a varying
piece on an i32; `seg` on a symbolic header; `char 0`; `words 40000`; an
unknown type; block ordinal ≥ 0x10000; the trailer counting symbolic blocks;
a block name in an expression; overflowing an entry's 0x76 range; a numeric
block still needing the sync list.

### 2.4 Executor and Machine

No executor change was needed for either space, exactly as sized at the
gate: the index wrap preserves bits 27:0, `Machine::run` consults
`IRExec::has(pc)` before any fetch, `ret` decodes WRTN through segment 7.
The 0x76 pages are mapped RW/no-exec on the clone next to the arena's
(`os/OSProcess.cpp`, +7 lines); 0x77 is never mapped.

**One Machine.cpp touch, 2 lines, worth knowing about:** `run_steps`
dereferenced `process->poke_armed` and `process->inject_armed`
unconditionally (the QUEST_POKE / QUEST_INJECT harness knobs). A scratch
Machine with no OSProcess — what every self-test builds — segfaulted there,
so the real dispatch path was un-testable outside a full process. Guarded
with `process &&`; behaviour-neutral in the emulator, where `process` is
never null. Without this the self-test could only have driven `run_block`,
which is what the gate wanted to avoid.

### 2.5 Sizing: did ~250 lines hold?

The gate estimated ~250 lines in `IRExec.cpp` `load()`/`primary()` plus a
`map_pages` copied from the arena precedent. Landed: **IRExec.cpp +267/−16,
IRExec.hpp +28/−1, OSProcess.cpp +7, Machine.cpp +5/−2** — 307 insertions.
The estimate held for the loader; the two things it did not foresee were
the `process &&` guards (2.4) and `Decoder::initialize()` being needed in
the rig (WRTN's fixed opcode decodes through the table; the strhooks rig
already did this, I had not noticed). The self-test itself is 328 + 26
lines. Total Stage C/D wall time in-container, including the two
misfires: ~1 h.

---

## 3. The self-test — what it covers and what it does not

`tests/vform_selftest.cpp`, `tests/run_vform_selftest.sh` (runs from
`tests/` like the others; builds against the emulator's objects; writes
its own synthetic 4-entry addrbook — migrated, nested `.1@`, `#`-unmigrated
`.2@`, plain — and program to /tmp).

**Covered (84 cases, GREEN; evidence/stageCD_selftests_k1fo.txt):**

1. teeth — the 33 refusals above, each matched on its message;
2. placement — nine `v`s of every vtype across three entries at the
   addresses §5.10.3 predicts; five block addresses incl. `ALPHA.1.b0` =
   0x77010000; `has()` true for every 0x77 block; 0x76 pages RW/no-exec,
   an untouched 0x76 range not mapped, **no 0x77 page mapped**;
3. execution **through `Machine::run_steps`** (role CLONE,
   `IRExec::instance` set — Machine.cpp's IR dispatch, not `run_block`):
   sx16/zx16/trunc16 conventions on i16/u16; an effectful `add` storing to a
   `v`; a five-iteration loop with a backward symbolic edge and a forward
   reference; a three-way `goto` table taking arm 2; a nested entry reading
   its parent's `v` (cross-entry); an unmigrated entry's `v`; a `words`
   aggregate by offset and directly; a varying `v` assigned from a longer
   literal (length 8, `HELLO WO`), a fixed `char 6` (`HELLO `), `cmp`
   between them (+1), `M8[bp(v, 4)]`; `assert`; `ret` over a rig-built
   WSAVS frame with the WRTN residues (wfp, ac0–ac3, c) checked;
4. an out-of-range `goto` index is a loud FAULT;
5. the broken-allocator build goes RED on the disjointness assertion.

**Not covered, said plainly:** the Mapper/checker's view of a 0x76 pointer
in a register at a rendezvous (would decode as form `None` → MISMATCH);
block-ordinal counting for 0x77 arrivals (unlisted → uncounted); any
`call`, `rt_call` or `@addr` from a symbolic block (refused in ir 7; the
calling bridge is P48's); placement to 0x74 (no placement input exists);
the compiler's emission shape (P47); substitution (P48); a v-form program
loaded through `QUEST_IR` in a lockstep pair (nothing in the game jumps to
0x77 yet — the run would be a no-op for the new blocks).

**Strict surface:** helpers / strings / strhooks self-tests GREEN with
teeth on the final binary; k1fo on the final binary with the unchanged ir 7
book: 0 div / 307,197 pairs / blk_equal all / clean; zero `0x76 space`
mapping lines (the book declares no `v`). Task 052 re-runs the full battery
on the final branch (queued; `run_vform_selftest.sh` added as a
precondition).

---

## 4. Anything the design got wrong — the full list

The gate's F1–F9 stand (F5/F6/F7 were accepted into DESIGN by a001; a002
refined F6: the slot bijection is mostly derivable). Found while building:

**F10 — a scratch Machine cannot run.** Not a design fault but a
verification-surface fact the design implies and the tree contradicted:
"a block name must be a first-class block identity in the runtime"
(DESIGN §6) is only testable through `Machine::run_steps`, which could not
run without an OSProcess (2.4). Two lines; recorded because the next
harness (P48's substitution rig) will hit the same wall on whatever else
in `run_steps` assumes a process.

**F11 — `[@<v>, n]` is a trap the design's "address constant" framing
invites.** A `v` name is a WORD address; a fixed located string takes a
BYTE pointer. `[@QUEST.v3, 6] = …` parses, would compute a garbage byte
address, and would fault somewhere downstream. The width tripwire now
refuses it with "needs a BYTE pointer — `[@bp(QUEST.v3, 0), 6]`". The
compiler (P47) must emit `bp(v, 0)` for every fixed-string operand; worth
a line in its prompt.

**F12 — the varying capacity is stated twice.** `v QUEST.v2 varying 8`
and `[@QUEST.v2, 8 varying]` both carry the 8 (DESIGN §10 says the
capacity "is in the code", which is the §5.8 form). Two sources of one
fact is a drift risk; the loader refuses a mismatch, so the redundancy is
checked rather than trusted. An alternative — `[@QUEST.v2, varying]` as an
lvalue meaning "the declared capacity" — was rejected because IR.md §5.8
reserves that spelling for reads with no capacity, and overloading it would
change an existing production.

**F13 — `i16` vs `u16` (and `i32`/`u32`) is not observable anywhere in
ir 7.** F2 said the types have no IR consumer beyond size; building it
confirmed the four scalar names collapse to two sizes and the tripwire
cannot tell `i16` from `u16` (a `zx16` read of an `i16` is legal — the
program does it on purpose in `ALPHA.b2`). They are kept as declaration
vocabulary for P47/P48. If the merge/binding transformations never consume
the sign either, the pair should collapse to `w16`/`w32`; decide then, not
now.

**F14 — the addrbook's `#` lines are load-bearing for the namespace and
nothing else reads them.** `AddressBook.cpp:36` drops `#` lines; the IR
loader now parses the same file for names including them. Two parsers of
one file with different views. If someone ever *removes* an unmigrated line
rather than commenting it, `idx(E)` shifts for every entry below it and
every 0x76/0x77 address moves — harmless today (nothing persists those
addresses), but the day a placement file or an oracle keys on a 0x76
address it becomes a silent renumbering. Recommendation: when the placement
input is designed (§8), key it by name, never by 0x76 address.

**F15 — DESIGN §5.1's headline metric needs one more word.** "Oracle length
is a count of unexplained decisions, starting at all of them" is right for
`v`s but not for blocks: a symbolic block has nothing to place *onto* until
the block-merge/split story (DESIGN §6) exists, so the block half of the
count starts undefined, not at "all of them".

---

## 5. Files

Branch `p46-ir7` (all pushed): Stage A commit a769640; Stage B 2de5c7e;
Stage C/D + this report: the head. Written: `docs/IR.md`,
`docs/Provenance.md`, `docs/Project46/{q001-plan-gate,REPORT}.md`,
`docs/Project46/evidence/*`, `docs/Project33/p33.{tsv,ledger}`,
`docs/Project31/strings.ledger` (R1), `compiler/ircmp.py` (R2),
`emulation/**` (tools, hw/IRExec.*, hw/Machine.cpp, os/OSProcess.cpp,
tests/vform_selftest.cpp, tests/run_vform_selftest.sh, regenerated
artifacts, the tracked `emulator` binary), `tasks/050-p46-stageA.sh` and
`tasks/052-p46-final.sh` on main. Not touched: `Disassembled/**`, `game/**`,
`docs/Salvage.md`, `docs/attic/**`, `docs/Project44/DESIGN.md`,
`docs/NextSession.md`, `docs/README.md`.
