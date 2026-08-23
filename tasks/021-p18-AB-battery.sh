#!/bin/bash
# Task 021 — Project 18 tranche A & B battery.
# Step A: load quest.pushmap.A (515 flat single-word sites; NO new code needed),
#         full battery, expect div=0, report per-leg site coverage.
# Step B: load quest.pushmap.AB (adds 20 WPSH multi-slot sites). Requires the
#         WPSH multi-slot store hook in EagleStack.cpp. If the hook is NOT present
#         the build/run of step B is skipped with a clear note (hand to a build
#         session). Step A stands alone.
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook
DRV=$W/docs/Project13/drive.py; PAT=$W/docs/Project14/drive_patient.py
RES=$ROOT/results/021-p18-AB-battery; mkdir -p $RES

leg(){ # tag mode driver pushmap env...
  local tag=$1 mode=$2 drv=$3 pmap=$4; shift 4
  local R=/tmp/run021-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
  pkill -f "[e]mulator .*QUEST" 2>/dev/null||true; sleep 1
  env QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$pmap "$@" stdbuf -o0 -e0 $EMU \
      -lockstep -silent -trace $R/trace -types lockstep,redirect,gcalls \
      QUEST QUEST_SERVER @QUEST @QUEST >$R/out 2>$R/err &
  local EP=$!; sleep 6; python3 $drv $mode $R/session.log >/dev/null 2>&1||true
  sleep 6; kill $EP 2>/dev/null||true; sleep 3; kill -9 $EP 2>/dev/null||true
  local div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out||true)
  local i2=$(grep -c 'MAPPER I2' $R/err||true)
  local prb=$(grep -c 'MAPPER PROBE' $R/err||true)
  local m4b=$(grep -c 'm4b\|args_written' $R/err||true)
  local argwr=$(grep -c 'ARGWR' $R/trace||true)
  local wwsavs=$(grep -c 'mode=W' $R/trace||true)
  local end="clean"; grep -q 'MAPPER I2' $R/err && end="I2"; grep -q 'WORLD ABORT' $R/out $R/err 2>/dev/null && end="WORLD-ABORT"; grep -q 'DETACHED at 7017FCE8' $R/err && end="I.STOP"
  printf "%-8s div=%-3s i2=%-3s probes=%-3s argwr=%-6s writeWSAVS=%-5s end=%s\n" "$tag" "$div" "$i2" "$prb" "$argwr" "$wwsavs" "$end" | tee -a $RES/$5
  # site coverage: distinct decorated call sites that fired
  grep 'LCALL.*marker\|ARGWR' $R/trace 2>/dev/null | grep -oE 'pc=[0-9A-F]+' | sort -u | wc -l >> $RES/$tag.$5.sites 2>/dev/null || true
  cp $R/err $RES/$tag.$5.err 2>/dev/null||true
}

echo "===== STEP A: quest.pushmap.A (515 single-word sites) ====="
PA=$W/c_src/quest.pushmap.A
leg m     m        $DRV $PA verdicts_A.txt
leg fo    m        $DRV $PA verdicts_A.txt
leg inj   play     $DRV $PA verdicts_A.txt   QUEST_INJECT=7016A896:-1:0x2006
leg abort m        $DRV $PA verdicts_A.txt   QUEST_TERMINAL=7016871D:ABORT
leg play  play     $PAT $PA verdicts_A.txt
echo "--- STEP A verdicts ---"; cat $RES/verdicts_A.txt

echo ""
echo "===== STEP B: quest.pushmap.AB (adds 20 WPSH multi-slot sites) ====="
# detect whether the WPSH multi-slot hook exists (a caller_write/note_arg_write in case WPSH)
if grep -A15 'case WPSH' $W/c_src/hw/EagleStack.cpp | grep -q 'caller_write\|note_arg_write'; then
  echo "WPSH hook present — running step B"
  PAB=$W/c_src/quest.pushmap.AB
  leg m-B   m        $DRV $PAB verdicts_B.txt
  leg fo-B  m        $DRV $PAB verdicts_B.txt
  leg inj-B play     $DRV $PAB verdicts_B.txt  QUEST_INJECT=7016A896:-1:0x2006
  leg play-B play    $PAT $PAB verdicts_B.txt
  echo "--- STEP B verdicts ---"; cat $RES/verdicts_B.txt
  echo "--- TERRAIN WPSH window sample (expect 3 words to C2/C4/C6, off += 6) ---"
  grep -m8 'ARGWR.*74009' /tmp/run021-m-B/trace 2>/dev/null || echo "(capture ARGWR lines for TERRAIN)"
else
  echo "WPSH multi-slot hook NOT present in EagleStack.cpp case WPSH."
  echo "STEP B SKIPPED — hand to a build session to implement the hook (see"
  echo "Project18/REPORT.md §4 for the AC0->lowest-slot ordering), then re-run"
  echo "with QUEST_PUSH_MAP=quest.pushmap.AB."
fi
echo "TASK 021 DONE"
