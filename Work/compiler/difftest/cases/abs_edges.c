/* ABS is a lowered builtin (a001 R1b), not a call.  ABS(INT_MIN) wraps back
 * to INT_MIN under -fwrapv and under the IR's `0 - x`, which is the only
 * value where the two could have parted company. */
#include "quest_rt.h"

int32_t z0, z1, z2, z3, z4;
int16_t h0;

void t(void)
{
    z0 = ABS(0);
    z1 = ABS(-1);
    z2 = ABS((-2147483647-1));
    z3 = ABS(2147483647);
    h0 = (int16_t)(-32768);
    z4 = ABS((int32_t)h0);
}
