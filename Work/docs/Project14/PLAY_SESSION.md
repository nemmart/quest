# M4a live-play coverage session

Goal: exercise the routines the scripted drivers don't reach, on the
101-live book, and confirm they redirect with 0 divergences. This is
the last thing before M4a is "done."

## Launch (lockstep, traces on)

From Work/c_src (after `make`):

```
QUEST_ADDRESS_BOOK=<repo>/Work/c_src/quest.addrbook \
  ./emulator -lockstep -silent \
  -trace /tmp/play.trace -types lockstep,redirect,gcalls \
  QUEST QUEST_SERVER @QUEST @QUEST
```
Then connect the terminal client as usual (login CL / Claude / quest /
Y / space / F).

## What to hit (priority = routines never seen in scripted play)

- **Combat** — attack a being, fight to a resolution. Exercises the
  nested sibling-link path (ATTACK.1/.2/.3 calling each other) and
  BEING_ATTACK/KNIGHT_ATTACK. **Highest value** — the sibling link is
  the one nested case only scripted so far.
- **Death (DIED)** — die in combat, or however the game allows. DIED is
  a dyn routine never exercised live.
- **Store / bargain** — visit the store, buy/sell, bargain. STORE +
  BARGAIN family.
- **Menu screens** — magic (DISPLAY_MAGIC), inventory
  (DISPLAY_INVENTORY), drop an item (DROP), list players
  (LIST_PLAYERS). These are quick.
- **Cave** — if reachable (DISPLAY_CAVE / CAVE_ATTACK). **OK to skip if
  you can't get into one** — note it and we'll reach it another way
  (a scripted approach, or just leave CAVE_* as LIVE-UNEXERCISED in the
  roll-call; the mechanism is identical to the other nested families
  already proven, so it's a coverage note, not a risk).

## What "good" looks like

- The session runs without the emulator printing `LOCKSTEP DIVERGENCE`
  or `MAPPER I2/PROBE`. If it does, that names a routine — grab the
  surrounding trace lines; that's a real finding.
- Quit cleanly (the game's quit path → I.STOP detach).

## Hand back

- /tmp/play.trace (or just the redirect + lockstep lines from it)
- a note of what you managed to exercise (esp. whether combat/death/
  cave happened)

I'll pull the routine coverage from the trace, write the roll-call, and
that closes M4a's coverage. Cave being skipped is fine.
