/* UPDATE_SCREENS @7017D635 (argc 3, frame 0x05) — write a cell into the
 * viewport of every player whose viewport contains map position (x, y).
 * The viewport is 9 rows x 11 cells (2 words each) centred on the player. */
#include "quest_rt.h"
#include "declarations.h"

void UPDATE_SCREENS(const int16_t *x, const int16_t *y, const int32_t *cell)
{
    int16_t i;
    int16_t n;
    n = SD_PTR->player_count;
    for (i = 1; i <= n; i++) {
        if (ABS(PLAYER[SUB(i, 10)].fm589 - *x) > 4) continue;
        if (ABS(PLAYER[SUB(i, 10)].fm588 - *y) > 5) continue;
        PLAYER[i].screen[SUB(*x - (PLAYER[i].fm589 - 5), 9)][SUB(*y - (PLAYER[i].fm588 - 6), 11)] = *cell;
    }
}
