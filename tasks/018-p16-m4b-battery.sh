#!/bin/bash
# Task 018 — Project 16 Stage 2: M4b first slice, ONE converted site
# (DIST,4 at 70166E1C writes args into DIST's area; every other site
# stays M4a copy mode). Full battery on the 101-live book + quest.pushmap,
# task-016 leg shape. Evidence: decorated-window trace excerpt, first two
# DIST write-mode calls' shadow_wsp (the W-2 fixup and +2*argc offset
# first interact across call 2), dangling-flag grep, mode=C coexistence.
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook; PMAP=$W/c_src/quest.pushmap
RES=$ROOT/results/018-p16-m4b-battery; mkdir -p $RES
DRV=$W/docs/Project13/drive.py; PAT=$W/docs/Project14/drive_patient.py
leg(){ local tag=$1 mode=$2 drv=$3; shift 3
  local R=/tmp/run018-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
  pkill -f "[e]mulator .*QUEST" 2>/dev/null||true; sleep 1
  env QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$PMAP "$@" stdbuf -o0 -e0 $EMU -lockstep -silent -trace $R/trace -types lockstep,redirect,gcalls QUEST QUEST_SERVER @QUEST @QUEST >$R/out 2>$R/err &
  local EP=$!; sleep 6; python3 $drv $mode $R/session.log >/dev/null 2>&1||true
  sleep 6; kill $EP 2>/dev/null||true; sleep 3; kill -9 $EP 2>/dev/null||true
  local div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out||true)
  local i2=$(grep -c 'MAPPER I2' $R/err||true)
  local prb=$(grep -c 'MAPPER PROBE' $R/err||true)
  local m4b=$(grep -c 'M4B:' $R/out $R/err 2>/dev/null|awk -F: '{s+=$NF}END{print s+0}')
  local wsavs_w=$(grep -cE 'WSAVS DIST +mode=W' $R/trace||true)
  local argwr=$(grep -c 'ARGWR pc=' $R/trace||true)
  local end="clean"; grep -q 'MAPPER I2' $R/err && end="I2"; grep -q 'WORLD ABORT' $R/out $R/err 2>/dev/null && end="WORLD-ABORT"; grep -q 'DETACHED at 7017FCE8' $R/err && end="I.STOP"; grep -q '7017F036' $R/err && end="${end}+FATAL"
  printf "%-8s div=%-3s i2=%-3s probes=%-3s m4b_aborts=%-3s writeWSAVS=%-5s argwr=%-6s end=%s\n" \
    "$tag" "$div" "$i2" "$prb" "$m4b" "$wsavs_w" "$argwr" "$end" | tee -a $RES/verdicts.txt
  cp $R/err $RES/$tag.err
  # m leg is the hammer (site 70166E1C ~600 clone calls / 20s): keep the evidence
  if [ "$tag" = "m" ]; then
    grep -E '^redirect .*(ARGWR pc=|LCALL pc=70166E1C)' $R/trace | head -24 > $RES/m_decorated_window.txt || true
    grep -E '^redirect .*(WSAVS DIST +mode=W|WRTN  DIST +mode=W)' $R/trace | head -8 > $RES/m_dist_first_calls.txt || true
    grep -E '^redirect .*mode=C' $R/trace | head -6 > $RES/m_copy_mode_sample.txt || true
    { echo "decorated-site LCALLs (clone): $(grep -c 'LCALL pc=70166E1C' $R/trace||true)"
      echo "ARGWR writes (clone):          $(grep -c 'ARGWR pc=' $R/trace||true)"
      echo "write-mode WSAVS (DIST):       $(grep -cE 'WSAVS DIST +mode=W' $R/trace||true)"
      echo "write-mode WRTN  (DIST):       $(grep -cE 'WRTN  DIST +mode=W' $R/trace||true)"
      echo "copy-mode WSAVS (all others):  $(grep -c 'mode=C pc=' $R/trace||true)"
      echo "gcalls at site 70166E1C:       $(grep -c 'site=70166E1C' $R/trace||true)"
    } > $RES/m_counts.txt
  fi
}
leg fo    failopen $DRV QUEST_FAIL_OPEN=USER_DATA_FILE
leg m     m        $DRV
leg inj   play     $DRV QUEST_INJECT=7016A896:-1:0x2006
leg abort m        $DRV QUEST_TERMINAL=7016871D:ABORT
leg play  play     $PAT
echo "== P16 verdicts (101-live + one converted site) =="; cat $RES/verdicts.txt
echo "== m-leg M4b counts =="; cat $RES/m_counts.txt
echo "== first decorated windows =="; cat $RES/m_decorated_window.txt
echo "== DIST write-mode calls 1-2 (shadow_wsp evidence) =="; cat $RES/m_dist_first_calls.txt
echo "P16 BATTERY OK"
