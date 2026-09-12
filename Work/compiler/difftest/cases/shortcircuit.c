/* Short circuit is SEMANTIC, not an optimisation: the right arm of && here
 * would trap DERR 17 if it were evaluated.  The IR's own && / || are EAGER
 * (IR.md §5.3), so lowering C's && to the IR's would be a silent semantic
 * change — q001 §1.5 records it as a known trap and this case is the guard. */
#include "quest_rt.h"

int32_t guard, idx, hit, miss, ordot;
int16_t a[4];

void t(void)
{
    a[0] = 10; a[1] = 20; a[2] = 30; a[3] = 40;
    guard = 0;
    idx   = 99;                       /* out of 1..4: SUB would trap */
    hit   = 0;
    miss  = 0;
    ordot = 0;

    if (guard && a[SUB(idx, 4) - 1] > 0) {
        hit = 1;
    } else {
        miss = 1;
    }
    /* || short-circuits on a true left arm for the same reason */
    if (1 || a[SUB(idx, 4) - 1] > 0) {
        ordot = 7;
    }
    guard = 1;
    idx = 2;
    if (guard && a[SUB(idx, 4) - 1] == 20) {
        hit = 5;
    }
}
