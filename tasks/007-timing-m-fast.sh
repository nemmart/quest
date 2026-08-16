#!/bin/bash
# Task 007 — m-leg with all driver waits divided by QUEST_DRIVE_SPEED (10x).
# Patch applied to a /tmp copy of drive.py; the repo driver is untouched.
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
sed 's|def drain(seconds):|def drain(seconds):\n    seconds = max(0.3, seconds / float(__import__("os").environ.get("QUEST_DRIVE_SPEED", "1")))|' \
  $W/docs/Project13/drive.py > /tmp/drive_fast.py
RUN=/tmp/run-007; rm -rf $RUN; mkdir -p $RUN
cp -r $ROOT/QUEST $RUN/QUEST
cd $RUN
pkill -f "[e]mulator .*QUEST" 2>/dev/null || true; sleep 1
START=$(date +%s.%N)
env QUEST_ADDRESS_BOOK=$W/c_src/quest.addrbook stdbuf -o0 -e0 $W/c_src/emulator -lockstep -silent -trace $RUN/trace -types scalls,rtcalls,redirect,gcalls QUEST QUEST_SERVER @QUEST @QUEST > $RUN/stdout 2> $RUN/stderr &
EPID=$!
sleep 3
QUEST_DRIVE_SPEED=10 time python3 /tmp/drive_fast.py m $RUN/session.log
sleep 2
kill $EPID 2>/dev/null || true; sleep 1; kill -9 $EPID 2>/dev/null || true
END=$(date +%s.%N)
grep "^redirect " $RUN/trace > $RUN/redirect.log || true
echo "== timing: total wall = $(echo "$END $START" | awk '{printf "%.1f s", $1-$2}')"
echo "== verdict: divergences=$(grep -c 'LOCKSTEP DIVERGENCE' $RUN/stdout || true) redirect_lines=$(grep -c '' $RUN/redirect.log) detach=$(grep -c 'DETACHED' $RUN/stderr || true)"
echo "TIMING OK"
