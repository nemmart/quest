/* GET_INPUT @7016AA35 — argc 1, frame 0x29 (41 wide words = 82 slots), WSAVS.
 * STAGED, NOT COMPLETE: the translator refuses at the BITS() argument.
 * See docs/Project37/GetInputFinding.md for the one thing not yet derived.
 *
 * Project 37, routine 2.  The plan gate already recorded that this routine is
 * NOT construct-free (the prompt's premise for the "cheap generality check"
 * was wrong); it needs byte pointers, a CHAR local, a PL/I BIT literal
 * materialised by X.CB, and a byte-wise AND.
 *
 * DERIVED — X.CB @7017E708 converts a CHARACTER literal to a BIT string, and
 * both of its two direct game call sites agree on the convention:
 *     ac2 = the destination's WORD address (a frame temp)
 *     ac0 = a byte pointer to the character form of the literal
 *     ac1 = its length
 *     then an embedded, undecorated `LCALL [0x7017E708],0`
 *   GET_INPUT 7016AA41: 3 bytes "001" at 0x7016A9B9  ->  '001'B
 *   .......... 701703A6: 1 byte  "1"   at 0x7017024D  ->  '1'B
 *   The 1986 compiler does NOT constant-fold a bit literal: it rebuilds it at
 *   run time at every evaluation.
 *
 * DERIVED — PL/I CHARACTER is an UNSIGNED byte: the book loads it with WLDB
 * (`ac2 = M8[ac0]`, zero-extending) and compares it `>s 128`, which is only
 * meaningful for an unsigned datum.  The native C99/C++17 check caught the
 * signed spelling as "comparison is always false"; the subset uses
 * `unsigned char`.
 *
 * DERIVED — R2a: an array local takes as many words as it needs, rounded up to
 * a whole even slot.  `char buf[144]` occupies slots 4..75, which puts the
 * frame's first temp at 76, where the book puts it (XLEF 2,[ac3+0x4C]).
 *
 * NOT DERIVED — the order in which this call's temps are ALLOCATED.  The book
 * gives slot 76 to argument 6 (the BIT literal) and slot 78 to argument 2 (the
 * buffer's byte-pointer dummy), yet EMITS the slot-78 store first and the X.CB
 * call second.  Neither "left to right" (R18) nor "call-materialised arguments
 * first" explains the allocation order and the emission order together.  One
 * routine cannot settle it, so this is left REFUSING rather than fitted.
 */
#include "declarations.h"

void GET_INPUT(unsigned char *c)
{
    int16_t n;
    unsigned char buf[144];
    READ$6(&IN_CHAN, buf, TMP(1), &n, TMP(0x1000), BITS("001"));
    *c = buf[1];
    if (*c > 128) *c = (*c - 128) & 0xFF;
}
