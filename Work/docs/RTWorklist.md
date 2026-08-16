# RT Worklist — Play Session 1 (Aug 2026)

Source: full -lockstep play session (castles, combat, store, money);
508,732 rtcalls entries, zero divergences, master/clone coverage
byte-identical (1255 of 9828 RT words).

## Reached routines (37 of 137), by call count

| Routine | Calls | Notes |
|---|---|---|
| SQR31?3 | 319,574 | hottest by far; callers 701687D9 / 7016872E (DIST) and 7017CF78; FPAC0 convention |
| D.MOD | 92,331 | ALL calls return to 7017DE49 — internal to ?RANDOM_NUMBER (one mod per random) |
| ?RANDOM_NUMBER | 92,331 | in-process PRNG, deterministic (lockstep-clean); callers 70176664, 70177C9C, 7016E972. Translation must replicate the PRNG bit-exactly |
| ?FILL_WORDS | 2,675 | trivial semantics; called RT-internally too (?WRITE_SCREEN, ?GET_SHARED_PAGE, ?READ_SCREEN) |
| ?WRITE_SCREEN | 1,255 | |
| X.CB | 153 | |
| ?READ | 153 | |
| ?UDIV32 | 101 | pure leaf |
| ?UNSIGNED_TO_CHAR | 65 | |
| C?INIT | 16 | startup/shutdown alias pile |
| ?DELAY | 14 | |
| I.PROLOG | 10 | condition frames: registration exercised… |
| I.EPILOG | 9 | |
| O.ON | 6 | |
| O.REVERT | 4 | |
| I.LOCK / I.UNLOCK | 3 each | |
| ?READ_SCREEN | 3 | |
| ?OPEN_SHARED_IO_FILE / ?GET_SHARED_PAGE | 3 each | |
| I.FPU, I.ALLOC | 2 each | I.ALLOC: STALE NOTE REMOVED (Aug 2026) — queue insns rewritten from the DG manual and I.ALLOC/I.FREE* translated + validated; see I_ALLOC.md |
| ?OPEN_FILE | 2 | terminal channels |
| singletons | 1 each | SWAT.NIN, MT?TASK, LANG?INIT/STOP, I?INHPW, I?HPOWNER, I.STOP, I.GINIT, I.FREEW, ?LOOKUP_PORT, ?CURRENT_PID, ?CREATE_TASK, ?CONNECT, ?AWAIT_CONSOLE_INTERRUPT |

## Notable absences

- **No condition ever signaled**: I.GOTO, O?SIGNAL, ?LIB_ERROR,
  ?DEFAULT_ERROR_HANDLER, O.S* all zero. The ON ERROR machinery was
  registered 10 times but never fired. Error paths need deliberate
  provocation later (disconnects, file errors) — they will not be
  validated by happy-path play.
- **?CHAR_TO_UNSIGNED: zero calls** — the planned first translation
  target was never reached this session (no numeric-input commands
  used, e.g. DROP quantity). First target should be a routine
  gameplay actually validates — see recommendation.
- ?UMUL32, ?CLOSE_FILE, ?WRITE, ?SEND/RECEIVE_TASK_MESSAGE,
  ?LIB_ERROR_CODE: zero. (No file save path exercised — ALLY/KILL/
  LIST_PLAYERS not played.)
- Most MT?* cluster, heap beyond I.ALLOC/I.FREEW, error reporting
  (?SNAP/?FATAL), line-number machinery: zero.

## Play Session 4 (user): signal-path validation
Store conversion-error playtest: full chain O?SIGNAL->I.GOTO through
native-built handler chain, live O.REVERT(native)+re-O.ON(native),
0 divergences in 491k rtcalls. O.ON/O.REVERT validation CLOSED.

## Coverage audit

**Covered but never entry-logged** (reached by unhooked paths —
fall-through, shared bodies, plain jumps): T?INITN→T.INIT (adjacent
entries, fall-through), I.FREEW→I.FREE (shared body, 54 words),
I.DISPLA (7, likely I.PROLOG interior), O.SET (26, likely O.ON
interior/shared tail), I.INIT (18), .UKIL (7, exit vector). None look
like hook gaps for *game-originated* calls; they are RT-internal
control flow. Verify per-routine during translation.

**Hidden live code — 40 covered words inside static `mem` ranges**
(StartStop misclassification, to be fixed in quest-rt.addrs or
StartStop):

- 7017EA34..7017EA51 cluster (~15 words): startup tail in the
  I.INIT/I.GINIT area — this code *makes* the logged calls to
  I?HPOWNER (ret 7017EA46), I.FPU (ret 7017EA4D), LANG?INIT
  (ret 7017EA51). Static analysis missed the path into it.
- 7017E86B..7017E9B2 scatter (~18 words): heap internals
  (I.ALLOC/I.FREE region) reached via paths StartStop can't follow.
- 7017E715..7017E71A (5 words): between X.CB and D.MOD.
- 7017FDF2 (1 word): .UKIL exit-vector area.

## Recommended first translations (task B), in order

1. **?FILL_WORDS** — trivial semantics, 2,675 gameplay validations,
   called from both game and RT-internal sites; ideal for building the
   calling-convention bridge with minimal routine-logic risk.
2. **?UDIV32 / ?UNSIGNED_TO_CHAR** — small pure leaves with healthy
   call counts.
3. **SQR31?3** — hottest routine; introduces the FPAC0 non-stack
   convention (CODE.md's documented exception).
4. **?RANDOM_NUMBER + D.MOD together** — high value, but requires
   bit-exact PRNG replication; do after the bridge is proven.

?CHAR_TO_UNSIGNED moves later, paired with a play script that
exercises numeric input.

## Play Session 2 (store run, native ?FILL_WORDS live)

487,779 calls; 2,407 native fills across all 5 sites, zero fallbacks,
zero divergences — first translation validated at scale.

**The condition chain fired live** (first time — deliberately
provoked: the user entered "ABC" at the store purchase prompt to
trigger a CONVERSION signal) → ?CHAR_TO_UNSIGNED → C.INDEX (scan) → ?LIB_ERROR
(CONVERSION) → T?AREA ×6 → ?DEFAULT_ERROR_HANDLER → O?SIGNAL → O.SET →
I.GOTO unwinding into STORE's handler (catalog #26) at 70179BFC, then
O.REVERT at the documented 0x7017A520 resume point, and play continued.
The whole stack-walking signal path ran verified in lockstep and
matches ON_ERROR_CATALOG.md exactly. **Reproducible trigger for
error-path testing: type garbage at a store prompt.**

New arrivals on the reached list: ?CHAR_TO_UNSIGNED ×2 (incl. its
error path), ?UMUL32 ×2, C.INDEX ×3, T?AREA ×7, plus the condition
chain singletons. Reached total: 45 of 137.

## Play Session 3 (user; native ?FILL_WORDS + ?UDIV32 live)

776,408 rtcalls; ?FILL_WORDS(native) ×3,398 and **?UDIV32(native)
×226, zero fallbacks, zero divergences** — every ?UDIV32 call returns
to 7017DADC (inside ?UNSIGNED_TO_CHAR: it has no direct game callers).
?UNSIGNED_TO_CHAR ×122 across 13 sites: 7017A116/7017A0DA (store
prices, ×33 each), 701678E2 ×20, 70167A27/7016796E ×6, 70167A9C ×4,
7017A047/70167C09/70167BA4/70167B33/7015EE10/7015E95E ×3, 70165C4A ×2.

## ?UNSIGNED_TO_CHAR — TRANSLATED (implementation session)

First native subtree (calls rt::udiv32_3 directly; the master's
run-to-return absorbs the inner emulated calls — ?UDIV32 dispatch
count drops to zero by design). First register-argument routine
(destination in caller's ac2 via the saved-ac2 slot). Scripted
lockstep session: 9 native calls, 9 sites (two new beyond the 13
above: 70167CA0, 70173527/7017354D area), 0 divergences, and an
empirical footprint diff of **0 differing words** (see
UNSIGNED_TO_CHAR.md RESOLUTION). Store playtest pending for the price
sites and multi-digit values.

## Dotted-helper analysis (B.*/C.*/X.* runtime internals) — from the parallel session

These PL/1 compiler-support internals use per-routine, non-standard
conventions (LJSR continuations: I.PROLOG returns pc+7, O.ON pc+3,
I.EPILOG/I.STOP never return, O.SERROR throws). Do NOT force a
uniform bridge model on them — that was the Opus-era failure mode.
Derive each convention individually with empirical captures.

Static call-site facts (from quest.code / quest-rt.dis):

- **B.MOVE** (0x7017E5CB): one game call site, 0x7015C565 in QUEST
  main, 6 args, on a conditional path not yet observed live.
- **C.INDEX** (0x7017E5F4): RT-internal callers only (from
  ?CHAR_TO_UNSIGNED; observed live ×3).
- **C.TRANS / C.TRANSL** (0x7017E64A): one game call site, 0x7016295B
  in CAST, 6 args — see hidden feature below. Zero live observations
  despite heavy spell-casting in play sessions.
- **C.COLLAT** (0x7017E688): **NOT CODE** — it is the identity
  collating-sequence table (bytes 00 01 02 03 ...) consumed by
  translate/compare instructions (WCTR at 0x7017E686 precedes it).
  Data to preserve; never a translation target. Its RTStubs stub is
  harmless (never called).
- **D.MOD / F.MOD** (0x7017E722): RT-internal only — every observed
  call (92K+/session) returns into ?RANDOM_NUMBER (one modulo per
  random draw).
- **X.CB** (0x7017E708): 2 direct game call sites plus RT-internal
  callers; observed live ×153/session.
- X.AIC / X.IC (0x7017FC4D/50): no direct game call sites found.

## Hidden feature: the "concentrate on" typed-target prompt (CAST)

Why C.TRANS never appears in traces: normal casting is menu-driven
(name shown, space casts / down-arrow advances). The C.TRANS site is
on a typed-input sub-path:

- At 0x7016294A..2959, CAST case-folds the player's typed IN_BUFFER
  input via C.TRANS (26-letter upper/lower translate tables), output
  to a frame-local buffer.
- The folded input is then WCMP'd sequentially against realm names in
  the constant pool at 0x70162153..: **MALDORK, BRISTLE, ALBION,
  CARRONE, TERRAK** (no Loric, no Sundar).
- The prompt string at 0x70162168: **"Who/what do you wish to
  concentrate on ?"** — the Summons / transport spell family's
  typed-target prompt. Just above the pool: "...eleport without a
  transport ring!" (a transport-ring gate message). Related game
  functions exist: TRANSPORT_TERRAK (0x7017D48F), TRANSPORT_SUNDAR
  (0x7017D492).

Interpretation: certain spells (Summons; transport/teleport family)
prompt for a typed target; realm names get realm-specific handling
(e.g., summoning that realm's legendary being per GAME_REFERENCE's
realm/being table, or realm teleport targets).

**Validation path for C.TRANS** when its translation turn comes: cast
a Summons/transport-type spell and TYPE a realm name (e.g. "terrak",
any case) at the concentrate prompt.

## Worklist impact (dotted helpers)

- B.MOVE, C.TRANS: real targets with known-but-unexercised validation
  paths; keep as stubs until a play path reaches them, then derive.
- C.COLLAT: reclassify as data (remove from any translation queue).
- No changes needed to existing translations, the bridge, or pairing.

## Play Session 5 (user; dragon kill, post-queue-rewrite)

1,054,426 rtcalls; **39 distinct routines, no new ones**. Arithmetic
checks out exactly: Session 1's 37, minus `?UDIV32`, plus
`?CHAR_TO_UNSIGNED` / `?UMUL32` / `C.INDEX` = 39.

- **`?UDIV32` absent by design** — it has no direct game callers, and
  native `?UNSIGNED_TO_CHAR` calls `rt::udiv32_3` as plain C++. Its
  dispatch count dropping to zero is the native-subtree behaviour
  SessionPlan predicted, now confirmed at scale.
- **Zero fallbacks.** Every translated routine appears only in its
  `(native)` form: `?FILL_WORDS` x4,978, `?UNSIGNED_TO_CHAR` x100,
  `O.ON` x7, `O.REVERT` x4.
- Volumes: `SQR31?3` x659,525 (up from 319K), `D.MOD` /
  `?RANDOM_NUMBER` x193,436 each (up from 92K). Combat is PRNG-heavy —
  relevant to the bit-exactness requirement for that pair.

### New call sites (combat)

`?RANDOM_NUMBER` was documented with 3 sites; this session hit 9.
`?UNSIGNED_TO_CHAR` gained 5 beyond the documented 15.

| Routine | New sites |
|---|---|
| `?RANDOM_NUMBER` | ATTACK 0x7015DCE9 / 0x7015DDB2 / 0x7015E90E, DEFEND 0x701659A7 / 0x70165A06, SIGNAL_TURN 0x70177D5B / 0x70177DEF |
| `?UNSIGNED_TO_CHAR` | ATTACK 0x7015D96B / 0x7015DFA0, DEFEND 0x70165A47, DISPLAY_MAGIC 0x70166514, DISPLAY_INVENTORY 0x70167D40 |

All exercised natively with zero divergences.

### The allocator's live caller is ?CREATE_TASK, not ?LIB_ERROR

Observed call sites:

```
I.ALLOC   7017DBD1, 7017DBFF   -> ?CREATE_TASK (0x7017DBB3)
I.FREEW   7017DC3D             -> ?CREATE_TASK
```

`?CREATE_TASK` runs once at startup: two allocations and one free.
**This is the validation path for native I.ALLOC/I.FREE** — every
session exercises it, with no fault injection needed. `?LIB_ERROR`'s
allocator use (0x7017E37E / 0x7017E399) is error-path-only and is
therefore the second validation source, not the first.

It also follows that the free must release the *most recent*
allocation: otherwise the insert path would have hit `XCT` and aborted.

`?CHAR_TO_UNSIGNED` was called once from STORE (0x7017A238); its
internal `C.INDEX` / `?UMUL32` calls match the documented
"RT-internal only" note.
