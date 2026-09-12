# Project 47 — THE CALL GRAPH AND THE TRANSLATION ORDER

## GOAL

Extract the game's call graph from the book and decide, on evidence, **which
routines get translated first and in what order**.

DESIGN §9.4 makes call-graph depth the primary ordering for translation, and
nobody currently knows what the leaves are. First-routine selection is
guesswork until this exists.

Success is three artifacts:

- **`compiler/callgraph.py`** — builds the graph from `quest.ir2.book` +
  `quest.addrbook` + `Disassembled/`, and answers queries (leaves, callers
  of, callees of, depth, frontier).
- **`docs/Project47/CallGraph.md`** — the graph as a document: the leaf set,
  depth strata, the cycles if any, and every edge the extraction could not
  resolve, named.
- **`docs/Project47/Order.md`** — the proposed first ~10 routines in order,
  each with its reason, its unresolved-edge risk, and what it would cost in
  checker coverage under DESIGN §9.3.

**The number that defines success: the fraction of game call edges
RESOLVED**, reported with the unresolved remainder enumerated rather than
estimated.

---

## Context of record — read in this order

| path | why |
|---|---|
| `docs/Project44/DESIGN.md` | **Read first.** §9.3 (what a C→Eagle-game call edge costs) and §9.4 (bottom-up) are why this project exists |
| `docs/Salvage.md` | **Read second.** P45's verified facts. F1 (static link, 63/63 XCALL sites), the `.N@` family structure, R40 multiple ENTRY, R39 hand assembly. Several bear directly on edge extraction |
| `docs/IR.md` | `call` / `rt_call` forms and what a `site=` means (§6) |
| `emulation/quest.addrbook` | the 130 entries, families, `#` unmigrated, `nocall` flags |
| `docs/METHOD.md` | law for implementation sessions; §16 |
| `docs/attic/README.md` | **only** to know what not to trust; do not read the projects |

---

## Carried-in rulings and known hazards

These are facts P45 verified. They are not optional context — each one
breaks a naive extraction.

1. **`.N@ADDR` entries are NESTED PIECES of one PL/I procedure, not separate
   routines.** Flow between members of a family is ordinary intra-procedure
   control flow, not a call. The unit of translation is the **family**, not
   the addrbook entry. `compiler/crossings.py` already encodes this
   distinction — read it.
2. **Nesting is exactly one level deep program-wide** (Salvage; the five
   apparent double-nestings are addrbook misattribution of `nocall`
   ON-units). If your extraction finds two levels, that is a STOP-and-report.
3. **R40 — PL/I multiple ENTRY.** Some procedures have two entry pairs, and
   per-routine addrbook statement counts MIS-SIZE them. A multiple-ENTRY
   procedure is ONE node with two entry points. TRANSPORT_SUNDAR is a known
   pair.
4. **R39 — hand assembly.** LOCK_FILE / UNLOCK_FILE are one hand-written
   unit at 70169B0F..70169D69 and branch into each other's ranges. They are
   **out of translation scope by definition**; mark the node, do not model
   its edges as PL/I calls.
5. **ON-unit bodies are separate WSAVS frames** (LOGON.1/.2, ALLY_PLAYER.1).
   A signal edge is **not** a call edge, but it IS a reachability edge, and
   DESIGN §11 makes ON-conditions a named blocker. Record signal edges as a
   SEPARATE edge class — do not merge them into the call graph and do not
   omit them.
6. **`$N` routines are runtime, not game.** A `rt_call` is not a game call
   edge. It costs nothing under §9.3 and must not inflate a node's depth.
7. **Unlifted embeds are where the edges hide.** 159 LCALL, 130 LJSR, 37
   XCALL remain as embeds in the book. Expect to parse instruction text, not
   only `call` statements.
8. **Register-indirect calls may be unresolvable statically.** The runtime
   has `XCALL 0,0,[ac2+0x0]` forms that static reachability cannot resolve
   (Plan.md, Tooling Status). If the game has any, they are a named
   unresolved edge, not a gap to paper over.

---

## Part 1 — PLAN GATE

Write `docs/Project47/q001-plan-gate.md`, push to main, **STOP**. Report:

1. **Your extraction method**, and specifically how you will find edges that
   are embeds rather than `call` statements.
2. **Your node definition.** Family vs entry, and how you handle R40 pairs
   and the 28 `#` unmigrated entries.
3. **A first measurement** — how many edges you can see, how many nodes, and
   your estimate of the unresolved fraction. Rough is fine; it is the
   estimate I want to compare against the final number.
4. **Whether the graph is acyclic**, and if not, where the cycles are.
   Plan.md says the game is non-reentrant with ~99% confidence — a cycle
   would be evidence against that, and is a STOP-and-report finding, not a
   detail.
5. **Anything in the hazard list above that does not survive contact.**

---

## Part 2 — Build

`compiler/callgraph.py`, then the two documents.

`Order.md` is the deliverable that matters most, and it is a judgement, not
a sort. For each proposed routine state: depth, callees (and whether any is
Eagle game code that would have to be un-checked), size in statements,
constructs it needs that do not exist yet (floats, twins, bit fields,
uplevel access — Salvage has these), and whether it is reached by the
current play battery at all.

**A leaf that play never executes is a bad first target** — under L1 its
behavioural oracle never fires. Say so where it applies.

---

## Boundaries — BINDING

1. **You may WRITE:** `compiler/callgraph.py`, `docs/Project47/**`.
2. **You may READ anything** except `docs/attic/Project3*` and
   `docs/attic/Project4*` — those are void and will cost you context.
   `docs/Salvage.md` is the sanctioned extract.
3. **Do NOT touch** `emulation/**` (P46's territory), `docs/IR.md`,
   `docs/Provenance.md`, `game/**`, `compiler/` anything else, or any
   artifact. **Regenerate nothing.**
4. **This is extraction and analysis, not translation.** You write no C and
   no IR.
5. If the graph contradicts a carried-in ruling — a cycle, two levels of
   nesting, a family that is not a family — **STOP and report**. Those are
   findings about the program and they are worth more than the graph.

---

## Part 3 — Report

`docs/Project47/REPORT.md`:

- resolved-edge fraction, with the unresolved remainder **enumerated**
- the leaf set, and how many leaves the play battery actually reaches
- your gate estimate vs the final number
- every hazard that did not survive contact
- **your own read**: is bottom-up actually tractable on this graph, or does
  the shape argue for something else? DESIGN §9.4 is a ruling made without
  the graph in hand. If it is wrong, this is the project that finds out.

---

## Coordination — questions and the plan gate (BINDING)

Write `docs/Project47/q00N-short-title.md`, commit, **push to main**, then
**STOP and tell the user it is there.** Do not work ahead while a question
is outstanding. The integrator answers in `a00N-short-title.md`; the user
will tell you when to look.

You own `q*.md`; the integrator owns `a*.md`. Never edit an `a*.md`.

Every question must contain: what you were doing and what you found, the
decision you need, the options with your read on each, and **your
RECOMMENDATION**. Full SOP: `docs/INTEGRATOR.md` §10.

---

## Delivery

**Push to your branch (`p47-callgraph`) at every stage boundary.**

**Send ONE `Work.tgz` of the whole tree with the final report** — not at
interim stages.
