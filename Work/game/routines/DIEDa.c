#include "quest_rt.h"
#include "declarations.h"
void DIED(const int16_t *msg UNUSED, const int16_t *unused UNUSED)
{
    if (BIT(PLAYER[SUB(PLAYER_NUM, 10)].fm590, 4)) goto reincarnate;
reincarnate:
    return;
}
