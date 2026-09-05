# P26 math & control design — rulings of record (user + integrator, Aug 29 2026)

> **LANDED Sep 5 2026 (Project 26).** The normative grammar is now
> **docs/IR.md (ir 3)** — spec-wins. This document is the design input
> of record; where it and IR.md differ, IR.md is the law. Open item §6.2
> was ruled at the P26 plan gate: ash/lsh only, no C `<<`/`>>` at any
> tier. Census + per-mnemonic semantics: Project26/Census.md.

Design discussion captured verbatim-in-substance; each item below is a
USER RULING unless marked open. Supersedes conflicting notes in
IR.md §8's roadmap; IR.md itself is amended when P26 lands (spec-wins
discipline — this doc is the design input, the spec is the law).

## 1. Terminator: `goto [label list] tN`

One terminator form for everything:
- `goto [L] 0` — unconditional (today's goto, degenerate case).
- `goto [Lfalse, Ltrue] t` — the if. **Index convention: false=0,
  true=1.**
- `goto [L0..Lk] t` — jump tables / switches.

**STRICT**: tN must already be a valid index in [0, count). No
coercion, no clamping — out of range is a loud fault (no defined arm
to take). The LOWERING inserts `tf()` to normalize tests; `goto`
never does. Side-effect statements come before the terminator; the
test expr may read post-op state (DG skips test the just-computed
result/carry); nothing follows the terminator in a block.

No asserts on table bounds (earlier idea DROPPED by ruling): under
lockstep a wrong arm cannot hide. (Distinct from roadmap item 2 —
DERR-cluster compression — which DOES use asserts: there they REPLACE
skip-chain control flow to shrink the block census, not double-check
an index the checker already verifies.) — the clone picks differently and
the next rendezvous flags blk/ordinal mismatch. The executor's
out-of-range fault is the only backstop. Checker untouched.

## 2. Boolean layer (strict 0/1)

- `tf(x)`: 0 if x==0 else 1 — THE normalizer at every word→boolean
  boundary, inserted by the lowering.
- Comparisons produce exactly 0/1:
  - Ordering: **mandatory signedness suffix** — `<s <=s >s >=s`,
    `<u <=u >u >=u`. Bare `< <= > >=` REFUSE at parse ("they will
    get dropped somewhere" — hidden defaults are how audits die).
  - `==` `!=` unsuffixed (signedness cannot matter).
  - The s/u suffix is *the* convention going forward (split divide
    when it lands, etc.).
- `&&` `||` `!`: strict 0/1 operands (else fault), EAGER — no
  short-circuit; exprs are pure, control flow lives only in the
  terminator. `!` is boolean negation only; negate a raw word as
  `tf(x) == 0`.
- Lowering parenthesizes emitted compound tests; no reliance on a
  precedence table.

## 3. Word layer

- Bitwise: `&` `|` `^` infix, `~` prefix (user rulings, Aug 29
  follow-ups — supersede the same-day functional
  and/or/xor/com spellings entirely). Word in, word out, no 0/1
  constraint. `~` flips 32 bits; `!` remains boolean-only (§2).
  Lowering parenthesizes compound emissions (no precedence
  reliance) — same rule as §2.
- Shifts, ISA-SHAPED (supersedes the ls/ars/lrs draft from earlier
  the same discussion): `ash(x, amount)` and `lsh(x, amount)`,
  SIGNED amount, positive=left / negative=right, semantics COPIED
  FROM THE ISA including out-of-range (from the reference source:
  amount>=32 -> 0 both kinds; ash <= -32 -> src>>31 sign smear;
  lsh <= -32 -> 0; amount 0 passthrough). Direction-split ops were
  rejected because the hardware op is one instruction with a signed
  amount, and register-amount (dynamic) shifts must be expressible.
- CAUTION carried from the wide-carry episode: the reference
  fragment discussed was Java (`>>>` vs `>>`); the session must diff
  c_src's shift helpers against it and treat any mismatch as a
  FINDING before the spec cites either (METHOD §5/§10).
- `ash` has a SIDE EFFECT in the ISA: ovr |= sign-change. Exprs stay
  pure — see §4.

## 4. Effectful op family (statement root only)

Named ops that maintain the machine flags, replacing the `#` prefix
family — **`#+` and `#-` GO AWAY**, superseded by:

    acd = add(a, b)      ; carry-out + ovr per the ISA recipe
    acd = sub(a, b)      ; complement-add borrow (P24 semantics)
    acd = ash(a, n)      ; value + the ovr accumulate
    ... (adc/inc/neg etc. per the census — each cites its emulator
    helper, wide-carry-style)

Rules:
- Effectful ops sit ONLY at statement root (`ac0 = add(ac0, t1)`);
  nested inside a larger expr REFUSES. Exactly one flag-writing op
  per statement, at the top — ordering never ambiguous. Arguments
  are pure exprs.
- Where a flag effect must be spelled out instead (e.g. today's
  lowerings before the family lands), it is emitted as an explicit
  statement via a temp:
      t1 = ash_value_form...   ; illustrative
      ovr = or(ovr, lsh(xor(t1, src), -31))
  (ovr and c are assignable statement destinations, like registers.)
- Same-helpers principle stands (P23 ruling): the executor calls the
  SAME EagleInstruction helpers as emulation. The spelling changes;
  the shared implementation does not.

## 5. The flag-conversion future (parked, direction ruled)

`add(a,b)` converts to bare `a + b` when no one reads the flags it
writes — BUT the checker reads c and ovr at every rendezvous (pair
surface, P22/P24), so "dead" is intra-block: overwritten before
block exit on every path, nothing consuming en route. Direction
ruled for WHEN this happens (not P26):

- An ANALYSIS TOOL proves per-block per-flag deadness; lower.py
  emits a block-header annotation ("flags unverified here",
  per-flag); the rendezvous compare consults the annotation. The
  artifact declares what it maintains; the checker holds it to
  exactly that (the sync-list-as-contract pattern extended to the
  compare surface).
- Degrades gracefully: a wrong proof loses immediate detection on
  that block, not detection — a truly-live flag steers a skip or
  stores a value and diverges at a later rendezvous.
- Eyes open (user): "if the analysis tools are buggy, we're going
  to get killed 10 ways from sunday" — the failure mode is
  landmines on rarely-walked paths. Mitigations when attempted:
  carry-census protocol (mechanical extraction, findings, spot
  checks), incremental slices behind K=1 gates, annotations
  auditable per block. Until then: flags fully maintained and fully
  on the surface everywhere.

## 6. Open items (not yet ruled — collect at the P26 plan gate)

1. RESOLVED (user, Aug 29): `&` `|` `^` `~` — C spellings, one
   bitwise vocabulary at all tiers; the functional
   and/or/xor/com forms are fully retired.
2. Pure-tier shift spelling after conversion: C `<<`/`>>` (with the
   `>>` s/u question) vs `ash`/`lsh`-as-pure (flag-free). Integrator
   lean: keep ash/lsh as the only shift vocabulary — one spelling,
   ISA amount semantics, no C-UB corners imported.

## 7. P26 scope reminders (from the day's other rulings)

- t-places: ALL 23 borrow brackets get the same treatment — the two
  in call-lowered blocks are NOT special.
- Conditional exits land on the §1 terminator.
- Signedness per comparison site is CENSUS work read from the
  emulator source per mnemonic (ByteEA-style table with citations),
  never guessed.
