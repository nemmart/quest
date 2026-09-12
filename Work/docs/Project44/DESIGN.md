# Project 44 — DESIGN OF RECORD

Written Sep 12 2026 (design session). **Supersedes the compiler line
P35–P43, which is void and lives in `docs/attic/`.**

This document is the design. It is not a work plan; the projects it implies
are listed in §10 and each gets its own prompt.

---

## 1. What went wrong, in one paragraph

P35 proved that a C subset could be compiled to our IR and compared against
the book register-exact. Nine projects later, four routines of ~130 had
matched and the last three had closed none. The reason is structural: the
translator was a recursive walk with ~45 rules as scattered conditionals,
so a divergence had no address — the wrong bytes could have come from the
parse shape, the lowering, the allocator or a peephole, and every diff got
patched wherever it was observed. Worse, the translator was edited to close
diffs against the book, so the thing under test and the thing doing the
testing were the same artifact. Rules got a single witness each and stayed
there.

**The exact match is not the mistake, and P44 keeps it.** An exact match
through a *sound* translator really does prove the C is a correct recovery.
The mistake was (a) making it the only acceptance criterion, and (b) never
establishing soundness independently, so the match was carrying a weight it
could not bear.

---

## 2. The two obligations

Everything below exists to keep these separate and individually testable.

- **(a) Soundness.** The naive C → IR lowering preserves semantics.
  Established by differential testing against gcc on programs that are
  **not** game routines. Never established against the book.
- **(b) Match.** The IR, after transformation, equals the book's IR.
  Established by `ircmp`.

(a) and (b) together prove the C is equivalent to the original. Fitting can
only ever touch (b), which is why (a) must not depend on the book in any
way.

**The standing rule that follows, and it is the most important sentence in
this document:**

> A match failure is NEVER a reason to edit the compiler.

A match failure means a missing transformation or a wrong oracle. If a match
failure appears to require a compiler change, that is either a soundness bug
or an unimplemented construct — it is a **finding**, investigated on its own
terms and fixed against the compiler's own tests. This rule is what P35–P43
did not have.

---

## 3. The three components

Kept separate in the source tree and in every discussion:

| component | job | may make choices? | obligation |
|---|---|---|---|
| **compiler** | C → naive IR | no | (a) soundness |
| **transformer** | IR → IR under an oracle | yes, supplied | each rewrite sound |
| **ircmp** | compare IR to the book, report divergence | — | — |

`ircmp` belongs to neither and lives outside both.

---

## 4. Storage classes

### 4.1 The classes

| class | what it is | size | address space |
|---|---|---|---|
| `v0…vn` | storage the source (or a transformation) needs — scalars `i16/u16/i32/u32`, plus `char n` and `words n` (P46 gate R4: a fixed string and a local array are ordinary PL/I locals) | **fixed** | 0x76 → 0x74 |
| `s@b.k` | string temps of a concatenation group (master's WMSP claims) | computed at runtime | 0x75 arena |
| `t0…t8` | byproducts of lowering ONE instruction | none | — |

**`t` is not a peer of `v`.** It exists because lifting a single machine
instruction has to split its effects (a result and a carry, say). Nine of
them serve the entire 80k-line book. The compiler never *chooses* a `t`; it
chooses to emit an instruction and the `t`s come with it. `t` is never
bound, never mapped, never addressed.

**`v` is the pre-allocation form.** The book contains no `v`s — only
`ac0…ac3`, `t`, and memory. So **a match requires every `v` to have been
eliminated**, by binding it to a register, to a real address, or to nothing
at all (consumed within the block, never materialised). Only
memory-resident `v`s participate in the address merge.

**`v` is globally scoped and QUALIFIED BY ADDRBOOK ENTRY.** The game is
non-reentrant (Plan.md), so each routine's storage exists in exactly one
copy. Names are `<ENTRY>.v<digits>`: `QUEST.v0`, `QUEST.1.b0`'s sibling
`QUEST.1.v3`, `FIRE.1.v0`. The entry spelling is the addrbook's, uppercase,
with the `@ADDR` suffix dropped (`FIRE.1@7016A3BD` → `FIRE.1`).

Uniqueness comes from the namespace, not from bookkeeping: one routine's C
can be written and renumbered without touching any other, and separate
compilation falls out. Block and `v` namespaces are therefore the SAME
granularity as addrbook entries and as `v` placement — one namespace
concept, not two.

Parsing is unambiguous on two counts: an entry suffix is `.<digits>` while
a local name is `.v<digits>` or `.b<digits>`, so the last dotted component
is always the local and everything before it names the addrbook entry; and
entry names are UPPERCASE while `v`/`b` are lowercase, so case disambiguates
even if a routine were ever named `B12`.

**`v` is always fixed size** — i16, u16, i32, u32, or a varying string with a
declared maximum — because a `v` is eventually laid down *exactly on top of*
the Eagle allocation at 0x74. Arbitrary-length string temporaries are not
`v`s; they are `s@` twins. Assigning a twin to a `v` truncates or pads.

### 4.2 Compiler-invented storage is also `v`

Some storage has a home but no name in the C source — e.g. a hoisted
loop-invariant subscript, stored in the loop header and reloaded each
iteration (QUEST.1: `XWSTA 0,[ac3+0x6]` then `XWLDA 2,[ac3+0x6]`). These
outlive their block, so they cannot be `t`, and they must be placed and
merged like anything else. They are `v`s **created by the hoisting
transformation**, not written in the C.

This requires an explicit carve-out from the old rule that a choice selects
and never creates: **a transformation may create a `v`.** It may still never
change which statements exist at the source level.

### 4.3 Rename: `t@b.k` → `s@b.k`

The arena twins are currently spelled `t@<block>.<k>`, colliding with
`t0…t8`, which they have nothing to do with. Rename to `s@<block>.<k>`;
the readable layer's `p@b ≡ t@b.last` becomes `s@b`.

Pure token rename: 198 uses in the book, `quest.arena`, IR.md §5.9, the
loader, `readable.py`. Regenerate and diff to confirm nothing but the token
moved. **ir 7 bump, Provenance row.** Do it before any oracle files exist
with the old spelling.

---

## 5. Address spaces

| range | contents | assigned by |
|---|---|---|
| 0x70 | original code | — |
| 0x74 | locals and args, per routine | M4a addrbook |
| 0x75 | string temps `s@b.k` | `quest.arena` |
| 0x76 | **unplaced `v`** | IR loader |
| 0x77 | **unplaced blocks `<ENTRY>.b<digits>`** | IR loader, within a per-entry reserved range |

All synthetic spaces stay in ring 7, with the real ones. (0x4xxxxxxx was
considered for blocks and rejected: block names land in the PC, and 0x4 is
a different ring with different protection semantics.)

### 5.1 Placement, and why the default matters

The IR declares a `v` **without saying where it lives**; the loader places
it. With an empty oracle, every `v` gets its own private 0x76 slot:
aliasing is impossible, and **the program runs**. Every oracle entry is
then one decision pulling one `v` down onto the address the original used.

Consequences worth stating:

- **Oracle length is literally a count of unexplained decisions**, starting
  at "all of them" and falling as the model improves. This is the headline
  metric.
- A `v` still sitting at 0x76 when `ircmp` runs is self-evidently unmatched;
  unplaced variables show up as divergences for free.
- Comparison happens **after** placement. The book spells locals as
  addresses, never as names, so `v` must be fully resolved before `ircmp`
  sees anything.

### 5.2 Placement is not binding

Two different mechanisms, easily confused because both answer "where does
this live":

- **Placement** (0x76 → 0x74) changes an address and nothing else. The
  instruction stream is identical. The **loader** can own it.
- **Binding** (`v` → `ac0`) *deletes* loads and stores. It is a
  **transformation** with a liveness precondition.

**OPEN — the slot bijection (P46 gate, F6; refined in P46/a002).**
"Placement changes an address and nothing else" is true of the ADDRESS and
false of the TEXT. The book never spells a local as an absolute 0x74
constant; it spells `wp(r, d)` — register-relative (IR.md §5.2; no
`wp(0, d)` is ever emitted). A placed `v` is an absolute constant. So the
equivalence between `M16[0x74001A04]` and `M16[wp(ac3, 6)]` has to come from
somewhere.

**It is mostly DERIVABLE, not supplied.** A `wp(r, d)` is a local reference
only when `r` currently holds the frame — and **ac3 is not pinned to the
frame**: P38 established the frame pointer is an ordinary R7-managed value
materialised by `LDAFP` into whatever register is free, with ac3 otherwise
ordinarily allocatable. FIRE.1 shows all three states within a dozen
instructions: ac3 holding the frame, ac3 repurposed as a record base, and
the frame materialised into ac2 so that frame references spell `wp(ac2, d)`.
Uplevel access adds a third base via the static link.

But "does `r` hold the frame here?" is a **dataflow question** whose sources
— the `LDAFP` points, the link loads, the intervening clobbers — are already
in the book. Reaching-definitions over base registers answers it, and the
state space is **two registers**: measured over the book, ac2 and ac3 carry
all 19,344 base uses and ac0/ac1 carry none, because the Eagle's wide
addressing modes admit no other index register. P48 builds the
base-register dataflow and supplies only the residue.

The same constraint explains the pressure: with one of two bases usually
holding the frame, there is ONE free base for every record access, string
pointer and static link. Frame re-materialisation via `LDAFP`, `FP_COST ≥ 2`
and the `WPSH`/`LDAFP`/`WPOP` dance around a live element address are
**spilling under a two-register constraint**, not compiler quirks — so base
spill/reload is an expected rewrite class, not a discovery waiting for P48.

**And it inverts into a check.** If a `wp(r, d)` resolves against a base the
analysis says is not the frame, while `d` lands in frame territory for that
routine, then either our base-tracking is wrong or the original compiler
emitted something odd. Either way it stops the build rather than matching
quietly — which is the property this design wants everywhere.

### 5.3 The merge is Milestone 5 run backwards

M5 was to be live-range analysis over the original's reused slots —
discovering that slot+4 held A early and B later. That is inference and it
is hard.

Here the naive form is **fully split** (one 0x76 slot per `v`, reuse
impossible) and the oracle **merges**: "put `v7` and `v19` both at
0x74001A04." Two `v`s may share an address exactly when their live ranges
are disjoint, which is mechanically checkable. The compiler proves it or
**hard-errors**.

Same information as M5, derived instead of guessed. And the failures are
informative: if two `v`s the original clearly shared refuse to merge, our C
has them live simultaneously and the original did not — i.e. **the C
structure is wrong**. A free structural check on the reconstruction.

---

## 6. Blocks

Compiler emits symbolic block names qualified by addrbook entry —
`QUEST.b1`, `QUEST.1.b0`, `FIRE.1.b7` — exactly as for `v` (§4.1). The
loader assigns each a numeric address within that entry's reserved 0x77
range. `goto [QUEST.b3, QUEST.b7] cond` runs unresolved. Placement maps each
to a real 0x70 block address. **A block name must be a first-class block
identity in the runtime**, not only a compiler-internal label.

Numeric order carries no meaning: every terminator is explicit, so there is
no fall-through for address order to encode.

**Oracle entries must NOT key on sequential position.** If a block's
identity is a counter-assigned `b3`, inserting one statement in the C shifts
everything downstream and every oracle file goes stale — the §7.4 fragility
arriving through the block door. Block identity for oracle purposes is
**construct-anchored** (`stmt14.then`, `loop7.body`), with the `b<digits>` name
and its 0x77 address as the runtime realisation, linked by provenance.

**Ruling: block structure is fixed by lowering.** The compiler emits a
canonical partition; merge and split are transformations under the oracle.
The rejected alternative — the oracle supplies the target partition and the
compiler lowers into it — reaches matches more cheaply but deletes the
question instead of answering it, and block structure is real compiler
behaviour we want to explain.

Preconditions are cheap CFG facts and neither rewrite can silently change
meaning:

- **merge** legal when the first block's only successor is the second and
  the second's only predecessor is the first
- **split** legal at any statement boundary

**The canonical form is revisable, under one rule:** if every routine's
oracle needs the same merge at the same construct, the canonical form is
wrong and lowering should change — but only on a **book-wide census**, never
to close one routine's diff. Measure merge/split entries per routine and let
the number decide. `quest.blocks.split` holds the book's partition, so a
count mismatch surfaces immediately rather than after a failed match.

**Correction (P46 gate, F5):** an earlier draft said block ordering is
observable because order changes fall-through. That describes the BOOK's
lowering of machine skips, not a property of this IR — IR.md §4 has no
fall-through, every terminator is explicit. `goto [X,Y] c` versus
`goto [Y,X] !c` is a **polarity choice with no ordering behind it**, and it
belongs to the oracle and to `ircmp`, not to the loader. Ruling: numeric
order carries no meaning (§6 above) stands; the entanglement claim is
withdrawn.

---

## 7. Transformations and the oracle

A compilation is: **source → AST → naive IR → transformations → matching
IR**, where the transformation sequence is supplied by an **oracle file**
alongside the C source. An exact match at the end means valid C.

Four classes, each with a different soundness argument and failure
signature:

| class | chosen by | precondition | failure if violated |
|---|---|---|---|
| **lowering** | nobody | — | sound by construction |
| **elimination** | automatic where provable; `keep` to suppress | the removed computation is redundant | — |
| **binding** (`v` → register) | oracle | register free across the live range | — |
| **placement/merge** (`v` → address) | oracle | live ranges disjoint | silent corruption |
| **reordering** | oracle | no dependence crossed | **silent: matches and is wrong** |
| **block merge/split** | oracle | CFG shape (§6) | — |

**Every precondition is a HARD ERROR, not a warning.** A transformation that
fires where its precondition does not hold voids the entire guarantee — and
it would still produce a match, which is the one failure mode with no
signature.

Reordering is the sharpest edge and the one that needs its preconditions
written out as a class rather than a footnote: a swap that is dependence-safe
within a block may not be safe for a value live across the terminator, or for
a store observable by a syscall or by shared memory.

### 7.1 Worked example

The user's example, which is the design in miniature. Naive lowering is
maximally explicit — every address materialised into a base register, every
value into a temp:

```
t0 = 1000
ac3 = fp
M32[ac3+0x100] = t0
```

Then, as removals and bindings on top:

- `ac3 = fp` **eliminated** — precondition: ac3 provably already holds fp
  here. A dataflow fact the checker computes; not something the oracle
  asserts.
- `t0` **bound** to `ac0` — precondition: ac0 free across the live range.

```
ac0 = 1000
M32[ac3+0x100] = ac0
```

Note what happened to the register model that consumed P38–P40. R33/R34/R41/
R8d were all attempts to *predict* which register the DG compiler picked.
Here the oracle supplies it and the redundant-load elimination falls out of
dataflow. "`ac3 = fp` disappears when ac3 already holds fp" is not a rule to
derive; it is a consequence. R8d ("the frame survives a join") stops being a
one-witness belief and becomes arithmetic.

### 7.2 Elimination vs `keep` — OPEN

Redundant-load elimination is always sound where the precondition holds, so
it could simply run to fixpoint. But the DG compiler demonstrably *fails* to
eliminate sometimes — RETURN_MESSAGE 70176FF5 opens a join with `WMOV 0,0`,
a real self-move. Automatic elimination makes such sites unmatchable, so the
oracle needs `keep`. Whether the default is automatic-with-`keep` or fully
oracle-driven is **open**; the self-move is the test case.

### 7.2a The rewrite set: small, general, and a tripwire

The target is roughly **20 general rewrites**, not 100 special ones — the
kind of thing any optimising compiler does: drop a value we already have,
hoist an invariant out of a loop, coalesce a copy, reuse a dead temp, fold a
spelling. Nothing exotic. And our task is easier than a compiler's, because
we can see the answer: we know what was hoisted, and where we do not want a
rewrite at all we can write the C with the hoist already expressed.

**An unjustified rewrite is permitted if it is sound.** Every rewrite here
is semantics-preserving with a checked precondition, so a rewrite sequence
can only carry X to Y when X ≡ Y. Reaching the book therefore PROVES the C
is equivalent to it, however many rewrites it took and whether or not each
one resembles something a 1988 compiler plausibly did. This is stated
explicitly because the natural instinct is to add a rule refusing rewrites
that are not "what a compiler would do" — see `docs/attic/REWRITE_PLAN.md`,
which did exactly that. **Do not reintroduce it.** "A compiler would do
this" is unfalsifiable, and it is unfalsifiable about precisely the question
this project cannot answer; it is a plausibility test dressed as a soundness
test, and it is the same move that produced 45 single-witness rules.
Soundness is the real guard, and the separation of obligations (§2) is what
makes it available.

**But the set's size is a TRIPWIRE.** If ~20 general rewrites cannot close a
routine, the first hypothesis is that **our C is structured differently from
the original** — wrong loop shape, an expression split the original did not
split, a temp introduced that PL/I would not have. Adding rewrite #21 to
bridge the gap HIDES that signal. It is the P35–P43 failure one level up:
patching the model to absorb what was really a finding about the program.

**Escalation order, binding:**

1. an existing rewrite closes it → done
2. it does not → **suspect the C first.** Restructure and retry.
3. only once the C is exonerated → consider a new rewrite, as a **reportable
   finding** with its evidence

Rewrite-set growth does not block a sound match — the proof stands
regardless — but it is a loud signal that step 2 is being skipped. Report
rewrite-set size and oracle length per routine, and drive both down.

The merge (§5.3) gives the same evidence by a different mechanism: two `v`s
the original clearly shared that refuse to merge means our C has them live
simultaneously and the original did not. The design keeps telling you where
the C is wrong.

### 7.2b Gradient descent — OPEN, measurable

The intended workflow is a walk: each rewrite brings the IR closer to the
book. That only works if `ircmp` distance is roughly **monotone under single
rewrites**, which nobody has measured and which also decides whether search
(§7.3) is viable later.

**Pre-register the measurement** rather than assuming it: once a handful of
routines have closed oracles, replay each oracle one rewrite at a time and
record whether distance falls at every step. Report the answer either way.

### 7.3 Hand-written oracles

Decided: hand-written, not searched. Hand-writing forces understanding of
each rewrite, which is the actual product. Search on a format nobody has yet
read by hand finds oracles that match without teaching anything.

The consequence is that **the divergence report is critical path**. The loop
is edit-oracle / compare / repeat, and its cost is dominated by how fast
`ircmp` answers "what is wrong and where." It must point at the first
divergence **in the source's terms** — statement N, this operand — and
ideally name the transformations whose preconditions hold at that site.
This is the difference between a tractable weekend per routine and what
P38–P40 felt like.

Search remains available later at no extra cost: same oracle format, same
distance metric.

### 7.4 Site addressing

An oracle entry names *which* instance to rewrite. If sites are identified
by IR position, every earlier transformation renumbers them and oracle files
become write-once. **Anchor site IDs to source constructs** (statement 14's
second operand) and carry them through lowering as provenance.

---

## 8. Two acceptance levels

This is the most consequential structural change from the old line, and it
is what makes the project safe.

### L1 — SUBSTITUTABLE (behavioural)

The C routine replaces the original in the clone; the game plays; the
checker stays green outside it. Equivalence is Plan.md Step 4's existing
definition: **same OS-layer calls, same shared-data changes**.

Reachable **now**, per routine, with an empty oracle and no placement — the
naive 0x76 form is substitutable. No codegen model required.

### L2 — EXACT (static)

The oracle closes and the IR matches the book register-exact.

### Why both

They are complementary precisely where each is weak:

- **L1** is behavioural truth on **executed paths only**. A block no play
  session reaches is unverified — but it is *identified* as unverified.

  **Measured (P47 gate, Sep 12 2026): the battery reaches 41 of 80 call-graph
  nodes and 15 of 20 leaves.** So L1 as it stands can validate roughly half
  the program, and the rest is **L2-only** until play coverage improves.
  This makes the owed play-driver fix (`NextSession.md`) potentially the
  gating item for the whole L1 strategy rather than hygiene — P47's
  `Coverage.md` is charged with judging how much of the gap is the driver bug
  versus genuinely hard game states.
- **L2** is static truth on **all paths**, including those play never
  reaches.

Neither alone suffices. Together they are stronger than either, and a
routine's status is honestly reportable as L1, L2, or both.

**Ordering ruling, and it is binding: L1 is the primary path.** ~130
routines at L1 is a playable C++ Quest validated against the original — that
is the deliverable. L2 is the stronger badge earned where the model supports
it, not a gate everything waits behind. A future session must not
re-prioritise toward exact match by default; that is how the last nine
projects went.

### 8.1 Partial credit — OPEN

The lockstep oracle's verdict is binary per routine, so a routine at 95%
oracle coverage scores the same as one at 0%. That is a bad gradient — it is
what made "staged, not fitted" tempting, which P38 had to ban outright.
Partial credit should come from `ircmp` **distance**, not from the checker.
Form of that metric is open.

---

## 9. Substitution mechanics

### 9.1 What exists already

- **Entry interception**: M4a's address book + WSAVS hijack, 45 routines
  live.
- **Sync list is file-driven** (`QUEST_SYNC_LIST=`), so excluding replaced
  blocks is an existing mechanism.
- **Locals and args are already at 0x74**, not on the MV stack — 102 of 130
  addrbook entries live. So arg pointers are stable global addresses,
  independent of call history.

### 9.2 The rendezvous contract

At return, what must agree: shared/global game state, the return value
(note `slotpatch` routines store a 16-bit result into the saved-ac0 image at
`wp(ac3,-7)`), and the frame/stack restored to the caller's expectation.
Private storage — our `v`s, the original's frame slots — is excluded by
construction.

**Outgoing calls are part of the contract.** PICK_X_Y calls
`RANDOM_NUMBER$3`, which advances the seed; same calls, same order, same
arguments, or everything downstream diverges. The checker should compare the
**outgoing call sequence**, not just end state — which additionally catches
wrong-but-coincidentally-same-result translations.

### 9.3 Runtime calls are free; game calls are not

`$N` routines are runtime, already lifted, not in the sync list, so a C
routine calling them costs nothing **as far as the CHECKER is concerned**.

**Correction (P46 gate, F7): they are not free in the IR.** `rt_call` and
`call` are validated against an LCALL word at a real site (IR.md §6). A
compiled routine calling `?WRITE_SCREEN` has no site. ir 7 therefore REFUSES
`@addr`, `call` and `rt_call` inside a symbolic block, and the calling
bridge is a **new IR production plus its validation rules** — not a detail
of the harness. It is the thing standing between "a naive program loads and
runs" and "a naive program is substitutable", and it is P48's keystone
within a keystone.

The constraint is on calls to **Eagle game code**: that callee's blocks *are*
compared, our argument setup differs, and it diverges. Each such edge costs
one of two things:

- **un-check the callee** — cheap, loses verification of it for that run
- **marshal arguments exactly** — escape placement at true 0x74 addresses,
  keeps the callee checked, more work

### 9.4 Therefore: translate BOTTOM-UP

Top-down is ruinous — QUEST in C means un-checking everything it reaches.
Bottom-up (leaves first) means the callee is **already replaced and already
out of the sync list**, so the marginal cost of each new routine is zero.
The replaced set stays downward-closed for free and checker coverage only
grows.

This reverses P37/P38's selection criterion, which picked by construct
coverage and statement count. **Call-graph depth is now the primary
ordering.** (PICK_X_Y was a leaf; that is part of why it went well.)

It also gives a progress metric that is not binary per routine: the replaced
frontier grows upward through the call graph, and everything inside it is C.

Needs: **the game call graph**, which does not exist yet. 159 LCALL / 130
LJSR / 37 XCALL embeds remain unlifted — a real but bounded extraction task.

### 9.5 Escape analysis — OPEN, may be moot

A `v` whose address is passed to Eagle game code must sit at the original's
exact 0x74 address, or the pointer value differs and the callee's register
compare diverges. That set is statically computable, and it is a **subset** —
purely internal `v`s can stay at 0x76 indefinitely.

**But M4a is described as doing "probe-class checking + pointer-normalized
mediation."** If the checker already normalises pointer values rather than
comparing them raw, escaping `v`s may not need true addresses at all and L1
becomes free of the addrbook entirely. **Read the checker source and settle
this** — it is the difference between "escape analysis required" and "not
required."

Also: 28 addrbook entries are not migrated (`#` lines, mostly `nocall`
nested). Calling one of those from C returns to stack-address territory.

---

## 10. Varying-string capacity — CLOSED

Recorded because it looked like a gap and is not.

A `CHAR(n) VARYING` has two numbers: the runtime length and the declared
capacity, which decides where truncation happens. IR.md §5.8 confirms the
capacity **is** in the code — `[@a, n varying]` has `n` = capacity, and
`assign_varying` sets the length word to `min(len, n)`.

But it is visible only conditionally:

- **Variable-length source** → the compiler emits its min diamond
  (`WSGE/WSLE + WMOV`, which stays in the CFG) with the capacity as a
  literal. **Exact.**
- **Constant literal source** → `min(27, cap)` folds at compile time and the
  capacity vanishes. This is why frame slot `wp(ac3,12)` shows 27/17/16/47
  across sites.

The two cases are exactly complementary, so the gap closes: where capacity
can affect behaviour it is readable; where it is invisible, no truncation is
possible in either program and declaring it as the largest constant observed
is behaviourally identical.

**Lowering rule derived from the spec (not fitted):** a varying assignment
emits a min diamond unless the source length is a compile-time constant ≤
capacity.

---

## 11. ON-conditions — NAMED BLOCKER, deferred

The CFG is not closed for ON-condition handling, and this is **not** a
construct to add on first contact like bits or floats. A signal is an edge
*into* a routine from outside, and possibly *out of* one mid-statement. It
is a CFG property, and it touches decisions already made:

- **escape analysis** — a handler runs with the establisher's locals live,
  so handler reachability counts as a use
- **live-range merge** — two `v`s disjoint on the normal path may both be
  live across a signal edge. Merging them without that edge in the graph is
  a silent bug, and the program still matches.
- **block structure** — ON-unit bodies are separate WSAVS frames (P34:
  LOGON.1/.2, ALLY_PLAYER.1), so entry/exit is not a mere edge annotation

**Ruling: it is a precondition on the liveness-dependent transformations,
not on the compiler or the harness.** Merge and binding must **hard-error**
on a routine that has an ON establisher whose edges are not in the CFG,
rather than quietly assuming none.

Deferrable because the first routines are leaves and there are only 26
handler sites in the game (`ON_ERROR_CATALOG.md`); most leaves have none.
Not deferrable indefinitely. Becomes its own project when the first
handler-bearing routine comes up. Existing material: `ON_ERROR_CATALOG.md`,
`ERROR_PROCESSING.md`.

---

## 12. Open items

| # | item | decides |
|---|---|---|
| 1 | does pointer-normalized mediation remove the escape-placement requirement? (§9.5) | real work either way; **read the checker source** |
| 2 | canonical block structure (§6) | start canonical; revise only on a book-wide census |
| 3 | elimination automatic-with-`keep` vs oracle-driven (§7.2) | RETURN_MESSAGE's self-move is the test case |
| 4 | partial-credit metric (§8.1) | `ircmp` distance, form open |
| 4b | is `ircmp` distance monotone under single rewrites? (§7.2b) | whether the gradient walk works, and whether search is viable later |
| 5 | ON-condition CFG closure (§11) | deferred, gated by hard error |
| 6 | the slot bijection: absolute 0x74 vs `wp(r, d)` (§5.2) | P48; mostly derivable from `LDAFP` dataflow — build the base tracking, supply only the residue |
| 7 | the calling bridge as an IR production (§9.3) | P48; resizes the harness |

---

## 13. Implied projects

Dependency order, each to get its own prompt. Not a schedule.

| # | project | notes |
|---|---|---|
| 0 | **this design** | done |
| 1 | **attic review / salvage** | mine `docs/attic/` per its README; produce `docs/Salvage.md`; mark tainted rows in `declarations.json` **in place**. *Salvage is the deliverable — read and extract first.* |
| 2 | **IR spec update + loader + runner** | `v` declarations, `<ENTRY>.b<digits>` blocks at 0x77, `s@` rename, loader placement. **ir 7.** |
| 3 | **the basic compiler** | C → naive IR. Choice-free. |
| 4 | **compiler differential tester** | gcc vs our lowering on non-game programs. Small, and it is the ENTIRE evidence base for obligation (a). Must not land after the compiler. |
| 5 | **L1 substitution harness** | entry interception, sync-list exclusion, the **calling bridge** (a new IR production + validation — P46 F7, bigger than first scoped), the **slot bijection** in `ircmp` (P46 F6), the rendezvous contract. **The keystone: this is the backup plan.** |
| 6 | **first leaf routines at L1** | **selected by P47's `Order.md`, on evidence.** "PICK_X_Y first" is WITHDRAWN (P47 gate): the battery never executes it, nor its only caller REPOSITION, so under L1 its oracle would never fire and the checker would stay green whether the C was right or catastrophic. **A first target must be a routine play actually reaches** |
| 7 | **call-graph extraction** | from the book; needed to order bottom-up |
| 8 | **transformer + oracle + L2** | the ambitious layer, on top of a working L1 |

Projects 2‖3 and 4 can overlap; 5 depends on 2.

---

## 14. Provenance

Design session Sep 12 2026. Tree: the Sep 9 post-P40 state, with P35–P43
and `CODEGEN_RULES.md` / `translate.py` moved to `docs/attic/` in this same
session. No code was written and no artifact regenerated.
