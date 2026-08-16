# Quest ON ERROR Exception Handler Catalog

All 26 `ON ERROR` handlers in the Quest client binary, documented from
the `O.ON` registrations found in the disassembly.

## Background

### PL/I ON ERROR Mechanism

Quest is compiled from PL/I, which uses structured condition handling.
The relevant runtime routines are:

| Routine | Address | Purpose |
|---|---|---|
| I.PROLOG | 0x7017E733 | Install handler frame (`try {`) |
| I.EPILOG | 0x7017E77D | Remove handler frame and return (`}`) |
| O.ON | 0x7017ED9B | Register condition handler (`catch`) |
| O.REVERT | 0x7017EDCB | Deregister condition handler |
| I.GOTO | 0x7017EC7C | Non-local goto (unwind from handler to enclosing frame) |

### How Errors Propagate

When a library routine (e.g. `?OPEN_FILE`, `?READ_SCREEN`) wraps a
system call that fails, the chain is:

    SYSCALL fails (no-skip return, error code in AC0)
    --> ?LIB_ERROR stores error in task-local area
    --> ?DEFAULT_ERROR_HANDLER (0x7017E3D2)
    --> O?SIGNAL (0x7017EDED) walks the handler chain
    --> Dispatches to nearest ON ERROR handler

### Condition Type

Every handler in Quest registers with `ac0 = -1` (WADC 0,0), meaning
**ON ERROR** -- catch all conditions.  No handler uses a specific
condition type (CONVERSION, SUBSCRIPTRANGE, etc.).

### RETURN_MESSAGE -- Fatal Process Exit

`RETURN_MESSAGE` (0x70176FDD) is **not** a display function -- it
terminates the process.  It builds a termination packet and executes
`SYSCALL 0310` (`?RETURN` in the AOS/VS API), which:

  1. Displays the message to the user's console
  2. Halts all tasks in the process
  3. Terminates the process (never returns)

Any handler that calls `RETURN_MESSAGE` is a fatal exit path.

---

## Handler Categories

### Category A -- Bare GOTO (20 handlers)

The handler body contains no processing; it immediately does I.GOTO
to a resume point in the enclosing function.  The PL/I equivalent is
`ON ERROR GOTO label;`

Assembly pattern:
```asm
WSAVS 0x0000          ; empty handler frame (0 locals)
WMOV  1,0             ; ac0 = enclosing frame pointer
XLEF  2,[resume_addr] ; ac2 = where to jump
LJSR  [I.GOTO]        ; unwind and transfer
```

C++ equivalent: `catch (...) { goto resume; }`

### Category B -- Write Error Message + GOTO (4 handlers)

The handler writes a 31-byte error string to the screen via
`?WRITE_SCREEN(SCREEN_CHAN, msg, 31)` before doing I.GOTO.

Assembly pattern:
```asm
WSAVS 0x0009          ; handler frame with 9 locals
NLDAI 31,0            ; message length
; ... copy string to buffer ...
LCALL ?WRITE_SCREEN   ; display error
XWLDA 0,[ac3+0x7FFA]  ; load enclosing frame pointer
XLEF  2,[resume_addr]  ; resume target
LJSR  [I.GOTO]         ; unwind and transfer
```

C++ equivalent: `catch (...) { write_screen(msg, 31); goto resume; }`

### Category C -- Store Default Value + GOTO (1 handler)

The handler stores a default value to a global variable, then does
I.GOTO to the function epilog.

C++ equivalent: `catch (...) { input_result = 0; }`

### Category D -- RETURN_MESSAGE + WRTN -- Fatal Exit (2 handlers)

The handler calls `RETURN_MESSAGE` to terminate the process with an
error message.  These handlers use `WRTN` (return from handler frame)
rather than `I.GOTO`, but `RETURN_MESSAGE` never returns -- it kills
the process via `SYSCALL 0310`.

C++ equivalent: `catch (...) { fprintf(stderr, "%s\n", msg); exit(1); }`

---

## Complete Handler Inventory

### #1 -- ALLY_PLAYER (handler 1 of 2)

| Field | Value |
|---|---|
| O.ON address | 0x7015D064 |
| Category | B -- Write error message + GOTO |
| Protected operation | File I/O section (open/read player ally data) |
| Handler action | Write 31-byte error message to SCREEN_CHAN |
| Resume target | 0x7015D2E3 (second O.ON registration in same function) |

### #2 -- ALLY_PLAYER (handler 2 of 2)

| Field | Value |
|---|---|
| O.ON address | 0x7015D2E7 |
| Category | A -- Bare GOTO |
| Protected operation | `?CLOSE_FILE` |
| Handler action | None |
| Resume target | 0x7015D2F9 (past ?CLOSE_FILE, at O.REVERT + I.EPILOG) |

Note: If the file close fails, the handler skips over it and exits
normally.  This is a common "ignore close errors" pattern.

### #3 -- ATTACK

| Field | Value |
|---|---|
| O.ON address | 0x7015F727 |
| Category | A -- Bare GOTO |
| Protected operation | `READ_IN` (terminal input) |
| Handler action | None |
| Resume target | 0x7015F70F (earlier in function -- retry/abort point) |

### #4 -- AUTO_MOVE

| Field | Value |
|---|---|
| O.ON address | 0x7015FBA3 |
| Category | A -- Bare GOTO |
| Protected operation | Player record write + `?CHAR_TO_UNSIGNED` + READ_IN |
| Handler action | None |
| Resume target | 0x7015FBDC (further into function body) |

### #5 -- CAST (handler 1 of 2)

| Field | Value |
|---|---|
| O.ON address | 0x7016292C |
| Category | A -- Bare GOTO |
| Protected operation | `READ_IN` (terminal input) |
| Handler action | None |
| Resume target | 0x7016367F (function exit point, shared with handler #6) |

### #6 -- CAST (handler 2 of 2)

| Field | Value |
|---|---|
| O.ON address | 0x70162CD2 |
| Category | A -- Bare GOTO |
| Protected operation | `READ_IN` (terminal input) |
| Handler action | None |
| Resume target | 0x7016367F (function exit point, shared with handler #5) |

### #7 -- CASTLE_INVENTORY

| Field | Value |
|---|---|
| O.ON address | 0x701639B9 |
| Category | A -- Bare GOTO |
| Protected operation | `READ_IN` + `?CHAR_TO_UNSIGNED` (read and convert input) |
| Handler action | None |
| Resume target | 0x701639E1 (past O.REVERT, into result processing) |

### #8 -- DROP

| Field | Value |
|---|---|
| O.ON address | 0x701696F6 |
| Category | A -- Bare GOTO |
| Protected operation | `READ_IN` + `?CHAR_TO_UNSIGNED` (read and convert input) |
| Handler action | None |
| Resume target | 0x70169719 (error recovery: stores 0 to result, reverts, epilog) |

Note: The resume point at 0x70169719 stores 0 to the same output
field, then does O.REVERT + I.EPILOG.  So on error, DROP treats the
input as 0 (no selection) and returns.

### #9 -- HELP

| Field | Value |
|---|---|
| O.ON address | 0x7016D56C |
| Category | A -- Bare GOTO |
| Protected operation | Help text display section (multi-page I/O) |
| Handler action | None |
| Resume target | 0x7016DE01 (function exit point) |

### #10 -- KILL_PLAYER (handler 1 of 3)

| Field | Value |
|---|---|
| O.ON address | 0x7016E2D8 |
| Category | B -- Write error message + GOTO |
| Protected operation | File I/O section (open/read character save data) |
| Handler action | Write 31-byte error message to SCREEN_CHAN |
| Resume target | 0x7016E5AD (third O.ON registration in same function) |

### #11 -- KILL_PLAYER (handler 2 of 3)

| Field | Value |
|---|---|
| O.ON address | 0x7016E313 |
| Category | A -- Bare GOTO |
| Protected operation | `?WRITE_SCREEN` + `READ_IN` section |
| Handler action | None |
| Resume target | 0x7016E5A8 (near function exit) |

Note: This handler is registered after an O.REVERT of handler #10.
It replaces the "write error + resume" handler with a simpler "just
skip to the end" handler for the interactive portion.

### #12 -- KILL_PLAYER (handler 3 of 3)

| Field | Value |
|---|---|
| O.ON address | 0x7016E5B1 |
| Category | A -- Bare GOTO |
| Protected operation | `?CLOSE_FILE` |
| Handler action | None |
| Resume target | 0x7016E5C3 (past ?CLOSE_FILE, at O.REVERT + I.EPILOG) |

Note: Same "ignore close errors" pattern as ALLY_PLAYER #2.

### #13 -- LIST_PLAYERS (handler 1 of 2)

| Field | Value |
|---|---|
| O.ON address | 0x7016EC53 |
| Category | B -- Write error message + GOTO |
| Protected operation | File I/O section (open/read player list data) |
| Handler action | Write 31-byte error message to SCREEN_CHAN |
| Resume target | 0x7016F1C4 (second O.ON registration in same function) |

### #14 -- LIST_PLAYERS (handler 2 of 2)

| Field | Value |
|---|---|
| O.ON address | 0x7016F1CC |
| Category | A -- Bare GOTO |
| Protected operation | `?CLOSE_FILE` |
| Handler action | None |
| Resume target | 0x7016F1DE (past ?CLOSE_FILE, at O.REVERT, then continue) |

Note: Same "ignore close errors" pattern.

### #15 -- OBSERVE

| Field | Value |
|---|---|
| O.ON address | 0x70173374 |
| Category | A -- Bare GOTO |
| Protected operation | `READ_IN` (terminal input) |
| Handler action | None |
| Resume target | 0x70173505 (function exit point) |

### #16 -- OP_EDIT (handler 1 of 6)

| Field | Value |
|---|---|
| O.ON address | 0x701738CB |
| Category | A -- Bare GOTO |
| Protected operation | `READ_IN` (terminal input) |
| Handler action | None |
| Resume target | 0x7017563C (OP_EDIT exit point, shared by handlers 16-18) |

### #17 -- OP_EDIT (handler 2 of 6)

| Field | Value |
|---|---|
| O.ON address | 0x70173957 |
| Category | A -- Bare GOTO |
| Protected operation | `READ_IN` + `?CHAR_TO_UNSIGNED` (read and convert input) |
| Handler action | None |
| Resume target | 0x7017563C (OP_EDIT exit point) |

### #18 -- OP_EDIT (handler 3 of 6)

| Field | Value |
|---|---|
| O.ON address | 0x70173DE0 |
| Category | A -- Bare GOTO |
| Protected operation | `?CHAR_TO_UNSIGNED` + player record write |
| Handler action | None |
| Resume target | 0x7017563C (OP_EDIT exit point) |

### #19 -- OP_EDIT (handler 4 of 6)

| Field | Value |
|---|---|
| O.ON address | 0x70175748 |
| Category | A -- Bare GOTO |
| Protected operation | `READ_IN` (terminal input) |
| Handler action | None |
| Resume target | 0x701757A0 (local exit point within OP_EDIT) |

### #20 -- OP_EDIT (handler 5 of 6)

| Field | Value |
|---|---|
| O.ON address | 0x70175871 |
| Category | A -- Bare GOTO |
| Protected operation | `READ_IN` + `?CHAR_TO_UNSIGNED` |
| Handler action | None |
| Resume target | 0x7017588F (O.REVERT + I.EPILOG -- normal return) |

### #21 -- OP_EDIT (handler 6 of 6)

| Field | Value |
|---|---|
| O.ON address | 0x70175956 |
| Category | A -- Bare GOTO |
| Protected operation | `READ_IN` (terminal input) |
| Handler action | None |
| Resume target | 0x7017597D (past input processing, into result handling) |

### #22 -- LOGON (handler 1 of 2)

| Field | Value |
|---|---|
| O.ON address | 0x70175EAC |
| Category | **D -- Fatal exit (RETURN_MESSAGE)** |
| Protected operation | `?LOOKUP_PORT` (find QUEST_SERVER IPC port) |
| Handler action | Call RETURN_MESSAGE with "Maximum number of players exceeded." |
| Resume target | N/A -- process terminates |

Note: If the server port lookup fails, the client cannot continue.
RETURN_MESSAGE calls `SYSCALL 0310` (`?RETURN`), which terminates
the process and displays the message on the user's console.

### #23 -- LOGON (handler 2 of 2)

| Field | Value |
|---|---|
| O.ON address | 0x70175EDE |
| Category | **D -- Fatal exit (RETURN_MESSAGE)** |
| Protected operation | `?CONNECT` (establish IPC link to QUEST_SERVER) |
| Handler action | Call `?LIB_ERROR_CODE` to retrieve the OS error code, then call RETURN_MESSAGE with formatted error info |
| Resume target | N/A -- process terminates |

Note: Registered after O.REVERT of handler #22.  If the server
connection fails (server not running, IPC error, etc.), the client
terminates with the OS-level error code included in the message.

### #24 -- READ_IN

| Field | Value |
|---|---|
| O.ON address | 0x701766FA |
| Category | **C -- Store default value + GOTO** |
| Protected operation | `?READ_SCREEN` (terminal input via INPUT_CHAN) |
| Handler action | Store 0 to `INPUT_RESULT` at 0x7000021C |
| Resume target | 0x7017671A (I.EPILOG -- normal return) |

Note: The only handler that modifies a variable.  By storing 0 to
INPUT_RESULT, the caller sees "no input" and can handle the error
gracefully (e.g. re-prompting or aborting the current command).

### #25 -- START_TURN

| Field | Value |
|---|---|
| O.ON address | 0x70178DA5 |
| Category | A -- Bare GOTO |
| Protected operation | `READ_IN` (terminal input for turn command) |
| Handler action | None |
| Resume target | 0x70178E5B (further into turn processing) |

### #26 -- STORE

| Field | Value |
|---|---|
| O.ON address | 0x70179BF0 |
| Category | A -- Bare GOTO |
| Protected operation | Division + I/O section (store item computation) |
| Handler action | None |
| Resume target | 0x7017A520 (function exit point) |

---

## Summary Statistics

| Category | Count | Description |
|---|---|---|
| A -- Bare GOTO | 20 | Skip to resume point, no processing |
| B -- Error message + GOTO | 4 | Write 31-byte error string to screen, then resume |
| C -- Store default + GOTO | 1 | Store 0 to INPUT_RESULT, then epilog |
| D -- Fatal exit | 2 | Call RETURN_MESSAGE --> SYSCALL 0310 --> process termination |
| **Total** | **26** | |

### Protected Operation Breakdown

| Operation | Handler Count |
|---|---|
| READ_IN (terminal input) | 14 |
| File I/O (?OPEN/?CLOSE/?READ) | 7 |
| ?LOOKUP_PORT / ?CONNECT (IPC) | 2 |
| ?READ_SCREEN (direct terminal read) | 1 |
| ?CHAR_TO_UNSIGNED (conversion) | 1 |
| Division + mixed I/O | 1 |

The dominant pattern is protecting `READ_IN` calls -- 14 of 26 handlers
exist solely to catch terminal input errors (disconnect, timeout, etc.)
and skip to an exit or recovery point.

### C++ Translation Implications

See `PLAN.md` for the full architecture.

1. All 26 handlers register with `ac0 = -1` (catch all conditions).
   In native code: `catch (const types::PLIError&)` or `catch (...)`.
2. Category A/C handlers (21): `try { ... } catch (...) { /* goto resume */ }`
   or simply wrapping the protected section in a try-catch block.
3. Category B handlers (4): `catch (const types::PLIError&) { rt::write_screen(...); /* continue */ }`
4. Category D handlers (2): `catch (...) { /* fatal */ exit(1); }`
5. The 3 functions with multiple handlers (ALLY_PLAYER: 2, KILL_PLAYER: 3,
   LIST_PLAYERS: 2) use sequential O.ON / O.REVERT pairs to change the
   active handler at different points in the function.  In C++ these
   become nested or sequential try-catch blocks.

---

## Revision History

- **Session 4**: Initial catalog of all 26 handlers.  Documented
  RETURN_MESSAGE as fatal exit via SYSCALL 0310 (?RETURN).
- **Session N**: Updated C++ translation references to use
  types::PLIError and rt:: naming conventions.
