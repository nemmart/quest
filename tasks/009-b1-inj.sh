#!/bin/bash
# Task 009 — the standing inj leg at NORMAL driver speed (timing-sensitive).
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
BOOK=$W/docs/Project14/evidence/quest.addrbook.b1fam
R=/tmp/run-009; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
pkill -f "[e]mulator .*QUEST" 2>/dev/null || true; sleep 1
env QUEST_ADDRESS_BOOK=$BOOK QUEST_INJECT=7016A896:-1:0x2006 stdbuf -o0 -e0 \
    $W/c_src/emulator -lockstep -silent -trace $R/trace -types scalls,rtcalls,redirect,gcalls \
    QUEST QUEST_SERVER @QUEST @QUEST > $R/out 2> $R/err &
EP=$!; sleep 4
python3 $W/docs/Project13/drive.py m $R/session.log >/dev/null 2>&1 || true   # NORMAL speed
sleep 6; kill $EP 2>/dev/null||true; sleep 1; kill -9 $EP 2>/dev/null||true
echo "div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out||true) redirect=$(grep -c '^redirect ' $R/trace||true)"
echo "endpoint: $(grep -oE 'FATAL|7017F036|DETACHED' $R/err $R/out | head -2 | tr '\n' ' ')"
grep -q 7017F036 $R/err $R/out && echo "INJ FIRED (?FATAL 7017F036)" || echo "INJ DID NOT FIRE"
echo "INJ-CHECK OK"
