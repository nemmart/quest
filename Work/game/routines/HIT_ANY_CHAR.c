/* HIT_ANY_CHAR @7016DE91 (argc 0, frame 0x09, WSAVS) — prompt the operator
 * and wait for a keystroke, then clear the prompt off the line.
 *
X for R24/R33 (the lazy LDAFP now runs
 * through the frame's register state rather than a pinned ac3).
 *
 * Frame: `c` is the declared local at slot 2 (R1/R2 — a one-byte local takes a
 * whole slot), so `&c` is the byte address `bp(ac3, 4)`.  The prompt's VARYING
 * dummy is a string temp (R3b) and lands at the high-water mark above it,
 * slot 4, with its data bytes from `bp(ac3, 10)` — which is what the book
 * does (`XNSTA 0,[ac3+0x4]`, `XLEFB 2,[ac3+0xA]`).
 *
 * The final write is the two-character sequence CR VT packed into one 32-bit
 * immediate (length word 2, bytes 0x0D 0x0B): a CHAR VARYING whose length word
 * and data together fit in a wide is stored by one WLDAI + XWSTA rather than a
 * WCMV.  REFRESH_SCREEN already spells that `TMP(0x00010C00)`; this is the
 * same construct with a two-byte payload, so it is not a new rule.
 */
#include "quest_rt.h"
#include "declarations.h"

/* The game->game callee.  GET_INPUT has one arity, so `GET_INPUT$1` is
 * GET_INPUT; natively it is the same non-variadic function (compiler/README,
 * "runtime calls are arity-in-name").  Declared here because no routine had
 * called another game routine before this one. */
void GET_INPUT$1(unsigned char *c);

void HIT_ANY_CHAR()
{
    unsigned char c;
    WRITE_SCREEN$2(&OUT_CHAN, "\x0BHit any character to continue");
    GET_INPUT$1(&c);
    WRITE_SCREEN$2(&OUT_CHAN, TMP(0x00020D0B));
}

/* ABANDONED AT 8/10 (2 DIFF, both from one register choice), P38.
 * Not staged and not partial: this translates completely and the slot
 * bijection is the identity (b4->4, w4->4).  The only divergence is
 * `ac1 = 0x00020D0B` in the book against `ac0` here, which needs a rule for
 * the caller's register state after a game->game call.  The evidence is three
 * call sites of one callee and the alternative reading has no mechanism, so
 * no rule was recorded: see CODEGEN_RULES.md §9.5.
 */
