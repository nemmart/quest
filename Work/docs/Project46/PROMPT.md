# Project 46 — ir 7: `v` declarations, symbolic blocks, the `s@` rename

## GOAL

Extend the IR so that a **naive, unplaced program is loadable and runnable**:
declared variables with no address yet, symbolic block names with no address
yet, and the loader assigning both. Plus one pure rename that must happen
before any oracle file exists with the old spelling.

Success is a number and an artifact:

- **`quest.ir2.book` and `quest.ir2.stock` regenerate with the ONLY textual
  change being `t@` → `s@`** (198 sites), diff-proven token-only, Provenance
  rows updated.
- **A hand-written v-form IR program loads, places, and executes correctly**
  under a self-test, with every `v` at 0x76 and every block at 0x77.

This project does **not** build the compiler (P47), the substitution harness,
or any transformation. It builds the representation they will both target.

---

## Context of record — read in this order

| path | why |
|---|---|
| `docs/Project44/DESIGN.md` | **Read first.** §4 storage classes, §5 address spaces, §6 blocks. Normative for this project |
| `docs/IR.md` | THE spec (currently **ir 6**). You are writing ir 7. Spec-wins; you are changing the law, so change it deliberately |
| `emulation/tools/lower.py` | Generates the book. Owns the `t@` spelling |
| `emulation/tools/arena.py` | Generates `quest.arena` from `quest.strhooks` |
| `compiler/readable.py` | Renders the readable layer; owns `p@b ≡ t@b.last` |
| `docs/Provenance.md` | Artifact checksums of record — you will add rows |
| `docs/METHOD.md` | Law for implementation sessions |
| `emulation/quest.addrbook` | The entry names your namespaces must match |

---

## Carried-in rulings (from the Sep 12 design session)

1. **`t@<block>.<k>` → `s@<block>.<k>`.** The arena twins collide in spelling
   with `t0…t8`, which they have nothing to do with. Readable layer's
   `p@b ≡ t@b.last` becomes `s@b`. This is a **token rename and nothing
   else** — prove it.
2. **Rename by REGENERATION, not by editing artifacts.** Change `lower.py`
   and `arena.py`, regenerate, then diff old vs new and demonstrate that the
   only differing tokens are the renamed ones. A `sed` over `quest.ir2.book`
   is a STOP-and-report, not a shortcut.
3. **Names are qualified by addrbook entry, uppercase, `@ADDR` dropped:**
   `<ENTRY>.v<digits>` and `<ENTRY>.b<digits>` — `QUEST.v0`, `QUEST.1.b0`,
   `FIRE.1.b7` (from `FIRE.1@7016A3BD`). Parsing is unambiguous on two
   counts: an entry suffix is `.<digits>` while a local is `.v<digits>` /
   `.b<digits>`, and entry names are uppercase while `v`/`b` are lowercase.
4. **`v` is fixed size** — i16, u16, i32, u32, or a varying string with a
   declared capacity — because a `v` is eventually laid down exactly on top
   of the Eagle allocation at 0x74. Arbitrary-length string temporaries are
   `s@` twins, not `v`s; assigning a twin to a `v` truncates or pads.
5. **`t` is not a peer class.** It is the byproduct of lowering ONE
   instruction. Never bound, never mapped, never addressed. Do not extend it.
6. **The declaration does NOT say where the variable lives.** The loader
   places it. Empty oracle ⇒ every `v` gets its own private 0x76 slot,
   aliasing impossible, **and the program runs**. This default is the whole
   point: it is what makes a naive program testable before any model exists.
7. **Placement is not binding.** Placement (0x76 → 0x74) changes an address
   and nothing else, so the loader owns it. Binding (`v` → `ac0`) deletes
   loads and stores and is a transformation — **out of scope here**.
8. **Address spaces:** 0x74 locals/args (addrbook), 0x75 arena, **0x76
   unplaced `v`**, **0x77 unplaced blocks**. All in ring 7 with the real
   ones; 0x4xxxxxxx was considered for blocks and rejected because block
   names land in the PC and 0x4 is a different ring.
9. **Numeric order of blocks carries no meaning** — every terminator is
   explicit, so there is no fall-through for address order to encode.
10. **Two `v`s sharing an address is legal only when live ranges are
    disjoint** — but that is P44's merge transformation. Here, the loader
    must simply never do it, and should **hard-error** if asked to.

---

## Part 1 — PLAN GATE. Stop and report.

Do not change `docs/IR.md` yet. Report:

1. **The proposed ir 7 grammar additions**, in IR.md's own notation: the `v`
   declaration form (name, type, capacity for varying), how a `v` is
   referenced in an expression and as an lvalue, and the symbolic block form
   in `block` headers and in `goto [...]` terminators.
2. **Where declarations live.** Header of the book? A sidecar file keyed like
   `quest.addrbook`? Per-routine? State the choice and why. Note that the
   compiler (P47) will emit these per routine and separate compilation must
   work.
3. **Segment mechanics for 0x76 and 0x77.** What the executor needs so that
   0x76 is ordinary readable/writable memory and 0x77 dispatches as a block
   identity. Is this new work or does the existing segment/override machinery
   cover it? **Be concrete — this is the main sizing unknown.**
4. **The rename's blast radius**, measured not estimated: every file and
   every tool that contains `t@`. Confirm 198 sites in the book, or correct
   the number.
5. **How far execution goes in this project.** Nothing yet jumps to a 0x77
   block — substitution (P44 §9) does not exist. So propose the self-test
   that proves the runner works: a hand-written v-form IR program, loaded,
   placed, executed, checked against expected results. Say what it covers.
6. **Anything in DESIGN §4–§6 that does not survive contact with IR.md.**
   The design was written in conversation; the spec is the law. If they
   conflict, that is a finding, not something to paper over.

STOP. Wait for a ruling.

---

## Part 2 — Build, in this order

**Stage A — the rename.** `lower.py`, `arena.py`, `readable.py`, IR.md §5.9,
the loader. Regenerate `quest.ir2.book`, `quest.ir2.stock`, `quest.arena`.
Diff-prove token-only. Update `docs/Provenance.md`. Re-run the self-tests
(`tests/run_helpers_selftest.sh`, `run_strings_selftest.sh`,
`run_strhooks_selftest.sh`) and a lockstep play leg. **Land this before
starting Stage B** — it touches existing artifacts and must not be entangled
with new features.

**Stage B — the spec.** Write ir 7 into `docs/IR.md`: `v` declarations,
symbolic blocks, the loader's placement rules, the 0x76/0x77 spaces, and a
version-history entry. IR.md is normative and self-contained; keep it so.

**Stage C — loader and executor.** Placement of `v` at 0x76 and blocks at
0x77 within per-entry reserved ranges. Hard-error on: two `v`s placed at one
address, an unknown entry name, a malformed qualified name, a `v` reference
with no declaration.

**Stage D — the self-test.** As agreed at the gate. It must be runnable from
`tests/` alongside the existing ones.

---

## Boundaries — BINDING

1. **You may WRITE:** `docs/IR.md`, `docs/Provenance.md`,
   `docs/Project46/**`, `emulation/**` (tools, loader, executor, tests,
   regenerated artifacts), `compiler/readable.py`.
2. **Do NOT touch** `Disassembled/**`, `game/**`, `docs/Salvage.md`,
   `docs/attic/**`, `docs/Project44/DESIGN.md`, `docs/NextSession.md`,
   `docs/README.md`.
3. **P45 (attic salvage) is running in parallel** and owns `docs/Salvage.md`,
   `docs/Project45/**`, `game/declarations.json`, and annotations in
   `docs/attic/**`. Do not write there.
4. **The strict surface is untouched.** ir 6 programs must continue to load
   and run identically after ir 7 lands, the rename aside. Any change that
   alters existing block semantics is a STOP-and-report.
5. **No transformation, no binding, no merging, no compiler.** If you find
   yourself needing liveness analysis, you have left this project's scope —
   stop and report.
6. **Do not read `docs/attic/**`.** It is void and will cost you context you
   need. If something there seems necessary, say so in the report.

---

## Part 3 — Report

`docs/Project46/REPORT.md`:

- the rename diff evidence — how you proved token-only, and the site count
- the ir 7 grammar as landed, and any departure from DESIGN §4–§6 with the
  reason
- the self-test: what it covers and what it does not
- Provenance rows before/after
- self-test and play-leg results
- **anything the design got wrong** — this is the first contact between the
  P44 design and the real spec, and disagreements are the most valuable thing
  you can report


---

## Coordination — the question channel (BINDING)

**You have the repo. Use it to ask questions; do not wait on chat.**

When you need a ruling, write `docs/Project46/q001-short-title.md`
(`q002`, `q003`, …), commit, **push to main**, and **keep working on
anything not blocked**. The integrator pulls, writes
`docs/Project46/a001-short-title.md` alongside it, and pushes. You own
`q*.md`; the integrator owns `a*.md`; neither edits the other's files.

Each question must contain, because the integrator has none of your context:

- what you were doing and what you found
- the decision you need
- the options you see, with your read on each
- **YOUR DEFAULT — what you will do if nobody answers.** Mandatory. It keeps
  you moving and tells the integrator how urgent this actually is.

Re-read `a*.md` before resuming affected work; an answer may land after you
have moved on. Full SOP: `docs/INTEGRATOR.md` §10.

**Push questions to main, not to your branch** — a question on a branch is
invisible.

## Delivery

Send a **`Work.tgz` of the whole tree**, not selected files. Deliver at the
gate, after Stage A, and at the end.
