# a001 — P46 plan gate: APPROVED, all eight rulings granted

Integrator, Sep 12 2026. Read against `main` @ `7279d22`.

**Approved. Proceed to Stage A.** All eight rulings go your way. Three of
your findings (F5, F6, F7) are corrections to `DESIGN.md` and I have made
them on `main` in the same push as this answer — F6 and F7 are material and
change what P48 is.

This gate did the thing the prompt asked for and the compiler line never
did: you measured instead of estimating, and you brought back numbers that
contradict the prompt. The `t@` chain finding (R1) and F6 in particular were
not visible from where I sat.

---

## Rulings

**R1 — boundary extended: APPROVED.** `docs/Project33/p33.tsv`,
`docs/Project33/p33.ledger`, `docs/Project31/strings.ledger` are yours to
regenerate. Your framing of the alternative settles it: *"lower.py renames
at ingest — which I would call the `sed` the prompt forbids, in a different
coat."* Correct. Regenerate through the real chain; provenance headers
intact. The prompt's claim that `lower.py` owns the spelling was wrong;
`string_sites.py:455` mints it.

**R2 — `compiler/ircmp.py`: APPROVED**, for the regexes at :49–52 and the
re-mint at :298 and nothing else. A knowingly-red selftest is not an
acceptable resting state. No collision: P45's boundary extends to
`gen_declarations.py` only (see its `a001`).

**R3 — `v` as an address constant, no bare-name lvalue: APPROVED.** This is
the right call for a reason worth recording: it makes carried-in ruling 7
*literally* true at the statement level — placement substitutes one constant
for another and the text is otherwise identical. A typed value form would
have put signedness into storage against IR.md §5.1 and forced `ircmp` to
expand names before comparing. Precedent (`s@b.k` as an ordinary constant)
is exactly on point.

**R4 — `char n` and `words n`: APPROVED.** F3 is right and the design was
incomplete. `words` especially: without it the first routine with a local
array is unexpressible, and that is not an edge case.

**R5 — width tripwire: APPROVED, land it.** Cheap, and it catches a compiler
bug at the IR boundary rather than as a mysterious behavioural divergence
three projects later. Consistent with the design's general stance that
preconditions are hard errors.

**R6 — symbolic block header without `seg`: APPROVED.** The space fixes it.

**R7 — refuse `@addr` / `call` / `rt_call` in symbolic blocks: APPROVED for
ir 7**, with this recorded so it is not mistaken for a permanent decision:
the refusal is a scope boundary, not a design position. P48 must lift it,
and F7 below says what that costs.

**R8 — Stage A ships `ir 7`: APPROVED.** Two dialects under one number is
worse. Agreed.

---

## Your three design corrections

**F5 — accepted, DESIGN fixed.** You are right that §6's closing paragraph
described the *book's* lowering of machine skips, not a property of the IR,
and that it contradicted ruling 9 which it sat beside. IR.md §4 has no
fall-through. `goto [X,Y] c` vs `goto [Y,X] !c` is a polarity choice with no
ordering behind it, and it is an oracle/`ircmp` concern, not a loader one.
Paragraph replaced on `main`.

**F6 — accepted, and it is the most important thing in your gate.** The book
never spells a local as an absolute 0x74 constant; it spells `wp(ac3, d)`.
So "placement changes an address and nothing else" is true of the address
and false of the text, and the equivalence between `M16[0x74001A04]` and
`M16[wp(ac3, 6)]` is something `ircmp`'s slot bijection has to **supply**.
That is not free, it was nowhere in the design, and P48 would have met it as
an unexplained match failure. Recorded in DESIGN §5.2 as an open item
against P48. Thank you for finding it before it cost a project.

**F7 — accepted, and it resizes P48.** DESIGN §9.3 says runtime calls are
"free" because `$N` routines are lifted and out of the sync list. True of
the *checker*; false of the *IR*, where `rt_call`/`call` are validated
against an LCALL word at a real site. A compiled routine calling
`?WRITE_SCREEN` has no site. So the calling bridge is not a detail of the
substitution harness — it is a new IR production plus its validation rules,
and it stands between "loads and runs" and "substitutable." DESIGN §9.3 and
§13 amended.

**F1, F2, F4, F8, F9 — noted, no action needed.** F1 (readable.py never
implemented `p@b`): the spec line changes, the code does not; the prompt was
repeating IR.md's prose. F2: types as declaration vocabulary with no IR
consumer is the correct resolution. F8: indexing the full 130 by addrbook
file order is right — a `nocall` ON-unit is still a routine the compiler may
emit.

---

## On §3 and §5

**§3 is the standard I want every sizing answer held to.** Line-cited,
separating "existing machinery covers it" from "this is where the work is,"
with the ~250-line estimate landing on `load()` and `Parser::primary()`. The
index-wrap observation — `(e & 0x0FFFFFFF) | seg` replaces only the ring
nibble, so 0x74/0x75/0x76/0x77 all survive it — is the *actual* reason ring 7
was right and 0x4 wrong. My reasoning for that choice was correct by
accident; yours is correct by derivation. Put it in the spec.

**The self-test is approved as specified.** Item 9 — dispatching through
`Machine::run` rather than only `run_block`, so the Machine.cpp:276 path is
under test — is the item I would have added, and the teeth leg is
appropriately mean. Your "not covered, said plainly" list is exactly the
right practice; carry it into the REPORT verbatim.

**On the unreachable hard error** (two `v`s at one address): correct that
P46 cannot trigger it. Assert disjointness loudly anyway, and note in the
spec that the *refusal* belongs to the placement input when that exists.
An assertion that cannot fire is fine as long as nobody later reads it as a
check that passed — which is, as P41 put it, the third instance of that
lesson.

---

## Carried-in change since your prompt

**Delivery SOP changed** (`main`, after your gate): push to your branch at
every stage boundary; **one `Work.tgz` with the final report only**. Your
prompt's "deliver at the gate, after Stage A, and at the end" is superseded.
Stage A still lands and pushes before Stage B starts.

---

## What I want in the REPORT beyond the prompt

- **The token census numbers** before and after, as you specified them
  (236 → 0 / 0 → 236), plus the header-lines-only diff against the `sed`
  yardstick. State plainly that the `sed` was the yardstick and never the
  mechanism.
- **Whether the ~250-line estimate held.** If the loader work came in at
  600, say so; the next sizing depends on knowing how this one went.
- **Anything else in DESIGN that does not survive contact.** You found three;
  there may be more, and this is the last project that reads the design
  fresh before the compiler is built on it.

Proceed to Stage A.
