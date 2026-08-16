#!/bin/bash
# Task 008 — finish B1: the four short battery legs + the (d1) inj, on the
# B1 family book (50 live). Serial. Reports verdict per leg.
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator
BOOK=$W/docs/Project14/evidence/quest.addrbook.b1fam
DRV=$W/docs/Project13/drive.py
RES=$ROOT/results/008-b1-finish; mkdir -p $RES

runleg() {  # name  drivermode  injectenv  expect-substr
  local name=$1 mode=$2 inj=$3 expect=$4
  local R=/tmp/run-008-$name; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST
  cd $R
  pkill -f "[e]mulator .*QUEST" 2>/dev/null || true; sleep 1
  env QUEST_ADDRESS_BOOK=$BOOK $inj stdbuf -o0 -e0 $EMU -lockstep -silent \
      -trace $R/trace -types scalls,rtcalls,redirect,gcalls QUEST QUEST_SERVER @QUEST @QUEST > $R/out 2> $R/err &
  local EP=$!; sleep 3
  QUEST_DRIVE_SPEED=10 python3 $DRV $mode $R/session.log >/dev/null 2>&1 || true
  sleep 2; kill $EP 2>/dev/null||true; sleep 1; kill -9 $EP 2>/dev/null||true
  local div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out || true)
  local red=$(grep -c '^redirect ' $R/trace || true)
  local end=$(grep -oE 'DETACHED|WORLD ABORT|FATAL' $R/err $R/out | head -1 || echo none)
  echo "$name: div=$div redirect=$red endpoint=$end expect=$expect" | tee -a $RES/verdicts.txt
  cp $R/err $RES/$name.err 2>/dev/null || true
}

runleg m         m        ""                                   "I.STOP detach"
runleg fo        failopen ""                                   "handler"
runleg injstd    m        "QUEST_INJECT=7016A896:-1:0x2006"    "FATAL 7017F036"
runleg abort     m        "QUEST_TERMINAL=7016871D:ABORT"      "WORLD ABORT"
echo "== B1 short legs done =="
cat $RES/verdicts.txt
echo "B1-FINISH OK"
