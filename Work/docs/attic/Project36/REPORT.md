# Project 36 — extending the translator to DIED: REPORT (PARTIAL)

Date: Wednesday, September 9, 2026.
TREE VINTAGE: `main` commit **d86f89a** (the P36 prompt commit); work on branch
**`p36-died`**, head `8cbf421` at the time of writing.  The three tarballs the
session was handed were verified byte-identical to d86f89a (`diff -rq` clean
across `Work/`, `Disassembled/`, `QUEST/`).
Provenance: every artifact matches its most recent table in `docs/Provenance.md`
(`quest.blocks` / `quest.tags` / `blocks.split` at the Sep 6 Follow.java-fix
values; `ir2.book` `e2f18f14…` / `ir2.stock` `c4340b49…` at the P33-B values;
`arena` `64ab09d4…`; p31/p32/p33 tsv unchanged).  The Sep 5 head table is
superseded, not violated.  **Nothing in `emulation/` or `Disassembled/` was
modified**; no artifact, no battery.
Plan gate: the Part 1 plan and its rulings A–D are in `PLAN.md`.

## 1. Result in one paragraph — and what it is NOT

**This project is incomplete.**  Five of the seven work items landed and are
committed with the P35 routines green throughout: the two carried-in rulings,
bit references, located strings into record fields (including the symbolic
address renderer that turned out to gate them), game→game calls, and a new
statement-shape rule.  Two did not: the **arena twins** (stage 4) and the
**ac2/ac3 frame relocation** (stage 2).  Because DIED is one routine and its
constructs interleave, the missing two mean the routine still translates only
as a prefix — **so there is no DIED match percentage in this report, and none
should be quoted.**  Every construct below was validated against the book's own
text for the statements it governs; that is real evidence, but it is not the
495/495 the prompt asked for.  The three P35 routines are **197/197 MATCH
primary, 128/128 folded, `--selftest` PASS**, before and after every change.

Pragma count: **0**.  Nothing here is asserted by directive; every rule below is
a derivation the comparator can falsify (and one of them was — §4).

## 2. What landed

| stage | construct | state |
|---|---|---|
| ruling 1 | explicit conversions `cvwn`/`sx16`/`trunc16`; implicit 32→16 narrowing REFUSES | done (R16b) |
| ruling B | `ircmp.succs_of` call fall-through | done — DIED 73/73 blocks reached, was 30/73 |
| 1 | bit references — `BIT`/`BIT_SET`/`BIT_CLR`/`BIT_PUT` | done (R26, R26a, R26b, R27, R28, R29, R29a) |
| 3 | located strings into record fields + the symbolic address renderer | done (R31, R32) |
| 5 | game→game calls, decorated and undecorated | done (R30) |
| — | the inverted if-diamond | done (R13c) |
| 2 | ac2/ac3 frame relocation | **not done** |
| 4 | arena twins / claim groups | **not done** |

All rules are in `compiler/CODEGEN_RULES.md` §7 with their motivating
instruction pair and a confidence.

## 3. Evidence per construct

**Bit references.**  The bit address is `16 * <scaled subscript> + (16*K + n)`
in a register, with the record base going separately into the instruction's
indirect operand (WSZB/WBTO/WBTZ take acS = base, acD = offset).  Fields
recovered: `PLAYER.fm591` bits 7,8,9,12,13,14,15; `PLAYER.fm590` bits
1,2,3,4,5,6,8,11; `PLAYER.fm63` bits 0,1,2,5.  Two findings worth their own
rules: `16*scaled` is a CSE temp **distinct** from the R9 scaled temp (DIED
saves both — slot 10 holds `P*686`, slot 8 holds `16*P*686`; R27), and a bit
reference is **always** materialised to 0/−1 before being tested, the compiler
never folding the WSZB skip into the branch (R29).

**Located strings into record fields — and the renderer they needed.**  This
was the largest item and the one the plan under-estimated.  A string
statement's address operand is not a register: lower.py prints the expression
the master's registers *unfold to*, computed by `string_sites.py`'s `Evaluator`
and printed by its `ir_word`.  `compiler/translate.py` now carries a faithful
**port** of that model — `Sym`, `_terms`, `_mk`, `s_add*`, `ir_const`,
`ir_word` (string_sites.py:221–360, :1521–1580) — restricted to the kinds a
translation can build.  It is a port on purpose: a disagreement between the two
is a finding, not a place to tune this file until the sites line up.

The port reproduces the book's text exactly on all three of DIED's sites,
including the two things easiest to get wrong:

    7016605B  (M32[0x70000210] + (sx16(M16[0x70000216]) * 686) - 0x271)
    7016606F  ((sx16(M16[0x70000216]) * 686) + M32[0x70000210] - 0x260)

— the **term order follows the instruction order**, because `_mk` merges a
linear form's terms in the order they were added (R31 explains which order the
compiler produces: base-already-in-ac2 gives `base + scaled`, scaled-in-ac1
gives `scaled + base`) — and the constant spelling is decimal below 256 and hex
above (`- 86` vs `- 0x271`), *not* translate.py's 8-digit `hexc`.

**Game→game calls.**  All six callees resolved against `quest.addrbook`.
Argument n lives at `wfp − 10 − 2n`, so the slots run downward from argument 1
and the stores appear in ascending address order — right to left in the source,
which is R18's push order.  `call 7017D635 args=3 marker=74009B62` and its
three arg-slot stores reproduce exactly; the two argc-0 sites stay embedded
`LCALL` instructions and the LJSR stays an audit line.

## 4. A rule proposed, falsified, and narrowed (the model working)

R13c's first draft triggered on "the `goto` target is beyond WBR range".  It
regressed **PICK_X_Y from 64/64 to 55/68 with 13 DIFF** — because PICK_X_Y's
far retries reach a one-word `WBR` to an R19 stub that *is* in range, so no
inversion happens there.  The trigger was narrowed to "the target **and** the
routine end — where the stub would sit — are both out of WBR range", which is
DIED 70166057's situation (target +0x154, end +0x360).  197/197 restored.  This
is recorded in CODEGEN_RULES §7.3 as the falsification it was, per METHOD §11.

## 5. Findings

1. **The frame pointer moves to ac2** (new; not in the P35 census).  Prompt
   item 2 describes ac3 leaving the frame pointer and returning via `LDAFP 3`.
   That is the short form (701660D6, 70166363).  But from 70166376 to 701663B3
   the compiler does something stronger: `ac2 = wfp` once, `ac3 = M32[0x70000210]`
   held as the record base for ~10 blocks, and **all 22 frame references in
   that stretch spell `wp(ac2, 8)`**.  This is a register *reassignment* of the
   frame pointer, not a borrow.  `Regs` in translate.py models only ac0–ac2 with
   ac3 pinned to the frame, so this is a structural change, and it is the
   reason stage 2 is unbuilt rather than merely unfinished.
2. **The twin sizing needs no sizing intrinsic** — the arithmetic is fully
   derivable from the CAT chain.  A claim count is `ceil(bytes/4)`, which is the
   compiler's `+3; lsh -1; lsh -1`, over the running byte length; the final
   varying twin adds 2 more for its length word (`+5; lsh -1; lsh -1`).  Every
   constant in both groups falls out of the literal piece lengths: group
   70166144's `+3, +0x17` are 3 and 23 bytes, group 701661AE's `+3, +0x4C` are
   3 and 76 — checked mechanically, both match.  The one thing the C cannot say
   is the length of a VARYING parameter, so the designed form is **`LEN(v)`**
   (declared in quest_rt.h, translator refuses it), not a sizing intrinsic.
   This answers the plan's open question in the direction of "derivable".
3. **DIED's signature was wrong in the P35 stub** (METHOD §11).  Argument 1 is
   a CHAR VARYING passed by reference — the death message — whose length word
   the routine reads to size the message temporaries and whose text it copies
   into them.  Argument 2 is never read.  Corrected in `game/routines/DIED.c`.
4. **`M16[wp(ac2,-625)] = trunc16(32)` at 70166096 is a separate PL/I
   statement**, not part of the string assignment two statements earlier: the
   length word of a `VARYING(32)` field written on its own.  It reads naturally
   as `.len` being a named subfield, which is how `VARYING(n)` is already
   declared in quest_rt.h.
5. **43 of DIED's 73 blocks were invisible to the comparator's canonical
   order** before ruling B, falling back to address order — which on the
   translator's side is emission order, a far weaker check.  Only a routine
   with undecorated calls could have exposed this.

## 6. R10 — NO VERDICT

The plan committed to a verdict.  It cannot be delivered honestly: judging R10
requires translating both the supporting site (701660F8, where the element
address is saved to slot 10, the first later reference recomputes, and the
second consumes it — R10's exact pattern, third instance) and the contradicting
site (70166110, five consecutive `REGION[r].field = 0` stores with four later
references at the first and **no element-address temp created at all**), and
70166110 sits downstream of the unbuilt stages.  What the reading of the IR
already shows is that **R10 as stated is falsified** — 70166110 is a
counter-instance — and that the discriminating variable is ac2 contention:
at 701660F8 ac2 is genuinely needed for a different address between the
references, at 70166110 nothing else wants it.  That is one of the two
alternatives R10 already lists.  Promoting it to a rule requires the loop to
run, so R10 stays at confidence **C**, now with a named counter-instance
recorded against it.

## 7. Deliverables and commands

Branch `p36-died` (three commits): `ebbf47c` rulings 1 + B, `b31e16d` bit
references + calls + R13c, `8cbf421` the symbolic renderer + record strings.

    python3 compiler/gen_declarations.py
    python3 compiler/translate.py game/routines/PICK_X_Y.c --routine PICK_X_Y -o out.ir
    python3 compiler/ircmp.py emulation/quest.ir2.book out.ir --routine PICK_X_Y [--folded]
    python3 compiler/ircmp.py --selftest --book emulation/quest.ir2.book

`docs/Project36/` holds the three P35 comparison reports and `DIED.partial.ir`.
Native checks: `game/routines/DIED.c` and the P35 three compile clean under
`gcc -std=c99` and `g++ -std=c++17` with `-Wall -Wextra -Werror`.

Runtimes: translate.py 0.05–0.5 s per routine (the 0.5 s is the literal lookup
in quest.mem); ircmp.py 0.8–1.1 s per comparison; `--selftest` ~1.0 s.

## 8. What P37 should be

1. **Stage 2, the frame relocation, first** — and it is a bigger change than
   "extend the register rules": `Regs` must model ac3 as an allocatable
   register and the frame pointer as a *value that lives somewhere*, which
   touches every emit site.  Finding 1 is the evidence.  This is also the place
   the user's pragma allowance is most likely to be needed (`#pragma fp ac2`
   over the run); the goal should be to derive the trigger, and to count the
   pragma loudly next to the match if it cannot be.
2. **Stage 4, the twins**, which is now unblocked in two ways: the symbolic
   renderer exists, and finding 2 says the sizing is derivable from `CAT` plus
   `LEN()`.
3. Then the 495-statement reconstruction and the translate→compare→fix loop,
   which is what finally produces a match number and settles R10.
4. Only after that is the prompt's closing question — "is the subset general
   enough for a routine nobody hand-picked" — worth asking.  On the evidence
   here the answer is not yet: DIED alone added eleven rules, and two of its
   five constructs are still unmodelled.
