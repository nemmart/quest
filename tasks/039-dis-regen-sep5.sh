#!/bin/bash
# Task 039 — Sep 5 disassembler fixes + artifact regeneration, on branch
# hw-findings-sep5 (which also carries the 038 helper fixes):
#   Follow.java: XJMP no longer tagged with a pc+2 fall-through edge
#     (quest.tags/quest.blocks: 1,160 successor lists lose one address;
#     targets/synclist unchanged);
#   OldDisassembler.java: XCALL renders ea,arg (63 lines; the old form
#     printed opcode bits as a phantom register), LNDO/LWDO gain their
#     register field (1 line);
#   build_address_book.py re_xcall updated (generated book byte-identical
#     to the pre-fix run); blocks.split / ir2.book / ir2.stock regenerated
#     (synclist identical; embeds 8,529 / 10,408 unchanged).
# Same two legs as 038 (k1fo + play-st) vs the 037/038 baseline: the
# only runtime consumer of the changed text is BlockSync's successor
# parse on the XJMP blocks, so 0 div + clean endpoints is the bar.
set -eu
exec 9>/tmp/quest-parallel-battery.lock
flock -n 9 || { echo "another battery attempt is still running; refusing overlap"; exit 1; }
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
git fetch --quiet origin hw-findings-sep5
git checkout --quiet origin/hw-findings-sep5 -- Work Disassembled
cd $W/c_src && make -j"$(nproc)" >/dev/null && ./tests/run_helpers_selftest.sh | tail -1 | tee /tmp/selftest039 && cd $ROOT
grep -q GREEN /tmp/selftest039 || { echo "HELPER SELFTEST RED"; exit 1; }
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook; PMAP=$W/c_src/quest.pushmap.M4
IRB=$W/c_src/quest.ir2.book; IRS=$W/c_src/quest.ir2.stock
BLK=$W/c_src/quest.blocks.split; SYN=$W/c_src/quest.synclist.split
RES=$ROOT/results/039-dis-regen-sep5; mkdir -p $RES
JOBS=${JOBS:-3}
T0=$(date +%s)
for prt in $(seq 8831 8843); do fuser -k -TERM $prt/tcp 2>/dev/null || true; done; sleep 2

sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project13/drive.py > /tmp/drive039.py
sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project14/drive_patient.py > /tmp/drive_patient034.py

cat > /tmp/blk039.py <<'PYEOF'
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
  local R=/tmp/run039-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
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
  local blk bstat=OK; blk=$(python3 /tmp/blk039.py $R/trace $k $floor) || bstat=FAIL
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
launch play-st   play     /tmp/drive039.py         8839 50 1000  clean,I.STOP   stock
launch k1fo      failopen /tmp/drive039.py         8837 1  1000  clean,I.STOP   book  QUEST_FAIL_OPEN=USER_DATA_FILE
wait || true

# Collate in canonical order.
: > $RES/verdicts.txt
FAILS=0
ORDER="k1fo play-st"
for tag in $ORDER; do
  cat $RES/$tag.verdict >> $RES/verdicts.txt 2>/dev/null || { echo "$tag: MISSING VERDICT" >> $RES/verdicts.txt; FAILS=$((FAILS+1)); }
  [ "$(cat $RES/$tag.status 2>/dev/null)" = "OK" ] || FAILS=$((FAILS+1))
done
echo "== Task 039 P26 math-grammar battery (JOBS=$JOBS) ==" | tee -a $RES/verdicts.txt
echo "-- 037 baseline for comparison --" | tee -a $RES/verdicts.txt
grep -E "^(k1fo|play-st) " $ROOT/results/037-p26-math-grammar/verdicts.txt $ROOT/results/038-hw-findings-sep5/run.log | tee -a $RES/verdicts.txt || true
echo "-- helper selftest --" | tee -a $RES/verdicts.txt; cat /tmp/selftest039 | tee -a $RES/verdicts.txt
echo "wall_clock_seconds=$(( $(date +%s) - T0 ))" | tee -a $RES/verdicts.txt
cat $RES/verdicts.txt
[ "$FAILS" = "0" ] || { echo "TASK 039 RED ($FAILS fails)"; exit 1; }
echo "TASK 039 GREEN"
