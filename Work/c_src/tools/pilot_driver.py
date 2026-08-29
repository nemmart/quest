import socket, subprocess, time, os
env = dict(os.environ,
  QUEST_BLOCKS="/home/claude/quest.blocks",
  QUEST_SYNC_LIST="/home/claude/Work/c_src/quest.synclist",
  QUEST_SYNC_K="1",
  QUEST_IR="/tmp/quest.ir")
p = subprocess.Popen(["./emulator","-lockstep","-silent",
                      "-trace","/tmp/t2.tr","-types","gcalls",
                      "/tmp/QSCRATCH","QUEST_SERVER","@QUEST","@QUEST"],
                     stdout=open("/tmp/emu2.out","wb"), stderr=open("/tmp/emu2.err","wb"), env=env)
time.sleep(2)
s = socket.create_connection(("127.0.0.1",8781), timeout=10)
s.settimeout(0.5); out=b""
def drain(sec):
    global out
    end=time.time()+sec
    while time.time()<end:
        try:
            d=s.recv(4096)
            if not d: return False
            out+=d
        except socket.timeout: pass
    return True
def send_when(prompt, reply, timeout=15):
    global out
    end=time.time()+timeout
    while time.time()<end:
        if prompt in out[-300:]:
            s.sendall(reply); return True
        drain(0.3)
    return False
drain(3)
send_when(b"initials", b"ZZ\r")
send_when(b"name", b"TESTER\r")
send_when(b"Password", b"PW\r")
send_when(b"create", b"Y\r")
send_when(b"barbarian", b"F\r", 25)
drain(8)
for cmd in [b"N\r", b"L\r", b"E\r", b"N\r", b"S\r", b"W\r"]:
    s.sendall(cmd)
    drain(35)
open("/tmp/term2.out","wb").write(out)
p.terminate()
try: p.wait(timeout=5)
except: p.kill()
open("/tmp/done.flag","w").write("done")
