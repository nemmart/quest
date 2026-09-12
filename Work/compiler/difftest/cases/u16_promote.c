/* uint16_t promotes to int32_t, NOT to uint32_t: `uint16_t / uint16_t` is a
 * SIGNED divide in C, and `uint16_t * uint16_t` overflows as a SIGNED int.
 * The most counter-intuitive rule in q001 §1.3 and the first thing this
 * suite exists to catch. */
#include "quest_rt.h"

uint16_t ua, ub;
int32_t  q, rem, prod, cmpres, shifted;

void t(void)
{
    ua = 65535;
    ub = 3;
    q      = ua / ub;          /* 21845, via int32 arithmetic */
    rem    = ua % ub;
    prod   = ua * ub;          /* 196605: fits int32, does NOT wrap at 16 bits */
    cmpres = (ua > ub);
    shifted = ua << 8;         /* 16776960: promoted to int, not masked to 16 */
}
