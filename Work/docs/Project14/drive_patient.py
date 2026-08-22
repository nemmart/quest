#!/usr/bin/env python3
# One-off patient variant of docs/Project13/drive.py `play` mode for the
# Finding A verification (container turn pacing outlasts the stock 160s
# drain). Same protocol/steps; only waits differ + a prompt-aware wait
# after auto-move. NOT a fork of record — repo driver untouched.
import socket, sys, time
mode = sys.argv[1] if len(sys.argv) > 1 else "play"
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
def wait_turn_done(ceiling):
    # keep draining until output goes quiet with no "Waiting for your turn"
    end = time.time() + ceiling
    while time.time() < end:
        data = drain(20)
        if b"Waiting for your turn" not in data and len(data) < 200:
            return
drain(10)
send("CL\r", 4); send("Claude\r", 4); send("quest\r", 4); send("Y\r", 4); send(" ", 4)
send("F\r", 12)
send("M", 4); send("n", 4); send("3\r", 30)
wait_turn_done(420)
send("O", 8); send("\x1b", 8)                    # OBSERVE inventory
send("D", 8); send("\x1b", 8)                    # DISPLAY_MAGIC
send("L", 8); send("\x1b", 8)                    # LIST_PLAYERS
send("\x1b", 8)
drain(15)
try: s.close()
except OSError: pass
print("driver done")
