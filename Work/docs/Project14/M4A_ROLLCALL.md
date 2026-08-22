# M4a Roll-Call — live-play coverage (Aug 22 2026)

Source: a live human play session on the 101-live book (both mapper
findings fixed), lockstep + redirect trace. **0 divergences, 0 mapper
aborts across 1,316,947 redirect events.** 44 of 101 live routines
exercised in real play, including the key nested-procedure cases.

## Headline

M4a MECHANISM COMPLETE and CONFIRMED IN LIVE PLAY. All 101 callable
game routines migrate to 0x74000000 areas; the full scripted battery
(fo/m/inj/abort/play) is green and a real human play session ran clean.

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

## Status

M4a is DONE for its mechanism goal. The 57 unexercised routines are a
coverage backlog (reachable by more play or targeted scripts), not
open risk. Recommend: proceed to M4b; sweep remaining coverage
opportunistically (or one more play session into caves/bargain/death)
when convenient.
