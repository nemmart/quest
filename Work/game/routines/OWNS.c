/* OWNS @70175CBF — argc 2, frame 0x03, WSAVS, slotpatch (a value-returning
 * PL/I function: the 16-bit result is stored into the saved-ac0 image at
 * wp(ac3, -7), so WRTN restores it into ac0).
 *
 * Project 37, routine 1: the SECOND witness for the bit rules R26-R29.
 * Seven WSZB tests on PLAYER(p).fm590 / .fm591, all in the R26 shape
 * (stride multiply, *16, one WNADI folding 16*K + n) with the record base
 * loaded separately (R28) and the result materialised to 0/-1 (R29).
 */
#include "declarations.h"

int16_t OWNS(const int16_t *p, const int16_t *item)
{
    int16_t i;
    for (i = 1; i <= 10; i++) {
        if (PLAYER[SUB(*p, 10)].fm390[SUB(i, 10)] != *item) continue;
        return -32768;
    }
    if (*item == 8) return BIT(PLAYER[SUB(*p, 10)].fm591, 15);
    if (*item == 3) return BIT(PLAYER[SUB(*p, 10)].fm590, 0);
    if (*item == 106) return BIT(PLAYER[SUB(*p, 10)].fm591, 14);
    if (*item == 105) return BIT(PLAYER[SUB(*p, 10)].fm590, 5);
    if (*item == 112) return BIT(PLAYER[SUB(*p, 10)].fm590, 2);
    if (*item == 111) return BIT(PLAYER[SUB(*p, 10)].fm590, 4);
    if (*item == 114) return BIT(PLAYER[SUB(*p, 10)].fm590, 3);
    return 0;
}
