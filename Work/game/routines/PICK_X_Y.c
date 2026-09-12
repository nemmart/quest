/* PICK_X_Y @701761E7 — argc 2, frame 0x05 (10 local words), WSAVS.
 *
 * Written BLIND for Project 51 from Disassembled/quest.dis
 * 701761e7..70176275.  The P35 file of the same name (which matched the book
 * 64/64 under the old translator) was on disk and NOT opened before this was
 * written; docs/Project51/REPORT.md records whether it agreed.  Two blind
 * readings (q001, q002) agree on the shape.
 *
 * What it does: pick a random spawn position.  Draw a region r; if the
 * region is unused (x == 0) or not of terrain class 3 (type in 200..299),
 * draw again; then draw *x within 20 of the region's x and *y within 20 of
 * its y, and accept only if the point lies inside the world rectangle
 * (F18: x in (0x3BF5, 0x3FAC], y in (0x3B73, 0x3FDE]).  Otherwise start
 * over, including a fresh region.
 *
 * The listing (loop head at 701761E9):
 *   701761e9 NLDAI 1,0; XWSTA 0,[ac3+0x4]          temp 4 := 1         (dummy lo)
 *   701761ed LWLDA 2,[OBJ_PTR]; XWLDA 1,[ac2+0x2CEE]; XWSTA 1,[ac3+0x6]
 *                                                   temp 6 := region_count (K 11502) (dummy hi)
 *   701761f4 LWLDA 2,[SD_PTR]; XPEF [ac2+0x28]; XPEF [ac3+0x6]; XPEF [ac3+0x4]
 *   701761fd LCALL ?RANDOM_NUMBER,3                 (lo, hi, &SD_PTR->seed K=40) -> ac0
 *   70176201 XWSTA 0,[ac3+0x2]                      r (slot 2, 32-bit) := ac0
 *   70176203 WUGTI 0,100000; WSGT 0,0; DERR 17      SUB(r, 100000) — the ONLY check on r
 *   70176208 NLDAI 9,1; WMUL 1,0; XWSTA 0,[ac3+0x4] temp 4 := r*9 (hoisted)
 *   7017620d LWADD 0,[OBJ_PTR]; WMOV 0,2; XNLDA 2,[ac2+0x2CE7]   REGION[r].x (K 11495)
 *   70176213 XWSTA 0,[ac3+0x6]                      temp 6 := element base (hoisted)
 *   70176215 MOV.# 2,2,SNR; WBR -45                 x == 0 -> loop        (branch shape)
 *   70176217 ..[ac2+0x2CE9]                         REGION[r].type (K 11497)
 *   7017621e NLDAI 100,1; WDIV 1,0; CVWN 0; WINC 0,0; WSEQI 0,3; WBR -60
 *                                                   type/100 + 1 != 3 -> loop  (branch)
 *   70176226 XWLDA 2,[ac3+0x6]; x-20 -> temp 6; x+20 -> temp 8 (XWSTA, 32-bit dummies)
 *   7017623d LCALL ?RANDOM_NUMBER,3; CVWN 0; XNSTA 0,@[ac3+0xFFF4]   *x := (16-bit)
 *   70176244 XWLDA 2,[ac3+0x4] + OBJ_PTR; y-20 -> temp 4; y+20 -> temp 6
 *   7017625e LCALL ?RANDOM_NUMBER,3; CVWN 0; XNSTA 0,@[ac3+0xFFF2]   *y :=
 *   70176265 XNLDA 0,@[ac3+0xFFF4]; WSGTI 0,0x3BF5; WBR stub
 *   7017626a WSLEI 0,0x3FAC; WBR stub
 *   7017626d XNLDA 0,@[ac3+0xFFF2]; WSGTI 0,0x3B73; WBR stub
 *   70176272 WSGTI 0,0x3FDE; WRTN                   all four hold -> return
 *   70176275 XJMP 701761E9                          the stub: start over
 *
 * The checks that constrain the reading:
 *  - the four gate constants are exactly F18's, and 0x3B73 (not the attic's
 *    0x3B77) is what 7017626F holds; FAKE_OCEAN clips against the same four
 *    numbers (15219/15220, 15349/15350, 16300, 16350), an independent site.
 *  - the seed is pushed as `[ac2+0x28]` off SD_PTR: K = 40 = declarations'
 *    sd_ptr_hdr.seed; region_count is K 11502 = obj_ptr_hdr.region_count;
 *    x/y/type are K 11495/11496/11497 with stride 9 = REGION.  Four table
 *    facts from declarations.json line up with the four field loads.
 *  - the DERR 17 bound is 100000 = REGION's declared bound.
 *  - both stores through the parameters are XNSTA after CVWN: the
 *    parameters are 16-bit (int16_t *).
 *
 * Dummy arguments (TMP): every lo/hi is a temp written just before its
 * address is pushed — the constant 1, `region_count` (COPIED to temp 6 before
 * the push, so the source argument was not a plain by-reference variable;
 * a type mismatch with the ENTRY declaration is the usual cause), and the
 * four `field +/- 20` expressions.  All six temps are 32-bit (XWSTA) even
 * where the source is 16-bit: ?RANDOM_NUMBER's parameters are FIXED BIN(31)
 * (recorded in RTConventions.md).  The seed is passed directly.
 *
 * SUB() placement: r is checked once, at 70176203, and every later REGION[r]
 * access reuses the hoisted r*9 (temp 4) or element base (temp 6) with no
 * re-check.  Per a001 Q-A the check is a bare `SUB(r, 100000);` statement
 * and every subscript below it is unchecked.
 *
 * Eager vs branch (a001 Q-D): every condition in this routine is a
 * branch/skip shape — the x==0 test, the type test, and the final 4-term
 * gate (a skip chain to one stub, 70176267..75) — so the C uses `&&`.
 *
 * CVWN (a001 Q-C): three CVWNs — after `type/100` (16-bit PL/I precision of
 * a FIXED BIN(15) / constant) and before each 16-bit store through *x/*y.
 * The C is plain; the narrowing is the lowering's.
 *
 * NOT reproduced: the hoisted r*9 and element-base temps; the temp-slot
 * reuse (4/6/8 serve as lo/hi, r*9, base, lo/hi again).
 *
 * What lower_c.py refuses today: the three ?RANDOM_NUMBER calls (value in
 * ac0), TMP(), `&` of a field.  The loop, the REGION accesses, SUB, `/`, the
 * 16-bit stores through parameters and the 4-term `&&` are in the subset.
 *
 * CONFIDENCE: verified (matched 64/64 under the old line, P35) — this
 * re-derivation is blind x2 and independent of that match.
 */
#include "quest_rt.h"
#include "declarations.h"

void PICK_X_Y(int16_t *x, int16_t *y)
{
    int32_t r;                                      /* slot 2: the region drawn */

    for (;;) {
        r = RANDOM_NUMBER$3(TMP(1), TMP(OBJ_PTR->region_count), &SD_PTR->seed);
        SUB(r, 100000);                             /* checked once; unchecked below */
        if (REGION[r].x == 0)
            continue;
        if (REGION[r].type / 100 + 1 != 3)          /* CVWN after the divide */
            continue;
        *x = RANDOM_NUMBER$3(TMP(REGION[r].x - 20), TMP(REGION[r].x + 20), &SD_PTR->seed);
        *y = RANDOM_NUMBER$3(TMP(REGION[r].y - 20), TMP(REGION[r].y + 20), &SD_PTR->seed);
        if (*x > 0x3BF5 && *x <= 0x3FAC && *y > 0x3B73 && *y <= 0x3FDE)
            return;
    }
}
