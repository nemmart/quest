# a002 — F-B amended: the cheap route to PICK_X_Y is CHARACTER CREATION, not a killed-off record

Integrator, Sep 12 2026. Post-merge. **Your graph is unaffected and correct.**
This amends the *narrative* in REPORT §F-B and, more usefully, replaces the
fixture it recommends with a much cheaper one.

## What prompted it

The user, from play: *"if you just say Initials and a character name that
doesn't exist, it'll ask you for the type and call PICK_X_Y."*

F-B explains PICK_X_Y's reachability through QUEST's startup reading the
`previously killed off` flag at 7015C31F → `DIED` (7015C332) → REPOSITION →
PICK_X_Y, and recommends *"one killed-off record in the user data file"* as
the fixture.

That path is real. **It is not the one the user is describing, and it is not
the cheapest one.**

## The trace

    "What would you like your character to be
     'W' = wizard, 'C' = cleric, 'D' = druid, 'F' = fighter 'B' = barbarian"
        literal 0x7017816B, 114 bytes, block 70178246

Block 70178246 is inside **START_TURN** (7017821F). START_TURN calls `DIED`
at **7017868D** and **70178801** — two of the ten DIED sites. So:

    START_TURN  ──(new character: prompt for type)──>  DIED
        DIED  ──70166323──>  REPOSITION
            REPOSITION  ──70176FC7──>  PICK_X_Y
            REPOSITION  ──70176FD1──>  PLACE_PLAYER

**`DIED` is not only the death routine.** It is "(re)initialise a character
and place them in the world" — used for first creation as well as for death;
*"Better luck next time!"* is the death branch of it. That is worth recording
on the node itself, because the name misleads and a future session will read
`DIED` as death-only.

Everything in your graph already contained this. All eight REPOSITION sites
and both START_TURN→DIED edges are in `callgraph.py`'s output. F-B chose one
route through the graph and presented it as *the* route; the graph never
claimed that.

## Why this is worth an answer rather than a footnote

**It replaces the fixture.** F-B's recommendation — inject a killed-off
record into the user data file — means constructing binary game state
out-of-band, which is fiddly, needs its own verification, and creates a test
artifact someone must maintain.

**Creating a character needs no fixture at all.** The driver types unused
initials and answers one prompt with a single character. That is two extra
keystroke groups in a scripted leg, and it reaches **PICK_X_Y, REPOSITION,
PLACE_PLAYER, DIED and part of START_TURN** in one go — four of them
currently unreached or barely reached, including two of the four unreached
leaves.

Set against your Coverage.md §3.2 costings, this is very likely the cheapest
statements-per-unit-of-work item in the whole table, and it is a *driver
script* change rather than a data-file change.

## What I want

Nothing re-run and nothing rebuilt. When the play-driver project is prompted
(now scheduled ahead of routine #5), it will carry this as the first fixture.
Recorded here so it is not lost, and so the killed-off-record route is not
built first by mistake.

Two small amendments if you touch these files again — otherwise I will make
them at integration:

1. `REPORT.md` §F-B: note that creation via START_TURN is the common route
   and the killed-off flag the rare one.
2. `CallGraph.md`: annotate the `DIED` node — *"(re)initialise and place a
   character; reached from death AND from first creation."*

## The general point, second instance today

This is the second time a play report has corrected a static narrative — the
first being the D3 check that produced F-B itself. In both cases the *graph*
was right and the *story told about the graph* was wrong, and in both cases
the correction came from someone who has played the game rather than from
more analysis.

The project has a source of evidence it barely uses. `Order.md`'s coverage
judgements are exactly the kind of claim it is good at checking cheaply.
