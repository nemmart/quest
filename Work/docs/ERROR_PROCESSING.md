# Quest Error Processing Catalog

Documents how each Quest function handles I/O errors and EOF
conditions, based on analysis of all ON ERROR handlers, ?READ/?WRITE
calls, and exception propagation paths.

## Error Processing Mechanisms

Quest uses three distinct mechanisms for error handling:

### 1. EOF Flag (Normal Return Path)

The `?READ` library wrapper checks specifically for AOS/VS error code
0x18 (`EREOF`).  When a ?READ call passes **more than 3 arguments**,
EOF is reported via a flag stored in the 4th argument rather than
through the exception system:

```
?READ detects EREOF:
  if arg_count > 3:
    store 0x8000 (PL/I true) to arg[4]   --> normal return
  else:
    call ?LIB_ERROR                        --> exception path
```

After ?READ returns, the caller checks the EOF flag:

```asm
XNLDA  0,[eof_flag]     ; load EOF flag
WCOM   0,0               ; complement
MOV.L# 0,0,SZC           ; skip if original was negative (0x8000)
WBR    continue_loop      ; not EOF, keep reading
XJMP   [cleanup]          ; EOF, exit loop
```

**All 11 ?READ calls in Quest pass 4 or more arguments**, so EOF
is always handled via the flag, never via exceptions.

### 2. ON ERROR Exception Handling (I.PROLOG / O.ON / I.EPILOG)

For non-EOF errors (open failure, write failure, terminal disconnect,
IPC failure), the library wrappers call `?LIB_ERROR`, which signals
through `O?SIGNAL` to the PL/I ON ERROR handler chain.

Functions that expect I/O errors register handlers via:
  I.PROLOG --> O.ON (handler_addr) --> risky operation --> O.REVERT

See `ON_ERROR_CATALOG.md` for the complete handler inventory.

### 3. Unprotected Calls (Error Propagates to Caller)

Some functions perform I/O without ON ERROR handlers.  If an error
occurs, `?LIB_ERROR` signals upward through the call stack until it
reaches a handler, or terminates the process if no handler exists.

### Error Sources by Library Routine

| Routine | SYSCALL | EOF? | Other Errors |
|---|---|---|---|
| ?READ | 0302 | Flag if args>3, else exception | Exception via ?LIB_ERROR |
| ?WRITE | 0303 | N/A | Exception via ?LIB_ERROR |
| ?WRITE_SCREEN | 0303 | N/A | Exception via ?LIB_ERROR |
| ?READ_SCREEN | 0302 | N/A (terminal) | Exception via ?LIB_ERROR |
| ?OPEN_FILE | 0300 | N/A | Exception via ?LIB_ERROR |
| ?CLOSE_FILE | 0301 | N/A | Exception via ?LIB_ERROR |
| ?OPEN_SHARED_IO_FILE | (internal) | N/A | Exception via ?LIB_ERROR |
| ?GET_SHARED_PAGE | 060 | N/A | Exception via ?LIB_ERROR |
| ?LOOKUP_PORT | (internal) | N/A | Exception via ?LIB_ERROR |
| ?CONNECT | (internal) | N/A | Exception via ?LIB_ERROR |

---

## Functions with File I/O (Open/Read/Write/Close)

These functions open, read, write, or close disk files.

### ALLY_PLAYER

| Item | Detail |
|---|---|
| I/O calls | ?OPEN_FILE, ?READ(7), ?READ(4), ?CLOSE_FILE |
| ON ERROR | Yes -- 2 handlers |
| EOF handling | Flag in arg 4 (normal loop exit) |
| File | Per-player character save file |
| Pattern | Open file, read records in loop (7-arg starts, 4-arg continues, EOF flag terminates loop), close file.  Handler #1 writes error to screen on open/read failure.  Handler #2 ignores close errors. |

### KILL_PLAYER

| Item | Detail |
|---|---|
| I/O calls | ?OPEN_FILE, ?READ(7) x2, ?READ(4) x2, ?CLOSE_FILE |
| ON ERROR | Yes -- 3 handlers |
| EOF handling | Flag in arg 4 (normal loop exit), two read loops |
| File | Per-player character save file |
| Pattern | Open file, two separate read loops (each with 7-arg init + 4-arg loop).  Handler #1 writes error to screen.  Handler #2 is a bare GOTO for the interactive section.  Handler #3 ignores close errors.  Also calls UPDATE_USER_DATA_FILE. |

### LIST_PLAYERS

| Item | Detail |
|---|---|
| I/O calls | ?OPEN_FILE, ?READ(7), ?READ(4), ?CLOSE_FILE |
| ON ERROR | Yes -- 2 handlers |
| EOF handling | Flag in arg 4 (normal loop exit) |
| File | Player list file |
| Pattern | Same as ALLY_PLAYER: open, read loop with EOF flag, close.  Handler #1 writes error to screen.  Handler #2 ignores close errors. |

### UPDATE_USER_DATA_FILE

| Item | Detail |
|---|---|
| I/O calls | ?READ(7), ?READ(4), ?WRITE(6), ?WRITE(3), SYSCALL 0232 (?UPDATE) |
| ON ERROR | **No** -- errors propagate to caller |
| EOF handling | Flag in arg 4 (normal return on EOF) |
| File | Operates on already-open file channel (passed as argument) |
| Pattern | Read loop searches for matching record (EOF = not found, returns).  If found, writes updated record.  ?WRITE(6) for positioned write, ?WRITE(3) for append.  ?UPDATE (SYSCALL 0232) commits changes.  Called by KILL_PLAYER (which has ON ERROR). |

### DISPLAY_MAP

| Item | Detail |
|---|---|
| I/O calls | ?OPEN_FILE, ?CLOSE_FILE |
| ON ERROR | **No** -- errors propagate to caller |
| EOF handling | N/A (no reads) |
| File | Map display file |
| Pattern | Opens file, calls TERRITORY (which reads from shared memory, not the file), outputs to screen, closes file.  No read calls -- the file may be used for metadata or locking only. |

### QUEST (main function)

| Item | Detail |
|---|---|
| I/O calls | ?OPEN_FILE x2 |
| ON ERROR | **No** |
| EOF handling | N/A |
| File | Terminal channels: SCREEN_CHAN (0x70000260) and INPUT_CHAN (0x70000262) |
| Pattern | Opens the two terminal I/O channels at startup.  These are the @INPUT and @OUTPUT pseudo-files for the user's terminal.  If these fail, the process cannot function. |

### INIT_SHARED_DATA

| Item | Detail |
|---|---|
| I/O calls | ?OPEN_SHARED_IO_FILE x3, ?GET_SHARED_PAGE x3 |
| ON ERROR | **No** -- but calls RETURN_MESSAGE on SYSCALL failure |
| EOF handling | N/A |
| File | SHARED_DATA_FILE, WORLD_DATA_FILE, CASTLE_DATA_FILE |
| Pattern | Opens 3 shared memory files and maps them.  SYSCALL 044 failure triggers inline error check and RETURN_MESSAGE (fatal exit).  Library call failures propagate. |

---

## Functions with Terminal I/O Only

These functions do terminal reads (via READ_IN or GET_INPUT) and
terminal writes (via ?WRITE_SCREEN) but no disk file I/O.

### Functions Protected by ON ERROR

All of these register ON ERROR handlers primarily to catch terminal
disconnect errors during READ_IN or ?WRITE_SCREEN calls.

| Function | Handlers | Protected Operations | Handler Action |
|---|---|---|---|
| ATTACK | 1 | READ_IN | Bare GOTO to retry/abort point |
| AUTO_MOVE | 1 | Shared memory + input section | Bare GOTO past operation |
| CAST | 2 | READ_IN (two separate input prompts) | Bare GOTO to function exit |
| CASTLE_INVENTORY | 1 | READ_IN + ?CHAR_TO_UNSIGNED | Bare GOTO past input |
| DROP | 1 | READ_IN + ?CHAR_TO_UNSIGNED | Bare GOTO to error recovery (stores 0) |
| HELP | 1 | Multi-page ?WRITE_SCREEN output | Bare GOTO to function exit |
| OBSERVE | 1 | READ_IN | Bare GOTO to function exit |
| OP_EDIT | 6 (across 4 I.PROLOG regions) | READ_IN (6 separate input prompts) | Bare GOTO to various exit points |
| START_TURN | 1 | READ_IN | Bare GOTO into turn processing |
| STORE | 1 | Division + I/O section | Bare GOTO to function exit |

### READ_IN (special case)

| Item | Detail |
|---|---|
| I/O calls | ?READ_SCREEN (SYSCALL 0302 on terminal) |
| ON ERROR | Yes -- 1 handler (Category C) |
| EOF handling | N/A (terminal read) |
| Pattern | The only function that modifies state in its error handler: stores 0 to INPUT_RESULT (0x7000021C) on failure, then exits via I.EPILOG.  Callers check for zero-length input to detect the error. |

### LOGON (IPC, not file I/O)

| Item | Detail |
|---|---|
| I/O calls | ?LOOKUP_PORT, ?CONNECT |
| ON ERROR | Yes -- 2 handlers (Category D -- fatal) |
| Pattern | Handler #1 calls RETURN_MESSAGE("Maximum number of players exceeded.") on ?LOOKUP_PORT failure.  Handler #2 calls ?LIB_ERROR_CODE + RETURN_MESSAGE on ?CONNECT failure.  Both are fatal (process exit via SYSCALL 0310). |

### GET_INPUT (no handler)

| Item | Detail |
|---|---|
| I/O calls | ?READ(6) on INPUT_CHAN (0x70000262) |
| ON ERROR | **No** -- errors propagate to caller |
| EOF handling | Flag present (6 > 3 args) but terminal reads don't normally EOF |
| Pattern | Reads single character from terminal.  Called by HIT_ANY_CHAR and the main game loop.  Terminal errors propagate to the calling function's ON ERROR handler. |

### Unprotected ?WRITE_SCREEN Callers

712 ?WRITE_SCREEN calls exist across the codebase.  Most game
functions call ?WRITE_SCREEN without a local ON ERROR handler.
If the terminal disconnects during a write, the error propagates
up the call stack.  This works because:

- The main game loop (START_TURN, CAST, ATTACK, etc.) has ON ERROR
  handlers that catch propagated terminal errors.
- Lower-level display functions (DISPLAY_SCREEN, DISPLAY_INVENTORY,
  MOVE_PLAYER, etc.) are always called from within a protected context.

---

## Summary: ?READ Argument Patterns

All disk file reads use a two-call loop pattern:

| Call | Args | EOF Flag | Purpose |
|---|---|---|---|
| ?READ(7) | 7 | Yes (arg 4) | Initial read with full options (positioning, record format) |
| ?READ(4) | 4 | Yes (arg 4) | Continuation read (next record, loop back to check) |

The 7-arg call sets up the read parameters (channel, buffer, length,
EOF flag, record number, format, position).  After processing each
record, the 4-arg call reads the next record with minimal parameters
(channel, buffer, length, EOF flag).  The 4-arg call then jumps back
to the same EOF-check code that follows the 7-arg call.

### ?READ(6) -- Terminal Read (GET_INPUT only)

GET_INPUT uses ?READ(6) on INPUT_CHAN for single-character terminal
reads.  The extra arguments include terminal-specific options (e.g.
0x1000 for single-character mode).

---

## Summary: Error Handling by Category

### Functions with ON ERROR + File I/O (3)

ALLY_PLAYER, KILL_PLAYER, LIST_PLAYERS

Pattern: Open file, read loop with EOF flag, close file.  ON ERROR
handlers catch open/read failures (display error message) and close
failures (ignore silently).

### Functions with ON ERROR + Terminal I/O Only (12)

ATTACK, AUTO_MOVE, CAST, CASTLE_INVENTORY, DROP, HELP, OBSERVE,
OP_EDIT, READ_IN, START_TURN, STORE, LOGON

Pattern: Protect READ_IN calls and ?WRITE_SCREEN sequences from
terminal disconnect.  Most handlers are bare GOTOs to an exit point.
Two LOGON handlers are fatal (RETURN_MESSAGE).

### Functions with File I/O but No ON ERROR (4)

UPDATE_USER_DATA_FILE, DISPLAY_MAP, QUEST, INIT_SHARED_DATA

These either rely on the caller's handler (UPDATE_USER_DATA_FILE is
always called from KILL_PLAYER which has handlers), handle errors
inline (INIT_SHARED_DATA checks SYSCALL results directly), or
operate on channels that should always succeed (QUEST opens terminal).

### Functions with No I/O and No Handlers (majority)

Pure computation on shared memory (DIST, OWNS, RANDOM,
DISTANCE_TO_PLAYER, MOVE, TERRAIN, FIND_OBJECT, etc.).
No error handling needed.

---

## C++ Translation Implications

See `PLAN.md` for the full architecture. Key error handling patterns:

1. **Library function errors (e.g., ?CHAR_TO_UNSIGNED)**:
   The `rt::` native implementation throws `types::PLIError(signal_code)`.
   While the caller is still emulated, the `emu_rt::` wrapper catches it and
   calls `EagleIntegration::throw_lib_error()`, which routes to the emulated
   `?LIB_ERROR` → PL/I signal chain → emulated caller's O.ON handler.
   Once the caller is also native, it catches `PLIError` directly with
   try/catch.

2. **EOF handling**: Implement as a loop with a boolean EOF flag
   returned by reference from the read function.  Never throw on EOF.
   All 11 ?READ calls in Quest pass 4+ args, so EOF is always via flag.

3. **File read pattern**: `while (!eof) { read_record(..., &eof); if (eof) break; process(); }`

4. **Terminal I/O errors**: In native game functions (`quest::`), use
   try-catch around terminal read/write sequences.  Most ON ERROR handlers
   in the original are bare GOTOs that just break out of the current
   operation — these become catch blocks that break/return.

5. **Fatal errors**: LOGON failures and RETURN_MESSAGE calls become
   `fprintf(stderr, ...) + exit(1)`.

6. **Unprotected callers**: Functions like DISPLAY_INVENTORY that
   call ?WRITE_SCREEN without handlers rely on the caller's handler.
   In C++, `types::PLIError` exceptions naturally propagate up the
   call stack to the nearest catch block.

---

## Revision History

- **Session 4**: Initial catalog.  Documented EOF flag mechanism in
  ?READ, all 11 ?READ calls, 2 ?WRITE calls, 6 ?OPEN_FILE calls,
  4 ?CLOSE_FILE calls, and complete handler inventory for all game
  functions.
- **Session N**: Updated C++ Translation Implications to reference
  actual architecture (types::PLIError, rt/emu_rt pattern,
  EagleIntegration::throw_lib_error boundary).
