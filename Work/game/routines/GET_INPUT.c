/* GET_INPUT @7016AA35 — argc 1, frame 0x29 (82 local words), WSAVS.
 *
 * Written BLIND for Project 51 from Disassembled/quest.dis 7016aa35..7016aa6d,
 * quest.mem 7016A9B9 ("001"), and Disassembled/quest-rt.dis 7017DE5F..DEE6
 * (?READ's body, for the argument roles).  The P37 file of the same name
 * (STAGED at the BITS() argument) was on disk and NOT opened before this was
 * written; docs/Project51/REPORT.md records whether it agreed.  Two blind
 * readings (q001, q002) agree.
 *
 * What it does: read one byte from the input channel into a 144-byte
 * buffer, hand the first byte back through the CHAR(1) parameter, and strip
 * the parity bit from anything above 128 (128 itself passes).
 *
 * The listing:
 *   7016aa37 XLEFB 2,[ac3+0x8]; XWSTA 2,[ac3+0x4E]   temp 78 := byte ptr to slot 4 (buf)
 *   7016aa3b XLEF 2,[ac3+0x4C]                       ac2 = word addr of temp 76
 *   7016aa3d XLEFB 0,[pc+..](7016A9B9:0); NLDAI 3,1  ac0 = "001", ac1 = 3
 *   7016aa41 LCALL [X.CB],0                          temp 76 := '001'B   (F12)
 *   7016aa45 NLDAI 1,0; XNSTA 0,[ac3+0x50]           temp 80 (16-bit) := 1
 *   7016aa49 NLDAI 0x1000,1; XNSTA 1,[ac3+0x52]      temp 82 (16-bit) := 4096
 *   7016aa4d XPEF [ac3+0x4C]                         arg 6: &temp76   the bit literal
 *   7016aa4f XPEF [ac3+0x52]                         arg 5: &temp82   options 0x1000
 *   7016aa51 XPEF [ac3+0x2]                          arg 4: &slot2    flag (output)
 *   7016aa53 XPEF [ac3+0x50]                         arg 3: &temp80   count in/out (1)
 *   7016aa55 XPEF [ac3+0x4E]                         arg 2: &temp78   -> byte ptr to buf
 *   7016aa57 LPEF [0x70000262]                       arg 1: &IN_CHAN
 *   7016aa5a LCALL ?READ,6
 *   7016aa5e XLDB 2,[ac3+0x8]                        ac2 = buf[0]  (byte 8 = slot 4)
 *   7016aa60 XWLDA 0,[ac3+0x7FF4]; WSTB 0,2          *ch := buf[0]  (arg 1 is a BYTE ptr)
 *   7016aa63 WLDB 0,2                                ac2 = *ch  (zero-extended, F13)
 *   7016aa64 WSGTI 2,128; WRTN                       if !(*ch > 128) return
 *   7016aa67 WNADI 2,0xFF80; WANDI 2,255; WSTB 0,2   *ch := (*ch - 128) & 255
 *   7016aa6d WRTN
 *
 * The checks that constrain the reading:
 *  - the frame closes: flag at 2 (16-bit, pair 2-3), buf at 4..75 (144 bytes
 *    = 72 words, F22), temps 76/78/80/82 (four wides) = slots 2..83 = 82
 *    words = 2 * 0x29 exactly (F14).
 *  - argument roles from ?READ's own body (RTConventions.md, ?READ row):
 *    it writes the transferred byte count through arg 3 (7017DEE4
 *    `XNSTA 0,@[ac3+0xFFF0]`) and 0x8000 through arg 4 on error code 24
 *    when argc > 3 (7017DED2..DED9).  So temp 80's `1` is the requested
 *    count and comes back as the actual count (a dummy the callee writes —
 *    PL/I permits it), and slot 2 is an end/error flag this routine never
 *    reads.  That is why slot 2 is a local and 1/4096 are temps.
 *  - arg 2's datum is a POINTER: the machine stores the byte pointer to
 *    buf into temp 78 and pushes &temp78 (P13's site).  The C says
 *    TMP(buf): a dummy holding the buffer's address (a001 Q-B).
 *  - `XLDB/WLDB` zero-extend and `WSGTI 2,128` is signed: with an unsigned
 *    byte the test is live for 129..255 (F13); with a signed char it is dead.
 *  - the callers push `XPEFB` (HIT_ANY_CHAR 7016DEA7 and 26 others): the
 *    parameter is a byte pointer, matching `WSTB 0,2` here.
 *
 * `& 255` is the narrowing of the 16-bit difference to CHAR(1); the C writes
 * the cast and the lowering owns the mask (an 8-bit `trunc8`, which
 * lower_c.py lacks — it has only trunc16).  Written as
 * `(unsigned char)(*ch - 128)`; a stage-2 reader may prefer `& 255` spelled
 * out, and either is right.
 *
 * Eager vs branch (a001 Q-D): the one condition is a skip (WSGTI ... WRTN).
 *
 * NOT reproduced: the byte-pointer temp 78, the constant temps 80/82, the
 * order the compiler wrote them in (P13).
 *
 * What lower_c.py refuses today: the ?READ call, BITS(), TMP() of a
 * pointer, the byte array (`char` is a WORD kind there: 144 bytes would
 * lower to 144 words), byte load/store through the parameter, trunc8.
 *
 * CONFIDENCE: derived (blind x2) — frame sum (F14), ?READ's body for the
 * argument roles, the callers' XPEFB for the parameter form; not run.
 */
#include "quest_rt.h"
#include "declarations.h"

void GET_INPUT(unsigned char *ch)
{
    int16_t flag;                                   /* slot 2: ?READ's end/error flag, never read */
    unsigned char buf[144];                         /* slots 4..75 */

    READ$6(&IN_CHAN, TMP(buf), TMP(1), &flag, TMP(0x1000), BITS("001"));
    *ch = buf[0];
    if (*ch > 128)
        *ch = (unsigned char)(*ch - 128);           /* WANDI 255 = the 8-bit narrowing */
}
