# Project 51 — q001: PLAN GATE

Worker session, Sep 12 2026. Tree: `main` @ `d8959e1`. Branch for the build:
`p51-candidate-c` (this file goes to main, per INTEGRATOR §10).

Read before writing anything: `Salvage.md`, `RTConventions.md`, `RTWorklist.md`,
`Project48/REPORT.md` §3 + `game/routines/UPDATE_SCREENS.c` (the model),
`compiler/lower_c.py` (what it accepts), `Project47/CallGraph.md`,
`declarations.{json,h}`, `quest_rt.h`, `IR.md` §5.8 (strings), the
DISASSEMBLER_BYTE_OPERANDS note, and `quest.dis`/`quest.mem` for all seven
routines. **Not read:** `docs/attic/`, `quest.ir2.book`, and the old
`game/routines/{HIT_ANY_CHAR,PICK_X_Y,GET_INPUT}.c` (ruling 1 — I will open
them only after my own file is written, and say so).

Nothing was compiled, loaded or run.

---

## 1. Reading method

How a C statement gets out of `quest.dis`, in the order I actually do it:

1. **Frame accounting first, and it must close.** `WSAVS n` gives locals
   at word slots `2 .. 2n+1` (F14's GET_INPUT: 41 wides → slots up to 83,
   and its temps at 76/78/80/82 sit exactly at the top). I list every
   `[ac3+d]` the body touches with its width — `XNLDA/XNSTA` 16-bit,
   `XWLDA/XWSTA/XLEF/XPEF` 32-bit or word address, `XLEFB/XPEFB/XLDB`
   **byte** displacement (`d/2` is the slot; F14's `XLEFB 2,[ac3+0x8]` = slot
   4 is the precedent) — and the slot map has to account for the whole
   frame with no overlap. When it does, that is a constraint a wrong
   reading breaks (it settles HIT_ANY_CHAR's capacity and INIT_SCREEN's
   variable reuse below). When it does not, something is unread.
2. **Arguments.** `@[ac3+0xFFF4 − 2(n−1)]` is `*arg_n`; `XPEF @[ac3+0xFFF4]`
   pushes the caller's pointer through unchanged (a pass-through call).
   Argument order at an LCALL: last pushed = arg 1 (RTConventions: arg n
   at `wsp−2n`).
3. **Record access.** `NLDAI stride,r; WMUL; LWADD [base]` names the table;
   the `K` in the field load/store names the field; both are checked
   against `declarations.json`. A K not in the table is named by its K
   and reported (FAKE_LAND_MASS has a whole table nobody has declared, §2.7).
4. **Bounds checks** are the `WSGTI r,n; WSGT r,r; DERR 17` (or `WUGTI` for
   n > 0x7FFF) triples. I record where each one **is** and where a reuse of
   a hoisted base means one **is not**, and place `SUB()` to match
   (ruling 6). Where that conflicts with naive recompute, §4 Q-A.
5. **Control flow.** Skip-family semantics from the emulator
   (`WSGT a,b` skips if `ac[a] > ac[b]`, `a == b` compares against 0;
   `WSGTI r,imm` skips if `ac[r] > imm`). `XNDO/XWDO r,disp,[slot]` is the
   DO step: `slot++`, `ac[r] := slot`, exit to `pc+1+disp` if `slot >
   limit` — so the counter is in the register at body entry, the limit in
   the register at the step, and the entry test is the separate
   `WSLE/WSLT + WBR` before the body. A chain of `skip; WBR stub` to one
   shared stub is a short-circuit condition; `WADC r,r; WSLE; WSUB r,r`
   pairs joined by `WIOR/WAND` and tested with `MOV.L# r,r,SZC` are
   **eager** PL/I booleans materialised as 0/−1 (P7's shape, used on
   comparisons here).
6. **Strings.** `WCMV` with `ac0` = destination count, `ac1` = source
   count, `ac2` = dest byte pointer, `ac3` = source byte pointer (IR.md
   §5.8); the literal bytes are read out of `quest.mem` at the `XLEFB`
   address and their count must equal `ac1`.
7. **Runtime calls** are read against `RTConventions.md`; anything not
   there gets worked out from `quest-rt.dis` and written there before the
   C uses it (the duty).

**When a reading is ambiguous:** if two *source* shapes produce the same
machine behaviour (one `IF a & b` vs two IFs; separate variables vs one
variable reused) I write the simpler C, record the alternative in the
header, and if the choice could move stage 3's measurement I mark the
confidence `claimed` for that statement. If a *fact* is missing — a
runtime contract, a field's meaning that changes what the C says — that is
a STOP-and-report, not a guess.

**Confidence vocabulary as I will apply it:** `verified` only for
PICK_X_Y and UPDATE_SCREENS (matched/run under the old line); `derived`
where an independent constraint (frame accounting, literal length, a
second routine using the same table or constant) closes the reading;
`claimed` where one reading is all there is.

---

## 2. First pass over the seven

| # | routine | entry | argc | frame | constructs needed | `lower_c.py` lacks | runtime to work out |
|---|---|---|---|---|---|---|---|
| 1 | HIT_ANY_CHAR | 7016DE91 | 0 | 0x09 | `VARYING(30)` local, `unsigned char` local, varying literal assignment ×2, `?WRITE_SCREEN` ×2, game call `GET_INPUT(&key)` | calls (game and `$N`), `&` address-of, `VARYING`, string literals, `unsigned char` byte local | none new (X.CB duty below is for #3) |
| 2 | PICK_X_Y | 701761E7 | 2 | 0x05 | `for(;;)`/`continue`/`return`, `RANDOM_NUMBER$3` as an expression ×3 with `TMP()` dummies, `&SD_PTR->seed`, REGION field reads, `/`, 16-bit narrowing store through a pointer param, a 4-term `&&` | `$N` call returning a value, `TMP()`, `&` of a static/field | ?RANDOM_NUMBER — documented; I will add the observed **32-bit dummy width** (params are FIXED BIN(31): `XWSTA` temps even for 16-bit sources) |
| 3 | GET_INPUT | 7016AA35 | 1 | 0x29 | `unsigned char buf[144]`, `int16_t count`, `BITS("001")`, `READ$6` with a **pointer-valued dummy** (arg 2 is `&slot78`, slot 78 holds the byte pointer to `buf`), byte load/store through the `unsigned char *` param, `> 128` then `(c − 128) & 255` | byte arrays, byte pointers and byte deref/store, `BITS`, `TMP()` of a pointer, calls | **X.CB** — F12 has it; it is NOT in `RTConventions.md`. I will add it (entry, regs, the two sites, evidence) and a `?READ` note on the arg-2 form |
| 4 | UPDATE_SCREENS | 7017D635 | 3 | 0x05 | already written and run by P48 | none | none |
| 5 | INIT_SCREEN | 7016E103 | 1 | 0x05 | two `for` nests (9×11 clear; region scan 1..N), `ABS`, `SUB` placement where the original hoists a check, `continue`, `screen[..][..]` read-test-write with the base reused (no re-check on the store), two pass-through game calls | game calls, bare `SUB()` statement (Q-A) | none |
| 6 | FAKE_OCEAN | 701699B5 | 1 | 0x06 | `if` chains on the F18 constants, **`MIN`/`MAX`** (the `WSGE/WMOV` diamond), `for` with computed lo/hi bounds, 9×11 conditional fill with 11 | `MIN`/`MAX` builtins | none |
| 7 | FAKE_LAND_MASS | 701697A1 | 1 | 0x0C | a **new record table** (via OBJ_PTR, stride 22, bound 1000, count word at OBJ_PTR+1035307, fields at +0 (32-bit cell) and +18..+21 (16-bit)), `for (l = n; l >= 1; l--)`, **eager** `\|`/`&` on comparisons (`WADC/WSLE/WSUB` + `WIOR/WAND`), `MIN`/`MAX`, nested computed-bound loops, `LDAFP` spills | the table declaration (I cannot write `declarations.json` — boundary), `MIN`/`MAX`, `--` in a for-step is fine | none |

Constructs that **are** in today's subset and will be used as-is: `int16_t`
/`int32_t` locals, by-reference parameters and `*p` reads/writes, statics
and the three declared tables, `ABS`, `SUB`, `for`/`if`/`continue`/
`return`, `for(;;)`, `++`/`--` statements, `&`/`|` as eager boolean
combination, explicit casts, `/`.

**What the shape says before any budget is spent.** The gap list is
dominated by three families, in decreasing centrality:

1. **Calls** — every routine but #4 needs game→game or `$N` calls, four
   need a `$N` call used as a *value*, and three need `TMP()` dummies.
   This is the keystone gap, and it is the same one P46 F7 / DESIGN §9.3
   already name.
2. **Strings and bytes** — #1 and #3 only, but they are the two the prompt
   picked for the head-to-head, and `char` today is a *word* kind: a
   144-byte buffer would lower to 144 words. Byte addressing is a real
   extension, not a spelling.
3. **Small builtins and declarations** — `MIN`/`MAX` (#6, #7), the LAND
   table (#7), bare `SUB()` (#5, if Q-A rules that way), `&` address-of
   (#1, #2, #3).

Nothing needs bits beyond the one `BITS("001")` literal, floats, uplevel
access, `goto`, or ON-units.

**Budget order** is the prompt's 1→7. My estimate: 1 and 3 are an hour each
with the RT entry; 2 is short; 5 and 6 are half a day together; 7 is the
one that could run long (the table has to be worked out field by field
from its uses, and the control flow has two long-jump stubs and a
`LDAFP`-spill block). If budget runs short it is 7 that gets thin, and I
will stop rather than ship it thin.

### 2.1 Salvage facts each routine leans on

F12 (X.CB, #3), F13 (unsigned CHAR, #1 #3), F14 (GET_INPUT frame, #3), F17
(field K folding and bit numbering — no bits here, but the K folding is
used everywhere), F18 (no offset; the four world constants, #2 #6), F22
(even-slot rounding, #1 #3), P4 (loop counter register / limit reload, #5
#6 #7), P8 (the DERR-17 idiom, all).

---

## 3. HIT_ANY_CHAR in full — the worked example

Entry 7016DE91, argc 0, `WSAVS 0x09` → slots 2..19. Callers: 26 routines, 64
sites (CallGraph). Callee: GET_INPUT (7016DEA9) and `?WRITE_SCREEN` ×2.

### 3.1 The listing, instruction by instruction

```
7016de91 WSAVS 0x0009                    frame: 9 wides, slots 2..19
7016de93 NLDAI 30,0                      ac0 = 30   destination count
7016de95 WMOV 0,1                        ac1 = 30   source count
7016de96 XNSTA 0,[ac3+0x4]               M16[slot 4] = 30   the varying's length word
7016de98 XLEFB 2,[ac3+0xA]               ac2 = byte ptr to slot 5   (byte disp 10 = word 5) = the data
7016de9a XLEFB 3,[pc+0xFED8] (7016DE07:0) ac3 = byte ptr to the literal
7016de9c WCMV                            copy 30 bytes  ->  msg = literal
7016de9d LDAFP 3                         ac3 := fp again (the literal pointer used the base register; P6)
7016de9e XPEF [ac3+0x4]                  push &msg          (word address of the length word)
7016dea0 LPEF [0x70000260]               push &OUT_CHAN
7016dea3 LCALL [0x7017E27A],2            ?WRITE_SCREEN(OUT_CHAN, msg)   arg1 = last pushed
7016dea7 XPEFB [ac3+0x4]                 push BYTE ptr to byte 4 = slot 2   -> &key
7016dea9 LCALL [0x7016AA35],1            GET_INPUT(key)
7016dead WLDAI 1,0x00020D0B              ac1 = { len 2 | bytes 0D 0B }
7016deb0 XWSTA 1,[ac3+0x4]               M32[slot 4..5] = that   ->  msg = '\r' || '\013'
7016deb2 XPEF [ac3+0x4]
7016deb4 LPEF [0x70000260]
7016deb7 LCALL [0x7017E27A],2            ?WRITE_SCREEN(OUT_CHAN, msg)
7016debb WRTN
```

The literal at `0x7016DE07:0`, from `quest.mem` (line 9275–9277): bytes
`0B 48 69 74 20 61 6E 79 20 63 68 61 72 61 63 74 65 72 20 74 6F 20 63 6F 6E
74 69 6E 75 65` = `\013` + `Hit any character to continue` (1 + 29 = **30**
bytes). `0x0B` is the Dasher *erase to end of line* control; `0x0D` is CR.
So the routine clears the line, prompts, waits for a key, and clears the
line again.

### 3.2 The cross-checks that constrain it

- **Frame accounting closes exactly.** 9 wides = 18 slots (2..19). `key`
  is a `CHAR(1)` at slot 2, which under F22 takes the even pair 2–3; `msg`
  has its length word at 4 and 30 bytes = 15 words at 5..19. 2 + 1 + 15 = 18.
  **Three independent numbers agree on the capacity 30:** the frame has
  room for exactly 15 data words, the WCMV destination count is 30, and
  the literal is 30 bytes. A `CHAR(31)` would not fit; a `CHAR(29)` would
  have a destination count of 29.
- **`key` is a separate variable, not the varying's length word.** The
  GET_INPUT argument is `XPEFB [ac3+0x4]` — a *byte* displacement of 4,
  i.e. word 2 — by the same convention that puts GET_INPUT's own buffer at
  slot 4 from `XLEFB 2,[ac3+0x8]` (F14). Read as a word displacement it
  would point GET_INPUT's one-byte store at the high byte of `msg`'s
  length word and leave slots 2–3 unaccounted for. The byte reading is
  what closes the frame.
- **GET_INPUT's side agrees.** Its body ends `XWLDA 0,[ac3+0x7FF4]; WSTB
  0,2` — a one-byte store through arg 1 *as a byte pointer* (7016AA60..62),
  which is what `XPEFB` supplies. So `GET_INPUT(unsigned char *c)`.
- **The second assignment is a folded immediate.** `WLDAI 1,0x00020D0B;
  XWSTA 1,[ac3+0x4]` writes length 2 and both data bytes in one wide store
  — a 2-byte constant source ≤ capacity, so no `WCMV`, no pool literal, no
  min (DESIGN §10). The same word `0D0B` sits pooled at `0x7016DE05`, two
  words before this routine's own literal, so the constant is one the
  compiler knows in both forms. (This is a rewrite stage 3 will have to
  name: *small varying literal assignment → immediate wide store*.)
- **Argument order.** Both `?WRITE_SCREEN` calls push `&msg` then
  `&OUT_CHAN`; last-pushed = arg 1 = channel, matching RTConventions'
  `(channel, text, …)` and the 436 two-argument sites it counts.
- **P12's site.** `WLDAI 1,…` right after the GET_INPUT call is the one
  Salvage records at 7016DEAD (the register choice that P38 abandoned on).
  Nothing in the C depends on it.

### 3.3 The C

```c
/* HIT_ANY_CHAR @7016DE91 — argc 0, frame 0x09 (18 slots), WSAVS.
 * (derivation header as in §3.1–3.2, in the shipped file)
 */
#include "quest_rt.h"
#include "declarations.h"

void HIT_ANY_CHAR(void)
{
    unsigned char key;          /* slot 2 (even pair 2-3, F22); CHAR(1) */
    VARYING(30) msg;            /* slot 4 = length, 5..19 = 30 bytes    */

    assign_varying(&msg, 30, "\013Hit any character to continue");
    WRITE_SCREEN$2(&OUT_CHAN, &msg);
    GET_INPUT(&key);
    assign_varying(&msg, 30, "\r\013");
    WRITE_SCREEN$2(&OUT_CHAN, &msg);
}
```

Ten machine statements; four C statements plus one call, which is the
right count for what it does. `key` is written by GET_INPUT and never read
here (it exists so the keystroke has somewhere to land).

**Confidence: `derived`** — the frame accounting and the literal length are
two constraints outside the instruction reading, and GET_INPUT's body is a
third (it fixes the byte-pointer form of the argument).

**What `lower_c.py` refuses in this file:** `VARYING(30)` (a struct
declared in the header), the string literals, `assign_varying`,
`WRITE_SCREEN$2`, `GET_INPUT` (all calls), `&key` / `&msg` / `&OUT_CHAN`
(unary `&`), and `unsigned char` as a *byte* (today `char` is a one-word
kind — fine for one variable, wrong for a buffer). Every one is on the
handoff list.

**Spellings used** are `quest_rt.h`'s existing ones — `VARYING(n)`,
`assign_varying(dst, cap, lit)`, `NAME$N` by-reference calls — see Q-B.

---

## 4. Decisions I want settled before Part 2

**Q-A — `SUB()` when the original hoists a check.** INIT_SCREEN's first
loop checks `i` once per *outer* iteration (7016E10F, before `i*22` is
stored to slot 8), and `*who` and `j` once per *inner* iteration
(7016E125, 7016E130); the store at 7016E13B reuses the hoisted `i*22`. The
source-shaped C, `PLAYER[SUB(*who,10)].screen[SUB(i,9)][SUB(j,11)] = 0`
inside the inner loop, checks `i` 99 times where the original checks it 9.
Ruling 6 says omit the check where the original omits it; ruling 4 says
do not pre-hoist for the compiler. Options: (a) source-shaped, and let
stage 3 attribute the extra checks to the hoist rule; (b) `row = SUB(i,
9);` in the outer loop and `screen[row][SUB(j,11)]` inside — names the
hoisted `v`, which is doing the rewrite's job in the C; (c) a bare
`SUB(i, 9);` statement at the outer-loop head and an **unchecked** `i` at
the store. **Recommendation: (c).** It puts each check exactly where the
original has one, introduces no storage, leaves the `i*22` recompute for
the rewrite to find, and is a one-line gap for stage 2 (an expression
statement whose only effect is `SUB`'s trap). I would apply the same rule
to every hoisted check in #5–#7 and say so in each header.

**Q-B — the dialect for calls, strings, dummies.** `quest_rt.h` already has
`VARYING(n)`, `assign_varying`, `TMP(e)`, `BITS("...")`, `NAME$N`
by-reference prototypes, and `SUB`/`ABS`. Options: reuse them as the
candidate-C dialect, or invent cleaner spellings (`msg = "..."`, a
`CALL`-free syntax) and hand stage 2 two vocabularies. **Recommendation:
reuse**, with three additions I need: `TMP()` accepting a **pointer**
(GET_INPUT's `?READ` arg 2 is a dummy holding a byte pointer to the
buffer — the machine pushes `&slot78` and slot 78 holds `bp(buf)`), `MIN`
/`MAX` builtins alongside `ABS`, and — pending Q-A — `SUB` in statement
position. The header is P44's and I will not edit it; the additions go in
the report's handoff and as comments at the first use.

**Q-C — the checked convert.** PICK_X_Y's `CVWN` before each 16-bit store
through `*x`/`*y` (70176241, 70176262) and the one after `type/100`
(70176221). P48 F7 chose C's implicit narrowing on assignment and reported
the departure from P36's `cvwn()` ruling. **Recommendation: follow P48** —
plain assignment, `CVWN` named in the header — so the seven files and
UPDATE_SCREENS speak the same C.

**Q-D — eager vs short-circuit booleans.** FAKE_LAND_MASS materialises
`(a <= b) | (c <= d)` as 0/−1 words and ORs them (701697EF..FF); PICK_X_Y's
final gate is a skip chain to one stub (70176267..75). **Recommendation:**
write `|`/`&` on comparison results where the machine materialises (that
is PL/I's eager semantics and it is already in the subset), and `&&`
where the machine branches. Both are the C I believe is right; the header
says which shape the listing has at each site, since stage 3's
`eager_bool` sensitivity (P48 F5) is exactly this distinction.

**Q-E — the LAND table.** FAKE_LAND_MASS indexes a record table through
OBJ_PTR that `declarations.json` does not have (stride 22, bound 1000 from
its DERR 17, 1-based origin OBJ_PTR+1035308, count at OBJ_PTR+1035307, a
32-bit field at +0 that is copied into `screen` cells, four 16-bit fields
at +18..+21 compared against `x±4`/`y±5`). `declarations.json` is
generated by `compiler/gen_declarations.py`, which is stage 2's; both are
outside my write boundary. **Recommendation:** declare it locally in
`FAKE_LAND_MASS.c` (a `struct land_rec` + `extern` table in a clearly
marked block that stage 2 moves into the generator) and put the full
field table in the handoff with the evidence per field. I will name the
fields by what the uses say they are, or by K if the uses do not say.

**Q-F — file for #4.** UPDATE_SCREENS is P48's and already on disk. I will
not rewrite it; it enters the confidence table as `verified` (ran, P48
§3.1) with a pointer to P48's header as its derivation.

---

## 5. Salvage.md against the listing

Everything I touched survives. Specifically:

| fact | checked | result |
|---|---|---|
| F12 | `quest.mem` at 0x7016A9B9 = `30 30 31` ("001"); at 0x7017024D = `31` ("1"); GET_INPUT 7016AA3D..41 loads exactly those with `NLDAI 3,1` / X.CB | **holds** |
| F13 | GET_INPUT 7016AA63..69 `WLDB; WSGTI 2,128; WNADI 2,0xFF80; WANDI 2,255` | **holds** (and the routine's C needs `unsigned char` for the `> 128` test to be live) |
| F14 | all six `XPEF`s 7016AA4D..55 + `LPEF [0x70000262]`; slot 78 := `XLEFB [ac3+0x8]` (byte ptr to slot 4); temps 76/78/80/82; arg mapping by last-pushed-first | **holds**; the frame closes exactly (count@2, buf@4..75, temps 76..83 = 41 wides) |
| F18 | `0x3B73` appears at **four** routines — 70160F88, 701654C0/C8, 70169A7A/83 (FAKE_OCEAN), 7017626F (PICK_X_Y); `0x3B77` appears **nowhere** in `quest.dis` | **holds, and the 0x3B73 correction now has four witnesses** |
| F22 | second instance: HIT_ANY_CHAR's `CHAR(1)` occupies the even pair 2–3 (the only way its frame closes) | **holds**, now two witnesses |
| P4 | INIT_SCREEN and FAKE_OCEAN loop counters in ac0/ac1 at body entry; the DO limit reloaded at every step (7016E16B `XWLDA 0,[ac3+0x8]`, 70169A16 `XNLDA 0,[ac3+0x8]`) | **holds** |
| P12 | 7016DEAD `WLDAI 1,0x20D0B` | **holds** |

One thing to **add**, not correct: RTConventions' `?READ` row says it
"writes through arg3, arg4". At GET_INPUT's site arg 3 is the constant `1`
in a frame temp (slot 80) and arg 4 is `count` (slot 2) — consistent (a
write into a dummy is harmless), and worth a sentence in the `?READ` entry
so the next reader does not take arg 3 for an output.

One thing the listing settles that no live doc states: the **byte-form
X-displacements are in bytes** (`XLEFB/XPEFB/XLDB [ac3+d]` → slot `d/2`).
F14 uses it silently; HIT_ANY_CHAR's frame accounting *depends* on it. I
will state it in `RTConventions.md`'s preamble as a reading convention
(additive) unless the integrator prefers it elsewhere.

---

## 6. What I will produce in Part 2

`game/routines/{HIT_ANY_CHAR,PICK_X_Y,GET_INPUT,INIT_SCREEN,FAKE_OCEAN,
FAKE_LAND_MASS}.c`, each with the derivation header in the P48 form
(instructions, cross-checks, `SUB()` placement, what the compiler refuses,
confidence); `RTConventions.md` gains an `X.CB` entry and `?READ`/
`?RANDOM_NUMBER` argument-width notes as I meet them; `REPORT.md` with
the confidence table, the handoff gap list ordered by centrality, the
per-routine "did the old file agree / had I read it" disclosure, and the
least-confident list.

STOP. Waiting for `a001`.
