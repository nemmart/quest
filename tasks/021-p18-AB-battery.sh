#!/bin/bash
# Task 021 (v2) — Project 18 tranche A battery. (Step B gated on the WPSH hook,
# which also needs a loader grammar extension for 3-field push lines — deferred
# to the build session; this task validates tranche A, which is 2-field and needs
# no new code.)
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook
DRV=$W/docs/Project13/drive.py; PAT=$W/docs/Project14/drive_patient.py
RES=$ROOT/results/021-p18-AB-battery; mkdir -p $RES
PA=$W/c_src/quest.pushmap.A
: > $RES/verdicts_A.txt

leg(){ # tag mode driver [env...]
  tag=$1; mode=$2; drv=$3; shift 3
  R=/tmp/run021-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
  pkill -f "[e]mulator .*QUEST" 2>/dev/null || true; sleep 1
  env QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$PA "$@" stdbuf -o0 -e0 $EMU \
      -lockstep -silent -trace $R/trace -types lockstep,redirect,gcalls \
      QUEST QUEST_SERVER @QUEST @QUEST > $R/out 2> $R/err &
  EP=$!; sleep 6; python3 $drv $mode $R/session.log >/dev/null 2>&1 || true
  sleep 6; kill $EP 2>/dev/null || true; sleep 3; kill -9 $EP 2>/dev/null || true
  div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out 2>/dev/null || true)
  i2=$(grep -c 'MAPPER I2' $R/err 2>/dev/null || true)
  prb=$(grep -c 'MAPPER PROBE' $R/err 2>/dev/null || true)
  argwr=$(grep -c 'ARGWR' $R/trace 2>/dev/null || true)
  wwsavs=$(grep -c 'mode=W' $R/trace 2>/dev/null || true)
  # distinct decorated call sites that fired
  sites=$(grep -oE 'ARGWR pc=[0-9A-F]+' $R/trace 2>/dev/null | sort -u | wc -l || echo 0)
  endp="clean"
  grep -q 'MAPPER I2' $R/err 2>/dev/null && endp="I2"
  grep -q 'WORLD ABORT' $R/out $R/err 2>/dev/null && endp="WORLD-ABORT"
  grep -q 'DETACHED at 7017FCE8' $R/err 2>/dev/null && endp="I.STOP"
  printf "%-6s div=%-3s i2=%-3s probes=%-3s argwr=%-6s writeWSAVS=%-5s sites_fired=%-4s end=%s\n" \
    "$tag" "$div" "$i2" "$prb" "$argwr" "$wwsavs" "$sites" "$endp" | tee -a $RES/verdicts_A.txt
  cp $R/err $RES/$tag.err 2>/dev/null || true
  grep -oE 'ARGWR pc=[0-9A-F]+' $R/trace 2>/dev/null | sort -u > $RES/$tag.sites 2>/dev/null || true
}

echo "===== Project 18 tranche A: quest.pushmap.A (515 single-word sites) ====="
# sanity: does the map even load? (emulator prints decorated-call lines to stderr)
echo "--- push_map load check ---"
env QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$PA $EMU -lockstep -silent -types none \
    QUEST QUEST_SERVER @QUEST @QUEST </dev/null >/dev/null 2>$RES/loadcheck.err & LP=$!
sleep 4; kill $LP 2>/dev/null||true; sleep 1; kill -9 $LP 2>/dev/null||true
echo "decorated calls loaded: $(grep -c 'decorated call' $RES/loadcheck.err 2>/dev/null||echo 0)"
grep -i 'not inside\|not a book\|error' $RES/loadcheck.err | head -5 || echo "(no load errors)"

leg m     m        $DRV
leg fo    m        $DRV
leg inj   play     $DRV   QUEST_INJECT=7016A896:-1:0x2006
leg abort m        $DRV   QUEST_TERMINAL=7016871D:ABORT
leg play  play     $PAT
echo "--- tranche A verdicts ---"; cat $RES/verdicts_A.txt
echo "--- total distinct A sites fired across legs ---"
cat $RES/*.sites 2>/dev/null | sort -u | wc -l
echo "(of 515 decorated; unfired = coverage backlog, not a blocker)"
echo "TASK 021 DONE"
