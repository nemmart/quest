/* Byte storage and byte addressing at their boundaries (P53).
 *
 * The generator reaches bytes in class D, but only by luck at the places that
 * matter.  What this case exists to pin down is the ONE bug the byte work can
 * introduce that a self-consistent compiler would hide: a wrong byte ADDRESS.
 * `bp(base, d)` scales the base to bytes and takes `d` ALREADY IN BYTES
 * (IR.md §5.2's recorded asymmetry), so applying the word element-scaling to a
 * byte subscript gives an index that is wrong by a factor of two — and a store
 * and a load that BOTH use it agree with each other while disagreeing with
 * gcc.  Hence: every write here is read back at a DIFFERENT index from the one
 * it was written at, and odd and even indices are both live.
 *
 * Also pinned: the byte at each end of a word pair (0/1, 14/15), truncation on
 * store (0x1FF -> 0xFF), unsigned promotion (200 + 100 is 300 as an int, not
 * 44), and the 8-bit narrowing GET_INPUT needs — `(unsigned char)(c - 128)`
 * for c in 128..255, which is P51 §2 item 5's `WANDI 255`.
 */
#include "quest_rt.h"

unsigned char b[16];
unsigned char c;
int32_t sum, hi, lo, wrapped, promoted, narrowed;
int32_t i;   /* file scope: the rig dumps every local `v`, so the harness must print it too */
uint32_t chk;

void t(void)
{
    chk = 0;
    for (i = 0; i < 16; i++)
        b[i] = (unsigned char)(i * 17);          /* 0, 17, 34 .. 255 */

    /* read back SHIFTED: index k holds what index k-1 wrote, so an address
     * that is uniformly off by one or scaled by two cannot agree with gcc */
    lo = b[0];                                   /* first byte of the pair */
    hi = b[15];                                  /* last byte of the last pair */
    sum = 0;
    for (i = 0; i < 16; i++) {
        sum = sum + b[i];
        chk = chk * 1000003u + (uint32_t)b[i];
    }

    /* odd and even both live, and adjacent: a half-word confusion moves one
     * of these onto the other */
    b[4] = 200;
    b[5] = 100;
    promoted = b[4] + b[5];                      /* 300: promotion to int */
    b[6] = (unsigned char)(b[4] + b[5]);         /* 300 & 255 = 44 */
    wrapped = b[6];

    /* truncation on store, with and without the cast */
    b[7] = 0x1FF;
    b[8] = (unsigned char)0x1FF;
    chk = chk * 1000003u + (uint32_t)(b[7] + b[8]);

    /* the GET_INPUT narrowing: c > 128 strips the parity bit */
    c = 200;
    if (c > 128)
        c = (unsigned char)(c - 128);
    narrowed = c;                                /* 72 */

    /* a byte used as a subscript of a byte array */
    b[1] = 9;
    c = b[b[1]];                                 /* b[9] = 153 */
    chk = chk * 1000003u + (uint32_t)c;
}
