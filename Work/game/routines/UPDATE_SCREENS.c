/* UPDATE_SCREENS @7017D635 — argc 3, frame 0x05 (10 local words), WSAVS.
 *
 * Written FRESH for Project 48 from Disassembled/quest.dis 7017d635..7017d6a8
 * and docs/Project34/readable/UPDATE_SCREENS.txt, with the record geometry
 * read out of game/declarations.json.  P35's file of the same name was on
 * disk and is reference-only under Plan.md's standing policy; whether it
 * turned out to be right is recorded in docs/Project48/REPORT.md.
 *
 * What it does: for every player whose viewport contains the map position
 * (*x, *y), write *cell into that player's screen buffer at the position the
 * cell occupies in THAT player's view.  The viewport is 9 columns of 11
 * cells, each cell two words, centred on the player.
 *
 * The reading, and the checks that close it:
 *
 *  - `WSGE 2,2` at 7017d65a and 7017d673 is the XX==YY compare-AGAINST-ZERO
 *    form (IR.md §5.6), so `WSUB; WSGE; WNEG` is the ABS diamond and not a
 *    self-compare.  Hence ABS(), the PL/I builtin.
 *  - the store's displacement 0x7D9D is -611 on the 15-bit signed field, and
 *    declarations.json gives `screen` K = -611, strides [22, 2], dims
 *    [9, 11].  -611 = -587 - 22 - 2: the 1-based origin is already folded
 *    into K, and the array sits immediately below fm588 at -588.
 *  - the two ABS bounds (4, 5) and the two subscript bounds (9, 11) agree:
 *    |dx| <= 4 gives a column in 1..9 and |dy| <= 5 a cell in 1..11.  That
 *    is an independent check on the reading, not a restatement of it.
 *  - i is bounds-checked TWICE (7017d64d, 7017d664) and the store is NOT
 *    checked a third time, because the original reuses the element base it
 *    hoisted into frame slot 6.  The SUB() calls are placed to match: two on
 *    the tests, none on the store.
 *
 * NOT reproduced here, and correctly so: the original hoists `i * 686` into
 * slot 6 and the element pointer into slot 8 and reuses both.  Those are
 * DESIGN §4.2 transformation-created `v`s — a rewrite's job, not the naive
 * compiler's.  This C recomputes, which is what naive means.
 */
#include "quest_rt.h"

void UPDATE_SCREENS(const int16_t *x, const int16_t *y, const int32_t *cell)
{
    int16_t i;
    int16_t n;

    n = SD_PTR->player_count;
    for (i = 1; i <= n; i++) {
        if (ABS(PLAYER[SUB(i, 10)].fm589 - *x) > 4)
            continue;
        if (ABS(PLAYER[SUB(i, 10)].fm588 - *y) > 5)
            continue;
        PLAYER[i].screen[SUB(*x - (PLAYER[i].fm589 - 5), 9)]
                        [SUB(*y - (PLAYER[i].fm588 - 6), 11)] = *cell;
    }
}
