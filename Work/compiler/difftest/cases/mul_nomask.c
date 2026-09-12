/* No mask after an arithmetic operator.  Both operands are 16-bit objects,
 * so both promote to int32 and the product is a 32-bit value: a compiler
 * that masks the RESULT back to 16 bits because the operands were 16-bit is
 * wrong, and this is the case that says so. */
#include "quest_rt.h"

int16_t  a, b;
uint16_t c, d;
int32_t  p_signed, p_mixed, sum, diff;
uint32_t p_unsigned;

void t(void)
{
    a = 300; b = 300;
    c = 60000; d = 3;
    p_signed   = a * b;                 /* 90000, not 24464 */
    p_unsigned = (uint32_t)c * (uint32_t)d;
    p_mixed    = c * d;                 /* 180000 via signed int32 */
    sum        = a + b + c + d;
    diff       = a - c;                 /* 300 - 60000 = -59700 */
}
