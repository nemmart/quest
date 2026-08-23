#!/bin/bash
# Task 020 — Project 17 Stage 2: stack_offset (mid-window checkpoints) +
# QUEST as the base record. Re-run of the task-018 battery shape on the
# now-102-live book (QUEST uncommented) + the DIST push map. Expect div=0
# on ALL legs (the P16 mid-window condition is the target: pairs at
# 70166E19 with off>0 must PASS). Speeds per P17 prompt: inj at NORMAL
# driver speed, m/fo/play/abort at 10x (QUEST_DRIVE_SPEED via the task-007
# sed patch; repo driver untouched).
# Evidence kept from the m leg:
#   - passing mid-window pairs (lockstep pair lines at window pcs, off>0,
#     no DIVERGED)
#   - the DIST window trace (off climbing +2/push, HOLDING through LCALL,
#     consumed to 0 at the write-mode WSAVS — P17 Stage-0 ruling)
#   - QUEST base record: first redirect line = WSAVS QUEST depth=1
#   - copy-mode coexistence sample
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook; PMAP=$W/c_src/quest.pushmap
RES=$ROOT/results/020-p17-stackoffset-battery; mkdir -p $RES
DRV=$W/docs/Project13/drive.py; PAT=$W/docs/Project14/drive_patient.py
sed 's|def drain(seconds):|def drain(seconds):\n    seconds = max(0.3, seconds / float(__import__("os").environ.get("QUEST_DRIVE_SPEED", "1")))|' \
  $DRV > /tmp/drive_fast_020.py
sed 's|def drain(seconds):|def drain(seconds):\n    seconds = max(0.3, seconds / float(__import__("os").environ.get("QUEST_DRIVE_SPEED", "1")))|' \
  $PAT > /tmp/drive_patient_fast_020.py
leg(){ local tag=$1 mode=$2 drv=$3 speed=$4; shift 4
  local R=/tmp/run020-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
  pkill -f "[e]mulator .*QUEST" 2>/dev/null||true; sleep 1
  env QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$PMAP "$@" stdbuf -o0 -e0 $EMU -lockstep -silent -trace $R/trace -types lockstep,redirect,gcalls QUEST QUEST_SERVER @QUEST @QUEST >$R/out 2>$R/err &
  local EP=$!; sleep 6; env QUEST_DRIVE_SPEED=$speed python3 $drv $mode $R/session.log >/dev/null 2>&1||true
  sleep 6; kill $EP 2>/dev/null||true; sleep 3; kill -9 $EP 2>/dev/null||true
  local div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out||true)
  local i2=$(grep -c 'MAPPER I2' $R/err||true)
  local prb=$(grep -c 'MAPPER PROBE' $R/err||true)
  local m4b=$(grep -c 'M4B:' $R/out $R/err 2>/dev/null|awk -F: '{s+=$NF}END{print s+0}')
  local map_ab=$(grep -c 'MAPPER:' $R/out $R/err 2>/dev/null|awk -F: '{s+=$NF}END{print s+0}')
  local wsavs_w=$(grep -cE 'WSAVS DIST +mode=W' $R/trace||true)
  local argwr=$(grep -c 'ARGWR pc=' $R/trace||true)
  local quest_base=$(grep -m1 '^redirect' $R/trace | grep -c 'WSAVS QUEST '||true)
  local midwin=$(grep -E '^lockstep .* pc=70166E(0E|10|14|19|1C)' $R/trace | grep -vc DIVERGED || true)
  local end="clean"; grep -q 'MAPPER I2' $R/err && end="I2"; grep -q 'WORLD ABORT' $R/out $R/err 2>/dev/null && end="WORLD-ABORT"; grep -q 'DETACHED at 7017FCE8' $R/err && end="I.STOP"; grep -q '7017F036' $R/err && end="${end}+FATAL"
  printf "%-8s div=%-3s i2=%-3s probes=%-3s m4b_aborts=%-3s mapper_aborts=%-3s writeWSAVS=%-5s argwr=%-6s quest_base=%-2s midwin_pass=%-3s end=%s\n" \
    "$tag" "$div" "$i2" "$prb" "$m4b" "$map_ab" "$wsavs_w" "$argwr" "$quest_base" "$midwin" "$end" | tee -a $RES/verdicts.txt
  cp $R/err $RES/$tag.err
  # mid-window pairs from every leg (the P17 target evidence)
  grep -E '^lockstep .* pc=70166E(0E|10|14|19|1C)' $R/trace > $RES/$tag.midwindow_pairs.txt || true
  if [ "$tag" = "m" ]; then
    grep -m1 '^redirect' $R/trace > $RES/m_first_record.txt || true
    grep -E '^redirect .*(ARGWR pc=|LCALL pc=70166E1C|WSAVS DIST +mode=W)' $R/trace | head -18 > $RES/m_decorated_window.txt || true
    grep -E '^redirect .*(WSAVS DIST +mode=W|WRTN  DIST +mode=W)' $R/trace | head -8 > $RES/m_dist_first_calls.txt || true
    grep -E '^redirect .*mode=C' $R/trace | head -6 > $RES/m_copy_mode_sample.txt || true
    { echo "decorated-site LCALLs (clone): $(grep -c 'LCALL pc=70166E1C' $R/trace||true)"
      echo "ARGWR writes (clone):          $(grep -c 'ARGWR pc=' $R/trace||true)"
      echo "write-mode WSAVS (DIST):       $(grep -cE 'WSAVS DIST +mode=W' $R/trace||true)"
      echo "write-mode WRTN  (DIST):       $(grep -cE 'WRTN  DIST +mode=W' $R/trace||true)"
      echo "copy-mode WSAVS (all others):  $(grep -c 'mode=C pc=' $R/trace||true)"
      echo "QUEST WSAVS (base record):     $(grep -c 'WSAVS QUEST ' $R/trace||true)"
      echo "mid-window pairs (passing):    $(grep -E '^lockstep .* pc=70166E(0E|10|14|19|1C)' $R/trace | grep -vc DIVERGED || true)"
      echo "mid-window pairs (DIVERGED):   $(grep -E '^lockstep .* pc=70166E(0E|10|14|19|1C)' $R/trace | grep -c DIVERGED || true)"
    } > $RES/m_counts.txt
  fi
}
leg fo    failopen /tmp/drive_fast_020.py         10 QUEST_FAIL_OPEN=USER_DATA_FILE
leg m     m        /tmp/drive_fast_020.py         10
leg inj   play     $DRV                            1 QUEST_INJECT=7016A896:-1:0x2006
leg abort m        /tmp/drive_fast_020.py         10 QUEST_TERMINAL=7016871D:ABORT
leg play  play     /tmp/drive_patient_fast_020.py 10
echo "== P17 verdicts (102-live book incl QUEST + DIST site) =="; cat $RES/verdicts.txt
echo "== m-leg counts =="; cat $RES/m_counts.txt
echo "== QUEST base record (first redirect of the m leg) =="; cat $RES/m_first_record.txt
echo "== decorated windows (off must climb 2..8, hold at LCALL, 0 at WSAVS) =="; cat $RES/m_decorated_window.txt
echo "== mid-window pairs, m leg (off>0, none DIVERGED) =="; head -12 $RES/m.midwindow_pairs.txt
echo "P17 BATTERY OK"
