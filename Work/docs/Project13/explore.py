#!/usr/bin/env python3
# explore.py <dir>  — background telnet driver. Reads step lines from <dir>/cmd (one per line, key:wait),
# appends screen text to <dir>/screen.txt, raw bytes to <dir>/session.log. A line "QUIT" closes.
import socket, sys, time, re, os
d=sys.argv[1]; log=open(d+"/session.log","ab"); out=open(d+"/screen.txt","a")
s=socket.create_connection(("127.0.0.1",8781),timeout=300); s.settimeout(0.5)
KEYS={"ESC":"\x1b","UP":"\x17","DOWN":"\x1a","LEFT":"\x19","RIGHT":"\x18","SP":" ","CR":"\r","EP":"\x0c"}
def clean(b):
    t=b.decode('latin1'); t=re.sub(r'[\x00-\x08\x0b-\x1f]','\n',t)
    return ' | '.join(l.strip() for l in t.split('\n') if l.strip())
def drain(sec):
    end=time.time()+sec; dd=b""
    while time.time()<end:
        try:
            c=s.recv(4096)
            if not c: break
            dd+=c; log.write(c)
        except socket.timeout: pass
    log.flush(); return dd
def emit(x): out.write(x+"\n"); out.flush()
emit("INIT: "+clean(drain(8))[:400])
done=0
while True:
    txt=open(d+"/cmd").read() if os.path.exists(d+"/cmd") else ""
    lines=[l for l in txt.split("\n")[:-1] if l.strip()]   # complete, non-empty lines only
    if len(lines)>done:
        st=lines[done].strip(); done+=1
        if st=="QUIT": break
        key,wait=st.rsplit(":",1); k=KEYS.get(key,key.replace("\\r","\r"))
        s.sendall(k.encode('latin1'))
        emit("SENT %r -> %s"%(key,clean(drain(float(wait)))[:600]))
    else:
        d2=drain(1)
        if d2: emit("ASYNC: "+clean(d2)[:600])
s.close(); emit("CLOSED")
