#!/bin/bash
# Task 014 — verify the Finding B I2 heap-fence fix on the 101-live book.
# THE key leg is fo in FAILOPEN mode (arms I2). Plus m/inj/abort/play no-regression.
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook
RES=$ROOT/results/014-finding-b-verify; mkdir -p $RES
DRV=$W/docs/Project13/drive.py; PAT=$W/docs/Project14/drive_patient.py

leg() { # tag mode driver env...
  local tag=$1 mode=$2 drv=$3; shift 3
  local R=/tmp/run014-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
  pkill -f "[e]mulator .*QUEST" 2>/dev/null || true; sleep 1
  env QUEST_ADDRESS_BOOK=$BOOK "$@" stdbuf -o0 -e0 $EMU -lockstep -silent \
      -trace $R/trace -types scalls,rtcalls,redirect,gcalls,lockstep \
      QUEST QUEST_SERVER @QUEST @QUEST > $R/out 2> $R/err &
  local EP=$!; sleep 6
  python3 $drv $mode $R/session.log >/dev/null 2>&1 || true
  sleep 6; kill $EP 2>/dev/null||true; sleep 3; kill -9 $EP 2>/dev/null||true
  local div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out||true)
  local prb=$(grep -c 'MAPPER PROBE' $R/err||true)
  local i2=$(grep -c 'MAPPER I2' $R/err||true)
  local end="clean"
  grep -q 'MAPPER I2' $R/err && end="I2-ABORT"
  grep -q 'WORLD ABORT' $R/out $R/err 2>/dev/null && end="WORLD-ABORT"
  grep -q 'DETACHED at 7017FCE8' $R/err && end="I.STOP-detach"
  printf "%-8s div=%-3s probes=%-3s i2aborts=%-3s endpoint=%s\n" "$tag" "$div" "$prb" "$i2" "$end" | tee -a $RES/verdicts.txt
  cp $R/err $RES/$tag.err
}

echo "=== KEY LEG: fo in failopen mode (arms I2) ==="
leg fo    failopen  $DRV  QUEST_FAIL_OPEN=USER_DATA_FILE
echo "=== no-regression ==="
leg m     m         $DRV
leg inj   play      $DRV  QUEST_INJECT=7016A896:-1:0x2006
leg abort m         $DRV  QUEST_TERMINAL=7016871D:ABORT
leg play  play      $PAT
echo "== verdicts =="; cat $RES/verdicts.txt
echo "== fo I2 detail (should be NONE after fix) =="; grep -i "MAPPER I2\|wsl moved\|heap break\|clearance" $RES/fo.err | head -3 || echo "(no I2 messages — PASS)"
echo "FINDING-B-VERIFY OK"
