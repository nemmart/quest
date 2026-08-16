#!/usr/bin/env python3
# Scripted Quest session driver for Project 12 regressions.
#   drive.py m         login, M + n + "abc" (CONVERSION signal), wait a turn, ESC (I.STOP detach)
#   drive.py failopen  login, L -> P (FAIL_OPEN handler), short wait, ESC
#   drive.py login     login, wait, ESC
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
    s.sendall(text.encode()); return drain(wait)
drain(10)
send("CL\r", 4); send("Claude\r", 4); send("quest\r", 4); send("Y\r", 4); send(" ", 4)
send("F\r", 12)
if mode == "m":
    send("M", 4); send("n", 4); send("abc\r", 8)   # CONVERSION at "For how many turns?"
    drain(70)                                    # let a turn run
elif mode == "failopen":
    send("L", 5); send("P", 15)
else:
    drain(15)
send("\x1b", 8)
drain(15)
try: s.close()
except OSError: pass
print("driver done")
