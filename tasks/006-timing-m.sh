#!/bin/bash
# Task 006 — timing benchmark: the m leg (login + M-trigger + ESC) on godspeed.
# Adapted from Work/docs/Project13/run.sh, paths made repo-relative.
# Reports wall-clock and the standard verdict line.
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd)
W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator
BOOK=$W/c_src/quest.addrbook
DRV=$W/docs/Project13/drive.py
RUN=/tmp/run-006; rm -rf $RUN; mkdir -p $RUN
cp -r $ROOT/QUEST $RUN/QUEST
cd $RUN
pkill -f "[e]mulator .*QUEST" 2>/dev/null || true; sleep 1
START=$(date +%s.%N)
env QUEST_ADDRESS_BOOK=$BOOK stdbuf -o0 -e0 $EMU -lockstep -silent -trace $RUN/trace -types scalls,rtcalls,redirect,gcalls QUEST QUEST_SERVER @QUEST @QUEST > $RUN/stdout 2> $RUN/stderr &
EPID=$!
sleep 6
time python3 $DRV m $RUN/session.log
sleep 5
kill $EPID 2>/dev/null || true; sleep 2; kill -9 $EPID 2>/dev/null || true
END=$(date +%s.%N)
grep "^redirect " $RUN/trace > $RUN/redirect.log || true
echo "== timing: total wall = $(echo "$END $START" | awk '{printf "%.1f s", $1-$2}') (includes 11 s of fixed sleeps in this wrapper)"
echo "== verdict: divergences=$(grep -c 'LOCKSTEP DIVERGENCE' $RUN/stdout || true) redirect_lines=$(grep -c '' $RUN/redirect.log) detach=$(grep -c 'DETACHED' $RUN/stderr || true)"
cp $RUN/redirect.log "$ROOT/results/006-timing-m/" 2>/dev/null || true
echo "TIMING OK"
