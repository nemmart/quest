#!/bin/bash
# Task 030 — P22 Gen-6.0 recalibration gate, RERUN of task 029.
#
# Task 029's GREEN was INVALID (environmental, recorded in the P22 report):
# attempt 1 hit the 45m runner timeout, its remnant processes overlapped the
# retry, the two runs pkill'd each other's emulators, and every leg's session
# truncated (fo pairs=0, m 120 pairs, no endpoints). The 029 gate was also
# too weak to notice (no pairs floor, no endpoint pin). Fixes here:
#   1. flock overlap guard — a retry cannot run beside a remnant.
#   2. per-leg QUEST_PORT (8791+i) + wait-for-port-free; driver patched to
#      read QUEST_PORT.
#   3. all legs at NORMAL driver speed (the game's turn cadence is
#      wall-clock; 10x starves the session) with real post-driver grace.
#   4. STRICT gate: div=0, blk_mismatch=0, gaps_over_k=0, per-leg pairs
#      floor, per-leg endpoint pinned (P14 §6 matrix: m/inj3->I.STOP,
#      inj/inj2->?FATAL 7017F036, abort->WORLD-ABORT, fo/k1fo/play->clean
#      or I.STOP).
set -eu
exec 9>/tmp/quest-p22-battery.lock
flock -n 9 || { echo "another battery attempt is still running; refusing overlap"; exit 1; }
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook; PMAP=$W/c_src/quest.pushmap.M4
RES=$ROOT/results/030-p22-blocksync-battery; mkdir -p $RES
: > $RES/verdicts.txt
pkill -f "[e]mulator .*QUEST_SERVER" 2>/dev/null || true; sleep 2

# driver copy that honors QUEST_PORT
sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project13/drive.py > /tmp/drive030.py
sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project14/drive_patient.py > /tmp/drive_patient030.py

cat > /tmp/blk030.py <<'PYEOF'
import re, sys
k = int(sys.argv[2]); floor = int(sys.argv[3])
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
ok = n >= floor and eq == n and over == 0
print("pairs=%d blk_equal=%d blk_mismatch=%d k_heartbeats=%d gaps_over_k=%d max_gap=%d floor=%d %s"
      % (n, eq, n - eq, hb, over, maxg, floor, "OK" if ok else "FAIL"))
sys.exit(0 if ok else 1)
PYEOF

FAILS=0
leg(){ local tag=$1 mode=$2 drv=$3 port=$4 k=$5 floor=$6 want=$7; shift 7
  local R=/tmp/run030-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
  for i in $(seq 1 20); do ss -ltn 2>/dev/null | grep -q ":$port " || break; sleep 2; done
  env QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$PMAP QUEST_PORT=$port \
      QUEST_BLOCKS=$ROOT/Disassembled/quest.blocks \
      QUEST_SYNC_LIST=$W/c_src/quest.synclist QUEST_SYNC_K=$k "$@" \
      stdbuf -o0 -e0 $EMU -lockstep -silent -trace $R/trace -types lockstep,redirect,gcalls \
      QUEST QUEST_SERVER @QUEST @QUEST >$R/out 2>$R/err &
  local EP=$!; sleep 8
  QUEST_PORT=$port timeout 360 python3 $drv $mode $R/session.log >/dev/null 2>&1 || true
  sleep 15; kill $EP 2>/dev/null||true; sleep 4; kill -9 $EP 2>/dev/null||true; sleep 2
  cd $ROOT
  local div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out||true)
  local i2=$(grep -c 'MAPPER I2' $R/err||true)
  local prb=$(grep -c 'MAPPER PROBE' $R/err||true)
  local guard=$(grep -c 'runaway guard' $R/out $R/err|awk -F: '{s+=$NF}END{print s+0}')
  local blk bstat=OK; blk=$(python3 /tmp/blk030.py $R/trace $k $floor) || bstat=FAIL
  local end="clean"
  grep -q 'DETACHED at 7017FCE8' $R/err && end="I.STOP"
  grep -q 'DETACHED at 7017F036' $R/err && end="FATAL"
  grep -q 'WORLD ABORT' $R/out $R/err 2>/dev/null && end="WORLD-ABORT"
  local estat=FAIL; case ",$want," in *",$end,"*) estat=OK;; esac
  local legstat=OK
  [ "$div" = "0" ] && [ "$bstat" = "OK" ] && [ "$estat" = "OK" ] || { legstat=FAIL; FAILS=$((FAILS+1)); }
  printf "%-6s K=%-3s div=%-3s i2=%-3s probes=%-3s guard_throws=%-2s %s end=%s want=%s leg=%s\n" \
    "$tag" "$k" "$div" "$i2" "$prb" "$guard" "$blk" "$end" "$want" "$legstat" | tee -a $RES/verdicts.txt
  cp $R/err $RES/$tag.err
  if [ "$legstat" != "OK" ]; then
    grep -A 30 'LOCKSTEP DIVERGENCE' $R/out | head -100 > $RES/$tag.divdump 2>/dev/null || true
    tail -40 $R/out > $RES/$tag.out_tail; tail -20 $R/session.log > $RES/$tag.session_tail 2>/dev/null || true
  fi
}

#   tag    mode     driver                  port k  floor  endpoints        [envs...]
leg fo     failopen /tmp/drive030.py         8791 50 1000  clean,I.STOP     QUEST_FAIL_OPEN=USER_DATA_FILE
leg m      m        /tmp/drive030.py         8792 50 1000  I.STOP
leg inj    play     /tmp/drive030.py         8793 50 1000  FATAL            QUEST_INJECT=7016A896:-1:0x2006
leg abort  m        /tmp/drive030.py         8794 50 10    WORLD-ABORT      QUEST_TERMINAL=7016871D:ABORT
leg play   play     /tmp/drive_patient030.py 8795 50 1000  clean,I.STOP
leg inj2   login    /tmp/drive030.py         8796 50 200   FATAL            QUEST_INJECT=70176AA7:-1:0x2006
leg inj3   m        /tmp/drive030.py         8797 50 1000  clean,I.STOP     QUEST_INJECT=70176AA7:-1:0x2006:RESUME
leg k1fo   failopen /tmp/drive030.py         8798 1  1000  clean,I.STOP     QUEST_FAIL_OPEN=USER_DATA_FILE
echo "== P22 Gen-6.0 recalibration verdicts (task 030) =="
cat $RES/verdicts.txt
[ "$FAILS" = "0" ] || { echo "P22 BATTERY RED ($FAILS legs)"; exit 1; }
echo "P22 BATTERY GREEN"
