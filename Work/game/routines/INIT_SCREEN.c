/* INIT_SCREEN @7016E103 — argc 1, frame 0x05 (10 local words), WSAVS.
 *
 * Written for Project 51 from Disassembled/quest.dis 7016e103..7016e1dc.
 * No earlier C for this routine exists.  Two blind readings (q001, q002)
 * agree, including the variable reuse below.
 *
 * What it does: clear player *who's 9x11 screen, then for every region
 * within 4 columns / 5 rows of the player's position write the region's
 * index into the corresponding screen cell if that cell is still empty,
 * then let FAKE_LAND_MASS and FAKE_OCEAN paint the rest.
 *
 * Frame (slots): 2 x (16), 3 y (16), 4 rx (16), 5 ry (16), 6-7 k (32),
 * 8-9 temp (x*22, then who*686, then the DO limit), 10-11 temp (k*9).
 * = 10 words = 2 * 0x05.
 *
 * The listing, by statement:
 *   7016e105 x := 1;  WBR body                      DO x = 1 TO 9
 *   7016e10a NLDAI 9,0; XNDO 0,50,[x]               step: x++, exit to 7016E13F if x > 9
 *   7016e10f SUB(x,9); temp8 := x*22; y := 1        (check on x ONCE per outer iteration)
 *   7016e11e NLDAI 11,1; XNDO 1,29,[y]              step: y++, exit to 7016E13E
 *   7016e123 SUB(*who,10); who*686 + temp8; SUB(y,11); + 2y; + SD_PTR
 *   7016e13a WSUB 1,1; XWSTA 1,[ac2+0x7D9D]         screen[x][y] := 0   (K -611)
 *   7016e13d WBR 7016E11E; 7016e13e WBR 7016E10A    the two loop tails
 *   7016e13f SUB(*who,10); temp8 := who*686; x := PLAYER[who].fm589 (K -589)
 *   7016e152 (temp8 reused, no check)  y := PLAYER[who].fm588 (K -588)
 *   7016e15b temp8 := OBJ_PTR->region_count (K 11502)   the DO limit, once
 *   7016e162 k := 1; if !(1 <= temp8) goto 7016E1D0
 *   7016e16b XWDO 0,98,[k]                          step: k++, exit to 7016E1D0
 *   7016e170 SUB(k,100000); temp10 := k*9; rx := REGION[k].x; ry := REGION[k].y
 *   7016e18b ac2=rx, ac0=x; WSUB 0,2; WSGE 2,2; WNEG 2,2; WSLEI 2,4; WBR step
 *                                                   if ABS(rx - x) > 4 continue
 *   7016e195 same with ry, y, 5                     if ABS(ry - y) > 5 continue
 *   7016e19f SUB(*who,10); who*686
 *   7016e1a8 x-5; rx - (x-5); SUB(.,9); *22; +
 *   7016e1b7 y-6; ry - (y-6); SUB(.,11); *2; +; + SD_PTR
 *   7016e1c7 XWLDA 0,[ac2+0x7D9D]; WSEQ 0,0; WBR step   if screen[..][..] != 0 continue
 *   7016e1cb XWLDA 0,[k]; XWSTA 0,[ac2+0x7D9D]      screen[..][..] := k  (base reused, no check)
 *   7016e1d0 XPEF @[ac3+0xFFF4]; LCALL FAKE_LAND_MASS,1
 *   7016e1d6 XPEF @[ac3+0xFFF4]; LCALL FAKE_OCEAN,1   the incoming pointer passed through
 *   7016e1dc WRTN
 *
 * The checks that constrain the reading:
 *  - the same bound pairing P48 used: |dx| <= 4 with a column bound of 9,
 *    |dy| <= 5 with a row bound of 11, on the same field pair (-589/-588)
 *    and the same screen K (-611, strides 22/2).  A wrong stride or a
 *    swapped pair breaks it.
 *  - the column/row formulas `rx - (x - 5)`, `ry - (y - 6)` are exactly
 *    UPDATE_SCREENS's, and FAKE_OCEAN's clips are their inverse (column i
 *    is world x = x - 5 + i).  Three routines agree on the mapping.
 *  - XNDO/XWDO exits land on 7016E13F / 7016E13E / 7016E1D0 only if the
 *    displacement is taken from pc+1: that is how the loop nest closes.
 *  - the two callees are the addresses quest.symbols gives FAKE_LAND_MASS
 *    and FAKE_OCEAN, both argc 1, both taking the same `who`.
 *
 * VARIABLE REUSE (a001 R5/q001): slots 2 and 3 are the loop counters of the
 * clear and, after it, the player's x/y.  One 16-bit variable each; the C
 * declares two and reuses them, because the frame has no room for four.
 *
 * SUB() placement (a001 Q-A): the clear loop checks x once per OUTER
 * iteration (7016E10F, before the hoisted x*22) and *who, y per inner
 * iteration — so a bare `SUB(x, 9);` at the outer head and an unchecked x
 * at the store.  `x = PLAYER[..].fm589` carries the *who check; the fm588
 * read reuses the base (none).  In the k loop, k is checked once at the
 * rx load and ry reuses k*9; the screen cell is checked on the LOAD (who,
 * column, row) and the store reuses the base — no third set.
 *
 * Eager vs branch (a001 Q-D): every condition here is a skip/branch
 * (the two ABS tests, the cell test, the DO entry tests).  No materialised
 * booleans in this routine.
 *
 * DO limit (a001 R2/P48): `n = OBJ_PTR->region_count` as a local, as
 * UPDATE_SCREENS spells `n = SD_PTR->player_count`; the original keeps it
 * in temp 8 and reloads it at every step (P4).
 *
 * NOT reproduced: the hoisted x*22, who*686 and k*9 temps; the base reuse
 * across the fm589/fm588 pair and the load/store pair.
 *
 * What lower_c.py refuses today: the two game calls (passing the by-ref
 * parameter through), and — pending stage 2 — `SUB` in statement position.
 * Everything else (nested for, ABS, SUB, the 2-D screen field, `*who`,
 * continue) is in the subset.
 *
 * CONFIDENCE: derived (blind x2) — bound pairing, the shared column/row
 * formula with two other routines, frame sum; not run.
 */
#include "quest_rt.h"
#include "declarations.h"

void INIT_SCREEN(const int16_t *who)
{
    int16_t x, y;                                   /* slots 2, 3: loop counters, then position */
    int16_t rx, ry;                                 /* slots 4, 5 */
    int32_t k;                                      /* slot 6 */
    int32_t n;                                      /* the DO limit (temp slot 8) */

    for (x = 1; x <= 9; x++) {
        SUB(x, 9);                                  /* checked once per outer iteration */
        for (y = 1; y <= 11; y++)
            PLAYER[SUB(*who, 10)].screen[x][SUB(y, 11)] = 0;
    }

    x = PLAYER[SUB(*who, 10)].fm589;
    y = PLAYER[*who].fm588;                         /* base reused: no check */

    n = OBJ_PTR->region_count;
    for (k = 1; k <= n; k++) {
        rx = REGION[SUB(k, 100000)].x;
        ry = REGION[k].y;                           /* k*9 reused: no check */
        if (ABS(rx - x) > 4)
            continue;
        if (ABS(ry - y) > 5)
            continue;
        if (PLAYER[SUB(*who, 10)].screen[SUB(rx - (x - 5), 9)][SUB(ry - (y - 6), 11)] != 0)
            continue;
        PLAYER[*who].screen[rx - (x - 5)][ry - (y - 6)] = k;   /* base reused: no check */
    }

    FAKE_LAND_MASS(who);
    FAKE_OCEAN(who);
}
