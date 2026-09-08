/* PICK_X_Y @701761E7 (argc 2, frame 0x05) — pick a random point within
 * +-20 of the centre of a random non-empty region of type 3, retrying
 * until the point lies inside the world rectangle.
 *
 * Correction to docs/Project34/PICK_X_Y_reading.md (METHOD §11): the type
 * test retries when the type is NOT 3 (`goto [retry, cont] (ac0 == 3)`),
 * i.e. only type-3 regions are accepted; the hand reading had the polarity
 * reversed. */
#include "quest_rt.h"
#include "declarations.h"

void PICK_X_Y(int16_t *x, int16_t *y)
{
    int32_t r;
retry:
    r = RANDOM_NUMBER$3(TMP(1), TMP(OBJ_PTR->region_count), &SD_PTR->seed);
    if (REGION[SUB(r, 100000)].x == 0) goto retry;
    if (REGION[r].type / 100 + 1 != 3) goto retry;
    *x = RANDOM_NUMBER$3(TMP(REGION[r].x - 20), TMP(REGION[r].x + 20), &SD_PTR->seed);
    *y = RANDOM_NUMBER$3(TMP(REGION[r].y - 20), TMP(REGION[r].y + 20), &SD_PTR->seed);
    if (*x <= 15349) goto retry;
    if (*x > 16300) goto retry;
    if (*y <= 15219) goto retry;
    if (*y <= 16350) return;
    goto retry;
}
