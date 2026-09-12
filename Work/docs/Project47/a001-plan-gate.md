# a001 — P47 plan gate: APPROVED. Both rulings your way, plus one scope addition.

Integrator, Sep 12 2026.

**Approved. Proceed to Part 2.** Both decisions go your way. One thing you
found changes a design ruling, and I am adding one deliverable because of it.

This gate answered the project's headline question — resolved-edge fraction —
in the gate itself, at **758/758 with zero indirect calls in the game range**.
That is a better outcome than the prompt expected; hazard 8 was written on
the assumption the runtime's register-indirect forms had game analogues, and
you showed they do not.

---

## Rulings

**D1 — SQR31?3 and INIT_SHARED_DATA: (b), approved.**

`SQR31?3` is the PL/I library square root linked into the game segment. It is
not Quest source, it will never be translated, and it should be edge-free
exactly as a `$N` routine is under DESIGN §9.3. Mark it `library`.

`INIT_SHARED_DATA` is a game routine. **And your instinct to flag rather than
silently fix is right — this is a real finding and it belongs in the
REPORT.** The addrbook is M4a's migration table; a game routine absent from
it has no per-routine 0x74 area and no WSAVS hijack. Your explanation (it
runs once before the first hijacked entry) is plausible and is probably why
nobody noticed. State it as an open item with your evidence; do not extend
your boundary to `quest.addrbook`, which is P46's.

**D2 — coverage from 047b's `.err` traces: (a), approved.** It is in the
tree, it is the current template, and re-running belongs to whoever holds
`emulation/`. Your plan to state the provenance of every "reached" claim
rather than treat it as ground truth is the right handling — keep it.

---

## The PICK_X_Y finding, and what it changes

You found that **PICK_X_Y is never executed by any leg**, that its only
caller REPOSITION is never executed either, and that M4A_ROLLCALL
independently lists it LIVE-UNEXERCISED. DESIGN §13 project 6 names it as the
first routine. That naming was mine, and it was made on the strength of "it
is a leaf and it already has a matched draft" — with no knowledge of whether
play reaches it.

**Under L1 its behavioural oracle never fires.** A substituted PICK_X_Y would
run zero times in the battery and the checker would stay green whether the C
was right or catastrophically wrong. That is the worst possible property for
a first target, because it would validate the harness against nothing while
appearing to work.

**Ruling: DESIGN §13's "PICK_X_Y first" is withdrawn.** `Order.md` selects on
evidence; I have amended the design accordingly. If PICK_X_Y still comes out
near the top on other grounds, say why — but it cannot be first on the
strength of being a leaf.

---

## The scope addition: the coverage number

Your §6 contains the first real answer to a question that has been open since
this project restarted: **41 of 80 nodes, and 15 of 20 leaves, are reached by
the battery.** That is roughly half the call graph.

This matters more than the ordering it was collected for, because DESIGN §8
makes **L1 the primary path** and L1's entire oracle is gameplay. If half the
program is never executed, L1 can never validate half the program — and the
design does not currently say so.

**Added deliverable — `docs/Project47/Coverage.md`** (or a section of
CallGraph.md; your call):

1. **Node coverage**: the 41/80 split, listed both ways.
2. **Block or statement coverage** — 2,090 distinct blocks out of how many
   in the book? Node counts understate the problem; a node counted "reached"
   because one block ran is not a validated node. If a per-statement number
   is cheap from the same traces, give it; if not, say so.
3. **The partition this implies**: which nodes are L1-reachable, which are
   **L2-only** (no behavioural oracle can ever exist for them under the
   current battery), and which would become L1-reachable if the play driver
   were fixed.
4. **Your read on how much of the gap is the known driver bug** versus
   genuinely hard-to-reach game states (dying badly, castle sieges, cave
   combat). `RETURN_MESSAGE` is only reachable by dying badly and that is a
   scripted leg away; `TERRAIN_HELP` may be unreachable at all.

Point 4 is the one I most want your judgement on. The play driver fix is
already owed work; this would tell us whether it is hygiene or whether it is
**the gating item for the entire L1 strategy**.

---

## Smaller notes

**TERRAIN_HELP with no inbound edge of any class** — record it as a finding,
not a gap. Candidate explanations worth one line each: dead like THIEF's body
(Plan.md limitation 1), reached through a data-driven dispatch your scan
cannot see, or a symbol for code that was never called in the shipped build.
Do not chase it far.

**The three I.GOTO edges landing in sibling pieces** (DROP.1→DROP,
ATTACK.4→ATTACK.3, KILL_PLAYER.1/.4→KILL_PLAYER.2) — good catch tying this to
F10 interleaving rather than reading it as nesting. Keep that reasoning in
CallGraph.md; a future session will meet those and reach for the wrong
conclusion.

**Your height strata are a better product than the prompt asked for.** QUEST
at h=9 over a 10-level graph with 80 nodes says bottom-up is tractable —
which is the §9.4 question the prompt asked you to second-guess. Answer it
explicitly in the REPORT.

---

## Proceed

Part 2 as planned. Your `callgraph.py` flag list is right; add `--coverage`
output feeding the new Coverage.md deliverable.

---

## D3 — DIRECTED CHECK, do this before Order.md: who really calls PICK_X_Y

The user reports from play that **PICK_X_Y runs when you create a new
character**. Your gate says its only caller is REPOSITION, whose eight sites
sit in seven families (ALCHEMIST_HOME ×2, ATTACK, CAST, CAVE_ATTACK, DEFEND,
DIED, MOVE_IN_CAVE) — none of them the login or creation path. I confirmed
the raw facts independently: one call site at `70176FC7`, inside REPOSITION
(70176FC1–70176FDD); `PLACE_PLAYER` calls only UPDATE_SCREENS and
FIND_OBJECT.

So the static picture and the play report disagree. Three candidate
explanations, and they have very different consequences:

1. **Character creation is in a DIFFERENT PROGRAM.** `QUEST/` contains
   `NEW_USERS.PR`, `NEW_CASTLES.PR`, `SAVE_USER.PR`, `FIXUP_OBJECTS.PR`
   alongside `QUEST.PR`. Only QUEST.PR is disassembled. If creation lives
   there, PICK_X_Y in QUEST.PR really is reposition-only, your graph is
   right, and the user is remembering a routine in an image we have never
   looked at. **Cheapest to check and most likely.**
2. **A block-ownership attribution is wrong.** You named this risk yourself:
   "nearest addrbook entry at or below the block pc" mis-assigns under F10
   interleaving. If even one of REPOSITION's eight sites actually belongs to
   QUEST or LOGON, the caller list is wrong — and so is part of the 758/758.
3. The user is recalling the reposition-on-death path rather than creation.

**This is worth doing properly because of what (2) would mean.** The project's
headline number is edge resolution, and an attribution error is the one
failure mode that produces a confident wrong graph rather than a visibly
incomplete one. A play report contradicting the graph is exactly the kind of
external check that should be chased, not explained away.

What I want:

- **Check (1) first** — it is nearly free. Does `QUEST/NEW_USERS.PR` exist
  and is it a separate image? You need not disassemble it; establishing that
  creation plausibly lives outside QUEST.PR settles the contradiction.
- **Then audit attribution on those eight sites specifically** against
  `quest.dis` — confirm each pc really lies in the family you assigned,
  reading the actual code rather than the addrbook ranges.
- **Report the answer either way**, including "the user's recollection is of
  another program" if that is what it is.

If (2) turns out to be the cause, **STOP and report before writing
Order.md** — the ordering rests on the graph and I would want to see the
correction first.

One process note, recorded because it generalises: this contradiction
surfaced because someone who has *played the game* looked at a static
result. That is a source of evidence this project has barely used, and it
found a discrepancy in the first five minutes. Worth remembering when a
census looks unanimous.
