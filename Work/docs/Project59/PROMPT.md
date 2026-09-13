# Project 59 — ONE SITE, PROVEN OR NOT

## GOAL

**The real question: is `ac1` non-negative at DISPLAY_INVENTORY's `WCMV`
`70167F05`?**

That is the obligation. Everything else here is how far we got tracing it.

We traced `ac1` back to its sole writer and reduced the obligation to a single
condition:

> **`ac1 ≥ 0` at the site  ⟺  `len ≤ 30`**, where `len` is the length word at
> frame slot 6.

**So the job is: check that reduction, then settle `len ≤ 30`.** Both halves
matter, and the first is not to be taken on trust — see ruling 3.

**Success is any of three outcomes**, stated at the right tier:

- **PROVEN** — `ac1 ≥ 0` on every path, with the argument
- **NOT PROVEN** — the specific missing fact, why no available evidence
  supplies it, and what would
- **THE REDUCTION IS WRONG** — `ac1 ≥ 0` holds (or fails) for a reason other
  than `len ≤ 30`. **This is a real possibility and would be the most useful
  outcome**, because two readings of this site have already been wrong.

### Why one site

P58 classified 134 sites as **asserted only** — unproven, carried by a runtime
assert. We picked this one at random and could not settle it by reading. So:
are those sites **genuinely unprovable**, or has the analysis not been careful
enough? **One worked example answers that for the class**, which is worth more
than the site.

---

## The site

`docs/Project58/asserts.tsv`, tier `unknown`, reason `register`:

```
70167F05   block 70167EF9   WCMV   DISPLAY_INVENTORY   dst/ac0 and src/ac1
```

### What is established

**Block 70167EF9 — the site:**

```
ac0 = sx16(M16[wp(ac2, 0)])       ; the string's current length
ac0 = add(ac0, ac1)               ; new length = old + piece
M16[wp(ac2, 0)] = trunc16(ac0)    ; store it back
ac2 = bp(ac2, 2)                  ; the data — 2W+2, the varying layout
ac2 = add(ac2, ac0)               ; to the new end
ac2 = sub(ac2, ac1)               ; back off by the piece → the old end
ac0 = ac1                         ; dst count := piece length
[@ac2, ac0] = [@0x701673AD:0, ac1] ; WCMV — THE SITE
```

**Block 70167EEF — the sole writer of `ac1`:**

```
ac2 = wp(ac3, 6)                            ; the string is frame slot 6
ac1 = 0x1E                                  ; 30
ac0 = 0x03                                  ; 3
ac1 = nsub(ac1, M16[wp(ac2, 0)])            ; ac1 = 30 − len
goto [70167EF8, 70167EF9] (ac0 >=s ac1)     ; WSGE, SIGNED
```

**Block 70167EF8** (taken when the guard is FALSE, i.e. `ac1 > 3`):
`ac1 = ac0` → 3.

So **`ac1 = min(3, 30 − len)`**, and it is **negative iff `len > 30`**.

Label order verified: IR.md §3 — *"a STRICT index into the label list:
false=0 / true=1"*. Signedness verified: `EagleCompute.cpp:198` casts both
operands to `int32_t`, and the machine has separate unsigned forms
(`WUSGT`/`WUSGE`) the compiler did not use.

**So the reduction is: `ac1 ≥ 0` ⟺ `len ≤ 30`.** Check that derivation
independently, then answer the second half: **can `len` — the length word at
`wp(ac3, 6)` — exceed 30?**

### What we found looking for the bound, and where we stopped

`wp(ac3, 6)` in DISPLAY_INVENTORY is written **three times, all bare length
stores**, never by a varying assignment with a capacity clamp:

```
556:  M16[wp(ac3, 6)] = trunc16(ac0)
622:  M16[wp(ac3, 6)] = trunc16(ac0)
917:  M16[wp(ac3, 6)] = trunc16(ac1)
```

So **`assign_varying`'s `min(len, cap)` induction does not apply here.** The
length is maintained by hand — the append primitive above. The bound, if there
is one, is a property of the program's arithmetic.

**Do not take that as settled.** Two readings of this site were already wrong:
first the direction of the `min`, then an inferred `CHAR(30)` capacity that
turned out to be a different slot in a different frame. **Re-derive
everything.** Trust nothing in this section you have not checked.

---

## What to do

**Trace `len`.** Every write to the length word at `wp(ac3, 6)` on every path
reaching `70167F05`, what each adds, and what the running total can reach.
The appends add known pieces; the question is whether the sum is bounded.

Things that may matter, none of them established:

- what slot 6's field actually is, and whether any declaration of it exists
  anywhere (`game/declarations.json` covers seven translated routines; is
  DISPLAY_INVENTORY one?)
- the other bounds in the same routine — line 646 uses `(0 − len + 19)`, a
  remaining-room subtraction against **19**, not 30. Different fields, or the
  same one against different limits?
- whether the appends are guarded by their own tests upstream
- whether a callee writes the slot through a by-reference argument
- P58's `<opaque slot 6 clobbered: WCMV destination (frame range unknown)
  @70167ED5:7>` — is that clobber real, or an over-approximation?

**Use `compiler/countflow.py`.** P58 built it; it does reaching definitions to
a true fixpoint over ac0–ac3 and frame slots. Extend it if you must (new file),
but **read `docs/Project58/REPORT.md` §2 first** — it lists what defeated it
and why.

---

## What you may assume — and the exception that matters

**A routine's frame is private.** Nothing in the outside world writes it: no
other process, no interrupt, no other routine reaching in. So the writers of
`wp(ac3, 6)` are the statements in DISPLAY_INVENTORY itself —

**…UNLESS the routine handed a pointer to that slot out.** Three ways, and all
three must be checked rather than assumed away:

1. **a game call** taking `wp(ac3, 6)` — or any pointer derived from it — as an
   argument; the callee writes through it
2. **a runtime call** doing the same. `?READ` filling a buffer is exactly this
   shape, and DISPLAY_INVENTORY is on the login path
3. **uplevel access** — a nested `.N@` entry reaching this frame through the
   static link (Salvage F1/F2). DISPLAY_INVENTORY's family, if it has one

So the assumption is **"only this routine writes its frame, unless it passed a
pointer to that slot out"**, and the second clause is the work: enumerate every
`call` and `rt_call` in the routine, and check whether any argument is slot 6
or derived from it.

**If none is, the slot has exactly the three writers already found** — and
that also settles P58's `<opaque slot 6 clobbered>` from the other direction:
not by bounding the scratch buffer, but by showing nothing outside the routine
can reach the slot at all.

---

## The CFG is closed — verified for this routine

**Nothing jumps into a routine's blocks except through its WSAVS entry.** Two
exceptions exist in general, and **both are absent here — checked**:

| exception | status for DISPLAY_INVENTORY |
|---|---|
| the routine's own `.N@` nested entries (separate WSAVS frames; P47 found three `I.GOTO` edges landing in sibling pieces — DROP.1→DROP, ATTACK.4→ATTACK.3, KILL_PLAYER.1/.4→KILL_PLAYER.2) | **none.** Zero `DISPLAY_INVENTORY.N@` rows in `quest.addrbook`; it sits alone between DISPLAY_SCREEN and DISPLAY_FLASK, one entry, argc 1, frame 0x9E |
| an ON-unit body it establishes (a signal enters mid-routine; DESIGN §11) | **none.** Zero mentions in `docs/ON_ERROR_CATALOG.md`, no establisher address in its range |

*(The third general case — hand-assembled code branching across ranges, R39's
LOCK_FILE/UNLOCK_FILE — is a named pair elsewhere and does not touch this
routine.)*

**So entry is via WSAVS at `701674B5` only, and every path to the site starts
there.** "Every write to the length word on every path" is a **finite walk**,
not an open-ended search. Re-confirm both rows cheaply, then rely on them.

**This is a better position than P58 worked from.** P58 had to assume an
unbounded copy *might* reach slot 6. With the CFG closed and the frame private,
the only clobber candidates are **this routine's own copies** — and those are
enumerable.

---

## Rulings

1. **"The game works" is evidence, not a proof**, and it is evidence about
   **executed paths with real data**. It is worth stating — if the only reason
   to believe `len ≤ 30` is that the program has never misbehaved, **say that
   is the reason**. That is a real finding and it is the answer for the whole
   tier if it is the answer here.
2. **Name the tier**: proven by construction / by dataflow / by a dominating
   guard / by provenance / unproven. P58's ruling 1, unchanged.
3. **Re-derive, do not inherit.** Everything above was worked out in
   conversation and twice got wrong. The block listings are transcriptions —
   check them against the book.
4. **If the site turns out PROVABLE, the finding is about the METHOD**, not
   about this site: what did P58's analysis miss, and is the miss systematic?
   That is worth more than the site.

---

## Part 1 — no plan gate

**This project is small enough to run straight through.** Push a question only
if you are blocked. Report when you have an answer or have established you
cannot get one.

---

## Deliverable

`docs/Project59/REPORT.md`:

- **the verdict**, at its tier
- **the trace**: every write to the length word, every path, what bounds each
- **what P58 missed**, if anything, and whether the miss is systematic — this
  is ruling 4 and it decides whether the other 133 sites get revisited
- **what would settle it** if it is not settled
- anything above that did not survive contact — expect some; two readings
  already failed

---

## Boundaries — BINDING

1. **You may WRITE:** `docs/Project59/**`, and tooling in `compiler/` (**new
   files only** — do not modify `countflow.py`).
2. **Do NOT modify** `emulation/**`, `game/**`, `docs/IR.md`,
   `docs/Project58/**`, or any artifact. **Regenerate nothing.**
3. **Nothing executes.**

## Coordination

`docs/Project59/q00N-short-title.md`, push to **main**, then **STOP and tell
the user**. You own `q*.md`; the integrator owns `a*.md`. SOP:
`docs/INTEGRATOR.md` §10.

## Delivery

**Push to `p59-onesite`.** One `Work.tgz` with the report.
