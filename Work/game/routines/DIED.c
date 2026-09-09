/* DIED @7016603D (argc 2, frame 0x07) — the player has died: clear the
 * player's name/status fields and object slot, drop the objects held back
 * into the world, announce the death, and then either wait for a key
 * (permanent death) or clear the status bits and reincarnate.
 *
 * CORRECTION to the P35 stub (METHOD §11): P35 wrote the signature as
 * `DIED(const int16_t *who, const int16_t *how)`.  Argument 1 is in fact a
 * CHAR VARYING passed by reference — the death message.  The routine reads
 * its LENGTH word (`sx16(M16[R[ac3 + -12]])` at 70166144, the word AT the
 * argument's address) to size the message temporaries, and copies the
 * string itself with `[@ac2, sx16(M16[M32[wp(ac3,-12)]])] =
 * [@M32[wp(ac3,-12)], varying]`.  Argument 2 is never read. */
#include "quest_rt.h"
#include "declarations.h"

void DIED(const int16_t *msg UNUSED, const int16_t *unused UNUSED)
{
    if (BIT(PLAYER[SUB(PLAYER_NUM, 10)].fm590, 4)) goto reincarnate;
    assign_varying(&PLAYER[SUB(PLAYER_NUM, 10)].fm625, 32, "                                ");
    assign_varying(&PLAYER[SUB(PLAYER_NUM, 10)].fm608, 32, "                                ");
reincarnate:
    return;
}
