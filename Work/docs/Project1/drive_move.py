#!/usr/bin/env python3
# Scripted Quest session: login, M -> ABC (CONVERSION trigger attempt), ESC.
import socket, sys, time

s = socket.create_connection(("127.0.0.1", 8781), timeout=120)
s.settimeout(0.5)
log = open("/home/claude/session.log", "ab")

def drain(seconds):
    end = time.time() + seconds
    while time.time() < end:
        try:
            chunk = s.recv(4096)
            if not chunk:
                break
            log.write(chunk)
        except socket.timeout:
            pass
    log.flush()

def send(text, wait):
    s.sendall(text.encode())
    drain(wait)

drain(8)
send("CL\r", 4)
send("Claude\r", 4)
send("quest\r", 4)
send("Y\r", 4)
send(" ", 4)
send("F\r", 10)
send("M", 5)                  # move command
send("ABC\r", 80)             # garbage where a number is expected; wait a turn
send("\x1b", 6)
try:
    s.close()
except OSError:
    pass
print("driver done")
