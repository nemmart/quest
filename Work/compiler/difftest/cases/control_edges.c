/* Zero-trip loops, an empty body, a backward goto, break and continue in
 * every position, and a checksum threaded through so the PATH is observable
 * and not just the final state (q001 §2.3). */
#include "quest_rt.h"

uint32_t chk;
int32_t  lc, back, acc, brk, cont;

void t(void)
{
    chk = 0; acc = 0; brk = 0; cont = 0; back = 0;

    for (lc = 0; lc < 0; lc++) {          /* zero trips: body never runs */
        acc = 999;
    }
    for (lc = 0; lc < 3; lc++) {          /* empty body */
    }
    for (lc = 0; lc < 6; lc++) {
        chk = chk * 1000003u + (uint32_t)lc;
        if (lc == 2) continue;
        if (lc == 4) break;
        acc = acc + lc;
    }
    brk = lc;
    lc = 0;
    while (lc < 4) {
        lc++;
        if (lc == 2) continue;
        cont = cont + lc;
    }
    back = 0;
L1: ;
    chk = chk * 1000003u + (uint32_t)back;
    back++;
    if (back < 5) goto L1;
    if (back == 5) goto L2;
    acc = -1;
L2: ;
    chk = chk * 1000003u + (uint32_t)acc;
}
