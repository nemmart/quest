#!/usr/bin/env python3
# M -> n -> wait out the turn -> then abc at whatever prompt follows.
import socket, time, re, sys

s = socket.create_connection(("127.0.0.1", 8781), timeout=180)
s.settimeout(0.5)
raw = open("/home/claude/session.log", "ab")
buf = b""

def drain(seconds):
    global buf
    end = time.time() + seconds
    while time.time() < end:
        try:
            chunk = s.recv(4096)
            if not chunk:
                break
            buf += chunk
            raw.write(chunk)
        except socket.timeout:
            pass
    raw.flush()

def wait_for(pat, timeout):
    global buf
    end = time.time() + timeout
    rx = re.compile(pat.encode())
    while time.time() < end:
        if rx.search(buf):
            return True
        drain(1)
    return False

def send(text):
    s.sendall(text.encode())

drain(8)
send("CL\r"); drain(4)
send("Claude\r"); drain(4)
send("quest\r"); drain(4)
send("Y\r"); drain(4)
send(" "); drain(4)
send("F"); drain(8)                 # class, no CR this time
buf = b""
send("M")
if wait_for("direction", 15):
    print("direction prompt seen")
buf = b""
send("n")
# wait out the move turn and watch for any follow-up prompt
drain(95)
tail = re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", buf)[-400:]
print("after-direction output:", tail.decode("ascii", "replace"))
buf = b""
send("abc\r")
drain(20)
tail = re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", buf)[-400:]
print("after-abc output:", tail.decode("ascii", "replace"))
send("\x1b"); drain(5)
try:
    s.close()
except OSError:
    pass
print("driver done")
