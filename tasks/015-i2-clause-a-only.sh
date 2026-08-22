#!/bin/bash
# Task 015 — isolate: does clause (a) (wsl-heap_break latch) alone fix fo, with
# clause (b) (stack clearance) DISABLED? Confirms (a) is the real fix and (b) is
# the mis-coordinated over-check (aborts on every leg incl. m, with a 0x74 'high
# water' = shadow_wsp fed a non-stack wsp). Patches a /tmp copy — repo unchanged.
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
# disable clause (b): comment the clearance abort to a warning-only, in a temp build
cp -r $W /tmp/Wb; MB=/tmp/Wb/c_src/hw/Mapper.cpp
python3 - <<'PY'
p='/tmp/Wb/c_src/hw/Mapper.cpp'; s=open(p).read()
# neuter clause (b): turn the "if(wsl_now <= clear){...abort...}" into a no-op guarded block
import re
i=s.index('// (b) Stack clearance.')
j=s.index('int32_t clear = shadow_wsp(m.wsp);')
# replace from clear-decl to the end of its abort block with 'return;'
k=s.index('mapper_abort(owner_, buf);', j)
k=s.index('}', k)+1
s=s[:j]+'return; // CLAUSE (b) DISABLED for task 015 isolation\n'+s[k:]
open(p,'w').write(s); print("clause b disabled")
PY
cd /tmp/Wb/c_src && make -j"$(nproc)" >/dev/null 2>&1 && echo "built (a)-only"
EMU=/tmp/Wb/c_src/emulator; BOOK=$W/c_src/quest.addrbook
RES=$ROOT/results/015-i2-clause-a-only; mkdir -p $RES
DRV=$W/docs/Project13/drive.py
leg(){ local tag=$1 mode=$2; shift 2
  local R=/tmp/run015-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
  pkill -f "[e]mulator .*QUEST" 2>/dev/null||true; sleep 1
  env QUEST_ADDRESS_BOOK=$BOOK "$@" stdbuf -o0 -e0 $EMU -lockstep -silent -trace $R/trace -types lockstep,redirect QUEST QUEST_SERVER @QUEST @QUEST >$R/out 2>$R/err &
  local EP=$!; sleep 6; python3 $DRV $mode $R/session.log >/dev/null 2>&1||true
  sleep 6; kill $EP 2>/dev/null||true; sleep 3; kill -9 $EP 2>/dev/null||true
  local div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out||true)
  local i2=$(grep -c 'MAPPER I2' $R/err||true)
  local end="clean"; grep -q 'MAPPER I2' $R/err && end="I2"; grep -q 'WORLD ABORT' $R/out $R/err 2>/dev/null && end="WORLD-ABORT"; grep -q 'DETACHED at 7017FCE8' $R/err && end="I.STOP"
  printf "%-8s div=%-3s i2=%-3s end=%s\n" "$tag" "$div" "$i2" "$end" | tee -a $RES/verdicts.txt; }
leg fo failopen QUEST_FAIL_OPEN=USER_DATA_FILE
leg m  m
echo "== verdicts (clause a only) =="; cat $RES/verdicts.txt
echo "CLAUSE-A OK"
