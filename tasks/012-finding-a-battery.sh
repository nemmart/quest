#!/bin/bash
# Task 012 — full battery on the 101-live book (Finding A fix, DISPLAY_SCREEN live).
# Legs: m, inj, abort, fo, and the PATIENT play driver. Serial (simple, ~minutes
# on godspeed). Verdict table at the end. fo is EXPECTED red (Finding B, unmasked).
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook
RES=$ROOT/results/012-finding-a-battery; mkdir -p $RES

leg() { # tag mode driver env...
  local tag=$1 mode=$2 drv=$3; shift 3
  local R=/tmp/run012-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
  pkill -f "[e]mulator .*QUEST" 2>/dev/null || true; sleep 1
  env QUEST_ADDRESS_BOOK=$BOOK "$@" stdbuf -o0 -e0 $EMU -lockstep -silent \
      -trace $R/trace -types scalls,rtcalls,redirect,gcalls \
      QUEST QUEST_SERVER @QUEST @QUEST > $R/out 2> $R/err &
  local EP=$!; sleep 6
  python3 $drv $mode $R/session.log >/dev/null 2>&1 || true
  sleep 5; kill $EP 2>/dev/null||true; sleep 3; kill -9 $EP 2>/dev/null||true
  local div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out||true)
  local red=$(grep -c '^redirect ' $R/trace||true)
  local prb=$(grep -c 'MAPPER PROBE' $R/err||true)
  local end=$(grep -oE 'DETACHED|WORLD ABORT|7017F036|7017FCE8|MAPPER I[0-9]' $R/err $R/out | head -1||echo none)
  printf "%-8s div=%-3s redirect=%-6s probes=%-3s end=%s\n" "$tag" "$div" "$red" "$prb" "$end" | tee -a $RES/verdicts.txt
  cp $R/err $RES/$tag.err 2>/dev/null||true; grep '^redirect ' $R/trace | awk '{print $2}' | sort | uniq -c | sort -rn > $RES/$tag.coverage 2>/dev/null||true
}

DRV=$W/docs/Project13/drive.py
PAT=$W/docs/Project14/drive_patient.py
leg m      m        $DRV
leg inj    play     $DRV  QUEST_INJECT=7016A896:-1:0x2006
leg abort  m        $DRV  QUEST_TERMINAL=7016871D:ABORT
leg fo     m        $DRV  QUEST_FAIL_OPEN=USER_DATA_FILE
leg play   play     ${PAT:-$DRV}
echo "== battery done =="; cat $RES/verdicts.txt
echo "== DISPLAY_SCREEN exercised? =="; grep -h DISPLAY_SCREEN $RES/*.coverage || echo "(not seen)"
echo "BATTERY OK"
