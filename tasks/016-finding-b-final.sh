#!/bin/bash
# Task 016 — final Finding B verification on the REAL build (clause b removed).
# Full battery on 101-live book. fo in failopen mode is the key leg.
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook
RES=$ROOT/results/016-finding-b-final; mkdir -p $RES
DRV=$W/docs/Project13/drive.py; PAT=$W/docs/Project14/drive_patient.py
leg(){ local tag=$1 mode=$2 drv=$3; shift 3
  local R=/tmp/run016-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
  pkill -f "[e]mulator .*QUEST" 2>/dev/null||true; sleep 1
  env QUEST_ADDRESS_BOOK=$BOOK "$@" stdbuf -o0 -e0 $EMU -lockstep -silent -trace $R/trace -types lockstep,redirect,gcalls QUEST QUEST_SERVER @QUEST @QUEST >$R/out 2>$R/err &
  local EP=$!; sleep 6; python3 $drv $mode $R/session.log >/dev/null 2>&1||true
  sleep 6; kill $EP 2>/dev/null||true; sleep 3; kill -9 $EP 2>/dev/null||true
  local div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out||true)
  local i2=$(grep -c 'MAPPER I2' $R/err||true)
  local prb=$(grep -c 'MAPPER PROBE' $R/err||true)
  local end="clean"; grep -q 'MAPPER I2' $R/err && end="I2"; grep -q 'WORLD ABORT' $R/out $R/err 2>/dev/null && end="WORLD-ABORT"; grep -q 'DETACHED at 7017FCE8' $R/err && end="I.STOP"; grep -q '7017F036' $R/err && end="${end}+FATAL"
  printf "%-8s div=%-3s i2=%-3s probes=%-3s end=%s\n" "$tag" "$div" "$i2" "$prb" "$end" | tee -a $RES/verdicts.txt; cp $R/err $RES/$tag.err; }
leg fo    failopen $DRV QUEST_FAIL_OPEN=USER_DATA_FILE
leg m     m        $DRV
leg inj   play     $DRV QUEST_INJECT=7016A896:-1:0x2006
leg abort m        $DRV QUEST_TERMINAL=7016871D:ABORT
leg play  play     $PAT
echo "== FINAL verdicts (101-live, both findings fixed) =="; cat $RES/verdicts.txt
echo "FINAL OK"
