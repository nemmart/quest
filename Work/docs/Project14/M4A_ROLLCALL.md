# M4a Roll-Call — live-play coverage (Aug 22 2026)

Source: a live human play session on the 101-live book (both mapper
findings fixed), lockstep + redirect trace. **0 divergences, 0 mapper
aborts across 1,316,947 redirect events.** 44 of 101 live routines
exercised in real play, including the key nested-procedure cases.

## Headline (corrected)

All 101 callable routines are MIGRATED (in the book, redirect on). The
MECHANISM is validated on the **44 routines actually EXERCISED** under
lockstep (this session + the scripted battery), 0 divergences across
1.3M redirect events — a wide, representative 44 (nested, large-dyn,
combat, WSAVR, hot leaves). The other **57 are MIGRATED-BUT-UNEXERCISED**:
configured to redirect but never yet executed migrated in any run, so
UNPROVEN. M4a is NOT fully closed until those 57 run clean — cf.
DISPLAY_SCREEN, which looked fine until it first executed and tripped
Finding A. This is a real coverage gap, not a footnote.

## Handler / signal machinery — LIVE-VALIDATED this session

The user triggered an ON exception during play. The trace shows
execution at pc=7017EC7C (the O?SIGNAL / error-unwind region, the same
pc as Finding B) with lockstep pairs CLEAN (ord=0, 4/4 insns) — while
migrated area frames were live. So a REAL signal dispatch + the
Ruling-A chain-walk-in-master-coordinates + unwind interacting with
0x74 area frames all executed in live play at 0 divergence, not only in
the scripted inj battery. Handler ESTABLISHMENT under migration also ran
(READ_IN ×6, ATTACK ×14, START_TURN ×1 — all have ON-units, fired
migrated). (Note: the play launch enabled only gcalls/lockstep/redirect
traces, not rtcalls, so O?SIGNAL itself is not individually logged — the
lockstep pair at its pc is the evidence.)

## LIVE-VALIDATED (fired in play, 0 div) — 44 routines

Nested procedures (static-link-through-area, live): ATTACK + ATTACK.2 +
ATTACK.3, CAST + CAST.1, MOVE_PLAYER.1, QUEST.1 — the nested mechanism
confirmed in real combat/casting, not just scripted B1.
Finding-A routine under live load: DISPLAY_SCREEN (312 hits) + the other
large dyn routine DISPLAY_INVENTORY (319) — the s>=W fix holds in play.
WSAVR variant: LOCK_FILE / UNLOCK_FILE.
Combat: ATTACK, BEING_ATTACK, DEFEND, TOWER_ATTACK, CATAPULT, THIEF.
Hot leaves: DISTANCE_TO_PLAYER (443k), RANDOM (147k), DIST (62k), OWNS.
Plus: TERRAIN, TERRITORY, UPDATE_SCREENS, GET_INPUT, STORMS_AT_SEA,
SIGNAL_TURN, REPORT, REGEN_SPELLS, MOVE_FAMILIAR, FIND_OBJECT,
MOVE_PLAYER, MOVE, TAKE, FAKE_OCEAN, FAKE_LAND_MASS, READ_IN,
REFRESH_SCREEN, STORE, START_TURN, LOGON, INIT_SCREEN, INIT_OBJ_TBL,
HIT_ANY_CHAR, GET_QUEST.

## LIVE-UNEXERCISED (in book, mechanism-proven, not seen this session) — 57

Reachable next time / other content paths — NOT a risk, the redirect
mechanism is identical to the validated routines; these are coverage
notes only:
- Cave family (user could not enter a cave this session): DISPLAY_CAVE,
  CAVE_ATTACK(+.1/.2), MOVE_IN_CAVE.
- Death: DIED (didn't die).
- Bargain: BARGAIN(+.1/.2).
- Menus not opened: DISPLAY_MAGIC, DISPLAY_MAP, DISPLAY_FLASK, DROP(+.1),
  LIST_PLAYERS(+.3), LOOK, SPYGLASS, BACKPACK, CASTLE_INVENTORY,
  OBSERVE, HELP, OP_HELP.
- Other content: ALCHEMIST_HOME(+.1), ALLY_PLAYER, ATTACK.1/.5,
  AUTO_MOVE, BOAT(+.1), CAST.4, CREATE_MAP, FIRE(+.1/.2/.3),
  GET_OBJECT_INDEX, KILL_PLAYER(+.4), KNIGHT_ATTACK, OP_EDIT(+.4/.6/.8),
  PICK_X_Y, PLACE_PLAYER, REPOSITION, SEIGE, TAKE_OVER_CASTLE,
  TERRITORY_MAP, TRANSPORT_SUNDAR/TERRAK, CLONE_SUNDAR,
  UPDATE_USER_DATA_FILE, WRITE_OBJECT, RETURN_MESSAGE (fatal-only path).

## EXCLUDED → M5 — 29 nocall

Boot QUEST, C_A_LISTENER (task entry), the 26 ON-unit bodies. Reached by
dispatch not call; become branch targets under M5 static handler
dispatch. Not M4 scope.

## Status — M4a CLOSED (Aug 22 2026, user ruling)

M4a is CLOSED. Mechanism validated on 44/101 routines exercised under
lockstep + the full signal/handler machinery (establishment, real
dispatch through O?SIGNAL/unwind, Ruling-A walks) — all 0 divergences
across 1.3M redirect events in live play, plus the scripted battery
(fo/m/inj/abort/play) green. The 44 span every hard case: nested
procedures live, large-dyn (DISPLAY_SCREEN), combat, WSAVR, hot leaves,
signal dispatch with area frames live. The remaining 57 are
MIGRATED-and-live-in-the-book but not yet individually exercised; they
carry low residual risk (identical mechanism) and are a COVERAGE
BACKLOG to sweep opportunistically or during M4b, NOT a blocker.
Original intent below retained for history. To close: the 57 migrated-but-unexercised routines
must execute under lockstep — targeted play (caves, death, bargain,
menus, castle ops, boats, fire, alchemist) and/or scripted approaches
into those paths. Each is low-probability-of-surprise (mechanism is the
same) but UNPROVEN until run — the Finding A precedent says run them.
Options: (1) more play sessions covering the missing content; (2)
scripted drivers that reach specific paths; (3) accept residual risk
and proceed to M4b, closing coverage in parallel. Recommend (1)/(2)
before declaring M4a done.
