#!/bin/bash
# Task 024 — P18 tranche A battery, retry after the XPEFB/LPEFB fix.
# Root cause of 021/023 divergence: XPEFB and LPEFB had no caller_write
# hook (P16 only hooked XPEF/LPEF; DIST never used the B-variants). A
# decorated site whose arg push is a B-variant then ran write-mode WSAVS
# with an UN-elided arg -> shadow +2*argc at first compare (captured in
# 023: GET_INPUT @ 701760C4, XPEFB @ 701760C2, shadow 700011A8 vs master
# 700011A6). Fix: same 6-line hook replicated into XPEFB/LPEFB.
# Full battery, quest.pushmap.A (515 sites), out/err/trace preserved.
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook
DRV=$W/docs/Project13/drive.py; PAT=$W/docs/Project14/drive_patient.py
RES=$ROOT/results/024-p18-A-battery-fixed; mkdir -p $RES
PA=$W/c_src/quest.pushmap.A
: > $RES/verdicts.txt

leg(){ # tag mode driver [env...]
  tag=$1; mode=$2; drv=$3; shift 3
  R=/tmp/run024-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
  pkill -f "[e]mulator .*QUEST" 2>/dev/null || true; sleep 1
  env QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$PA "$@" stdbuf -o0 -e0 $EMU \
      -lockstep -silent -trace $R/trace -types lockstep,redirect,gcalls \
      QUEST QUEST_SERVER @QUEST @QUEST > $R/out 2> $R/err &
  EP=$!; sleep 6; python3 $drv $mode $R/session.log >/dev/null 2>&1 || true
  sleep 6; kill $EP 2>/dev/null || true; sleep 3; kill -9 $EP 2>/dev/null || true
  div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out 2>/dev/null || true)
  i2=$(grep -c 'MAPPER I2' $R/err 2>/dev/null || true)
  prb=$(grep -c 'MAPPER PROBE' $R/err 2>/dev/null || true)
  m4ab=$(grep -c 'm4b_abort\|M4B ABORT' $R/err 2>/dev/null || true)
  mab=$(grep -c 'MAPPER.*abort\|mapper_abort' $R/err 2>/dev/null || true)
  argwr=$(grep -c 'ARGWR' $R/trace 2>/dev/null || true)
  wwsavs=$(grep -c 'mode=W' $R/trace 2>/dev/null || true)
  wrtnw=$(grep -c 'WRTN.*mode=W' $R/trace 2>/dev/null || true)
  sites=$(grep -oE 'ARGWR pc=[0-9A-F]+' $R/trace 2>/dev/null | sort -u | wc -l || echo 0)
  printf "%-6s div=%-3s i2=%-3s probes=%-3s m4b_ab=%-3s map_ab=%-3s argwr=%-6s wWSAVS=%-5s wWRTN=%-5s push_pcs=%-4s\n" \
    "$tag" "$div" "$i2" "$prb" "$m4ab" "$mab" "$argwr" "$wwsavs" "$wrtnw" "$sites" | tee -a $RES/verdicts.txt
  # preserve evidence: out always (divergence dumps live there), err, site lists
  cp $R/out $RES/$tag.out; cp $R/err $RES/$tag.err 2>/dev/null || true
  grep -oE 'ARGWR pc=[0-9A-F]+' $R/trace 2>/dev/null | sort -u > $RES/$tag.pushpcs || true
  grep -oE 'LCALL pc=[0-9A-F]+' $R/trace 2>/dev/null | sort -u > $RES/$tag.callpcs || true
  if [ "$div" != "0" ]; then grep -A 30 'LOCKSTEP DIVERGENCE' $R/out | head -80 > $RES/$tag.divdump; fi
}

leg m     m        $DRV
leg fo    m        $DRV
leg inj   play     $DRV   QUEST_INJECT=7016A896:-1:0x2006
leg abort m        $DRV   QUEST_TERMINAL=7016871D:ABORT
leg play  play     $PAT

echo "--- tranche A verdicts (post-fix) ---"; cat $RES/verdicts.txt
echo "--- coverage: distinct decorated call sites fired across legs ---"
cat $RES/*.callpcs 2>/dev/null | sort -u | wc -l
echo "(of 515 decorated; unfired = coverage note, not a blocker)"
cat $RES/*.callpcs 2>/dev/null | sort -u > $RES/all.callpcs
cat $RES/*.pushpcs 2>/dev/null | sort -u > $RES/all.pushpcs
echo "distinct push pcs redirected: $(wc -l < $RES/all.pushpcs)"
echo "TASK 024 DONE"
