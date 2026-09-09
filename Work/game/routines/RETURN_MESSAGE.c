/* RETURN_MESSAGE @70176FDD — argc 6, `mixed:3/6`, frame 0x03, WSAVS.
 * STAGED, NOT COMPLETE: no match number is quoted for it.
 * See docs/Project37/ReturnMessageFinding.md.
 *
 * Project 37, routine 3.  Like GET_INPUT, it is NOT construct-free — the plan
 * gate's correction to the prompt holds for all three "tiny" routines.
 *
 * What it is: the game's fatal-error exit.  docs/ERROR_PROCESSING.md names it
 * as the terminal path for LOGON and INIT_SHARED_DATA failures, and the tail
 * is `SYSCALL 0310` = ?RETURN, the AOS/VS process-exit call, which never
 * returns (docs/HeapSignalPlan.md §32, emulation/os/OSTask.cpp:145).
 *
 * The optional message is argument 3, a CHAR VARYING by reference.  When it is
 * absent the routine substitutes the literal at 0x70000CCD, which reads
 * "Unexpected error" — exactly the 16 bytes the default length constant says
 * (`NLDAI 16`).  Recovering the string and the constant independently and
 * having them agree is what confirms the reading.
 */
#include "declarations.h"

void RETURN_MESSAGE(const int32_t *severity, const int32_t *code, const void *msg)
{
    int32_t len;
    void *text;
    int32_t packed;

    if (msg == NULL) { text = "Unexpected error"; len = 16; }
    else { text = DATA(msg); len = LEN(msg); }
    packed = len | (*code | 0x8000);
    if (*severity != 0) packed = packed | 0x1000;
    RETURN$2(text, packed);
}
