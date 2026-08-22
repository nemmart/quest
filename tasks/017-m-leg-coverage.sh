#!/bin/bash
# Task 017 — what routines fire in the FAST m leg? (site selection for M4b P16)
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
R=/tmp/run017; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
pkill -f "[e]mulator .*QUEST" 2>/dev/null||true; sleep 1
env QUEST_ADDRESS_BOOK=$W/c_src/quest.addrbook stdbuf -o0 -e0 $W/c_src/emulator \
  -lockstep -silent -trace $R/trace -types redirect,gcalls \
  QUEST QUEST_SERVER @QUEST @QUEST >$R/out 2>$R/err &
EP=$!; sleep 6; python3 $W/docs/Project13/drive.py m $R/session.log >/dev/null 2>&1||true
sleep 4; kill $EP 2>/dev/null||true; sleep 2; kill -9 $EP 2>/dev/null||true
RES=$ROOT/results/017-m-leg-coverage; mkdir -p $RES
echo "=== routines redirected in m leg (count) ==="
grep '^redirect ' $R/trace | sed -n 's/.*WSAVS \([A-Za-z0-9_.@]*\).*/\1/p' | sort | uniq -c | sort -rn | tee $RES/m_coverage.txt
echo "=== game->game CALLS in m leg to candidate callees ==="
for r in DIST DISTANCE_TO_PLAYER RANDOM OWNS GET_INPUT REFRESH_SCREEN DISPLAY_SCREEN; do
  n=$(grep -c "target=.* $r " $R/trace 2>/dev/null || echo 0); echo "  $r called: $n"
done | tee $RES/m_calls.txt
echo "COVERAGE OK"
