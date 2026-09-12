# Indirection — `R[]`, `M32[M32[a]]`, and why the rewrite is legal

**Status: REFERENCE.** The single place to cite for anything about Eagle
hardware indirection. Consolidates IR.md §5.2/§5.10.8, `quest.assumptions`,
DESIGN §5.2/§7, and findings from P46, P52, P54 and P55.

---

## 1. What `R[e]` is

`R[e]` is **hardware indirect resolution of an effective-address operand**:
deref the wrapped index, then **follow bit 31 until it is clear**
(`eagle_resolve_indirect`, IR.md §5.2). It is a **chain**, not a single
dereference. An `R` result used directly as a memory index is not re-wrapped —
chain pointers are full addresses.

In the disassembly it is the instruction's indirect bit: `XNLDA 0,@[ac3+0xFFF2]`
lifts to `M16[R[ac3 + -14]]`.

## 2. The census — one level, everywhere

**1,035 occurrences on 1,032 lines** of `quest.ir2.book` (stock 914/917), in
four buckets:

| bucket | count | where the value comes from | levels | cell written? |
|---|---:|---|---:|---|
| own-frame argument slots, `R[ac3 + -d]` | 939 | the caller pushed the address | 1 | — |
| uplevel argument slots, `R[ac2 + -d]` | 41 | the static link, in a nested `.N@` entry | 1 | — |
| statics — `R[0x70000212]` ×22, `R[0x70000210]` ×3, `R[0x700007A0]` ×2 | 27 | a pointer word in the shared-data page | 1 | the pointer word, no |
| positive frame offsets | 28 | the routine computes it itself | 1 | **YES — 42 writes across 11 cells** |

**No `R[]` contains another `R[]` in either artifact: one level, everywhere.**

Negative offsets from ac3 are argument slots (`wfp-10-2N`: arg 1 at −12, arg 2
at −14, …), which is why the distribution is dominated by arg 1.

### Per-routine, for the seven candidate routines

| routine | `R[]` sites |
|---|---:|
| HIT_ANY_CHAR | 0 |
| GET_INPUT | 0 |
| FAKE_LAND_MASS | 3 |
| UPDATE_SCREENS | 4 |
| PICK_X_Y | 4 |
| INIT_SCREEN | 5 |
| FAKE_OCEAN | 5 |

21 between them. The dereference rewrite is a **comfortable first exercise**
on any of them, not a stress test.

## 3. The precondition, and why it holds at EVERY site

`R[a]` equals a plain double fetch **only when bit 31 of the fetched word is
clear**. That is the rewrite's precondition, and it is **provable
program-wide**:

- **1,008 sites by construction** — arguments were pushed by the caller;
  positive-offset locals were built by the routine from `wp`/`bp`. Every
  address an ir 8 program can produce comes from `wp()`, `bp()` or a cell
  name, all masked into **ring 7** (0x70–0x77), so bit 31 is never set.
- **27 static sites by provenance**, each with exactly **one writer**:

| address | sole writer | value |
|---|---|---|
| `0x70000210` (`SD_PTR`) | 7015BE43, startup | the buffer passed to `?GET_SHARED_PAGE` as argument 2 |
| `0x70000212` | 7015BEE3 | written immediately after `?GET_SHARED_PAGE @7015BEDF` returns a page address |
| `0x700007A0` | 7015C2CD | computed as `add(ac0, M32[0x70000210])` — `SD_PTR` + an offset |

Two are OS shared-page mappings; the third is arithmetic on one of them.
Adding a small offset to a ring-7 address cannot set bit 31.

These three rows are recorded in **`emulation/quest.assumptions`**, which is
required to be *checkable*: the facts hold only while each address has exactly
one writer.

**What would falsify this:** a second writer to any of the three statics, or a
translated routine writing one of those words.

## 4. The compiler NEVER emits `R[]`

The naive form spells a dereference plainly — fetch the pointer with `M32[a]`,
then read through the result. `R[]` stays in the IR because the book uses it.

### The rule, stated precisely

It acts on the **inner fetch**, wherever that fetch appears — not on the whole
`M32[M32[a]]` shape:

```
M32[a]  →  R[a]        iff bit 31 of the word at `a` is clear
```

So `M16[M32[a]] → M16[R[a]]`, `M32[M32[a]] → M32[R[a]]`, and a bare
`ac2 = M32[a]` feeding an address use becomes `ac2 = R[a]` by the same rule.

**Stating it on the outer shape would be wrong**, and the counts show why:

| outer form | sites |
|---|---:|
| `M16[R[…]]` | **861** |
| `M32[R[…]]` | 35 |

Most dereferences read a **16-bit datum through a 32-bit pointer**, so a rule
written as `M32[M32[a]] → M32[R[a]]` would cover 35 of 896. There are also
`= R[…]` sites where the resolved pointer is the value rather than something
read through — which the inner-fetch formulation covers and an outer-shape
pattern does not.

**This is a REWRITE**, with the precondition above.

Why it is not a lowering:

- `M32[M32[a]]` says exactly what is meant. `R[a]` says "fetch and keep
  following while bit 31 is set", which is only *equivalent* when the bit is
  clear.
- Emitting `R[]` naively would **assert the invariant at every dereference**
  instead of checking it once.
- The 1,008 provable-by-construction sites and the 27 provable-by-provenance
  sites deserve different treatment, and the rewrite's precondition is where
  that distinction lives rather than being invisible.

It is **one rule with ~994 applications** — the *one rule, many applications*
shape DESIGN §7.2a's tripwire wants to see, as against *many rules applied once
each*, which is fitting.

*(994, not 1,035: the 41 uplevel sites are a **different** rewrite — Salvage
F2's link → arg slot → datum. In the naive form they are free, because the
child names the parent's `a` cell cross-entry and no link exists, DESIGN
§4.1a. Their L2 rewrite reintroduces the static link.)*

## 5. Why this is load-bearing, not a nicety

**The book uses `R[]`.** Without the precondition the rewrite would be
**illegal** — not a failed match, an inadmissible transition — and every one of
those ~994 sites would be a permanent diff. L2 would be unreachable for
essentially every routine that touches a parameter.

And the failure would have been **silent in a bad way**: the naive form still
runs correctly, so **L1 would pass everywhere while L2 never closed a single
routine**. The likely conclusion would have been that the rewrite approach did
not work, when what had actually happened was one unprovable precondition
blocking the most common rewrite in the program. DESIGN §7.2a's escalation
order — *suspect the C first* — would have sent people hunting their C for a
structural error that was not there.

This is also the concrete case for making `R[]` a rewrite rather than
something the compiler emits: emit it naively and the precondition is never
stated, so it is never verified, so nobody notices it was never established.

## 6. No `**` type is needed

Our pointer vtypes are one level — `*i16 *u16 *i32 *u32 *char`, plus
`*varying <n>` / `*words <n>` (IR.md §5.10.1a). **No `**`.**

The book's apparent double indirection is an artefact of **addressing**: it
indexes by the *address of* the cell where we *name* the cell, so our types are
**one star shallower**.

```
R[ac3 + -12]      ≡   FOO.a1            (the cell's contents)
M32[R[ac3 + -12]] ≡   M32[FOO.a1]       (dereferenced)
```

Salvage F2's "triple indirection" through the static link is the same: one of
those levels is computing where the parent's slot lives. Still one pointer.

## 7. Scope of the claim

The census covers `quest.ir2.book` and `quest.ir2.stock` — the **reachable**
program. If code that is currently unreached is lifted later, the one-level
property should be **re-checked, not assumed**.

---

## Cross-references

`docs/IR.md` §5.2 (the `R[e]` primary), §5.10.8 (the census as landed) ·
`emulation/quest.assumptions` (the three static rows, checkable) ·
`docs/Project44/DESIGN.md` §5.2 (the slot bijection), §7.2a (the rule/
application metric split) · `docs/Salvage.md` F2 (uplevel), F23 (the two base
registers) · `docs/Project52/q001-plan-gate.md` §4.1 (the census as first
measured) · `docs/Project55/BlockCensus.md` (block-level context)
