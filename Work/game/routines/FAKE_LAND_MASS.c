/* FAKE_LAND_MASS @701697A1 — argc 1, frame 0x0C (24 local words), WSAVS.
 *
 * Written for Project 51 from Disassembled/quest.dis 701697a1..70169939,
 * plus CREATE_MAP 701744CA..701744EC for the record layout.  No earlier C
 * exists.  Two blind readings (q001, q002) agree, down to the table's field
 * offsets and bound.
 *
 * What it does: for every land mass m (a named rectangle with corners
 * (x1,y1)-(x2,y2) and a terrain code `cell`), walked from the last to the
 * first, if the player's 9x11 view (world x-4..x+4, y-5..y+5) overlaps the
 * rectangle, write `cell` into every empty screen cell of the overlap.
 *
 * ---- THE LANDMASS TABLE (a001 Q-E) ---------------------------------------
 * Not in game/declarations.json.  Declared below for stage 2 to move into
 * gen_declarations.py.  Evidence, per field:
 *   base OBJ_PTR (LWLDA 2,[0x70000212] before every access), stride 22
 *   (NLDAI 22; WMUL at 701697E1, 7016980C, 70169901, 701744D4 ...), bound
 *   1000 (the DERR 17 at 701697DD/7016980A/701698FD and CREATE_MAP 701744D0).
 *   count     K 1035307 (0xFCC2B) direct, 16-bit: 701697C5 loads it as the
 *             first m; CREATE_MAP 701744CD checks it against 1000.  The word
 *             before element 1 — the same idiom as REGION's count at 11502.
 *   cell      K 1035286 (0xFCC16) +0, 32-bit: 7016992B LWLDA, stored into a
 *             screen cell (32-bit) at 70169930.  Name from its use.
 *   name      K 1035288 (0xFCC18) +2 .. +17, CHAR(30) VARYING: CREATE_MAP
 *             701744DA..EC stores MIN(len,30) at 0xFCC18 and WCMVs bytes to
 *             byte address 2*0xFCC19 = word +3.  1 + 15 words.  Name from
 *             its use (a title string).
 *   x1 y1 x2 y2  K 1035304..1035307 (0xFCC28..2B) +18..+21, 16-bit: compared
 *             with x-4/x+4 (x1, x2) and y-5/y+5 (y1, y2); x1/y1 are the DO
 *             lower bounds, x2/y2 the upper.  Names from their use.
 *   2 + 16 + 4 = 22 = the stride: the layout closes.
 *   origin base+1035308 if `cell` is field 0 (1-based m), minK 1035286.
 * JSON for declarations.json is in docs/Project51/REPORT.md.
 * --------------------------------------------------------------------------
 *
 * Frame (slots): 2 x (16), 3 y (16), 4 i (16), 5 j (16), 6 m (16),
 * 8-9 temp (who*686 wide; then x-4 wide), 10-11 temp (m*22 wide),
 * 12..25 temps of mixed width (element pointers, x-4/y-5 copies, booleans,
 * the two DO limits, the hoisted 22*i, the cell pointer).  24 = 2 * 0x0C.
 *
 * The listing, by statement:
 *   701697a3 SUB(*who,10); temp8 := who*686; x := PLAYER[who].fm589; y := .fm588
 *   701697bf temp8 := x-4 (wide); m := LANDMASS_COUNT; if m < 1 return   DO m = count TO 1 BY -1
 *   701697d4 step: m := m-1 (NSBI 1); if m < 1 return                  (hand-built descending step)
 *   701697dd SUB(m,1000); temp10 := m*22   (checked ONCE per m)
 *   701697e9 b0 := (x-4 <= x1); b2 := (x-4 <= x2); b0|b2; MOV.L# SZC   materialised `|`
 *              if !( x-4 <= x1 | x-4 <= x2 ) next m
 *   70169803 b := (x+4 >= x1) [temp14]; | (x+4 >= x2); element ptrs to temps 12/16
 *              if !( x+4 >= x1 | x+4 >= x2 ) next m
 *   70169832 (y-5 <= y1)|(y-5 <= y2) -> temp22; (y+5 >= y1)|(y+5 >= y2); WAND; SZC
 *              if !( (y-5 <= y1 | y-5 <= y2) & (y+5 >= y1 | y+5 >= y2) ) next m
 *   7016987a temp14 := MIN(x2 - (x-4) + 1, 9)                           DO i = .. TO that
 *   7016988e temp16 := x-4; i := MAX(x1 - (x-4), 0) + 1; if i > temp14 next m
 *   701698a8 XNDO 1,138,[i]  exit -> 70169935 (next m)
 *   701698ad SUB(i,9); 22*i (hoisted to temp20 at 701698D6)
 *   701698b4 temp16 := MIN(y2 - (y-5) + 1, 11)                          DO j = .. TO that
 *   701698c5 j := MAX(y1 - (y-5), 0) + 1; if j > temp16 next i
 *   701698db XNDO 0,85,[j]  exit -> 70169933 (next i)
 *   701698e0 SUB(*who,10); SUB(j,11); screen[i][j]; WSEQ 0,0; WBR step   if != 0 continue
 *   701698fb SUB(m,1000); SUB(*who,10); SUB(i,9); SUB(j,11)              (ALL FOUR re-checked)
 *   7016992b screen[i][j] := LANDMASS[m].cell (32-bit)
 *   70169935 next m: XJMP 701697D4;  70169939 WRTN
 *
 * The checks that constrain the reading:
 *  - the record layout sums to the stride (2 + 16 + 4 = 22) using a second
 *    routine's writer (CREATE_MAP) for the middle 16 words this routine never
 *    touches.
 *  - the clip formulas are INIT_SCREEN/FAKE_OCEAN's column mapping again:
 *    column i is world x-5+i, so world x1..x2 is i = x1-x+5 .. x2-x+5, which
 *    is MAX(x1-(x-4),0)+1 .. MIN(x2-(x-4)+1, 9) exactly; rows with 6 / 11.
 *  - the overlap test is `[x-4,x+4] meets [min(x1,x2),max(x1,x2)]` written
 *    corner-agnostically; with the fill loop using x1 as low and x2 as high,
 *    the data must have x1 <= x2, and the `|` forms are then just the
 *    programmer's spelling.  Both halves of the test agree with the loop.
 *  - the descending loop's two exits (count < 1; m-1 < 1) both go to WRTN,
 *    and every "next m" lands on the decrement at 701697D4.
 *
 * SUB() placement (a001 Q-A): m is checked once per m iteration (701697DD,
 * before the hoisted m*22) -> bare `SUB(m, 1000);` at the m loop head; i once
 * per i iteration (701698AD, hoisted 22*i) -> bare `SUB(i, 9);`.  The load
 * checks *who and j inline.  The STORE re-checks all four (m, who, i, j),
 * so the store carries all four SUBs.
 *
 * Eager vs branch (a001 Q-D) — the routine that MATERIALISES: all three
 * overlap conditions are `WADC r,r; W<cmp>; WSUB r,r` booleans combined
 * with WIOR/WAND and tested with MOV.L# SZC (701697EF..FF, 70169819..2E,
 * 70169840..76), so the C writes `|` and `&` on comparison results.  The DO
 * entry tests and the cell test are branches.  This is the site stage 3's
 * eager_bool distinction (P48 F5) must reproduce; FAKE_OCEAN's `||`s are the
 * other shape.
 *
 * MIN / MAX: four diamonds (7016988A, 7016989A [MAX against 0 via
 * WSGE r,r], 701698C1, 701698CF); the "+ 1" is applied after the MAX/MIN
 * (NADI 1 / WINC), so the C keeps that order.
 *
 * DO limits (a001 R2): `n_i = MIN(..); for (i = MAX(..)+1; i <= n_i; ..)`,
 * limit first then the initial value, as the machine orders them.
 *
 * NOT reproduced: the element-pointer temps (12/16/22), the hoisted m*22 and
 * 22*i, the x-4 / y-5 copies in three widths, the LDAFP spills.
 *
 * What lower_c.py refuses today: the LANDMASS table (not declared), MIN,
 * MAX, `SUB` in statement position.  The descending `for` with `--`, the
 * `|`/`&` on comparisons and the nested computed-bound loops are in the
 * subset.
 *
 * CONFIDENCE: derived (blind x2) — stride/layout sum via a second routine,
 * the shared column mapping; the field NAMES rest on use only.  Not run.
 */
#include "quest_rt.h"
#include "declarations.h"

/* ---- LANDMASS: P51 finding, not yet in declarations.json (see header) ---- */
struct landmass_rec {
    int32_t     cell;       /* K=1035286: terrain code copied into screen cells */
    VARYING(30) name;       /* K=1035288: length word, data from word +3 (CREATE_MAP) */
    int16_t     x1;         /* K=1035304 */
    int16_t     y1;         /* K=1035305 */
    int16_t     x2;         /* K=1035306 */
    int16_t     y2;         /* K=1035307 */
};
extern ARRAY1(struct landmass_rec, 1000) LANDMASS;  /* base OBJ_PTR, stride 22, origin base+1035308 */
extern int16_t LANDMASS_COUNT;                      /* OBJ_PTR->landmass_count, K=1035307 */
/* ------------------------------------------------------------------------- */

void FAKE_LAND_MASS(const int16_t *who)
{
    int16_t x, y;                                   /* slots 2, 3 */
    int16_t i, j;                                   /* slots 4, 5 */
    int16_t m;                                      /* slot 6 */
    int16_t n_i, n_j;                               /* the two DO limits (temps 14, 16) */

    x = PLAYER[SUB(*who, 10)].fm589;
    y = PLAYER[*who].fm588;                         /* base reused: no check */

    for (m = LANDMASS_COUNT; m >= 1; m--) {
        SUB(m, 1000);                               /* checked once per m */
        if (!((x - 4 <= LANDMASS[m].x1) | (x - 4 <= LANDMASS[m].x2)))          /* materialised */
            continue;
        if (!((x + 4 >= LANDMASS[m].x1) | (x + 4 >= LANDMASS[m].x2)))          /* materialised */
            continue;
        if (!(((y - 5 <= LANDMASS[m].y1) | (y - 5 <= LANDMASS[m].y2))
              & ((y + 5 >= LANDMASS[m].y1) | (y + 5 >= LANDMASS[m].y2))))      /* materialised */
            continue;

        n_i = MIN(LANDMASS[m].x2 - (x - 4) + 1, 9);
        for (i = MAX(LANDMASS[m].x1 - (x - 4), 0) + 1; i <= n_i; i++) {
            SUB(i, 9);                              /* checked once per i */
            n_j = MIN(LANDMASS[m].y2 - (y - 5) + 1, 11);
            for (j = MAX(LANDMASS[m].y1 - (y - 5), 0) + 1; j <= n_j; j++) {
                if (PLAYER[SUB(*who, 10)].screen[i][SUB(j, 11)] != 0)
                    continue;
                PLAYER[SUB(*who, 10)].screen[SUB(i, 9)][SUB(j, 11)] = LANDMASS[SUB(m, 1000)].cell;
            }
        }
    }
}
