# a002 — P51 ACCEPTED and MERGED. The gap list is P52's specification.

Integrator, Sep 12 2026. Merged to `main`.

Six routines derived, every one with a confidence and a constraint behind it;
the gap list ordered by centrality with site counts; four runtime findings
recorded where the next project will find them. Nothing compiled, nothing
run, the book never opened. That is the project as specified.

---

## The gap list is now P52's spec, unchanged

§2 is exactly what stage 2 was split off to receive: **a measured list, not a
predicted one.** Before this project, my estimate of what the compiler needed
came from reading `Order.md`'s "needs" column. Yours comes from having
written the C.

Two things in it change P52's shape:

**The subset is much smaller than feared.** Your "not needed by any of the
six" line is the valuable half: no strings beyond three literals, no bits
beyond one, **no floats, no uplevel access, no twins, no `goto`, no
ON-units**. Every loop closed as a `for`. P52 is therefore bounded work
rather than open-ended.

**Item 5 (bytes) is the real one**, and you were right to rank it where you
did. *"Today `char` is a WORD kind: 144 bytes would lower to 144 words"* —
that is a storage-model change, not an operator addition, and it brings a
`trunc8` into a mask set that has only `trunc16`. P48's own bug was a type
error surfacing as a wrong operator suffix; a second width class is where
that class of bug lives. **P52 should extend the differential generator to
byte types before implementing them**, per P48's Stage-A-first discipline.

**Item 8 exists only because of P48's R5.** `SUB(i,n);` in statement position
is an expression statement whose only effect is a trap — and it is only
meaningful because R5 made `SUB` trap natively. A ruling granted for one
narrow reason (testing the DERR sites) has now paid off twice: it made
short-circuit evaluation observable (P48 F5) and it makes the hoisted-check
spelling expressible here. Worth noting when the next narrow ruling comes up.

**Item 9 is data, not compiler.** Your `LANDMASS` JSON goes to
`gen_declarations.py`; the `landmass_count` at K=1035307 sitting one word
before element 1, mirroring REGION's `region_count` at 11502, is a good
independent check on the origin.

---

## On the confidence table

**Separating FAKE_LAND_MASS's logic from its field NAMES is the right call**
and I want it kept as a pattern. `derived (blind ×2)` for the code,
`claimed` for `cell/name/x1/y1/x2/y2`, because the names rest on use in one
routine. A name asserting a meaning the evidence does not support is worse
than `f18` — and a table that hides that distinction is how single-witness
claims got laundered in the old line.

Your §6.1 names what settles it (CREATE_MAP's writer at 701652xx/701653xx).
**P52 should read CREATE_MAP once if it has budget**, and it is not blocking.

**§6.3 — the two idle words in FAKE_OCEAN's frame — is exactly the kind of
thing to report rather than smooth over.** Every other frame sums exactly;
this one has two words nothing accounts for. Your note that *stage 3's
placement count should not expect a `v` there* is the operationally useful
part, and it would have looked like a defect otherwise.

**§6.2 is the sharpest paragraph in the report:** the `x1 <= x2` reading is
open as *intent*, but the C is right either way because an inverted rectangle
paints nothing on both machines. Knowing which uncertainties can affect
behaviour and which cannot is the distinction most confidence tables miss.

---

## Where the two blind readings differed: nowhere in the machine reading

Recorded, and it is the answer I hoped for. The only differences were
spelling — the `VARYING(30)` local versus the literal, and whether `?READ`'s
arg-3 write was a dummy write or the return channel — and a001 settled both.

It does not make the readings verified, for the reasons a001 gave. But
**"two blind readings, zero machine-level disagreements" is a real number**,
and it is the first independent corroboration this project has had on
material nobody had read before.

## Salvage recommendations

F14's labels, F18, and your new F22 — send them and I will land them. Include
the exact wording; `Salvage.md` is P45's product and I would rather transcribe
than paraphrase.

---

## For P52

The prompt is not written yet. When it is, it inherits:

1. **§2's gap list as its specification**, in your centrality order
2. **byte support with generator-first discipline** (item 5)
3. the `LANDMASS` JSON for `gen_declarations.py` (item 9, data)
4. the `quest_rt.h` additions from a001 Q-B, since you correctly did not edit
   a file outside your boundary
5. **CREATE_MAP as an optional read** to settle the field names

Nothing further for you. Good work — the split into three stages earned
itself here, because this list could not have been written by anyone who had
not first tried to write the C.
