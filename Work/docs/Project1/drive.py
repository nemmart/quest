#!/usr/bin/env python3
# Scripted Quest session: login, L->P (LIST_PLAYERS trigger), ESC quit.
# Usage: drive.py [extra_wait_seconds]
import socket, sys, time

wait_scale = float(sys.argv[1]) if len(sys.argv) > 1 else 1.0

s = socket.create_connection(("127.0.0.1", 8781), timeout=120)
s.settimeout(0.5)
log = open("/home/claude/session.log", "ab")

def drain(seconds):
    end = time.time() + seconds
    data = b""
    while time.time() < end:
        try:
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk
            log.write(chunk)
        except socket.timeout:
            pass
    log.flush()
    return data

def send(text, wait):
    s.sendall(text.encode())
    return drain(wait * wait_scale)

drain(8)                      # banner / initials prompt
send("CL\r", 4)               # initials
send("Claude\r", 4)           # name
send("quest\r", 4)            # password
send("Y\r", 4)                # confirm
send(" ", 4)                  # any key
send("F\r", 10)               # class F, enter game
send("L", 5)                  # list sub-menu
send("P", 15)                 # players -> LIST_PLAYERS (trigger point)
send("\x1b", 6)               # ESC quits cleanly
try:
    s.close()
except OSError:
    pass
print("driver done")
