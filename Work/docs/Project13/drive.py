#!/usr/bin/env python3
# Scripted Quest session driver — Project 13 regression battery.
# Reusable and GROWN across sessions (Project 13 ruling): add steps to `play`,
# do not fork. Modes:
#   m         login, M+n+"abc" (CONVERSION signal), a turn, ESC (I.STOP detach)
#   failopen  login, L -> P (FAIL_OPEN handler)
#   play      login, then a scripted free-play tour that exercises the hot
#             movement/render leaves (FIND_OBJECT, DIST, DISTANCE_TO_PLAYER,
#             RANDOM via AUTO_MOVE + map render), menu screens (O, D, L),
#             then ESC. This is the routine-COVERAGE run; extend it, never fork.
#   login     login, wait, ESC
import socket, sys, time
mode = sys.argv[1] if len(sys.argv) > 1 else "m"
logname = sys.argv[2] if len(sys.argv) > 2 else "/tmp/session.log"
s = socket.create_connection(("127.0.0.1", 8781), timeout=300)
s.settimeout(0.5)
log = open(logname, "ab")
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
    # --- movement: AUTO_MOVE drives FIND_OBJECT / DIST / DISTANCE_TO_PLAYER /
    #     RANDOM through the map render and territory scan every turn ---
    send("M", 4); send("n", 4); send("3\r", 160)    # auto-move 3 turns north (hot leaves)
    # --- menu screens (leaf named routines, batch 2 coverage too) ---
    send("O", 5); send("\x1b", 4)                    # OBSERVE inventory
    send("D", 5); send("\x1b", 4)                    # DISPLAY_MAGIC (spell status)
    send("L", 5); send("\x1b", 5)                    # LIST_PLAYERS
elif mode == "login":
    drain(15)
send("\x1b", 8)
drain(15)
try: s.close()
except OSError: pass
print("driver done")
