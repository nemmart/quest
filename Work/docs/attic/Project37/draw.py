#!/usr/bin/env python3
"""Project 37, routine 6 — the random draw.

The METHOD was fixed and stated at the plan gate BEFORE the draw was run
(docs/Project37/PLAN.md Sec.5).  It is reproduced here verbatim so the result can
be re-derived by anyone:

  frame     every live (non-'#') entry of emulation/quest.addrbook whose
            statement count, sliced out of emulation/quest.ir2.book by
            ircmp.locate + ircmp.routine_blocks, lies in [50, 150];
  exclude   the routines already matched or already targeted by P37 that
            fall in that band: PICK_X_Y, UPDATE_SCREENS, REFRESH_SCREEN,
            FIRE.1@7016A3BD.  LOCK_FILE is deliberately NOT excluded.
  order     by entry address, ascending;
  draw      random.Random(37).choice(pool)  -- seed = the project number,
            one draw, no re-rolls.

Run:  python3 docs/Project37/draw.py     (from the tree root)
"""
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "compiler"))

import ircmp as C  # noqa: E402
import readable as R  # noqa: E402

BOOK = os.path.join(ROOT, "emulation", "quest.ir2.book")
AB = os.path.join(ROOT, "emulation", "quest.addrbook")

EXCLUDE = {"PICK_X_Y", "UPDATE_SCREENS", "REFRESH_SCREEN", "FIRE.1@7016A3BD"}
LO, HI = 50, 150
SEED = 37


def main():
    _, blocks, _ = C.load_ir(BOOK)
    ents = R.load_addrbook(AB)

    frame = []
    for pc, e in sorted(ents.items()):
        if e.get("stacked"):
            continue
        entry, lo, hi, _ = C.locate(e["name"], AB)
        order, _un = C.routine_blocks(blocks, entry, lo, hi)
        n = sum(len(blocks[p]) for p in order if p in blocks)
        if LO <= n <= HI:
            frame.append((entry, e["name"], n))

    pool = [f for f in frame if f[1] not in EXCLUDE]
    pool.sort(key=lambda f: f[0])

    print("frame (%d..%d statements): %d routines" % (LO, HI, len(frame)))
    print("pool after exclusions:     %d routines" % len(pool))
    for entry, name, n in pool:
        print("    %08X  %-32s %4d" % (entry, name, n))
    pick = random.Random(SEED).choice(pool)
    print()
    print("seed %d -> DRAW: %s @%08X (%d statements)" % (SEED, pick[1], pick[0], pick[2]))


if __name__ == "__main__":
    main()
