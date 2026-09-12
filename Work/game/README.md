# game/ — the reconstructed source of Quest

The C-subset source that P35's translator turns into our IR (and that
gcc/g++ turns into native Quest). Nothing here yet but the scaffolding:

- `quest_rt.h` — the runtime header: `varying` strings, `ARRAY1()`,
  the `$N` runtime-call prototypes with their `const` conventions, the
  `SUB()` bounds-check macro. C and C++ views of the same declarations.
- `declarations.h` (to be GENERATED) — the world/record layout as
  `ARRAY1(...)` declarations: SD_PTR (player records, stride 686),
  OBJ_PTR (objects, stride 9 / 20), CAS_PTR (castles, stride 23), the
  being/magic/treasure arrays. Seed: `docs/GAME_REFERENCE.md` (the world
  file layout worked out in the Java era) + the P34 record census
  (`docs/Project34/Census.md`). This is the ONLY file whose C and C++
  forms differ.
- `routines/` — one .c per game routine as they are recovered
  (PICK_X_Y first).

---

## Status of `routines/` (annotated Sep 12 2026)

Produced by the **P35–P43 compiler line, now void** (`docs/attic/README.md`).
Kept because it is the best available reading of these routines and the
matched ones are real evidence. **Not finished source.**

| routine | status |
|---|---|
| PICK_X_Y | **MATCHED** 64/64 (P35) |
| UPDATE_SCREENS | **MATCHED** 72/72 (P35) |
| REFRESH_SCREEN | **MATCHED** 61/61 (P35) |
| OWNS | **MATCHED** 152/152 (P37) |
| GET_INPUT | staged — refused at the `BITS()` argument |
| RETURN_MESSAGE | staged — refused at the VARYING accessors |
| FIRE.1, FIRE.2, HIT_ANY_CHAR, LIST_PLAYERS.3, QUEST.1, DIED, DIEDa | abandoned or partial |

**`declarations.json` carries single-witness data.** FIRE's parent-frame
layout rests on one witness per slot, and P41 showed the remedy is
impossible: the FIRE.1/FIRE.2 mutual check is **VACUOUS** — the siblings
reach disjoint slot sets and cannot check each other
(`docs/attic/Project41/FIRE_MUTUAL_CHECK.md`). Do not re-run it.

Under P44 (`docs/Project44/DESIGN.md`) a routine's acceptance level is
**L1 — substitutable and behaviourally equivalent under lockstep** — with
exact match (L2) as a further badge. None of these has been through L1.
