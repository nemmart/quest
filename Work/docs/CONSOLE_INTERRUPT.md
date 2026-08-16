# Quest Console-Interrupt (Move-Abort) Feature

Documents the console-interrupt handling that lets a player abort a
multi-step movement command with ctrl-C / ctrl-A.  This closes the two
runtime routines (`?CREATE_TASK`, `?AWAIT_CONSOLE_INTERRUPT`) that were
not covered by `ERROR_PROCESSING.md` or `ON_ERROR_CATALOG.md`.

## Feature

Quest supports a "move N/S/E/W `n` times" command that walks the player
`n` squares in one direction.  Pressing the console interrupt key
(ctrl-C / ctrl-A) aborts the remaining moves immediately.

The mechanism is a dedicated background **task** that blocks on the
console interrupt and zeroes the player's remaining-move counter when
it fires.  The movement loop decrements that same counter each step and
stops when it reaches zero -- so an interrupt ends the walk cleanly on
the next step boundary.

## The shared counter: `[player_record + 0x7F55]`

Per-player field in the shared player record.  Record base is computed
as `686 * player_index + [0x70000210]`, where `player_index` is the
global at `[0x70000216]` (asserted `<= 10`, else `DERR 17`).

The field is a **remaining-moves counter**, not a boolean:

| Site | Function | Access | Meaning |
|---|---|---|---|
| 0x70178968 | START_TURN | `XNSTA 0` | clear before setting up a walk |
| 0x70178978 | START_TURN | `XNLDA 2` | read remaining count |
| 0x7017897D | START_TURN | `XNSBI 1` | decrement per move |
| 0x7017898B | START_TURN | `XNLDA 2` | re-read to test for zero |
| 0x701789C8 | START_TURN | `XNSTA 0` | clear at end of walk |
| 0x7015FBD3 | AUTO_MOVE  | `XNSTA 0` | initialize counter |
| 0x70165728 | C_A_LISTENER | `XNSTA 1(=0)` | **abort: zero the counter** |

When C_A_LISTENER writes 0, START_TURN's next read sees zero and the
walk terminates.

## Task creation (in QUEST main, 0x7015C390)

```
WLDAI 0x70165713        ; task body = C_A_LISTENER entry
XWSTA 0,[ac3+0xE]        ; arg: entry address
WSUB 0,0
XWSTA 0,[ac3+0x10]       ; arg: (zero)
NLDAI 100,1
XWSTA 1,[ac3+0x14]       ; arg: priority/id = 100 (0x64)
XPEF [ac3+0x14]
XPEF [ac3+0xE]
LCALL [0x7017DBB3],2     ; ?CREATE_TASK(entry, 100)
```

QUEST spawns C_A_LISTENER once at startup, right after INIT_SCREEN.

## C_A_LISTENER (0x70165713)

Task body -- an infinite listen/abort loop:

```
70165713  WSAVS 0x0001
70165715  LCALL ?AWAIT_CONSOLE_INTERRUPT   ; block until ctrl-C / ctrl-A
70165719  LNLDA 0,[0x70000216]             ; player_index
7016571C  WSGTI 0,10 / WSGT / DERR 17      ; assert index <= 10
70165720  NLDAI 686,1 / WMUL               ; 686 * index
70165723  LWADD 0,[0x70000210]             ; + base = player record
70165726  WMOV 0,2                          ; ac2 = record
70165727  WSUB 1,1                          ; ac1 = 0
70165728  XNSTA 1,[ac2+0x7F55]              ; remaining-moves = 0  (ABORT)
7016572A  WBR -21 (0x70165715)             ; loop: wait for next interrupt
```

## Runtime routines (contract for the port)

| Routine | Address | Behavior |
|---|---|---|
| ?CREATE_TASK | 0x7017DBB3 | Spawn a concurrent task: `(entry_address, priority)`. Returns; the new task runs the body independently. |
| ?AWAIT_CONSOLE_INTERRUPT | 0x7017DB4B | Block the calling task until the user presses the console interrupt key (ctrl-C / ctrl-A). Returns when it fires. |

## C++ Translation Implications

The whole feature is one small concurrency pattern:

1. `?CREATE_TASK(C_A_LISTENER, 100)` -> spawn a watcher thread, or
   install a SIGINT handler.
2. `?AWAIT_CONSOLE_INTERRUPT` -> block on a SIGINT/condition variable.
3. On interrupt -> set the current player's `remaining_moves = 0`.
4. The multi-move loop in START_TURN checks `remaining_moves` each step
   and stops at zero -- no change needed beyond making the counter
   visible to the handler (shared state / atomic).

No busy-waiting or polling is involved: the listener blocks in the OS
until the interrupt, then makes a single store and blocks again.

## Revision History

- **This session**: Documented the console-interrupt move-abort feature,
  closing the ?CREATE_TASK / ?AWAIT_CONSOLE_INTERRUPT coverage gap.
  Confirmed the `[player_record + 0x7F55]` counter is written by
  C_A_LISTENER (abort) and decremented by START_TURN (per-move),
  initialized by AUTO_MOVE.