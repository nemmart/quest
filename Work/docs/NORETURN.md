# Noreturn calls in QUEST — RETURN_MESSAGE / SYSCALL 0310

*Aug 22 2026. How the compiler encoded process-termination, and what
the analysis tooling must record about it.*

## The fact

RETURN_MESSAGE (0x70176FDD) is a normal compiled PL/I GAME routine
(standard WSAVS 0x0003 frame, by-reference args, in the book/census)
whose body ends in **SYSCALL 0310** (process-terminate) with an inline
message-data table and NO WRTN anywhere. The C++ port is an
unconditional terminate_process(message). So it never returns — a
game-level "die with this message" routine (its data table holds combat
death strings: "A mighty blow has been struck to the citadel wall!").

## How the compiler knew (and how it shows in codegen)

The DG PL/I toolchain treated SYSCALL 0310 as noreturn (a
noreturn-equivalent attribute on the declared terminate entry it
compiled against — the runtime declaration files are lost with the
source). Evidence: 0310 is the ONLY syscall of the 7 distinct numbers in
the program that is terminal — 0245/0246/0142/073/044/0232 are all
followed by normal code and their routines have WRTNs. Reachability
analysis then pruned dead code. The effect is **TWO-SIDED**:

- **Callee side:** RETURN_MESSAGE has no epilogue; its message-data
  table is packed where the WRTN would be.
- **Caller side:** the LCALL to RETURN_MESSAGE has NO post-call code —
  no result read, no fall-through, no WRTN on that path. The caller's
  basic block simply ENDS at the LCALL. (Contrast a normal call, e.g.
  FIND_OBJECT at 70161351, immediately followed by
  XWLDA result / WSNE / WBR.)
- **Arg marshalling is atypical too:** RETURN_MESSAGE takes args BY
  REFERENCE, so a caller builds stack temporaries (WPSH r,r / LDASP r,
  repeated) and pushes their ADDRESSES, rather than the flat
  XPEF/XPEF/XPEF of a by-value call. The pushed args are pointers into
  the caller's stack; the referenced temporaries are never popped
  (process death reclaims the stack).

## Call sites (all CLEAN, all censused)

    call RETURN_MESSAGE,6 at 7015BE74
    call RETURN_MESSAGE,3 at 70169B82
    call RETURN_MESSAGE,6 at 70175EC8
    call RETURN_MESSAGE,6 at 70175EFF
    call RETURN_MESSAGE,6 at 7017D7D9

(mixed arity 3/6, per-site — convertible, M4bNotes.)

## What the tooling must record (gap + fix)

The census (quest.callsites) marks these `CLEAN` like any call — it does
NOT flag noreturn. For M4b/M5 that property matters:
- **M4b:** redirecting args to a noreturn callee is the easiest case
  (no return, no frame reuse, no cleanup) but the redirect must not
  expect a post-call continuation to exist.
- **M5:** a call to RETURN_MESSAGE (and a direct SYSCALL 0310) is an
  INVOKE WITH NO NORMAL SUCCESSOR — the caller's block ends there, one
  edge, to process termination. RETURN_MESSAGE is a graph SINK. This is
  the simplest instance of the invoke-style two-successor treatment
  (here: zero normal successors). Structurally detectable: an LCALL
  with no epilogue/continuation on its path is a noreturn call site,
  exactly as the original compiler encoded it.

FIX: ArgWindows/the census should carry a NORETURN marker — on the
RETURN_MESSAGE book entry and on each call site to it (and on a direct
SYSCALL 0310). Cheap: RETURN_MESSAGE is the sole game noreturn routine,
0310 its sole mechanism.
