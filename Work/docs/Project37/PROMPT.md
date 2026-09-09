# Project 37 — finish the constructs, and prove them on SEVERAL routines

> **NOTE (integrator, at merge)**: this prompt's routine table was built
> from addrbook statement counts and is therefore WRONG for
> TRANSPORT_SUNDAR (138 is the shared body of an R40 multiple-ENTRY pair,
> not the routine's own size) and mis-described UNLOCK_FILE (hand
> assembly, dropped) and `wp(ac2, …)` (ordinary record spelling, not
> frame relocation). The session's plan gate corrected all three; see
> docs/Project37/REPORT.md and METHOD §16.

GOAL: complete the two constructs P36 left unbuilt (the frame
relocation and the arena twins), and — the change of approach for this
project — **prove every construct on more than one routine**. P36
showed the cost of a single-routine target: DIED interleaves all five
constructs, so nothing produced a match number until everything landed,
and a stubborn diff could not be told apart from a DIED peculiarity.
P37 spreads the same work across five routines chosen so that each
isolates one construct, plus one routine nobody hand-picked. DIED
becomes the capstone, attempted last, not the gate.

Success is measured in **completed routines** (a routine at 100 %
MATCH, primary and `--folded`, native-clean) and in **rules promoted
from one instance to several**, not in percentage-of-DIED.

Hi Claude! Read docs/METHOD.md first. Foundation: **compiler/
CODEGEN_RULES.md** (P35's 25 rules + P36's R26–R32 with their
confidences and instruction pairs — you extend, promote and falsify
these), **compiler/translate.py** (the translator: pycparser front end,
the DG-compiler model, the symbolic address renderer P36 ported from
string_sites.py), **compiler/ircmp.py** (the comparator; its seven
equivalences are FIXED unless a construct forces an eighth — a ruling),
**docs/Project35/REPORT.md** and **docs/Project36/REPORT.md** (what was
built, what was found, DIED's census), game/routines/*.c (four models),
game/quest_rt.h, game/declarations.h, compiler/gen_declarations.py.
Also docs/IR.md (ir 6), docs/Project29/StringsDesign.md §5.2/§6 (twins,
claim/release), docs/Project28/RTConventions.md, docs/GAME_REFERENCE.md
and the P34 record census. TREE VINTAGE: main after the P36 merge
(9f33953 or later) — state it; verify docs/Provenance.md. Nothing here
touches emulation/ or Disassembled/; no battery.

## The routine set (censused for this project; counts are book statements)

Take them in this order. Each is translated → compared → fixed → 100 %
before the next, so every one is a delivered result.

| # | routine | stmts | why this one |
|---|---|---|---|
| 1 | **UNLOCK_FILE** (7017xxxx, 38) | 38 | smallest routine with bit ops (2) and `wp(ac2, …)` frame refs (7) and nothing else new — proves R26–R29a and the ac2 spelling on a routine that fits on a page |
| 2 | **DISTANCE_TO_PLAYER** (30) / **RETURN_MESSAGE** (30) / **GET_INPUT** (22) | 30/30/22 | three tiny routines with NO new constructs — the cheap generality check: if the P35/P36 model is right these should fall out almost free. Do all three; any that resists is a finding about the model, not about a construct |
| 3 | **INIT_OBJ_TBL** (173) | 173 | the smallest routine with a claim group (2 claims) — proves twins/claim/release and the sizing derivation IN ISOLATION (no bit fields, no frame relocation, 4 string statements, 19 `wp(ac2, …)`) |
| 4 | **FIRE.1@7016A3BD** (89) | 89 | the smallest routine with the FRAME RELOCATION: 3 × `ac2 = wfp` and 17 `wp(ac2, …)` in 89 statements — the construct at a size where it can be understood. If the pattern here matches DIED's, R-frame is derivable; if it does not, that is the finding and the pragma is justified |
| 5 | **OWNS** (152) | 152 | 8 bit operations, no twins, no relocation — the second, independent witness for the bit rules |
| 6 | **one routine chosen at random**, 50–150 statements, by a method fixed BEFORE looking (e.g. seeded RNG over the addrbook entries in that size band, excluding the ones above) | — | the honesty test: everything so far was hand-picked. Report the selection method, the routine, and the result whatever it is |
| 7 | **DIED** (495) | 495 | capstone. Attempt only after 1–6; report its percentage and its residual honestly, and do not let it consume the project |

If a routine in 1–6 proves intractable, STOP on it, record why, and move
to the next — a completed routine elsewhere is worth more than a
half-finished one here.

## The two unbuilt constructs

**Frame relocation (routine 4, then DIED).** P36's finding, restated:
it is not ac3-borrow-and-restore. In DIED 70166376–701663B3 the
compiler emits `ac2 = wfp` ONCE and keeps a record base in ac3 for ~10
blocks, so all 22 frame references in that stretch spell `wp(ac2, 8)`.
`Regs` models ac0–ac2 with ac3 pinned to fp, so this touches every emit
site. The census says 25 routines contain `ac2 = wfp`; FIRE.1 has 3 in
89 statements. Derive the trigger from FIRE.1 + OWNS + DIED if you can
(a rule at confidence B beats a pragma); if the evidence genuinely does
not settle it, `#pragma fp ac2` is allowed — recorded in CODEGEN_RULES
as an UNEXPLAINED directive with its instruction evidence, and the
**pragma count reported next to every match number**.

**Arena twins (routine 3, then DIED).** P36 established the sizing is
derivable — claim count `ceil(bytes/4)` over the CAT chain's running
byte length, +2 for the final varying's length word, every constant
falling out of the literal piece lengths — needing only a `LEN(v)`
header form for a VARYING parameter's length. Build it on INIT_OBJ_TBL's
two claims first (isolated), then DIED's six.

## Part 1 — plan gate

1. Re-baseline: the four existing routines still 100 % (197/197 + any
   P36 additions), `--selftest` PASS. State it.
2. For each routine 1–6: its construct inventory (from the book), the C
   you expect to write, and any rule you predict will need extending.
3. The random-selection method for routine 6, fixed and stated BEFORE
   the draw.
4. The `LEN(v)` form and the twin-sizing derivation as you will
   implement them; the frame-relocation rule you will attempt, and what
   evidence would make you reach for the pragma instead.
5. Success criteria: routines completed (target 6 of 7 plus a measured
   DIED), rules promoted/falsified with their new confidence, pragma
   count (target 0), `--selftest` green after every rule change, native
   C99 + C++17 clean, and the four existing routines never regressing.

STOP AND REPORT at the plan gate.

## Part 2 — build, in the order above

Commit and push at every routine boundary (P35's session crashed with
everything on disk; P36 pushed a branch and lost nothing — do that).
Every rule added, promoted or falsified goes into CODEGEN_RULES.md with
its instruction pair and confidence. A DIFF no rule predicts is a
FINDING with the instruction pair and a proposed rule.

## Part 3 — report

Per routine: the C, the match table (primary and `--folded`), the rules
it exercised, the pragma count. Then: the promotion/falsification
ledger (which rules moved confidence and on what evidence — R10's
verdict included, if routines 3–7 provide the instances), the random
routine's result as its own section, the findings list (the frontier),
and an assessment of whether the model now generalises — with the
honest number: **how many of Quest's ~130 routines could be attempted
today, and what stands in the way of the rest.**

## Boundaries — BINDING

1. Files: compiler/, game/, docs/Project37/. Nothing in emulation/,
   Disassembled/, no artifact, no battery.
2. The translator emits ir 6 exactly; it never invents a production.
3. Refuse-don't-guess in the translator; classify-don't-hide in the
   comparator; no match number quoted for a routine that translates
   only as a prefix (P36's discipline — keep it).
4. Existing routines must never regress; `--selftest` green after every
   rule change.
5. A pragma is an ADMISSION, not a tool of first resort: derive first,
   record every one, report the count.
6. Deliver a Work.tgz AND push the branch (`p37-routines`) if the clone
   URL is available.
