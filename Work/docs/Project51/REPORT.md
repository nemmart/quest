# Project 51 — REPORT: CANDIDATE C (stage 1 of three)

Worker session, Sep 12 2026, branch `p51-candidate-c`. Gate: `q001-plan-gate.md`
+ `q002-gate-addendum.md`, rulings `a001-plan-gate.md` (all granted; the two
q002 findings adopted). Carried by the q002 session: the q001 session's
connection was lost after the gate (user, 16:37), so the session a001 named
to stand down is the one that built. Nothing in Part 2 conflicts with a001;
where a001 says "your" reasoning it means q001's, which this session
followed.

**Outcome.** Six `.c` files in `game/routines/` (HIT_ANY_CHAR, PICK_X_Y,
GET_INPUT re-derived blind; INIT_SCREEN, FAKE_OCEAN, FAKE_LAND_MASS new),
each with its derivation and confidence in the header; UPDATE_SCREENS as
the calibration row; the compiler gap list (§2); four runtime findings in
`RTConventions.md`. **Nothing was compiled, loaded or run.** `quest.ir2.book`
and `docs/attic/` were not opened. The old `game/routines/*.c` were opened
only after the corresponding new file was written (§4).

---

## 1. The confidence table

| routine | stmts | confidence | what constrains it | blind ×2? |
|---|---|---|---|---|
| UPDATE_SCREENS | 72 | **verified** | ran (P48 §3.1: 594+594+198 expectations, DERR leg); P48's header is the derivation | — (not re-derived) |
| PICK_X_Y | 64 | **verified** | matched 64/64 under the old line (P35); this re-derivation is blind and reaches the same body; F18's four gates; K 40 / 11502 / 11495–7 / bound 100000 all match `declarations.json` | yes |
| HIT_ANY_CHAR | 10 | **derived (blind ×2)** | frame sum 1+1+1+15 = 18 words fixes the 30-byte literal; byte-displacement reading closes the frame and is confirmed by GET_INPUT's `WSTB` through arg 1; `quest.mem` bytes | yes |
| GET_INPUT | 22 | **derived (blind ×2)** | F14's frame closes (2 + 72 + 4×2 = 82); ?READ's body fixes the roles of args 3/4; 27 callers push `XPEFB` | yes |
| INIT_SCREEN | 134 | **derived (blind ×2)** | the 4/5 vs 9/11 bound pairing (P48's constraint, same fields); the column/row formula shared with UPDATE_SCREENS and inverted by FAKE_OCEAN; XNDO/XWDO exits close the nest only from pc+1 | yes |
| FAKE_OCEAN | 224 | **derived (blind ×2)** | F18's constants from an independent site (0x3B73's second witness) with clip origins gate+1; the clip arithmetic is the inverse of INIT_SCREEN's column mapping; 10 of 12 frame words accounted (2 idle) | yes |
| FAKE_LAND_MASS | 255 | **derived (blind ×2)** for the logic; **claimed** for the field NAMES | record layout sums to the stride using CREATE_MAP's name writer (2 + 16 + 4 = 22); the same column mapping; both overlap halves agree with the fill loop. The names `cell/name/x1/y1/x2/y2` rest on use only | yes |

"Blind ×2" per a001 §0: two sessions sharing the same clone, `Salvage.md`
and method read every routine independently before either file existed and
agreed on every substantive point. It corroborates; it does not verify.

**Where the two readings differed (a001 §5 asked):** nowhere in the
machine reading. Only in spelling — q001 would have declared HIT_ANY_CHAR's
message as a `VARYING(30)` local (a001 §2 chose the literal), and q001
recorded ?READ's arg-3 write as a harmless dummy write where q002 had read
its meaning from the body (a001 §1). Both are settled.

---

## 2. THE HANDOFF — what `lower_c.py` cannot accept, by centrality

Every construct below was written because the routine needs it (ruling 4).
Counts are sites in the six files. "Central" = how many of the six need it
and whether the routine is expressible at all without it.

| # | construct | routines (sites) | central | notes for stage 2 |
|---|---|---|---|---|
| 1 | **rt call `NAME$N(...)`**, by-reference stack args (`LPEF`/`XPEF`), incl. **a value returned in ac0** | PICK_X_Y (3, valued), GET_INPUT (1), HIT_ANY_CHAR (2) | 3 of 6; each is one statement or nothing | the rows in RTConventions give argc, which args are written; a valued call is an expression |
| 2 | **game→game call** with by-ref args, incl. **passing the routine's own parameter through** (`XPEF @[ac3+0xFFF4]`) | INIT_SCREEN (2 pass-through), HIT_ANY_CHAR (1) | 2 of 6 | the callee's frame/argc from the addrbook; pass-through pushes the incoming pointer unchanged |
| 3 | **dummy arguments `TMP(e)`**: constant, expression, and **pointer-valued** (`TMP(buf)`) | PICK_X_Y (6, all 32-bit), GET_INPUT (3: two 16-bit constants, one byte pointer) | 2 of 6 | width = the callee parameter's width (?RANDOM_NUMBER: 32; ?READ args 3/5: 16; arg 2: a word address holding a byte pointer) |
| 4 | **CHAR constant argument → CHAR VARYING dummy** (length word + WCMV; or one `WLDAI` wide store when ≤ 2 bytes) | HIT_ANY_CHAR (2) | 1 of 6, 2 of its 3 statements | the temp is reused across statements; capacity = the literal length; even-slot start (F22) |
| 5 | **bytes**: `unsigned char` scalar and array locals, byte pointers as arguments (`XPEFB`), byte load/store through a byte-pointer parameter (`XLDB/WLDB/WSTB`), 8-bit narrowing (`WANDI 255`, a `trunc8`) | GET_INPUT (all of it), HIT_ANY_CHAR (1 local, 1 arg) | 2 of 6 | today `char` is a WORD kind: 144 bytes would lower to 144 words; the mask set has trunc16 only |
| 6 | **`BITS("001")`** — a BIT literal built by X.CB at run time | GET_INPUT (1) | 1 of 6 | RTConventions X.CB row: ac2 = dest word, ac0 = byte ptr, ac1 = len, undecorated LCALL |
| 7 | **`MIN(a,b)` / `MAX(a,b)`** builtins | FAKE_OCEAN (4), FAKE_LAND_MASS (4) | 2 of 6 | the `WSGE a,b; WMOV a,b` diamond, like ABS; MAX against 0 uses `WSGE r,r; WSUB r,r` |
| 8 | **`SUB(i,n);` in statement position** (a001 Q-A) | INIT_SCREEN (1), FAKE_OCEAN (2), FAKE_LAND_MASS (2), PICK_X_Y (1) | 4 of 6 | an expression statement whose only effect is the DERR-17 trap; it exists because P48 R5 made SUB trap |
| 9 | **the LANDMASS table** + `OBJ_PTR->landmass_count` | FAKE_LAND_MASS | 1 of 6, the whole routine | data, not compiler: JSON in §2.1; declared locally in the file for now |
| 10 | `&` of a static / field / local as a call argument | all callers | with 1–2 | today `&` is refused outside by-ref params |
| 11 | prototypes: `GET_INPUT(unsigned char *)`, `FAKE_LAND_MASS(const int16_t *)`, `FAKE_OCEAN(const int16_t *)`, `INIT_SCREEN(const int16_t *)`, `PICK_X_Y(int16_t *, int16_t *)`, `MIN`, `MAX`; `READ$6`'s arg 2 is really a pointer-to-pointer under TMP | header (`quest_rt.h`, P44/P48-owned; not edited) | — | one vocabulary (a001 Q-B) |

**Not needed by any of the six** (so not on the list): strings beyond the
three literals, bits beyond the one literal, floats, uplevel access / the
static link, twins, `goto` (the loops all close as `for`), ON-units,
`switch`, compound assignment.

**Already in the subset and used as-is:** `int16_t`/`int32_t` locals,
`*param` reads and 16-bit stores, statics, the PLAYER/REGION tables and the
2-D `screen` field, `ABS`, `SUB` in expressions, nested `for` (constant and
computed bounds, ascending and descending with `--`), `continue`, `return`,
`for (;;)`, `/`, `&&`/`||`, `|`/`&` on comparison results, casts.

### 2.1 The LANDMASS declaration for `gen_declarations.py`

```json
"LANDMASS": {"base": "OBJ_PTR", "bound": 1000, "stride": 22, "minK": 1035286,
  "origin": 1035308,
  "comment": "P51 FAKE_LAND_MASS + CREATE_MAP 701744CA..EC: named land-mass rectangles; bound 1000 = DERR 17 at 701697DD; origin base+1035308 if `cell` is field 0; `name` is CHAR(30) VARYING (length word at +2, data at byte 2*0xFCC19)",
  "fields": {"cell": {"K": 1035286, "width": 32},
             "name_len": {"K": 1035288, "width": 16},
             "x1": {"K": 1035304, "width": 16}, "y1": {"K": 1035305, "width": 16},
             "x2": {"K": 1035306, "width": 16}, "y2": {"K": 1035307, "width": 16}}}
```
and in `obj_ptr_hdr`: `"landmass_count": {"K": 1035307, "width": 16}` (the
word before element 1 — REGION's `region_count` at 11502 is the same idiom).
Other users of the table, for stage 2's cross-check: CREATE_MAP (701652xx,
701653xx, 701744xx), 7016A9xx, 701743xx–7017 46xx, 7017BAxx–BBxx (63
displacement sites program-wide).

### 2.2 The eager / branch table (a001 Q-D, gathered)

| site | routine | PL/I | machine shape | C |
|---|---|---|---|---|
| 70176215 | PICK_X_Y | `x = 0` | `MOV.# SNR; WBR` | `if (...) continue` |
| 70176223 | PICK_X_Y | `type/100+1 ¬= 3` | `WSEQI; WBR` | `if (...) continue` |
| 70176267..75 | PICK_X_Y | 4-term `&` | skip chain to one stub | `&&` |
| 701699D3..DF | FAKE_OCEAN | `x+4>16300 \| x-4<=15349` | two skips to one stub | `\|\|` |
| 701699E1 | FAKE_OCEAN | `x-4 <= 15349` | skip | `if/else` |
| 70169A6F..7D, 7F | FAKE_OCEAN | y versions | same | `\|\|`, `if/else` |
| 701697EF..FF | FAKE_LAND_MASS | `x-4<=x1 \| x-4<=x2` | **materialised** (WADC/WSLE/WSUB, WIOR, MOV.L# SZC) | `\|` |
| 70169819..2E | FAKE_LAND_MASS | `x+4>=x1 \| x+4>=x2` | **materialised** | `\|` |
| 70169840..76 | FAKE_LAND_MASS | `(..\|..) & (..\|..)` | **materialised** (WIOR, WIOR, WAND) | `\|`, `&` |
| all cell tests, ABS tests, DO entry/step tests | INIT_SCREEN, FAKE_OCEAN, FAKE_LAND_MASS | | skips | `if`, `for` |
| 7016AA64 | GET_INPUT | `> 128` | skip, WRTN | `if` |

The same PL/I `|` on the same kind of operands (two comparisons) is emitted
both ways by the 1986 compiler: jumps in FAKE_OCEAN (immediate compares of
one variable), materialised in FAKE_LAND_MASS (register–register compares
of field loads, inside a larger boolean). Stage 3's `eager_bool` axis is
observable exactly at the three FAKE_LAND_MASS sites.

### 2.3 Other rewrites stage 3 will meet (recorded in the headers)

- DO-limit temp (`n = ..` spelled as a local per a001 R2): INIT_SCREEN,
  FAKE_OCEAN ×2, FAKE_LAND_MASS ×2; reloaded at every step (P4).
- Hoisted scaled subscripts and element bases: PICK_X_Y (r*9, base),
  INIT_SCREEN (x*22, who*686, k*9), FAKE_OCEAN (i*22, 2j), FAKE_LAND_MASS
  (m*22, 22i, three element pointers) — with SUB placement matching the
  hoist (a001 Q-A).
- CVWN: PICK_X_Y ×3 (after `/`, before both 16-bit stores); the C is plain
  (a001 Q-C).
- 16-bit N-op arithmetic in MIN/MAX operands (FAKE_OCEAN, FAKE_LAND_MASS).
- Variable reuse INIT_SCREEN slots 2/3 (a source fact, spelled as such).
- CHAR constant ≤ 2 bytes → `WLDAI` + one wide store (HIT_ANY_CHAR).
- Temp-slot reuse across statements everywhere (PICK_X_Y 4/6/8; HIT_ANY_CHAR
  4..19; FAKE_LAND_MASS 12..25 in mixed widths).

---

## 3. Runtime routines

Documented in `docs/Project28/RTConventions.md` (additive section "Additions
from Project 51"): **X.CB** (entry, three registers, call form, both sites —
the row F12 implied but the file lacked); **?READ** argument roles from its
body (arg 3 count in/out at 7017DEE4, arg 4 = 0x8000 flag at 7017DED9, arg 2
a pointer-valued dummy); **?RANDOM_NUMBER** caller-side facts (all lo/hi
dummies are 32-bit temps; seed direct); **?WRITE_SCREEN** at a CHAR-constant
site (the VARYING dummy; the ≤ 2-byte immediate form); and the reading
convention that byte-form X displacements are bytes.

Could not work out: nothing was left open. Not met: no other runtime routine.

---

## 4. Did the old `game/routines/` file agree — and was it read first?

All three were opened **only after** the new file was complete (the old
file's body was diffed against the new one in the same command that wrote
it; the headers were read afterwards). Both q001 and q002 stated at the gate
that none had been opened.

| routine | agreed? | differences (all spelling, none in the reading) |
|---|---|---|
| HIT_ANY_CHAR (P38) | **yes** | old: `GET_INPUT$1(&c)` and the second message as `TMP(0x00020D0B)` — the machine's packed form; new: `"\r\v"`. Same statements. |
| PICK_X_Y (P35, 64/64) | **yes** | old: `retry:` + `goto`s, `SUB()` inline at the first REGION access, explicit `cvwn()` (P36 ruling), the 4-term gate as four `if/goto`; new: `for(;;)`/`continue`, bare `SUB(r,100000);` (a001 Q-A), plain narrowing (Q-C), `&&` (Q-D). Same dummies, same seed, same constants. |
| GET_INPUT (P37, staged) | **yes on every statement, one index-base difference** | old: `READ$6(&IN_CHAN, buf, ...)` (arg 2 as the buffer itself — the machine passes a dummy holding its byte pointer, which new spells `TMP(buf)`), `*c = buf[1]` under a 1-based `ARRAY1` convention where new has C's `buf[0]` (byte 8 = slot 4 = the first byte in both), `& 0xFF` explicit where new casts. |

So the old translator's shape and the candidate C agree on what the
routines DO; stage 3's measured rewrite distance will not be inherited
shape, because every structural choice above was made by a001's rulings,
not by the old files.

---

## 5. `Salvage.md` — recommended corrections (file not in my boundary)

1. **F14 (wording, a001 §1 says the integrator lands it).** Replace
   `?READ$6(chan=0x70000262, buffer, one, count&, options=0x1000, flags)` with
   `?READ$6(chan=0x70000262, TMP(buf) [a dummy holding the byte pointer],
   count (in/out: 1 requested, bytes read written back at 7017DEE4),
   flag& (16-bit; 0x8000 on error 24 at 7017DED9, never read by the game),
   options=0x1000, '001'B)`. Frame facts unchanged.
2. **F18**: add FAKE_OCEAN 70169A7A/70169A83 (0x3B73) and its clip origin
   0x3B74 as witnesses; a001 counts four routines.
3. **F22**: add HIT_ANY_CHAR (CHAR(1) at the even pair 2–3, VARYING temp
   starting at 4) as the second witness; "Single" → "High".
4. Nothing else touched needed correction (F12, F13, F17's K folding, P4,
   P8, P12, P13 all re-seen as stated).

---

## 6. Where I am least confident, and what would settle it

1. **FAKE_LAND_MASS's field names** (`claimed`). `cell`, `x1..y2` are named
   from their uses in this routine only. What settles it: CREATE_MAP's
   writer (701652xx/701653xx) — if it stores the rectangle from PICK_X_Y-like
   coordinates and the code from a terrain table, the names hold; the
   VARYING(30) `name` is already settled by 701744DA..EC. A stage-2 reader
   with budget should read CREATE_MAP once.
2. **The `x1 <= x2` assumption.** The overlap test is corner-agnostic but the
   fill loop takes x1/y1 as low and x2/y2 as high. If the data ever has
   x1 > x2 the loop is empty and the routine paints nothing for that mass —
   which is what the machine does too, so the C is right either way; it is
   the *reading of the programmer's intent* that is open, not the C.
3. **The two idle words in FAKE_OCEAN's frame** (slots 12–13). Every other
   frame sums exactly. A temp the compiler allocated and never used is the
   likely story; nothing in the C depends on it, but stage 3's placement
   count should not expect a `v` there.
4. **`TMP(OBJ_PTR->region_count)` in PICK_X_Y.** The copy-then-push says
   "dummy"; WHY the source needed one (declared type of the ENTRY's
   parameters vs the field's) is not visible in the listing. Stage 2 need
   only reproduce the copy; the reason is a curiosity.
5. **GET_INPUT's `(unsigned char)(*ch - 128)` vs `& 255`.** Same bits; the
   choice affects only whether stage 2 emits the mask from a cast or from an
   explicit AND. Either is fine; the header says so.
6. **Operand order inside MIN/MAX** follows the machine's evaluation order;
   PL/I's source order is unrecoverable from the listing. Cosmetic.

Not on the list: SUB placement (read instruction by instruction and stated
per site), the loop closures (checked on all eleven XNDO/XWDO/hand-built
loops), the literal bytes (read from `quest.mem`).

---

## 7. Files

Branch `p51-candidate-c` (also `q001`/`q002` on main). Written:
`game/routines/{HIT_ANY_CHAR,PICK_X_Y,GET_INPUT,INIT_SCREEN,FAKE_OCEAN,FAKE_LAND_MASS}.c`,
`docs/Project28/RTConventions.md` (one additive section),
`docs/Project51/{q001-plan-gate,q002-gate-addendum,REPORT}.md`.

Not touched: `compiler/**`, `emulation/**`, `docs/Project50/**`,
`Disassembled/**`, `docs/IR.md`, `docs/Salvage.md`, `docs/Project44/DESIGN.md`,
`game/{declarations.json,declarations.h,quest_rt.h}`,
`game/routines/UPDATE_SCREENS.c`, any artifact. Nothing compiled, loaded or
run; `lower_c.py` was read, never invoked.
