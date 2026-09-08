/* REFRESH_SCREEN @70176A93 (argc mixed 0/1, frame 0x17) — redraw the
 * viewport frame: optionally clear the screen (when called with no
 * argument, or with a negative flag), then the top border (row 0), the
 * bottom border (row 10) and the nine side-walled rows between.
 * Two-arity routine: the leading `int arg_count` is the frame marker word. */
#include "quest_rt.h"
#include "declarations.h"

void REFRESH_SCREEN(int arg_count, const int16_t *flag)
{
    int16_t i;
    if (arg_count == 0) {
        WRITE_SCREEN$2(&OUT_CHAN, TMP(0x00010C00));
    } else if (*flag < 0) {
        WRITE_SCREEN$2(&OUT_CHAN, TMP(0x00010C00));
    }
    WRITE_SCREEN$5(&OUT_CHAN, "______________________", TMP(0), TMP(2), TMP(2048));
    WRITE_SCREEN$5(&OUT_CHAN, "----------------------", TMP(10), TMP(2), TMP(2048));
    for (i = 1; i <= 9; i++)
        WRITE_SCREEN$5(&OUT_CHAN, CAT(CAT("|", "                      "), "|"), &i, TMP(1), TMP(2048));
}
