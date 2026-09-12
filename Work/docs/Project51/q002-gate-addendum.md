# Project 51 — q002: addendum to the plan gate (Sep 12 2026)

A second worker session (this one) reached the plan gate independently,
sharing the same container and clone as the session that pushed
`q001-plan-gate.md` (2840a02). Rather than a second q001, this is the
delta: an independent blind reading of all seven routines that **agrees
with q001 on every substantive point** — HIT_ANY_CHAR's 18-word frame sum
and 30-byte literal, byte displacements for `XLEFB/XPEFB`, GET_INPUT's
byte-pointer parameter, the packed `\r\v` immediate, PICK_X_Y's shape and
0x3B73, INIT_SCREEN's loop variables reused as the coordinates, FAKE_OCEAN's
MIN/MAX clips with short-circuit `|`, FAKE_LAND_MASS's undeclared stride-22
table (cell @+0, CHAR(30) VARYING name @+2..+17, x1 y1 x2 y2 @+18..+21,
bound 1000, count at OBJ_PTR+1035307) with eager booleans, MIN/MAX as the
new builtins, CVWN per P48 F7, the gap list's ordering. Two things q001 does
not have, and one recommendation for a001.

## 1. ?READ's argument roles, from its body (for the RT duty and F14)

`Disassembled/quest-rt.dis`, ?READ @7017DE5F:

```
7017decd XWLDA 0,[ac3+0x12]        ; error code
7017decf WSEQI 0,24                ; == 24 ?
7017ded2 XNLDA 0,[ac3+0x18]        ; argc
7017ded4 WSGTI 0,3                 ; argc > 3 ?
7017ded7 NLDAI 0x8000,0
7017ded9 XNSTA 0,@[ac3+0xFFEE]     ; *arg4 = 0x8000  (16-bit)
...
7017dee2 XNLDA 0,[ac3+0xA]
7017dee4 XNSTA 0,@[ac3+0xFFF0]     ; *arg3 = bytes transferred (16-bit)
7017dee6 WRTN
```

So **arg 3 is the byte count in/out** (GET_INPUT passes the constant 1 in
temp slot 80; ?READ overwrites it with the count — PL/I allows a callee to
write a dummy) and **arg 4 is a 16-bit end/error flag** set to `'1'B`
(0x8000) on error code 24 when the site passes ≥ 4 arguments. GET_INPUT's
slot 2 is that flag, written by ?READ and read by nobody. Salvage F14's
labels "one, count&" are therefore better read as "count (in/out), flag&";
the frame facts, pushes and constants in F14 are exactly right. q001 §5
records the arg-3 write as a harmless dummy write; this is its meaning. I
would put both facts in the ?READ row of `RTConventions.md` in Part 2.

## 2. Spelling of a CHAR constant argument (adds to q001 Q-B)

q001 §3.3 spells HIT_ANY_CHAR's message as an explicit local,
`VARYING(30) msg; assign_varying(&msg, 30, "...")`, then `&msg`. The
alternative is to pass the literal itself —
`WRITE_SCREEN$2(&OUT_CHAN, "\vHit any character to continue")` — and let
the lowering build the VARYING dummy, which is what the frame says it is:
slot 4..19 is a compiler temp reused for both literals (the second one
packed by `WLDAI`), not a variable the source names. Same for the 2-byte
one. Both are defensible; the second keeps the same rule as `TMP(e)` for
numeric dummies (the source says the value, the compiler owns the temp)
and makes "CHAR constant → VARYING dummy (WCMV, or one wide store when
≤ 2 bytes)" a single named gap for stage 2 rather than a header struct plus
`assign_varying`. **Recommendation:** the literal-as-argument form, one
rule for all dummies; if the integrator prefers q001's, no objection —
what matters is one vocabulary.

## 3. Coordination

Two sessions on one clone will clobber each other. **Recommendation:**
a001 names one session to carry Part 2; the other stands down. The
readings are interchangeable, so nothing is lost either way.

STOP. Waiting for `a001`.
