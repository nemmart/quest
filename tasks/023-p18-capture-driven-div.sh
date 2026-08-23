#!/bin/bash
# Task 023 — P18 tranche A: CAPTURE the driven divergence (task 021 reproduced
# it on every driven leg but copied only $R/err, losing the LOCKSTEP DIVERGENCE
# dump in $R/out; task 022 dropped the driver and so never reproduced).
# One driven m leg with quest.pushmap.A, preserving out/err/trace in results.
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook
DRV=$W/docs/Project13/drive.py
RES=$ROOT/results/023-p18-capture-driven-div; mkdir -p $RES
PA=$W/c_src/quest.pushmap.A

R=/tmp/run023-m; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
pkill -f "[e]mulator .*QUEST" 2>/dev/null || true; sleep 1
env QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$PA stdbuf -o0 -e0 $EMU \
    -lockstep -silent -trace $R/trace -types lockstep,redirect,gcalls \
    QUEST QUEST_SERVER @QUEST @QUEST > $R/out 2> $R/err &
EP=$!; sleep 6; python3 $DRV m $R/session.log >/dev/null 2>&1 || true
sleep 6; kill $EP 2>/dev/null || true; sleep 3; kill -9 $EP 2>/dev/null || true

div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out 2>/dev/null || true)
echo "m leg: div=$div" | tee $RES/verdict.txt

# The evidence: full out (divergence dump lives here), err, and the trace
# tail around the end (last 400 lines is plenty — divergence is early).
cp $R/out $RES/out
cp $R/err $RES/err 2>/dev/null || true
tail -400 $R/trace > $RES/trace.tail 2>/dev/null || true
grep -n 'WSAVS' $R/trace 2>/dev/null | tail -20 > $RES/wsavs.tail || true
grep -n 'ARGWR\|LCALL' $R/trace 2>/dev/null | tail -20 > $RES/argwr.tail || true

echo "--- divergence dump (from out) ---"
grep -n -A 30 'LOCKSTEP DIVERGENCE' $R/out | head -80 || echo "(none found)"
echo "--- last write-mode WSAVS lines ---"
cat $RES/wsavs.tail
echo "TASK 023 DONE"
