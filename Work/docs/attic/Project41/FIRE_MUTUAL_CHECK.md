# Item 1 — the FIRE mutual check: VERDICT

Step 3. The two derivations are in `FIRE_DERIVATION_FROM_2.md` and
`FIRE_DERIVATION_FROM_1.md`, each written from the book before this file
and before `game/declarations.json` was opened.

## The verdict in one line

> **VACUOUS — not agreement, not disagreement. FIRE.1 and FIRE.2 reach
> DISJOINT sets of slots in their parent's frame, so they cannot check
> each other, and never could have.**

The P39 guard was not merely unperformed. It was **unperformable on the
pair it names**, and that is a stronger finding than either of the two
outcomes P41 anticipated.

## The two derivations side by side

| parent slot | FIRE.2 says | FIRE.1 says |
|---|---|---|
| **−14** (arg 2) | — | — |
| **−12** (arg 1) | by-reference, datum **16-bit** — 7016A479, 7016A4A6, 7016A4E8 | — |
| **+8**  | — | — |
| **+10** | — | local, **32-bit** — 7016A3C0, 7016A424, 7016A44F |
| **+12** | local, **32-bit** — 7016A485, 7016A4D2 | — |
| **+14** | — | local, **32-bit**, written uplevel — 7016A3DC, 7016A401, 7016A3F2 |

Intersection: **empty**. There is no slot on which the two siblings could
have agreed or disagreed. Where both derivations have an entry the other
lacks, that is not a conflict — it is simply two procedures using
different variables of their parent.

## What this means for `declarations.json`

The committed table is:

| entry | slot | width | witness recorded | from |
|---|---|---|---|---|
| `args.1` | −12 | 16 | `FIRE.2@7016A479` | FIRE.2 |
| `locals.w10` | +10 | 32 | `FIRE.1@7016A3C0` | FIRE.1 |
| `locals.w12` | +12 | 32 | `FIRE.2@7016A483` | FIRE.2 |
| `locals.w14` | +14 | 32 | `FIRE.1@7016A3DA` | FIRE.1 |

Every slot, widths included, **matches my derivations**. P39's readings
were correct. But every entry carries exactly one witness from exactly one
sibling, and because the sibling sets are disjoint, **no amount of work on
FIRE.1 and FIRE.2 could ever have raised any entry above one witness.**
The table is therefore still fitting rather than derivation *on P39's own
criterion* — and would have stayed so however long P40 or P41 stared at
those two routines.

### Two witness PCs are wrong

The P39 ruling makes a named witness mandatory and `check_frames()`
enforces its presence — but nothing checks that the PC names the
instruction the comment describes. Two of the four do not:

- **`locals.w12` witness `7016A483` is not an instruction at all.**
  `7016A482 LWADD 1,[0x70000210]` is a three-word instruction occupying
  7016A482–7016A484; the next instruction boundary is 7016A485. The
  reference the comment quotes (`XWLDA 0,[ac2+0xC]`) is at **7016A485**.
- **`locals.w14` witness `7016A3DA` is the link LOAD, not the reference.**
  7016A3DA is `XWLDA 3,[ac3+0x7FFA]`. The reference the comment quotes
  (`XWSTA 0,[ac3+0xE]`) is at **7016A3DC**.

Both are recorded here rather than silently corrected in the table; see
"What I changed" below.

## The check CAN be discharged — by the sibling P39 did not use

FIRE has **three** nested children, not two:

    70169D69  FIRE            argc 2  frame 0xA9
    7016A3BD  FIRE.1@…        argc 0  frame 0x04  push,nested
    7016A461  FIRE.2@…        argc 0  frame 0x0A  nested
    7016A4F8  FIRE.3@…        argc 0  frame 0x69  nested   <- unused by P39

FIRE.3 is called by FIRE at 7016A309, has the same `WSAVS` + `WMOV 1,2`
link prologue as FIRE.1, and is ordinary compiled code (not hand-assembly,
so R39 does not bite). It is by far the heaviest user of the parent frame:
18 link loads against FIRE.1's 4 and FIRE.2's 4. And it **overlaps both
siblings**:

| parent slot | FIRE.1 | FIRE.2 | **FIRE.3** | FIRE's own body | verdict |
|---|---|---|---|---|---|
| **−14** (arg 2) | — | — | 16-bit ind. @7016A6C7 | 2 sites `@[ac3+0xFFF2]` | **MISSING from the table** |
| **−12** (arg 1) | — | 16-bit ind. ×3 | 16-bit ind. ×12 | 10 sites `@[ac3+0xFFF4]` | **CONFIRMED** ×2 siblings + parent |
| **+8** | — | — | 16-bit ×8, `XPEF` ×1 | 11 sites (`XNLDA`/`XNSTA`) | **MISSING from the table** |
| **+10** | 32-bit ×3 | — | 32-bit ×2 | 1 site (`XWSTA 2,[ac3+0xA]`) | **CONFIRMED** ×2 siblings + parent |
| **+12** | — | 32-bit ×2 | — | 10 sites (`XWLDA`/`XWSTA`) | **CONFIRMED by the parent only** |
| **+14** | 32-bit ×3 | — | 32-bit ×6 | **never touched** | **CONFIRMED** ×2 siblings |

So the mutual check, performed against the *family* rather than the named
pair, **passes with zero disagreements**: every slot two witnesses reach is
read at the same displacement and the same width by both. Nothing in the
table needs to move.

Three independent strands, all agreeing:

1. **FIRE.3 vs FIRE.1** on +10 and +14.
2. **FIRE.3 vs FIRE.2** on −12.
3. **FIRE's own body**, which reads its own frame directly and is not an
   uplevel witness at all, corroborating −14, −12, +8, +10 and +12. This
   is the strongest strand and P39 used none of it: a parent's own
   accesses to its own frame are direct evidence of that frame's layout,
   available without any sibling.

`+14` is the interesting slot: FIRE **never touches it**. It is declared in
FIRE and used only by FIRE.1 and FIRE.3 — which is exactly why it needed
two sibling witnesses and why the parent could not supply one.

## Consequences

1. **The guard as written is the wrong instrument.** "Sibling nested
   procedures must agree on their common parent's layout independently"
   silently assumes the siblings' reference sets overlap. For FIRE.1/FIRE.2
   the overlap is empty; the guard passes vacuously and reads as
   confirmation. It should be restated so that it can fail:
   > A parent slot is DERIVED when at least two independent witnesses
   > reach it — any two of {a sibling, another sibling, the parent's own
   > body} — and agree on displacement and width. A slot with one witness
   > is confidence C and named as such. **A pair of procedures with no
   > shared slot is not a check.**
2. **The table is correct but incomplete.** −14 and +8 are reached by
   FIRE.3 and by FIRE's own body and are absent from `declarations.json`.
3. **`check_frames()` verifies that a witness exists, not that it is
   real.** Both defective PCs would have been caught by checking the PC
   against the disassembly — one of them is not even an instruction
   boundary. This is the same shape as the P40 `quest_rt.h` incident and
   as METHOD §10: the check passed because it was not the check anyone
   thought it was.

## What I changed

Nothing in `declarations.json` yet — under the P41 boundary "derive before
asserting", and because adding −14 and +8 changes an artifact that Item 2
must audit *neutrally*. The three findings (two bad witness PCs, two
missing slots, the restated guard) are recorded here for the plan gate.
The widths and slots already in the table are confirmed and need no
change.
