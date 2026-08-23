#!/bin/bash
# Task 022 — diagnose the tranche-A boot divergence (task 021 finding).
# 021: quest.pushmap.A (515 sites) loads fine (516 decorated calls registered)
# but the m/fo/inj/abort/play legs all diverge EARLY, during boot message-passing,
# argwr=0 (before any arg write). DIST-only (P16/17) booted clean, so a BOOT-PATH
# decorated site is the trigger (INIT_OBJ_TBL @7015C2B7 and others in 7015C2xx run
# during boot). This task CAPTURES the divergence detail (stdout) and bisects.
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook
RES=$ROOT/results/022-p18-diagnose-boot-div; mkdir -p $RES

run_map(){ # label mapfile
  local label=$1 mapf=$2
  local R=/tmp/run022-$label; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
  pkill -f "[e]mulator .*QUEST" 2>/dev/null||true; sleep 1
  env QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$mapf $EMU \
      -lockstep -silent -trace $R/trace -types lockstep,redirect,gcalls \
      QUEST QUEST_SERVER @QUEST @QUEST </dev/null >$R/out 2>$R/err &
  local EP=$!; sleep 8; kill $EP 2>/dev/null||true; sleep 2; kill -9 $EP 2>/dev/null||true
  local div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out 2>/dev/null||echo 0)
  echo "[$label] div=$div decorated=$(grep -c 'decorated call' $R/err||echo 0)"
  # CAPTURE the divergence detail (the missing piece from 021)
  cp $R/out $RES/$label.out 2>/dev/null||true
  grep -A40 'LOCKSTEP DIVERGENCE' $R/out > $RES/$label.divergence.txt 2>/dev/null||true
}

# 1. Full map — capture the divergence detail (stdout) this time.
run_map full $W/c_src/quest.pushmap.A
echo "=== FULL-MAP DIVERGENCE DETAIL ==="; head -50 $RES/full.divergence.txt || true

# 2. Bisect: does a map with ONLY the boot-region sites (pc < 7015D000) diverge?
awk '/^push|^call/{ if (strtonum("0x"$2) < strtonum("0x7015D000")) print; next } {print}' \
    $W/c_src/quest.pushmap.A > $RES/boot_only.pushmap
run_map boot_only $RES/boot_only.pushmap

# 3. And a map EXCLUDING boot-region sites (pc >= 7015D000) — expect clean boot.
awk '/^push|^call/{ if (strtonum("0x"$2) >= strtonum("0x7015D000")) print; next } {print}' \
    $W/c_src/quest.pushmap.A > $RES/no_boot.pushmap
run_map no_boot $RES/no_boot.pushmap

# 4. Just INIT_OBJ_TBL's site (first boot decorated call) — isolate it.
grep -E '^# INIT_OBJ_TBL|7015C2B|7015C2B7' $W/c_src/quest.pushmap.A > $RES/init_obj.pushmap || true
head $RES/init_obj.pushmap
run_map init_obj $RES/init_obj.pushmap

echo "=== SUMMARY ==="
for l in full boot_only no_boot init_obj; do
  echo "$l: div=$(grep -c 'LOCKSTEP DIVERGENCE' $RES/$l.out 2>/dev/null||echo 0)"
done
echo "TASK 022 DONE"
