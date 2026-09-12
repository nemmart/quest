#!/usr/bin/env python3
"""make_update_screens_fixture.py — the end-to-end check for Stage C.

Builds the memory image UPDATE_SCREENS runs against, and the expectations it
must satisfy, from TWO things only:

  * game/declarations.json — the record geometry (stride, K, dims, strides)
  * a model of the routine written from the DISASSEMBLY reading, below

It never runs the compiler.  That is the point: the expectations and the
thing under test come from independent readings of 7017d635..7017d6a8, so a
disagreement means one of the two readings is wrong, which is a finding
either way.  (METHOD §10 in spirit: an expected value has to come from
something other than the thing it is checking.)

The scenario deliberately puts a player on EVERY boundary the routine has:
both ABS bounds at their limit, and the column and cell subscripts at 1 and
at 9 / 11 — the four places an off-by-one in the folded K would show.
"""

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.join(os.path.dirname(os.path.dirname(HERE)), "game")
D = json.load(open(os.path.join(GAME, "declarations.json")))

SD_PTR_ADDR = D["statics"]["SD_PTR"]["addr"]          # 0x70000210
COUNT_K = D["direct"]["SD_PTR"]["player_count"]["K"]  # 43
P = D["tables"]["PLAYER"]
STRIDE = P["stride"]                                  # 686
K589 = P["fields"]["fm589"]["K"]                      # -589
K588 = P["fields"]["fm588"]["K"]                      # -588
SCREEN = P["fields"]["screen"]
K_SCR, SX, SY = SCREEN["K"], SCREEN["strides"][0], SCREEN["strides"][1]

SD = 0x70200000          # where we put the shared-data record
ARGS = 0x70201F00        # the three argument cells, inside the same mapping
CELL = 0x12345678

# (px, py) for players 1..N
PLAYERS = [
    (100, 200),   # 1: ordinary hit,            dx=7  dy=9
    (110, 200),   # 2: |dx|=8 > 4               first continue
    (101, 210),   # 3: |dy|=7 > 5               second continue
    (106, 203),   # 4: |dx|=4 at the bound,     dx=1  (column low bound)
    (98,  208),   # 5: |dy|=5 at the bound,     dx=9  dy=1
    (98,  198),   # 6: both bounds,             dx=9  dy=11 (cell high bound)
]
X, Y = 102, 203

# --neg mirrors every coordinate through zero.  Salvage F18 says the game
# itself computes in the raw, positive 0x3Bxx space, so the positive scenario
# is the realistic one — but a fixture whose values are all positive cannot
# tell sx16 from zx16, and a --mutate no_sign_extend build passes it.  The
# mirrored scenario is what gives this check teeth; the mirror maps each
# boundary player onto the opposite boundary, so the coverage is preserved.
if "--neg" in sys.argv:
    X, Y = -X, -Y
    PLAYERS = [(-px, -py) for (px, py) in PLAYERS]

# --wrap: the ONE scenario that can tell sx16 from zx16 here.
#
# UPDATE_SCREENS uses every 16-bit datum inside a DIFFERENCE of two 16-bit
# data, and such a difference is invariant under a uniform +65536 offset, so
# neither the positive nor the mirrored scenario can distinguish a
# sign-extending read from a zero-extending one — a --mutate no_sign_extend
# build passes both.  The distinguishing case needs the difference itself to
# cross the 16-bit boundary:
#
#   player 1 at x = 32767 with *x = -32765:
#       signed:   |32767 - (-32765)| = 65532 > 4  -> skipped, nothing written
#       unsigned: |32767 -   32771 | =     4 <= 4 -> passes, and writes
#   so "player 1's cell stays zero" is exactly the sign-extension check.
#
#   player 2 is an ordinary hit that both readings agree on, so a build that
#   does nothing at all still fails the scenario.
if "--wrap" in sys.argv:
    X, Y = -32765, 0
    PLAYERS = [(32767, 0), (-32763, 0)]


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def model():
    """UPDATE_SCREENS, read off the disassembly.  Returns one expectation for
    EVERY cell of EVERY player's screen: the written value at the one cell a
    hit lands on, zero everywhere else.

    Checking only the cell a CORRECT reading would touch is not enough, and
    that is not a hypothetical — it is how the first version of this fixture
    let a --mutate no_sign_extend build pass.  A wrong lowering writes to a
    DIFFERENT cell, which a check aimed at the right cell never looks at.
    The full sweep has no such blind spot."""
    exps = []
    for i in range(1, len(PLAYERS) + 1):
        base = SD + STRIDE * i
        px, py = PLAYERS[i - 1]
        hit = None
        if abs(px - X) <= 4 and abs(py - Y) <= 5:
            dx, dy = X - (px - 5), Y - (py - 6)
            assert 1 <= dx <= 9, "dx %d out of the SUB(.,9) range" % dx
            assert 1 <= dy <= 11, "dy %d out of the SUB(.,11) range" % dy
            hit = (dx, dy)
        for cx in range(1, 10):
            for cy in range(1, 12):
                addr = base + K_SCR + SX * cx + SY * cy
                if hit == (cx, cy):
                    exps.append((addr, CELL, "player %d [%d][%d] written" % (i, cx, cy)))
                else:
                    exps.append((addr, 0, "player %d [%d][%d] untouched" % (i, cx, cy)))
    return exps


TRAP = "--trap" in sys.argv
NEG = "--neg" in sys.argv


def main():
    out = []
    a = out.append
    a("; GENERATED by compiler/difftest/make_update_screens_fixture.py")
    a("; geometry from game/declarations.json; expectations from a model of")
    a("; Disassembled/quest.dis 7017d635..7017d6a8 — never from the compiler.")
    a("; scenario: %s coordinates" % ("mirrored (negative)" if NEG else "positive"))
    a("map %08X 8" % SD)
    a("")
    a("; SD_PTR itself, in the page the rig already maps at 0x70000000")
    a("%08X %08X 32          ; SD_PTR -> the record" % (SD_PTR_ADDR, SD))
    count = 11 if TRAP else len(PLAYERS)
    a("%08X %04X 16          ; SD_PTR->player_count = %d"
      % (SD + COUNT_K, count, count))
    a("")
    a("; player positions")
    for i, (px, py) in enumerate(PLAYERS, 1):
        base = SD + STRIDE * i
        a("%08X %04X 16          ; PLAYER[%d].fm589 = %d"
          % (base + K589, px & 0xFFFF, i, px))
        a("%08X %04X 16          ; PLAYER[%d].fm588 = %d"
          % (base + K588, py & 0xFFFF, i, py))
    a("")
    a("; the three by-reference arguments, and the v's that point at them")
    a("%08X %04X 16          ; *x = %d" % (ARGS, X & 0xFFFF, X))
    a("%08X %04X 16          ; *y = %d" % (ARGS + 1, Y & 0xFFFF, Y))
    a("%08X %08X 32          ; *cell" % (ARGS + 2, CELL))
    a("setv UPDATE_SCREENS.v0 %08X 32   ; x" % ARGS)
    a("setv UPDATE_SCREENS.v1 %08X 32   ; y" % (ARGS + 1))
    a("setv UPDATE_SCREENS.v2 %08X 32   ; cell" % (ARGS + 2))
    a("")
    if TRAP:
        a("; player_count = 11 drives i past PLAYER's bound of 10, so the")
        a("; FIRST SUB(i, 10) must raise DERR 17 and the run must stop there.")
        sys.stdout.write("\n".join(out) + "\n")
        return
    a("; expectations: every cell of every player's screen")
    for addr, val, label in model():
        a("expect %08X %08X 32   %s" % (addr, val, label))
    sys.stdout.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
