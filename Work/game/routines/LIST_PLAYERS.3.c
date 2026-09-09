/* LIST_PLAYERS.3@7016F556 — argc 1, frame 0x03, WSAVS, nested, slotpatch.
 *
 * PARTIAL — ABANDONED.  See docs/Project39/LIST_PLAYERS3_ABANDONED.md.  NO
 * MATCH NUMBER IS QUOTED FOR IT.  It is kept as the witness for R21d (a
 * by-reference parameter as the DO control variable) and R21e (a DO limit that
 * is an expression is evaluated once into a temp).  What it still needs is an
 * R9/R10 correction that has nothing to do with the static link.
 *
 * Project 39, routine 1: the FIRST validator for the static link.  Chosen
 * first because it is the smallest use of the construct that is otherwise
 * built already — ONE uplevel read, and everything else is machinery P36/P37
 * landed (bit tests R26-R29, the DO loop R21, the slotpatch return R35).
 *
 * A nested PL/I procedure of LIST_PLAYERS.  It scans the players and answers
 * "is there one that is present, not <bit 1>, and whose fm629 differs from the
 * enclosing procedure's w13?"  It returns -32768 (PL/I '1'B) on the first such
 * player and 0 if the loop runs out; the result is stored into the saved-ac0
 * image at wp(ac3, -7), which is the addrbook's `slotpatch` flag.
 *
 * The static link (R42/R43): the enclosing frame pointer arrives in ac1 and
 * WSAVS saves it at wp(fp, -6).  `UP(LIST_PLAYERS, w13)` is the one uplevel
 * reference — 7016F59F `XWLDA 2,[ac3+0x7FFA]` then `XNLDA 1,[ac2+0xD]`, the
 * double indirection the accessor spelling exists to keep visible.
 *
 * Note the argument AND the link together: argc is 1, and the argument `i` is
 * by reference at wfp-12 (`@[ac3+0xFFF4]`) while the link is in ac1 — the two
 * mechanisms do not compete, which is R42's point.  `i` is also the DO
 * variable, so the loop's XNDO is the INDIRECT form.
 */
#include "declarations.h"

int16_t LIST_PLAYERS_3(UPLINK(LIST_PLAYERS) __up, int16_t *i)
{



    for (*i = 1; *i <= SD_PTR->player_count; (*i)++) {
        if (!BIT(PLAYER[SUB(*i, 10)].fm591, 0)) continue;
        if (BIT(PLAYER[SUB(*i, 10)].fm591, 1)) continue;
        if (PLAYER[SUB(*i, 10)].fm629 == UP(LIST_PLAYERS, w13)) continue;
        return -32768;
    }
    return 0;
}
