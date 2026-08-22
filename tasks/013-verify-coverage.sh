#!/bin/bash
# Task 013 — corrected verdict extraction for the 012 battery. Re-run m/fo/play,
# parse routine names properly (field after "WSAVS" in the redirect line),
# report DISPLAY_SCREEN + the 5 menu dyn routines explicitly, and read the TRUE
# fo endpoint (I2 abort vs I.STOP detach vs WORLD ABORT).
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook
RES=$ROOT/results/013-verify-coverage; mkdir -p $RES
PAT=$W/docs/Project14/drive_patient.py; DRV=$W/docs/Project13/drive.py

leg() { # tag mode driver env...
  local tag=$1 mode=$2 drv=$3; shift 3
  local R=/tmp/run013-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
  pkill -f "[e]mulator .*QUEST" 2>/dev/null || true; sleep 1
  env QUEST_ADDRESS_BOOK=$BOOK "$@" stdbuf -o0 -e0 $EMU -lockstep -silent \
      -trace $R/trace -types scalls,rtcalls,redirect,gcalls,lockstep \
      QUEST QUEST_SERVER @QUEST @QUEST > $R/out 2> $R/err &
  local EP=$!; sleep 6
  python3 $drv $mode $R/session.log >/dev/null 2>&1 || true
  sleep 6; kill $EP 2>/dev/null||true; sleep 3; kill -9 $EP 2>/dev/null||true
  local div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out||true)
  local prb=$(grep -c 'MAPPER PROBE' $R/err||true)
  # routine name = field after "WSAVS" on a redirect line
  grep '^redirect ' $R/trace | sed -n 's/.*WSAVS \([A-Za-z0-9_.@]*\).*/\1/p' | sort | uniq -c | sort -rn > $RES/$tag.cov
  local ds=$(grep -c 'DISPLAY_SCREEN' $RES/$tag.cov||true)
  # true endpoint
  local end="?"
  grep -q 'MAPPER I2' $R/err && end="I2-ABORT"
  grep -q 'WORLD ABORT' $R/out $R/err 2>/dev/null && end="WORLD-ABORT"
  grep -q 'DETACHED at 7017FCE8' $R/err && end="I.STOP-detach"
  grep -q '7017F036' $R/err && grep -q 'FATAL\|DETACH' $R/err && end="${end}/FATAL7017F036"
  printf "%-6s div=%-3s probes=%-3s DISPLAY_SCREEN_lines=%-3s endpoint=%s\n" "$tag" "$div" "$prb" "$ds" "$end" | tee -a $RES/verdicts.txt
  cp $R/err $RES/$tag.err
}

leg m    m    $DRV
leg fo   m    $DRV  QUEST_FAIL_OPEN=USER_DATA_FILE
leg play play $PAT

echo "== menu dyn routines exercised in play? =="
for r in DISPLAY_SCREEN DISPLAY_MAGIC DISPLAY_CAVE DISPLAY_INVENTORY DIED DROP LIST_PLAYERS; do
  n=$(grep -w $r $RES/play.cov 2>/dev/null | awk '{print $1}'); echo "  $r: ${n:-0}"
done
echo "VERIFY OK"
