/* Narrowing to 16 bits at the three places C says a conversion happens, at
 * every interesting boundary value.  Both spellings of the narrowing
 * assignment appear: with an explicit cast and without one. */
#include "quest_rt.h"

int16_t  n0, n1, n2, n3, n4;
uint16_t m0, m1, m2, m3;
int32_t  back0, back1, back2;

void t(void)
{
    n0 = 32767;
    n1 = (int16_t)32768;              /* wraps to -32768 */
    n2 = 65535;                       /* implicit narrowing: -1 */
    n3 = (int16_t)(-32768);
    n4 = (int16_t)(2147483647);
    m0 = 32768;
    m1 = (uint16_t)(-1);
    m2 = 65536;                       /* implicit: 0 */
    m3 = (uint16_t)(-32768);
    back0 = n2;                       /* sign-extends: -1 */
    back1 = m1;                       /* zero-extends: 65535 */
    back2 = (int32_t)n1 * (int32_t)n1;
}
