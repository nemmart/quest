#!/bin/bash
# Task 043 — P30: the C++ string library, shipped DARK (docs/Project30/PROMPT.md,
# docs/Project29/StringsDesign.md §2.4/§3/§6), on branch p30-string-library:
#   hw/strings/EagleString.{hpp,cpp}  pieces as spans; copy/assign/compare/
#                                      block_move mirroring EagleSpecial's
#                                      WCMV/WCMP/WBLM (+WSTB); the §3 residues
#   hw/strings/ClaimDelta.{hpp,cpp}   the per-frame Δ accumulator
#   hw/Mapper.{hpp,cpp}               the ARENA FORM: codec rows 0x75/0xEA/0xF5,
#                                      19 static rows, clone->master only in
#                                      equivalent(); clone_location() refuses
#   tests/strings_selftest.cpp        library vs EagleSpecial oracle, field by
#                                      field; arena form; Δ (18,370 cases)
# The emulator links the library and NOTHING calls it; the only touched
# emulator path is Mapper::equivalent/clone_location/frame_precedes, which
# behave identically for every non-arena value and no arena value exists
# today. Bar (PROMPT Part 3): both self-tests GREEN (strings: library GREEN
# AND the -DP30_BROKEN_RESIDUE build RED), then the 038/039 pair — k1fo
# (book, K=1) + play-st (stock, K=50) — vs the 042 baseline: 0 div, same
# endpoints, pair counts in the usual band (042: k1fo 298566 clean,
# play-st 3397585 clean).
# Source tree via bin/task_source.sh (no index side effects).
set -eu
exec 9>/tmp/quest-parallel-battery.lock
flock -n 9 || { echo "another battery attempt is still running; refusing overlap"; exit 1; }
cd "$(dirname "$0")/.."
ROOT=$(pwd)
SRC=$(bin/task_source.sh p30-string-library 043-p30-string-library); W=$SRC/Work
cd $W/emulation && make -j"$(nproc)" >/dev/null
./tests/run_helpers_selftest.sh | tail -1 | tee /tmp/selftest043
grep -q "SELFTEST GREEN" /tmp/selftest043 || { echo "HELPER SELFTEST RED"; exit 1; }
./tests/run_strings_selftest.sh | tee /tmp/strings043
grep -q "teeth confirmed" /tmp/strings043 || { echo "STRINGS SELFTEST NOT GREEN/RED AS REQUIRED"; exit 1; }
cd $ROOT
EMU=$W/emulation/emulator; BOOK=$W/emulation/quest.addrbook; PMAP=$W/emulation/quest.pushmap.M4
IRB=$W/emulation/quest.ir2.book; IRS=$W/emulation/quest.ir2.stock
BLK=$W/emulation/quest.blocks.split; SYN=$W/emulation/quest.synclist.p27
RES=$ROOT/results/043-p30-string-library; mkdir -p $RES
JOBS=${JOBS:-3}
T0=$(date +%s)
for prt in $(seq 8831 8843); do fuser -k -TERM $prt/tcp 2>/dev/null || true; done; sleep 2

sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project13/drive.py > /tmp/drive043.py
sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project14/drive_patient.py > /tmp/drive_patient034.py

cat > /tmp/blk043.py <<'PYEOF'
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

# leg <tag> <mode> <driver> <port> <k> <floor> <endpoints> <cfg> [envs...]
# Writes $RES/$tag.verdict and $RES/$tag.status; NEVER touches shared state.
leg(){ local tag=$1 mode=$2 drv=$3 port=$4 k=$5 floor=$6 want=$7 cfg=$8; shift 8
  local R=/tmp/run043-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
  fuser -k -TERM $port/tcp 2>/dev/null || true
  for i in $(seq 1 20); do ss -ltn 2>/dev/null | grep -q ":$port " || break; sleep 2; done
  local cfgenv=()
  case $cfg in
    book)  cfgenv=(QUEST_IR=$IRB QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$PMAP);;
    stock) cfgenv=(QUEST_IR=$IRS);;
    emu)   cfgenv=();;
  esac
  env QUEST_PORT=$port QUEST_BLOCKS=$BLK QUEST_SYNC_LIST=$SYN QUEST_SYNC_K=$k \
      "${cfgenv[@]}" "$@" \
      setsid stdbuf -o0 -e0 $EMU -lockstep -silent -trace $R/trace -types lockstep,rtcalls \
      QUEST QUEST_SERVER @QUEST @QUEST >$R/out 2>$R/err 9>&- &
  local EP=$!; sleep 8
  QUEST_PORT=$port timeout 480 python3 $drv $mode $R/session.log >/dev/null 2>&1 || true
  sleep 15; kill $EP 2>/dev/null||true; sleep 4; kill -9 $EP 2>/dev/null||true; fuser -k -KILL $port/tcp 2>/dev/null||true; sleep 2
  local div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out||true)
  local guard=$(grep -c 'runaway guard' $R/out $R/err|awk -F: '{s+=$NF}END{print s+0}')
  local blk bstat=OK; blk=$(python3 /tmp/blk043.py $R/trace $k $floor) || bstat=FAIL
  local end="clean"
  grep -q 'DETACHED at 7017FCE8' $R/err && end="I.STOP"
  grep -q 'DETACHED at 7017F036' $R/err && end="FATAL"
  grep -q 'WORLD ABORT' $R/out $R/err 2>/dev/null && end="WORLD-ABORT"
  local estat=FAIL; case ",$want," in *",$end,"*) estat=OK;; esac
  local legstat=OK
  [ "$div" = "0" ] && [ "$bstat" = "OK" ] && [ "$estat" = "OK" ] || legstat=FAIL
  local lib=$(grep -c '?LIB_ERROR(native)' $R/trace||true)
  printf "%-9s cfg=%-5s K=%-3s div=%-3s guard=%-2s liberr_native=%-2s %s end=%s want=%s leg=%s\n" \
    "$tag" "$cfg" "$k" "$div" "$guard" "$lib" "$blk" "$end" "$want" "$legstat" > $RES/$tag.verdict
  echo $legstat > $RES/$tag.status
  cp $R/err $RES/$tag.err
  if [ "$legstat" != "OK" ]; then
    grep -A 30 'LOCKSTEP DIVERGENCE' $R/out | head -100 > $RES/$tag.divdump 2>/dev/null || true
    tail -40 $R/out > $RES/$tag.out_tail; tail -20 $R/session.log > $RES/$tag.session_tail 2>/dev/null || true
  fi
}

# Slot pool: at most $JOBS legs in flight.
pids=()
launch(){ 
  while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n || true; done
  leg "$@" & pids+=($!)
}

# Long poles first, then the rest; canonical report order is fixed below.
launch play-st   play     /tmp/drive043.py         8839 50 1000  clean,I.STOP   stock
launch k1fo      failopen /tmp/drive043.py         8837 1  1000  clean,I.STOP   book  QUEST_FAIL_OPEN=USER_DATA_FILE
wait || true

# Collate in canonical order.
: > $RES/verdicts.txt
FAILS=0
ORDER="k1fo play-st"
for tag in $ORDER; do
  cat $RES/$tag.verdict >> $RES/verdicts.txt 2>/dev/null || { echo "$tag: MISSING VERDICT" >> $RES/verdicts.txt; FAILS=$((FAILS+1)); }
  [ "$(cat $RES/$tag.status 2>/dev/null)" = "OK" ] || FAILS=$((FAILS+1))
done
echo "== Task 043 P30 string library (dark) battery (JOBS=$JOBS) ==" | tee -a $RES/verdicts.txt
echo "-- 042 baseline for comparison --" | tee -a $RES/verdicts.txt
grep -E "^(k1fo|play-st) " $ROOT/results/042-p28-rt-call/verdicts.txt | tee -a $RES/verdicts.txt || true
echo "-- helper selftest --" | tee -a $RES/verdicts.txt; cat /tmp/selftest043 | tee -a $RES/verdicts.txt
echo "-- strings selftest --" | tee -a $RES/verdicts.txt; cat /tmp/strings043 | tee -a $RES/verdicts.txt
echo "wall_clock_seconds=$(( $(date +%s) - T0 ))" | tee -a $RES/verdicts.txt
cat $RES/verdicts.txt
[ "$FAILS" = "0" ] || { echo "TASK 043 RED ($FAILS fails)"; exit 1; }
echo "TASK 043 GREEN"
