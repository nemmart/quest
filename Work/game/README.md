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
