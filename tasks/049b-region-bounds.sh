#!/bin/bash
# Task 049 — dump the region table's x/y/type after INIT_OBJ_TBL, to check
# PICK_X_Y's map bounds (15349<x<=16300, 15219<y..16350) against real data.
# Single normal game (no lockstep); QUEST_CAPTURE arms a RETURN snapshot at
# INIT_OBJ_TBL (7016DF39) with a window over the region table. OBJ_PTR
# (0x70000212) is 0 in the .PR — the table is built at login — so we read it
# live. The region records are at *OBJ_PTR + 11495 + 9*i (x,y,type = +0,+1,+2).
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=$(bin/task_source.sh main 049b-region-bounds); W=$SRC/Work
RES=$ROOT/results/049b-region-bounds; mkdir -p $RES
cd $W/emulation && make -j"$(nproc)" >/dev/null 2>&1 && cd $ROOT
EMU=$W/emulation/emulator

# 1) find *OBJ_PTR by capturing a wide window at 0x70000212 on INIT_OBJ_TBL return,
#    and a big window at a guess base; simplest: dump 0x70000212 (2 words) AND
#    the table. We don't know *OBJ_PTR statically, so capture WINDOWS: the
#    pointer itself + a large span from a plausible base. Two-pass:
#    pass A dumps the pointer; pass B dumps the table.
run() { # $1=dest $2=len $3=windows $4=dumpfile
  local R=/tmp/run049b; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST
  ( cd $R && QUEST_CAPTURE=7016DF39 QUEST_CAPTURE_DEST="$1" QUEST_CAPTURE_LEN="$2" \
      QUEST_CAPTURE_WINDOWS="$3" \
      timeout 120 $EMU QUEST QUEST_SERVER @QUEST @QUEST > $RES/game.log 2>$R/cap.txt ) &
  local emupid=$!
  sleep 6
  python3 $W/docs/Project13/drive.py login /dev/null >/dev/null 2>&1 || true
  sleep 8
  kill $emupid 2>/dev/null; wait $emupid 2>/dev/null
  cp $R/cap.txt $RES/cap.txt; cp $R/cap.txt $4
}
# pass A: the OBJ_PTR wide at 0x70000212
run "0x70000212" 2 "" $RES/objptr.txt
echo "== OBJ_PTR words ==" | tee $RES/verdicts.txt
grep -A2 "region base=70000212" $RES/objptr.txt | tee -a $RES/verdicts.txt

# extract *OBJ_PTR (little end at +1, high at +0 per the loader), compute base,
# dump the table span, decode x/y/type.
python3 - "$RES" <<'PY' | tee -a $RES/verdicts.txt
import re,sys,os
res=sys.argv[1]
txt=open(os.path.join(res,'objptr.txt')).read()
m=re.search(r'region base=70000212\n70000212:\s*([0-9A-Fa-f]{4})\s+([0-9A-Fa-f]{4})',txt)
if not m:
    print("OBJ_PTR window not found"); sys.exit(0)
hi=int(m.group(1),16); lo=int(m.group(2),16); ptr=((hi<<16)|lo)&0x7fffffff
print("*OBJ_PTR = 0x%08X"%ptr)
open(os.path.join(res,'base.txt'),'w').write("0x%08X"%ptr)
PY
BASE=$(cat $RES/base.txt 2>/dev/null)
if [ -n "$BASE" ] && [ "$BASE" != "0x00000000" ]; then
  # region_count at BASE+11502 ; table records at BASE+11495+9*i
  run "$BASE" 12000 "" $RES/table.txt
  python3 - "$RES" "$BASE" <<'PY' | tee -a $RES/verdicts.txt
import re,sys,os
res,base=sys.argv[1],int(sys.argv[2],16)
words={}
for l in open(os.path.join(res,'table.txt')):
    m=re.match(r'([0-9A-Fa-f]{8}):\s*(.*)',l)
    if not m: continue
    a=int(m.group(1),16)
    for i,v in enumerate(re.findall(r'[0-9A-Fa-f]{4}',m.group(2))): words[a+i]=int(v,16)
def sw(a):
    v=words.get(a); return None if v is None else (v-0x10000 if v>=0x8000 else v)
rc=sw(base+11502)
print("region_count(f11502) =", rc)
xs=[];ys=[];ts=[]
if rc and 0<rc<50000:
    for i in range(1,rc+1):
        rb=base+11495+9*i
        x=sw(rb);y=sw(rb+1);t=sw(rb+2)
        if x is None: break
        xs.append(x);ys.append(y);ts.append(t)
if xs:
    print("n=%d  x:[%d..%d]  y:[%d..%d]  types:%s"%(len(xs),min(xs),max(xs),min(ys),max(ys),sorted(set(ts))))
    print("PICK_X_Y gates: 15349<x<=16300, 15219<y, y<=16350")
    print("x in-gate:%d/%d  y in-gate:%d/%d"%(
      sum(1 for x in xs if 15349<x<=16300),len(xs),
      sum(1 for y in ys if 15219<y<=16350),len(ys)))
else:
    print("no region rows decoded (table window may be short or base wrong)")
PY
else
  echo "OBJ_PTR still 0 at INIT_OBJ_TBL return — table built later; needs a post-move capture" | tee -a $RES/verdicts.txt
fi
echo "TASK 049b DONE" | tee -a $RES/verdicts.txt
