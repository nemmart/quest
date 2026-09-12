/* FAKE_OCEAN @701699B5 — argc 1, frame 0x06 (12 local words), WSAVS.
 *
 * Written for Project 51 from Disassembled/quest.dis 701699b5..70169b0d.
 * No earlier C exists.  Two blind readings (q001, q002) agree.
 *
 * What it does: for player *who at (x, y), every screen cell whose world
 * position lies OUTSIDE the world rectangle (x in (15349, 16300],
 * y in (15219, 16350] — F18's four constants) and is still empty gets 11
 * (ocean).  The 9 columns are world x-5+i, the 11 rows world y-6+j
 * (INIT_SCREEN / UPDATE_SCREENS's mapping), so the columns off the left
 * edge are i <= 15350 - (x-4), off the right edge i >= 10 - (x+4-16300),
 * and likewise rows with 15220 / 12 / 16350.
 *
 * Frame (slots): 2 x (16), 3 y (16), 4 i (16), 5 j (16), 6 lo (16),
 * 7 hi (16), 8-9 temp (who*686 wide, then the DO limit narrow),
 * 10-11 temp (i*22 or 2*j, hoisted).  10 of the 12 words are accounted
 * for; 12-13 are never touched.
 *
 * The listing, by statement:
 *   701699b7 SUB(*who,10); temp8 := who*686; x := PLAYER[who].fm589; y := .fm588 (base reused)
 *   701699d3 x+4; WSLEI 0,16300; WBR L        if x+4 > 16300 goto L      \ short-circuit
 *   701699d9 x-4; WSGTI 0,15349; WBR L        if x-4 <= 15349 goto L     / `||`, branch shape
 *   701699df XJMP 70169A6F                    else skip to the y half
 *   701699e1 L: x-4; WSLEI 0,15349; WBR else  if x-4 <= 15349 (branch)
 *   701699e7   lo := 1; hi := MIN(15350 - (x-4), 9)        (NSBI/NSUB 16-bit; WSGE 0,2; WMOV 0,2)
 *   701699f8 else: lo := MAX(1, 10 - (x+4-16300)); hi := 9 (NADI/NADDI/NSUB; WSGE 0,2; WMOV 2,0)
 *   70169a0a temp8 := hi; i := lo; if !(lo <= hi) goto 70169A6F        DO i = lo TO hi
 *   70169a16 XNDO 0,86,[i]  (limit reloaded from temp8, P4)  exit -> 70169A6F
 *   70169a1b SUB(i,9); temp10 := i*22; j := 1                          DO j = 1 TO 11
 *   70169a2a XNDO 1,65,[j]  exit -> 70169A6E (-> 70169A16)
 *   70169a2f SUB(*who,10); SUB(j,11); screen[i][j]; WSEQ 1,1; WBR step  if != 0 continue
 *   70169a4a SUB(*who,10); SUB(i,9); SUB(j,11); screen[i][j] := 11      (ALL re-checked)
 *   70169a6f y+5; WSLEI 0,16350; WBR M; y-5; WSGTI 0,15219; WBR M; XJMP WRTN
 *   70169a7f M: y-5; WSLEI 0,15219; WBR else
 *   70169a86   lo := 1; hi := MIN(15220 - (y-5), 11)
 *   70169a98 else: lo := MAX(1, 12 - (y+5-16350)); hi := 11
 *   70169aab temp8 := hi; j := lo; if !(lo <= hi) return               DO j = lo TO hi
 *   70169ab6 XNDO 1,85,[j]  exit -> 70169B0E (WRTN)
 *   70169abb SUB(j,11); temp10 := 2*j; i := 1                          DO i = 1 TO 9
 *   70169ac7 XNDO 0,67,[i]  exit -> 70169B0D (-> 70169AB6)
 *   70169acc SUB(*who,10); SUB(i,9); screen[i][j]; WSEQ; WBR step      if != 0 continue
 *   70169ae9 SUB(*who,10); SUB(i,9); SUB(j,11); screen[i][j] := 11
 *   70169b0e WRTN
 *
 * The checks that constrain the reading:
 *  - the four edge constants are PICK_X_Y's spawn gates (F18), and the clip
 *    origins 15350 / 15220 are those gates + 1: the same world rectangle
 *    from an independent site, and 0x3B73's second witness.
 *  - the clip arithmetic is the inverse of INIT_SCREEN's column mapping:
 *    column i is world x-5+i, so "world x <= 15349" is "i <= 15350-(x-4)"
 *    and "world x > 16300" is "i >= 10-(x+4-16300)"; rows with 6 / 12.  A
 *    wrong sign or off-by-one anywhere breaks the pairing with the 9/11
 *    bounds.
 *  - lo/hi are the same two slots (6, 7) in both halves and feed both DO
 *    heads: named locals, not per-statement temps; the DO limit is copied
 *    to temp 8 each time (P4: reloaded at every step).
 *  - x-half loops columns outside then all rows; y-half loops rows outside
 *    then all columns — the nest order follows the hoisted temp (i*22 in
 *    the first, 2*j in the second).
 *
 * MIN / MAX: the `WSGE a,b; WMOV a,b` diamond, four sites (701699F3,
 * 70169A02, 70169A93, 70169AA3); PL/I builtins like ABS.  Operand order in
 * the C follows the order the machine computes them.  The arithmetic inside
 * them is 16-bit (N-ops: FIXED BIN(15) precision); the C is plain int16.
 *
 * SUB() placement (a001 Q-A): each half checks the OUTER counter once per
 * outer iteration (i at 70169A1B, j at 70169ABB, where the row/column term
 * is hoisted) -> a bare SUB statement at the outer head, unchecked in the
 * load.  The inner counter and *who are checked in the load; the STORE
 * re-checks all three (who, i, j — the compiler did not reuse the base
 * here, unlike INIT_SCREEN), so the store carries all three SUBs.
 *
 * Eager vs branch (a001 Q-D): all conditions are branch shapes — the two
 * `||` gates (701699D3..DF, 70169A6F..7D) are skip chains to one stub, the
 * inner `if lo <= hi` and the cell tests are skips.  No materialised
 * booleans.  Contrast FAKE_LAND_MASS, whose `|`s are materialised.
 *
 * DO limit (a001 R2): `n = hi;` then `i <= n`, as UPDATE_SCREENS.
 *
 * NOT reproduced: temp 8/10 reuse, the who*686 hoist, the 16-bit N-ops.
 *
 * What lower_c.py refuses today: MIN, MAX, and `SUB` in statement position.
 *
 * CONFIDENCE: derived (blind x2) — F18 constants from an independent site,
 * the clip/column inverse relation, frame accounting (10 of 12); not run.
 */
#include "quest_rt.h"
#include "declarations.h"

void FAKE_OCEAN(const int16_t *who)
{
    int16_t x, y;                                   /* slots 2, 3 */
    int16_t i, j;                                   /* slots 4, 5 */
    int16_t lo, hi;                                 /* slots 6, 7 */
    int16_t n;                                      /* the DO limit (temp slot 8) */

    x = PLAYER[SUB(*who, 10)].fm589;
    y = PLAYER[*who].fm588;                         /* base reused: no check */

    if (x + 4 > 16300 || x - 4 <= 15349) {          /* branch shape */
        if (x - 4 <= 15349) {
            lo = 1;
            hi = MIN(15350 - (x - 4), 9);
        } else {
            lo = MAX(1, 10 - (x + 4 - 16300));
            hi = 9;
        }
        n = hi;
        for (i = lo; i <= n; i++) {
            SUB(i, 9);                              /* once per outer iteration */
            for (j = 1; j <= 11; j++) {
                if (PLAYER[SUB(*who, 10)].screen[i][SUB(j, 11)] != 0)
                    continue;
                PLAYER[SUB(*who, 10)].screen[SUB(i, 9)][SUB(j, 11)] = 11;
            }
        }
    }

    if (y + 5 > 16350 || y - 5 <= 15219) {          /* branch shape */
        if (y - 5 <= 15219) {
            lo = 1;
            hi = MIN(15220 - (y - 5), 11);
        } else {
            lo = MAX(1, 12 - (y + 5 - 16350));
            hi = 11;
        }
        n = hi;
        for (j = lo; j <= n; j++) {
            SUB(j, 11);                             /* once per outer iteration */
            for (i = 1; i <= 9; i++) {
                if (PLAYER[SUB(*who, 10)].screen[SUB(i, 9)][j] != 0)
                    continue;
                PLAYER[SUB(*who, 10)].screen[SUB(i, 9)][SUB(j, 11)] = 11;
            }
        }
    }
}
