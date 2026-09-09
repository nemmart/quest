/* QUEST.1@7015C5E1 — argc 0, frame 0x06, WSAVS, nested.
 *
 * Project 40, routine 1: the EXPRESSION-LEVEL DIAGNOSTIC.  Chosen because it
 * uses no construct at all — no float, no twin, no divide, no syscall, no
 * string, and no call of any kind (0 rt_call, 0 LJSR, 0 game→game call in 70
 * statements).  Arithmetic, control flow and bit operations only.  If the
 * model is right about ordinary expressions this closes; if it does not, the
 * failure is expression-level modelling and nothing can hide behind a
 * construct.
 *
 * Is the current player standing on a castle?  Scan the castles; on the first
 * whose (fm23, fm22) equal the player's (fm589, fm588) — which UPDATE_SCREENS
 * shows are map x and y — set PLAYER(PLAYER_NUM).fm591 bit 10 and return.  If
 * the loop runs out, clear the same bit.
 *
 * NESTED, but the link is never used (R42): QUEST.1 is flagged `nested` and
 * WSAVS saves the enclosing frame pointer at wp(fp, -6), yet not one of its
 * blocks loads it.  The UPLINK parameter is declaration-side only; the
 * PARENT_FRAMES entry for QUEST is empty and asserts nothing about QUEST.
 *
 * R36 (the loop-invariant hoist) is the interesting one.  PLAYER(PLAYER_NUM)
 * does not depend on the loop variable, so its evaluation is lifted to the
 * loop head — DERR 17 in the entry block, the *686 opening the init block,
 * the temp store AFTER the loop-variable init (7015C5FD, slot 6).  Unlike
 * OWNS, where only the OUTER scale could be lifted because the inner
 * subscript varies with i, here the WHOLE reference is invariant, so the temp
 * holds the element ADDRESS (reloaded straight into ac2 at 7015C60E with no
 * base add) rather than the scaled subscript.
 */
#include "quest_rt.h"
#include "declarations.h"

void QUEST_1(UPLINK(QUEST) __up)
{
    int16_t i;
    for (i = 1; i <= OBJ_PTR->f911504; i++) {
        if (PLAYER[SUB(PLAYER_NUM, 10)].fm589 != CASTLE[SUB(i, 1000)].fm23) continue;
        if (PLAYER[SUB(PLAYER_NUM, 10)].fm588 != CASTLE[SUB(i, 1000)].fm22) continue;
        BIT_SET(PLAYER[SUB(PLAYER_NUM, 10)].fm591, 10);
        return;
    }
    BIT_CLR(PLAYER[SUB(PLAYER_NUM, 10)].fm591, 10);
}
