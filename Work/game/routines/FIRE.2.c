/* FIRE.2@7016A461 — argc 0, frame 0x0A, WSAVS, nested.
 *
 * INCOMPLETE — NOT ABANDONED, NOT FINISHED, and NO MATCH NUMBER IS QUOTED.
 * The session ended mid-routine.  translate.py REFUSES it at the inner
 * `cvwn(PLAYER[...].fm376 / 10)`: the width model types a divide of a 16-bit
 * value as 16-bit, so the explicit convert reads as redundant, while the book
 * emits `CVWN 2` there (7016A49C).  That is a WIDTH-MODEL question about
 * divide, not a static-link question — the uplevel machinery below translates.
 * The next session should start here; see docs/Project39/REPORT.md §4.
 *
 * Project 39, routine 2: the second static-link validator, and one half of the
 * FIRE MUTUAL CHECK.  FIRE.1 and FIRE.2 are siblings nested in the same
 * procedure, so they must agree on FIRE's frame layout INDEPENDENTLY — each
 * derives the slots it touches from its own references, and a disagreement is
 * a finding, not something to reconcile (P39 user ruling).  That is the
 * difference between deriving the parent's frame and fitting it.
 *
 * FIRE.2 is the routine that shows the link reaching the parent's ARGUMENTS as
 * well as its locals.  `*UPARG(FIRE, 1)` is a triple indirection — the link,
 * then FIRE's argument slot at wfp-12, then the datum — and it appears three
 * times (7016A479, 7016A4A6, 7016A4E8) as `XNLDA r,@[ac2+0xFFF4]`.  No routine
 * that only read its parent's LOCALS would have shown that the spelling has to
 * cover parameters too.
 *
 * The link is loaded fresh in every block that needs it (R43/R8): four times,
 * always into ac2, never cached across a block boundary — and reused WITHIN a
 * block across a DERR continuation (7016A477 -> 7016A47F), which is what makes
 * it an ordinary cached address rather than a special case.
 *
 * What it does: announce the wizard's death, then move score/count fields
 * between the acting player (FIRE's argument 1) and the target player (FIRE's
 * local w12), with a ?RANDOM_NUMBER draw in the middle.
 */
#include "declarations.h"

void FIRE_2(UPLINK(FIRE) __up)
{
    WRITE_SCREEN$2(&OUT_CHAN, "The wizard has been killed!\n");

    PLAYER[SUB(*UPARG(FIRE, 1), 10)].fm378 =
        cvwn(PLAYER[SUB(*UPARG(FIRE, 1), 10)].fm378
             + cvwn(PLAYER[SUB(UP(FIRE, w12), 10)].fm376 / 10) + 1);

    PLAYER[SUB(*UPARG(FIRE, 1), 10)].fm379 =
        cvwn(PLAYER[SUB(UP(FIRE, w12), 10)].fm89 * 5
             + (PLAYER[SUB(*UPARG(FIRE, 1), 10)].fm379
                + RANDOM_NUMBER$3(TMP(3), TMP(10), &SD_PTR->seed)));
}
