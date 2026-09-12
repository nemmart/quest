# a001 — P51 gate + addendum: APPROVED. All decisions granted. One session carries Part 2.

Integrator, Sep 12 2026. Answers `q001` and `q002` together.

---

## 0. Coordination — the q001 session carries Part 2

**The session that pushed `q001` (2840a02) continues. The q002 session stands
down.** q002's recommendation is right that two sessions on one clone will
clobber each other, and its own judgement that the readings are
interchangeable settles which one continues — so it goes to the one holding
the full plan and the worked example.

**Both of q002's substantive findings are ADOPTED and carry into Part 2**
(§1 and §2 below). The standing-down session's work is not discarded; it is
merged into the continuing one's instructions.

### And the accident is worth more than the inconvenience

Two sessions derived **all seven routines blind** and agree on HIT_ANY_CHAR's
18-word frame sum and 30-byte literal, the byte displacements, GET_INPUT's
byte-pointer parameter, the packed `\r\v` immediate, PICK_X_Y's shape and
0x3B73, INIT_SCREEN's loop variables, FAKE_OCEAN's MIN/MAX clips,
FAKE_LAND_MASS's undeclared stride-22 table down to its field offsets and
bound, and the gap list's ordering.

This project has spent considerable effort worrying about single-witness
claims. **A blind second reading is the cheapest independent corroboration
available**, and it arrived by accident.

**Ruling on what it is worth:** record it per routine as corroboration and
say so in the confidence table. But it does **not** upgrade `claimed` to
`verified` — the two sessions shared a container, a clone, the same
`Salvage.md`, and the same reading method, so they share priors and a common
misreading is possible. Two blind readings agreeing is real evidence and
weaker than two *methods* agreeing. Mark it as `derived (blind ×2)` where it
applies, which is more informative than either existing label.

---

## 1. `?READ`'s argument roles — ADOPTED, and it corrects Salvage F14

q002 read `?READ`'s body at 7017DE5F and found:

- **arg 3 is the byte count, in/out** — GET_INPUT passes the constant 1 in
  temp slot 80; `?READ` overwrites it with the transferred count
  (`XNSTA 0,@[ac3+0xFFF0]`)
- **arg 4 is a 16-bit end/error flag**, set to `0x8000` on error code 24 when
  the site passes ≥ 4 arguments (`XNSTA 0,@[ac3+0xFFEE]`)

So F14's labels *"one, count&"* are better read as *"count (in/out),
flag&"*. F14's frame facts, pushes and constants are unaffected and remain
right.

**This is the RT duty working exactly as intended.** Put both facts in the
`?READ` row of `RTConventions.md` during Part 2, with the body addresses as
evidence. I will land the F14 label correction in `Salvage.md` myself —
recommend the exact wording in the report.

It also explains something q001 recorded but could not interpret: the arg-3
write is not a harmless dummy write, it is the *return channel*.

## 2. Q-B, literal-as-argument — q002's form GRANTED

For a CHAR constant argument, pass the literal:

```c
WRITE_SCREEN$2(&OUT_CHAN, "\vHit any character to continue")
```

and let the lowering build the VARYING dummy — rather than declaring a local
and calling `assign_varying`.

Three reasons, in order of weight:

1. **The frame says it is a temp, not a variable.** Slots 4..19 are a
   compiler temp reused for *both* literals, with the second packed by
   `WLDAI`. A named local in the C would be asserting a source construct the
   evidence does not support.
2. **One rule for all dummies.** It is the same principle as `TMP(e)` for
   numerics: the source says the value, the compiler owns the temp.
3. **It gives stage 2 one named gap** — *CHAR constant → VARYING dummy (WCMV,
   or one wide store when ≤ 2 bytes)* — instead of a header struct plus
   `assign_varying`.

---

## 3. The rest of q001's decisions

**Q-A — (c) GRANTED**, the bare `SUB(i, 9);` statement at the outer-loop head
with an unchecked subscript at the store.

Your reasoning is the right one and I will restate it because it generalises:
it **puts each check exactly where the original has one**, introduces no
storage, and leaves the `i*22` recompute for the rewrite to find. Option (b)
would have been the C doing the rewrite's job, which is precisely what
carried-in ruling 4 forbids — and stage 3 would then have measured a hoist
that never happened. Apply the same rule to every hoisted check in #5–#7 and
say so in each header.

Note for stage 2's gap list: *an expression statement whose only effect is
`SUB`'s trap*. Small, and it depends on `SUB` trapping — which exists only
because P48's R5 was granted.

**Q-B — reuse the `quest_rt.h` dialect: GRANTED**, with your three additions
(`TMP()` accepting a pointer, `MIN`/`MAX` builtins, `SUB` in statement
position) plus §2's literal-as-argument. **One vocabulary matters more than
the best vocabulary** — you are right about that, and inventing a second
spelling would hand stage 2 two dialects to support.

Correct not to edit `quest_rt.h`; the additions go in the handoff and as
comments at first use.

**Q-C — follow P48: GRANTED.** Plain assignment, `CVWN` named in the header.
The seven files and UPDATE_SCREENS must speak one C, and under P48's R2
`cvwn` is effectful and is a rewrite's job anyway.

**Q-D — GRANTED**, and this is the most valuable of the six. Write `|`/`&`
where the machine materialises and `&&` where it branches, with the header
naming which shape the listing has at each site.

You have spotted that this feeds P48's F5 directly: the `eager_bool`
sensitivity was measured at 1 catch in 18 programs, and it is observable only
where evaluating the right operand can trap. Your per-site record turns that
from a generator property into **evidence about the program**. Make it
explicit in each header — stage 3 will want it.

**Q-E — GRANTED as recommended.** Declare the LAND table locally in
`FAKE_LAND_MASS.c` in a clearly marked block for stage 2 to move into
`gen_declarations.py`, with the full field table and per-field evidence in
the handoff. Name fields by what the uses say they are, by K otherwise —
and say which is which, since a name asserting a meaning the uses do not
support is worse than `f18`.

**Q-F — GRANTED.** UPDATE_SCREENS enters the confidence table as `verified`
(ran, P48 §3.1) pointing at P48's header. Do not rewrite it.

---

## 4. On `Salvage.md`

All checks holding is a good result for P45, and **0x3B73 now having four
witnesses** closes the one attic correction that was carrying a single one.

---

## 5. For the report

Beyond what the prompt asks:

- **the blind-agreement record** — which routines two independent readings
  agreed on, and any point where they did not
- **the `?READ` row** as landed in `RTConventions.md`, and your recommended
  wording for the F14 correction
- the Q-D per-site eager/branch table, gathered in one place rather than only
  scattered through headers

Proceed to Part 2.
