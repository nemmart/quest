/* HIT_ANY_CHAR @7016DE91 — argc 0, frame 0x09 (18 local words), WSAVS.
 *
 * Written BLIND for Project 51 from Disassembled/quest.dis 7016de91..7016debb
 * and quest.mem 7016DE04..7016DE1A.  The P38 file of the same name was on
 * disk and was NOT opened before this was written; whether it agreed is
 * recorded in docs/Project51/REPORT.md.  Read blind by two sessions (q001,
 * q002), which agreed on every point below (a001 §0).
 *
 * What it does: prompt on the output channel, wait for one keystroke, then
 * write CR + the Dasher "erase to end of line" control.
 *
 * The listing:
 *   7016de93 NLDAI 30,0; WMOV 0,1        ac0 = ac1 = 30
 *   7016de96 XNSTA 0,[ac3+0x4]           slot 4 (16-bit) := 30   the length word
 *   7016de98 XLEFB 2,[ac3+0xA]           ac2 = byte ptr to word 5 (byte disp 0xA)
 *   7016de9a XLEFB 3,[pc+..](7016DE07:0) ac3 = byte ptr to the literal
 *   7016de9c WCMV                        30 bytes -> slots 5..19
 *   7016de9d LDAFP 3
 *   7016de9e XPEF [ac3+0x4]; LPEF [0x70000260]; LCALL ?WRITE_SCREEN,2
 *                                        (last pushed = arg 1 = OUT_CHAN)
 *   7016dea7 XPEFB [ac3+0x4]             BYTE ptr to word 2 = &ch
 *   7016dea9 LCALL GET_INPUT,1
 *   7016dead WLDAI 1,0x00020D0B          length 2, bytes 0D 0B
 *   7016deb0 XWSTA 1,[ac3+0x4]           the SAME temp, one wide store
 *   7016deb2 XPEF [ac3+0x4]; LPEF [0x70000260]; LCALL ?WRITE_SCREEN,2
 *   7016debb WRTN
 *
 * quest.mem at 7016DE07 byte 0: 0B 48 69 74 .. 75 65 = '\v' + "Hit any
 * character to continue" (1 + 29 = 30 bytes, equal to the length word).
 *
 * The checks that constrain the reading:
 *  - frame sum: ch at slot 2 (a CHAR(1) scalar takes the even pair 2-3, F22)
 *    + length word at 4 + 15 data words at 5..19 = 18 = 2 * 0x09 exactly.
 *    A 29- or 31-byte literal, or a temp anywhere else, does not fit; the
 *    WCMV count (30), the frame room (15 words) and the literal bytes (30)
 *    agree independently.
 *  - XLEFB/XPEFB displacements are BYTES (word 5 must be byte 0xA for the
 *    data to follow the length word), so `XPEFB [ac3+0x4]` is word 2 and
 *    GET_INPUT receives a byte pointer; GET_INPUT's own body stores through
 *    arg 1 with WSTB (7016AA60..62), which is the callee-side witness.
 *  - the second literal is a folded immediate: length 2 in the high half,
 *    0D 0B in the low, stored as one wide into the same temp.  Stage 3 must
 *    name this: "CHAR constant <= 2 bytes -> WLDAI + one wide store".
 *
 * Spelling (a001 §2): the CHAR constants are passed as literals and the
 * lowering builds the VARYING dummy.  Slots 4..19 are a compiler temp (one
 * temp, two literals), not a variable the source names.
 *
 * NOT reproduced: the temp's reuse, the packed-immediate form, LDAFP.
 *
 * What lower_c.py refuses today: both calls (all three statements), the two
 * string literals, `unsigned char`.
 *
 * CONFIDENCE: derived (blind x2) — frame sum, byte-displacement check,
 * callee body, quest.mem bytes; not run, not matched.
 */
#include "quest_rt.h"
#include "declarations.h"

void HIT_ANY_CHAR(void)
{
    unsigned char ch;                               /* slot 2 (pair 2-3); CHAR(1) */

    WRITE_SCREEN$2(&OUT_CHAN, "\vHit any character to continue");   /* 30 bytes */
    GET_INPUT(&ch);                                 /* byte pointer (XPEFB) */
    WRITE_SCREEN$2(&OUT_CHAN, "\r\v");              /* 0D 0B, packed immediate */
}
