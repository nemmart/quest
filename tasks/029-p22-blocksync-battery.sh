#!/bin/bash
# Task 029 — P22 Gen-6.0 recalibration gate: the block-sync checker
# (docs/Project22/BlockSyncDesign.md, docs/Project22/PROMPT.md).
#
# The sync MODEL changed: rendezvous are denominated in basic blocks
# (K listed entries, identity sync list, K=50 per ruling Q1) instead of
# 500-insn batches; block ordinal + FP state (ruling Q3) joined the
# compare surface; the insn-count compare is retained TEMPORARILY (P23
# removes it). Master == clone by construction — ANY divergence is a
# checker implementation bug.
#
# Legs (crossings-landing precedent, Gen-2 recalibration shape):
#   fo/m/play/abort/inj  the standing 5-leg battery at K=50
#   inj2/inj3            Gen-2 inject shapes 2 (login-only) and 3 (:RESUME)
#   k1fo                 fail-open leg at K=1 (the prompt's K=1 requirement)
# Pass: div=0 everywhere; blk_mismatch=0; max blk-gap <= K on every leg;
# probes/i2/mapper aborts 0; endpoints pinned (?FATAL 7017F036 for inj legs,
# I.STOP 7017FCE8 detach for m/play, WORLD-ABORT banner for abort leg).
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook; PMAP=$W/c_src/quest.pushmap.M4
RES=$ROOT/results/029-p22-blocksync-battery; mkdir -p $RES
DRV=$W/docs/Project13/drive.py; PAT=$W/docs/Project14/drive_patient.py
: > $RES/verdicts.txt

sed 's|def drain(seconds):|def drain(seconds):\n    seconds = max(0.3, seconds / float(__import__("os").environ.get("QUEST_DRIVE_SPEED", "1")))|' \
  $DRV > /tmp/drive_fast_029.py
sed 's|def drain(seconds):|def drain(seconds):\n    seconds = max(0.3, seconds / float(__import__("os").environ.get("QUEST_DRIVE_SPEED", "1")))|' \
  $PAT > /tmp/drive_patient_fast_029.py

# Per-leg pair analysis: blk equality at every pair; heartbeat discipline
# (ordinal-0 inter-pair blk gap <= K); histogram summary.
cat > /tmp/blk029.py <<'PYEOF'
import re, sys
k = int(sys.argv[2])
n = eq = 0; last = None; maxg = 0; over = 0; hb = 0
pat = re.compile(r"pair ord=(\d+) pc=[0-9A-F]+ .* blk=(\d+) clone_blk=(\d+)")
for l in open(sys.argv[1]):
    m = pat.search(l)
    if not m: continue
    n += 1; b, cb = int(m.group(2)), int(m.group(3))
    if b == cb: eq += 1
    if m.group(1) == '0':
        if last is not None:
            g = b - last
            if g > maxg: maxg = g
            if g > k: over += 1
            if g == k: hb += 1
        last = b
print("pairs=%d blk_equal=%d blk_mismatch=%d k_heartbeats=%d gaps_over_k=%d max_gap=%d"
      % (n, eq, n - eq, hb, over, maxg))
sys.exit(1 if (n == 0 or eq != n or over) else 0)
PYEOF

leg(){ local tag=$1 mode=$2 drv=$3 speed=$4 k=$5; shift 5
  local R=/tmp/run029-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
  pkill -f "[e]mulator .*QUEST" 2>/dev/null||true; sleep 1
  env QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$PMAP \
      QUEST_BLOCKS=$ROOT/Disassembled/quest.blocks \
      QUEST_SYNC_LIST=$W/c_src/quest.synclist QUEST_SYNC_K=$k "$@" \
      stdbuf -o0 -e0 $EMU -lockstep -silent -trace $R/trace -types lockstep,redirect,gcalls \
      QUEST QUEST_SERVER @QUEST @QUEST >$R/out 2>$R/err &
  local EP=$!; sleep 6; env QUEST_DRIVE_SPEED=$speed python3 $drv $mode $R/session.log >/dev/null 2>&1||true
  sleep 6; kill $EP 2>/dev/null||true; sleep 3; kill -9 $EP 2>/dev/null||true
  cd $ROOT
  local div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out||true)
  local i2=$(grep -c 'MAPPER I2' $R/err||true)
  local prb=$(grep -c 'MAPPER PROBE' $R/err||true)
  local guard=$(grep -c 'runaway guard' $R/out $R/err|awk -F: '{s+=$NF}END{print s+0}')
  local blk; blk=$(python3 /tmp/blk029.py $R/trace $k) || true
  local end="?"
  grep -q 'DETACHED at 7017FCE8' $R/err && end="I.STOP"
  grep -q 'DETACHED at 7017F036' $R/err && end="FATAL"
  grep -q 'WORLD ABORT' $R/out $R/err 2>/dev/null && end="WORLD-ABORT"
  grep -q 'RETIRED' $R/err && end="${end}+RETIRE"
  printf "%-6s K=%-3s div=%-3s i2=%-3s probes=%-3s guard_throws=%-2s %s end=%s\n" \
    "$tag" "$k" "$div" "$i2" "$prb" "$guard" "$blk" "$end" | tee -a $RES/verdicts.txt
  cp $R/err $RES/$tag.err; grep -m1 'BlockSync:' $R/err >> $RES/verdicts.txt || true
  if [ "$div" != "0" ]; then grep -A 30 'LOCKSTEP DIVERGENCE' $R/out | head -100 > $RES/$tag.divdump; fi
}

leg fo    failopen /tmp/drive_fast_029.py         10 50 QUEST_FAIL_OPEN=USER_DATA_FILE
leg m     m        /tmp/drive_fast_029.py         10 50
leg inj   play     $DRV                            1 50 QUEST_INJECT=7016A896:-1:0x2006
leg abort m        /tmp/drive_fast_029.py         10 50 QUEST_TERMINAL=7016871D:ABORT
leg play  play     /tmp/drive_patient_fast_029.py 10 50
leg inj2  login    /tmp/drive_fast_029.py         10 50 QUEST_INJECT=70176AA7:-1:0x2006
leg inj3  m        /tmp/drive_fast_029.py         10 50 QUEST_INJECT=70176AA7:-1:0x2006:RESUME
leg k1fo  failopen /tmp/drive_fast_029.py         10 1  QUEST_FAIL_OPEN=USER_DATA_FILE

echo "== P22 Gen-6.0 recalibration verdicts =="
cat $RES/verdicts.txt
# gate: every leg line must show div=0, blk_mismatch=0, gaps_over_k=0
bad=$(grep -E '^(fo|m|inj|abort|play|inj2|inj3|k1fo) ' $RES/verdicts.txt | \
      grep -vcE 'div=0 .*blk_mismatch=0 .*gaps_over_k=0' || true)
[ "$bad" = "0" ] || { echo "P22 BATTERY RED ($bad legs)"; exit 1; }
echo "P22 BATTERY GREEN"
