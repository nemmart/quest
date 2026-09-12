#!/usr/bin/env python3
# Scripted Quest session driver — Project 13 regression battery.
# Reusable and GROWN across sessions (Project 13 ruling): add steps to `play`,
# do not fork. Modes:
#   m         login, M+n+"abc" (CONVERSION signal), a turn, ESC (I.STOP detach)
#   failopen  login, L -> P (FAIL_OPEN handler)
#   kp        login, K + name + password (KILL_PLAYER)
#   play      login, then a scripted free-play tour that exercises the hot
#             movement/render leaves (FIND_OBJECT, DIST, DISTANCE_TO_PLAYER,
#             RANDOM via AUTO_MOVE + map render), the menu screens
#             (OBSERVE, DISPLAY_MAGIC, LIST_PLAYERS, HELP), then a real quit.
#             This is the routine-COVERAGE run; extend it, never fork.
#   login     login, wait, ESC
#
# P49 Stage B — `play` rewritten; every other mode byte-for-byte unchanged.
# Three bugs fixed, all found by reading a session log for the first time
# (docs/Project49/q001-plan-gate.md section 1):
#
#   1. ESC AFTER `D` QUIT THE GAME. The old sequence assumed `send(key);
#      send(ESC)` for every menu key. DISPLAY_MAGIC does not open a screen --
#      it prints "All spells ready!" and returns to the command prompt -- so
#      that ESC landed ON the prompt, which detaches. The session died there
#      and L/H were never sent. There is no ESC after `D` now.
#   2. `L` NEEDS A SUB-KEY. LIST_PLAYERS opens a chooser ("Hit (P) for
#      players (C) for castles ..."); the old driver answered it with ESC, so
#      the menu rendered and the listing body never ran. `P` is sent now.
#   3. FIXED WAITS. The old `play` drained a fixed 160s after the auto-move,
#      and drive_patient.py's variant needed ~498s inside a `timeout 480`, so
#      under lockstep the keys were sent mid-turn or never sent at all.
#      Waits are now keyed to the GAME'S OWN handshake (the "-> " command
#      prompt going quiet), so the same script works at emulation speed and
#      at lockstep speed with no retuning.
#
# P49 ruling 7a: docs/Project14/drive_patient.py -- the one-off `play` fork
# that had quietly become the thing running in the battery's `play` leg -- is
# RETIRED. Its prompt-awareness is folded in here and both play legs point at
# this file. The file stays on disk so P14's record reads.
#
# Ceiling hits are LOUD (P49 ruling 7b): they go to stdout AND into the
# session log, because a silent timeout is exactly the bug this replaces.
import socket, sys, time, os

mode = sys.argv[1] if len(sys.argv) > 1 else "m"
logname = sys.argv[2] if len(sys.argv) > 2 else "/tmp/session.log"
port = int(os.environ.get("QUEST_PORT", "8781"))
s = socket.create_connection(("127.0.0.1", port), timeout=300)
s.settimeout(0.5)
log = open(logname, "ab")

PROMPT = b"-> "
BUSY = b"Waiting for your turn"
ceilings_hit = []

# How many consecutive 4 s silent slices count as "the game has stopped
# talking". Under -lockstep the game runs orders of magnitude slower than in
# plain emulation, and a gap WITHIN one screen render can be long, so this is
# deliberately generous and env-tunable: the battery raises it for the
# lockstep legs rather than the script being retuned per run.
QUIET_SLICES = int(os.environ.get("QUEST_DRIVE_QUIET_SLICES", "5"))

def drain(seconds):
    end = time.time() + seconds; data = b""
    while time.time() < end:
        try:
            chunk = s.recv(4096)
            if not chunk: break
            data += chunk; log.write(chunk)
        except socket.timeout: pass
    log.flush(); return data

def send(text, wait):
    s.sendall(text.encode('latin1')); return drain(wait)

def note(msg):
    """Loud on both channels: the battery reads one or the other."""
    line = "##### DRIVER: %s\n" % msg
    log.write(line.encode()); log.flush()
    sys.stdout.write(line); sys.stdout.flush()

def settle(ceiling, label, grace=120):
    """Wait on the game's handshake, not the clock.

    Measured behaviour (P49 Stage B, /tmp idle probe): sitting at the command
    prompt the game emits NOTHING AT ALL -- 100 s sampled, 0 bytes -- so
    quiescence is a sound signal. Two traps it has to dodge:

      * the response to a key may already have been consumed by a preceding
        drain, so "wait for fresh output first" can hang forever. After
        `grace` seconds of silence we accept that there was nothing to wait
        for.
        * MID-AUTO-MOVE the client is also silent: it prints "Waiting for your
        turn" and then blocks on the server tick. Quiescence alone would
        declare that finished. So quiet only counts when the tail is NOT
        sitting in a turn wait -- and "in a turn wait" must be decided by
        ORDER, not by presence. A spent "Waiting for your turn" from an
        earlier turn stays in the buffer, so testing for it anywhere in the
        window wedges the wait forever (observed: the auto-move had finished
        and the prompt was up, and settle still burned its whole 900 s
        ceiling). The turn is over once a prompt appears AFTER the last
        BUSY marker.
    """
    start = time.time()
    deadline = start + ceiling
    seen = False; quiet = 0; tail = b""
    while time.time() < deadline:
        d = drain(4)
        if d:
            seen = True; quiet = 0; tail = (tail + d)[-400:]
            continue
        quiet += 1
        if tail.rfind(BUSY) > tail.rfind(PROMPT):
            quiet = 0                      # a turn is in flight; keep waiting
            continue
        if seen and quiet >= QUIET_SLICES:
            return True
        if not seen and (time.time() - start) > grace:
            return True                    # nothing was coming; not an error
    ceilings_hit.append(label)
    note("CEILING HIT after %ds waiting for: %s" % (ceiling, label))
    return False


def step(keys, label, ceiling=120):
    s.sendall(keys.encode('latin1'))
    return settle(ceiling, label)


drain(10)
send("CL\r", 4); send("Claude\r", 4); send("quest\r", 4); send("Y\r", 4); send(" ", 4)
send("F\r", 12)

if mode == "m":
    send("M", 4); send("n", 4); send("abc\r", 8)   # CONVERSION at "For how many turns?"
    drain(70)
elif mode == "failopen":
    send("L", 5); send("P", 15)
elif mode == "kp":
    # P19 tranche-C coverage: KILL_PLAYER prompts "What player name to
    # kill off ?" then "Password ?" — kill our own freshly created player.
    send("K", 5); send("Claude\r", 8); send("quest\r", 15)
elif mode == "play":
    settle(180, "command prompt after login/creation", grace=20)
    # --- movement: AUTO_MOVE drives FIND_OBJECT / DIST / DISTANCE_TO_PLAYER /
    #     RANDOM through the map render and territory scan every turn ---
    send("M", 4); send("n", 4); send("3\r", 10)
    settle(900, "auto-move (3 turns) to finish")     # lockstep is far slower here
    # --- menu screens (leaf named routines, batch 2 coverage too) ---
    step("O", "OBSERVE")                             # -> "Observe item" + object screen
    step("\x1b", "exit OBSERVE")                     # that screen DOES need an ESC
    step("D", "DISPLAY_MAGIC")                       # returns to the prompt: NO ESC (bug 1)
    step("L", "LIST_PLAYERS chooser")                # -> "Hit (P) for players ..."
    step("P", "LIST_PLAYERS player list")            # the body (bug 2)
    step("\x1b", "exit LIST_PLAYERS")
    # P32: HELP topic 1 (Terrain) runs the two-stage 164 || 150 -> 314 scratch
    # chain (7016D5E3..7016D602) and its ?WRITE_SCREEN; 0 leaves HELP
    step("H", "HELP topic menu")
    step("1\r", "HELP topic 1 (Terrain)")
    step(" ", "HELP 'hit any character'")
    step("0\r", "exit HELP")
elif mode == "login":
    drain(15)

send("\x1b", 8)            # ESC at the command prompt = the real quit (I.STOP)
drain(15)
try: s.close()
except OSError: pass

if ceilings_hit:
    note("FINISHED WITH %d CEILING HIT(S): %s" % (len(ceilings_hit), ", ".join(ceilings_hit)))
else:
    note("all steps settled on the prompt; no ceilings hit")
print("driver done")
