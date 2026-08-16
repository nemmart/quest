#!/usr/bin/env python3
# combat_recon.py <logfile> [port] — adaptive wizard combat hunter (Project 14 B1).
# Login as wizard, patrol, and when a being is adjacent: A -> C -> SP (cast
# Lightning bolt) each turn until it dies or the attempt budget runs out.
# Prototype for the drive.py play-mode combat step.
import socket, sys, time
logname = sys.argv[1] if len(sys.argv) > 1 else "/tmp/combat.log"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 8781
s = socket.create_connection(("127.0.0.1", port), timeout=300)
s.settimeout(0.5)
log = open(logname, "ab")
def drain(seconds):
    end = time.time() + seconds; data = b""
    while time.time() < end:
        try:
            chunk = s.recv(4096)
            if not chunk: return data, True
            data += chunk; log.write(chunk)
        except socket.timeout: pass
        except OSError: return data, True
    log.flush(); return data, False
def send(text, wait):
    try: s.sendall(text.encode('latin1'))
    except OSError: return b"", True
    return drain(wait)
drain(10)
for step in ("WI\r", "Wizard\r", "quest\r", "Y\r", " "):
    send(step, 4)
send("W\r", 14)
kills = 0; casts = 0; died = False
dirs = "nneessww" * 3
di = 0
for attempt in range(16):
    txt, closed = send("A", 5)
    if closed: died = True; break
    t = txt.decode('latin1', 'replace')
    if "Do you want to" in t:
        t2, closed = send("C", 5)
        if closed: died = True; break
        if b"Hit space bar" in t2:
            t3, closed = send(" ", 8)
            casts += 1
            print("CAST %d -> %r" % (casts, t3.decode('latin1','replace')[:120]))
            if closed: died = True; break
            if b"done away with you" in t3: died = True; break
            # wait out the rest of the turn (being counterattack, prompts)
            t4, closed = send(" ", 45)
            if closed or b"done away with you" in t4: died = True; break
        else:
            send("I", 4)
    else:
        # no target adjacent: patrol one step
        d = dirs[di % len(dirs)]; di += 1
        t2, closed = send(d, 52)
        if closed or b"done away with you" in t2: died = True; break
print("done: casts=%d died=%s" % (casts, died))
if not died:
    send("\x1b", 8); drain(10)
try: s.close()
except OSError: pass
