/* a001: signed >> at n = 0 and n = 31, and x = 0x80000000.  q001 §1.4a
 * lowers this to a six-statement pure decomposition because IR.md §8 parks
 * the pure arithmetic-shift primary and `ash` writes ovr — the most
 * intricate thing the compiler does, and a named suspect. */
#include "quest_rt.h"

int32_t  s0, s1, s2, s3, s4, s5;
uint32_t u0, u1, u2, u3;

void t(void)
{
    s0 = (int32_t)(-2147483647-1) >> 0;
    s1 = (int32_t)(-2147483647-1) >> 31;
    s2 = (int32_t)(-2147483647-1) >> 1;
    s3 = (int32_t)(-1) >> 17;
    s4 = (int32_t)(1073741824) >> 30;
    s5 = (int32_t)(-1073741824) >> 30;
    u0 = (uint32_t)(2147483648u) >> 0;
    u1 = (uint32_t)(2147483648u) >> 31;
    u2 = (uint32_t)(4294967295u) >> 16;
    u3 = (uint32_t)(1u) << 31;
}
