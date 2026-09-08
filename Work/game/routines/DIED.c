/* DIED @7016603D (argc 2, frame 0x07) — the player's death: clear the two
 * 32-character name/status fields, announce, reposition, refresh.
 *
 * STAGED STRETCH (Project 35 PLAN §7, ruling R4).  Only the opening is
 * written: the routine's first statement is a bit-field test on the
 * player record (bit address 16*(PLAYER_NUM*686) - 9436 = word -590, bit
 * 4), which is outside the translator's subset — translate.py REFUSES at
 * this line, by design (METHOD: refuse, don't guess).  The rest of the
 * routine needs, per the P35 census of its 495 book statements: bit-field
 * addressing (51 statements), ac3 repurposed as an address register (13),
 * string statements into record fields (12), arena twins/claims (20) and
 * six game->game calls.  See docs/Project35/REPORT.md §DIED. */
#include "quest_rt.h"
#include "declarations.h"

void DIED(const int16_t *who UNUSED, const int16_t *how UNUSED)
{
    if (BIT(PLAYER[SUB(PLAYER_NUM, 10)].fm590, 4)) goto out;
    assign_fixed(&PLAYER[SUB(PLAYER_NUM, 10)].fm625, 32, "                                ");
    assign_fixed(&PLAYER[SUB(PLAYER_NUM, 10)].fm608, 32, "                                ");
out:
    return;
}
