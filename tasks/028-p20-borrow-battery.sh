#!/bin/bash
# Task 028 — P20 FINAL M4b tranche: WPSH/WPOP frame-borrows off-stack.
# Map = quest.pushmap.M4 (566 arg/call sites rebased +0x5C onto the
# shifted book + 46 borrow pcs / 23 pairs at [74000000,7400005C)).
# Landing criteria (PROMPT.md step 7):
#   - div=0 on all legs
#   - AC[r] ROUND-TRIP BY VALUE: every fired WPSH's stored value == its
#     WPOP's loaded value (mismatch would be silent like P18 WPSH-order)
#   - offset net-zero: ARGRD off == paired ARGWR off + 2, bracket closes
#   - i2/probes/m4b/mapper aborts all 0; baseline counters vs task 026
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook
DRV=$W/docs/Project13/drive.py; PAT=$W/docs/Project14/drive_patient.py
RES=$ROOT/results/028-p20-borrow-battery; mkdir -p $RES
PM=$W/c_src/quest.pushmap.M4
: > $RES/verdicts.txt

# round-trip verifier: pairs ARGWR/ARGRD at borrow slots by value+pc+offset
cat > /tmp/rt028.py <<'PYEOF'
import re,sys,collections
pair_of={}
for l in open(sys.argv[2]):
    m=re.match(r"borrow ([0-9A-F]{8}) ([0-9A-F]{8})\s+# slot(\d+) (WPSH|WPOP)",l)
    if m: pair_of.setdefault(m.group(2),[None,None])[0 if m.group(4)=="WPSH" else 1]=m.group(1)
state={}; rt=0; bad=0; fired=set()
for l in open(sys.argv[1]):
    m=re.search(r"(ARGWR|ARGRD) pc=([0-9A-F]{8}) slot=([0-9A-F]{8}) value=([0-9A-F]{8}) off=(-?\d+)",l)
    if not m: continue
    kind,pc,slot,val,off=m.groups()
    if slot not in pair_of: continue
    wpsh,wpop=pair_of[slot]
    if kind=="ARGWR":
        if pc!=wpsh or slot in state: bad+=1; print("BAD WR",l.strip())
        else: state[slot]=(val,int(off)); fired.add(slot)
    else:
        if pc!=wpop or slot not in state: bad+=1; print("BAD RD",l.strip())
        else:
            v,o=state.pop(slot)
            if v!=val: bad+=1; print("VALUE MISMATCH",slot,v,val)
            if int(off)!=o+2: bad+=1; print("OFFSET MISMATCH",slot,o,off)
            rt+=1
print("roundtrips=%d unclosed=%d bad=%d pairs_fired=%d/23 slots=%s"%(
    rt,len(state),bad,len(fired),",".join(sorted(s[-2:] for s in fired))))
sys.exit(1 if (bad or state) else 0)
PYEOF

leg(){ # tag mode driver [env...]
  tag=$1; mode=$2; drv=$3; shift 3
  R=/tmp/run028-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
  pkill -f "[e]mulator .*QUEST" 2>/dev/null || true; sleep 1
  env QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$PM "$@" stdbuf -o0 -e0 $EMU \
      -lockstep -silent -trace $R/trace -types lockstep,redirect,gcalls \
      QUEST QUEST_SERVER @QUEST @QUEST > $R/out 2> $R/err &
  EP=$!; sleep 6; python3 $drv $mode $R/session.log >/dev/null 2>&1 || true
  sleep 6; kill $EP 2>/dev/null || true; sleep 3; kill -9 $EP 2>/dev/null || true
  div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out 2>/dev/null || true)
  i2=$(grep -c 'MAPPER I2' $R/err 2>/dev/null || true)
  prb=$(grep -c 'MAPPER PROBE' $R/err 2>/dev/null || true)
  m4ab=$(grep -c 'M4B ABORT\|m4b_abort' $R/err 2>/dev/null || true)
  mab=$(grep -c 'MAPPER.*abort\|mapper_abort' $R/err 2>/dev/null || true)
  argwr=$(grep -c 'ARGWR' $R/trace 2>/dev/null || true)
  argrd=$(grep -c 'ARGRD' $R/trace 2>/dev/null || true)
  wsavsw=$(grep -c 'WSAVS.*mode=W' $R/trace 2>/dev/null || true)
  wrtnw=$(grep -c 'WRTN.*mode=W' $R/trace 2>/dev/null || true)
  rtv=$(python3 /tmp/rt028.py $R/trace $W/c_src/quest.pushmap.borrows 2>&1 | tail -1) || rtfail=1
  printf "%-6s div=%-3s i2=%-3s probes=%-3s m4b_ab=%-3s map_ab=%-3s argwr=%-6s argrd=%-5s wWSAVS=%-6s wWRTN=%-6s %s\n" \
    "$tag" "$div" "$i2" "$prb" "$m4ab" "$mab" "$argwr" "$argrd" "$wsavsw" "$wrtnw" "$rtv" | tee -a $RES/verdicts.txt
  cp $R/out $RES/$tag.out; cp $R/err $RES/$tag.err 2>/dev/null || true
  grep -E 'ARGWR pc=(7015F7|70165(34|35)|70166|7016A3|7016BC|7016DF|7016E7|70178A)' $R/trace 2>/dev/null | \
    grep -E 'slot=740000[0-5]' > $RES/$tag.borrow_wr || true
  grep ARGRD $R/trace > $RES/$tag.borrow_rd 2>/dev/null || true
  if [ "$div" != "0" ]; then grep -A 30 'LOCKSTEP DIVERGENCE' $R/out | head -80 > $RES/$tag.divdump; fi
}

echo "--- book + pushmap.M4 load check ---"
R=/tmp/run028-load; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
env QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$PM $EMU -lockstep -silent \
    QUEST QUEST_SERVER @QUEST @QUEST </dev/null >/dev/null 2>$RES/loadcheck.err & LP=$!
sleep 4; kill $LP 2>/dev/null||true; sleep 1; kill -9 $LP 2>/dev/null||true
cd $ROOT
grep -E 'borrow block|caller map' $RES/loadcheck.err || true
echo "expect: 23 slots [74000000,7400005C), 1313 pushes, 566 calls, 46 borrow pcs"
grep -iE 'not inside|not a book|out of range|bad push-map|duplicate|not a reserved|bad line' $RES/loadcheck.err | head -5 || echo "(no load errors)"

# the five standing legs
leg m     m        $DRV
leg fo    m        $DRV
leg inj   play     $DRV   QUEST_INJECT=7016A896:-1:0x2006
leg abort m        $DRV   QUEST_TERMINAL=7016871D:ABORT
leg play  play     $PAT

echo "--- P20 verdicts ---"; cat $RES/verdicts.txt
echo "--- distinct borrow pairs fired across all legs (of 23) ---"
cat $RES/*.borrow_rd 2>/dev/null | grep -oE 'slot=740000[0-9A-F]{2}' | sort -u | tee $RES/pairs_fired | wc -l
echo "--- baseline comparison: task 026 verdicts for reference ---"
cat $ROOT/results/026-p19-CD-battery/verdicts.txt 2>/dev/null || echo "(026 verdicts not present)"
echo "TASK 028 DONE"
