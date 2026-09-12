/* The DERR 17 trap itself: the subscript goes out of range and BOTH sides
 * must abort at the SAME source site (a001 R5).  Nothing after the trap is
 * observable on either side, so the whole comparison is the trap line. */
#include "quest_rt.h"

int32_t i, sink;
int16_t a[8];

void t(void)
{
    a[0] = 1; a[1] = 2; a[2] = 3; a[3] = 4;
    a[4] = 5; a[5] = 6; a[6] = 7; a[7] = 8;
    i = 8;
    sink = a[SUB(i, 8) - 1];        /* in range: 8 is the last legal index */
    i = 9;
    sink = a[SUB(i, 8) - 1];        /* DERR 17 here */
    sink = 999;                     /* never reached */
}
