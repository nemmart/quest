#!/bin/bash
# Task 025 — P18 tranche B: full battery on quest.pushmap.AB (515 A + 20
# WPSH sites) after landing (a) the loader 3-field grammar (push <pc>
# <base_slot> <wides>, every wide's slot validated in the arg region) and
# (b) the WPSH multi-slot caller_write hook (AC[XX] -> base slot, ASCEND,
# note_arg_write(m,wides), group-size==wides cross-check, fail loud).
# Verification beyond div=0 (HANDOFF: descending order corrupts VALUES
# silently): show a TERRAIN WPSH window — 3 ARGWR lines at consecutive
# ascending slots from one WPSH pc, and the site's LCALL off= reflecting
# the accrued 2*argc (WPSH contributing 2*wides).
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook
DRV=$W/docs/Project13/drive.py; PAT=$W/docs/Project14/drive_patient.py
RES=$ROOT/results/025-p18-AB-battery; mkdir -p $RES
PM=$W/c_src/quest.pushmap.AB
: > $RES/verdicts.txt

leg(){ # tag mode driver [env...]
  tag=$1; mode=$2; drv=$3; shift 3
  R=/tmp/run025-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
  pkill -f "[e]mulator .*QUEST" 2>/dev/null || true; sleep 1
  env QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$PM "$@" stdbuf -o0 -e0 $EMU \
      -lockstep -silent -trace $R/trace -types lockstep,redirect,gcalls \
      QUEST QUEST_SERVER @QUEST @QUEST > $R/out 2> $R/err &
  EP=$!; sleep 6; python3 $drv $mode $R/session.log >/dev/null 2>&1 || true
  sleep 6; kill $EP 2>/dev/null || true; sleep 3; kill -9 $EP 2>/dev/null || true
  div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out 2>/dev/null || true)
  i2=$(grep -c 'MAPPER I2' $R/err 2>/dev/null || true)
  prb=$(grep -c 'MAPPER PROBE' $R/err 2>/dev/null || true)
  m4ab=$(grep -c 'M4b WPSH\|m4b_abort\|M4B ABORT' $R/err 2>/dev/null || true)
  mab=$(grep -c 'MAPPER.*abort\|mapper_abort' $R/err 2>/dev/null || true)
  argwr=$(grep -c 'ARGWR' $R/trace 2>/dev/null || true)
  wsavsw=$(grep -c 'WSAVS.*mode=W' $R/trace 2>/dev/null || true)
  wrtnw=$(grep -c 'WRTN.*mode=W' $R/trace 2>/dev/null || true)
  sites=$(grep -oE 'LCALL pc=[0-9A-F]+' $R/trace 2>/dev/null | sort -u | wc -l || echo 0)
  printf "%-6s div=%-3s i2=%-3s probes=%-3s m4b_ab=%-3s map_ab=%-3s argwr=%-6s wWSAVS=%-6s wWRTN=%-6s call_sites=%-4s\n" \
    "$tag" "$div" "$i2" "$prb" "$m4ab" "$mab" "$argwr" "$wsavsw" "$wrtnw" "$sites" | tee -a $RES/verdicts.txt
  cp $R/out $RES/$tag.out; cp $R/err $RES/$tag.err 2>/dev/null || true
  grep -oE 'LCALL pc=[0-9A-F]+' $R/trace 2>/dev/null | sort -u > $RES/$tag.callpcs || true
  # WPSH window evidence: every ARGWR at a known WPSH pc (from pushmap.B)
  grep -E 'ARGWR pc=(7015C50A|70160EA8)' $R/trace > $RES/$tag.wpsh_argwr 2>/dev/null || true
  # broader: all 20 B-site WPSH pcs
  for wpc in $(awk '$1=="push" && NF>=4 && $4~/^[0-9]$/ {print $2}' $PM); do
    grep "ARGWR pc=$wpc" $R/trace >> $RES/$tag.wpsh_argwr 2>/dev/null || true
  done
  sort -u $RES/$tag.wpsh_argwr -o $RES/$tag.wpsh_argwr 2>/dev/null || true
  # keep the first TERRAIN window in full context for the report
  if grep -qE 'WSAVS TERRAIN.*mode=W' $R/trace 2>/dev/null; then
    grep -n -E 'ARGWR|LCALL|WSAVS' $R/trace | grep -B 12 -m1 'WSAVS TERRAIN' > $RES/$tag.terrain_window 2>/dev/null || true
  fi
  if [ "$div" != "0" ]; then grep -A 30 'LOCKSTEP DIVERGENCE' $R/out | head -80 > $RES/$tag.divdump; fi
}

echo "--- push_map.AB load check (3-field grammar) ---"
R=/tmp/run025-load; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
env QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$PM $EMU -lockstep -silent \
    QUEST QUEST_SERVER @QUEST @QUEST </dev/null >/dev/null 2>$RES/loadcheck.err & LP=$!
sleep 4; kill $LP 2>/dev/null||true; sleep 1; kill -9 $LP 2>/dev/null||true
cd $ROOT
grep 'caller map' $RES/loadcheck.err || true
echo "decorated calls loaded: $(grep -c 'decorated call' $RES/loadcheck.err 2>/dev/null||echo 0) (expect 535)"
grep -i 'not inside\|not a book\|out of range\|bad push-map\|error' $RES/loadcheck.err | head -5 || echo "(no load errors)"

leg m     m        $DRV
leg fo    m        $DRV
leg inj   play     $DRV   QUEST_INJECT=7016A896:-1:0x2006
leg abort m        $DRV   QUEST_TERMINAL=7016871D:ABORT
leg play  play     $PAT

echo "--- A+B verdicts ---"; cat $RES/verdicts.txt
echo "--- multi-slot WPSH ARGWR lines (all legs, unique) ---"
cat $RES/*.wpsh_argwr 2>/dev/null | sort -u | head -30
echo "--- first TERRAIN write-mode window (context) ---"
for f in $RES/*.terrain_window; do [ -s "$f" ] && { echo "[$f]"; tail -14 "$f"; break; }; done 2>/dev/null || echo "(TERRAIN not exercised by drivers — coverage note)"
echo "--- coverage ---"
cat $RES/*.callpcs 2>/dev/null | sort -u > $RES/all.callpcs; wc -l < $RES/all.callpcs
echo "(decorated call sites fired, of 535)"
echo "TASK 025 DONE"
