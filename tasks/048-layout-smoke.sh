#!/bin/bash
# Task 048 — layout smoke (Sep 8 2026): Work/c_src → Work/emulation rename
# (main 5fde934). Two legs (k1fo book K=1, fo-st stock) on MAIN, string
# checker ON, to prove bin/battery.sh, task_source.sh and the template
# paths still work. No artifact or code change; bar = 0 div, clean.
set -eu
exec 9>/tmp/quest-parallel-battery.lock
flock -n 9 || { echo "another battery attempt is still running; refusing overlap"; exit 1; }
cd "$(dirname "$0")/.."
ROOT=$(pwd)
SRC=$(bin/task_source.sh main 048-layout-smoke); W=$SRC/Work
cd $W/emulation && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/emulation/emulator; BOOK=$W/emulation/quest.addrbook; PMAP=$W/emulation/quest.pushmap.M4
IRB=$W/emulation/quest.ir2.book; IRS=$W/emulation/quest.ir2.stock
BLK=$W/emulation/quest.blocks.split; SYN=$W/emulation/quest.synclist.p27
RES=$ROOT/results/048-layout-smoke; mkdir -p $RES
ARENA=$W/emulation/quest.arena; HOOKS=$W/emulation/quest.strhooks
STR=(QUEST_STRINGS_CHECK=1 QUEST_STRHOOKS=$HOOKS QUEST_ARENA=$ARENA)
( cd $W/emulation && bash tests/run_helpers_selftest.sh ) > /tmp/selftest047b 2>&1 || { echo "helpers selftest RED"; cat /tmp/selftest047b; exit 1; }
( cd $W/emulation && bash tests/run_strings_selftest.sh ) > /tmp/strings047b 2>&1 || { echo "strings selftest RED"; cat /tmp/strings047b; exit 1; }
( cd $W/emulation && bash tests/run_strhooks_selftest.sh ) > /tmp/strhooks047b 2>&1 || { echo "strhooks selftest RED"; cat /tmp/strhooks047b; exit 1; }
grep -q "teeth confirmed" /tmp/strhooks047b || { echo "strhooks teeth NOT confirmed"; exit 1; }
JOBS=${JOBS:-3}
T0=$(date +%s)
for prt in $(seq 8891 8907); do fuser -k -TERM $prt/tcp 2>/dev/null || true; done; sleep 2

sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project13/drive.py > /tmp/drive048.py
sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project14/drive_patient.py > /tmp/drive_patient048.py

cat > /tmp/blk047b.py <<'PYEOF'
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
  local R=/tmp/run047b-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
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
      setsid stdbuf -o0 -e0 $EMU -lockstep -silent -trace $R/trace -types lockstep,rtcalls,strings \
      QUEST QUEST_SERVER @QUEST @QUEST >$R/out 2>$R/err 9>&- &
  local EP=$!; sleep 8
  QUEST_PORT=$port timeout 480 python3 $drv $mode $R/session.log >/dev/null 2>&1 || true
  sleep 15; kill $EP 2>/dev/null||true; sleep 4; kill -9 $EP 2>/dev/null||true; fuser -k -KILL $port/tcp 2>/dev/null||true; sleep 2
  local div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out||true)
  local guard=$(grep -c 'runaway guard' $R/out $R/err|awk -F: '{s+=$NF}END{print s+0}')
  local blk bstat=OK; blk=$(python3 /tmp/blk047b.py $R/trace $k $floor) || bstat=FAIL
  local end="clean"
  grep -q 'DETACHED at 7017FCE8' $R/err && end="I.STOP"
  grep -q 'DETACHED at 7017F036' $R/err && end="FATAL"
  grep -q 'DETACHED at IR assert' $R/err && end="IR-ASSERT"
  grep -q 'WORLD ABORT' $R/out $R/err 2>/dev/null && end="WORLD-ABORT"
  grep -q 'LOCKSTEP DIVERGENCE' $R/out && end="DIVERGENCE"
  local estat=FAIL; case ",$want," in *",$end,"*) estat=OK;; esac
  local legstat=OK
  local wantdiv=0; case ",$want," in *",DIVERGENCE,"*) wantdiv=1;; esac
  [ "$div" = "$wantdiv" ] && [ "$bstat" = "OK" ] && [ "$estat" = "OK" ] || legstat=FAIL
  local lib=$(grep -c '?LIB_ERROR(native)' $R/trace||true)
  printf "%-9s cfg=%-5s K=%-3s div=%-3s guard=%-2s liberr_native=%-2s %s end=%s want=%s leg=%s\n" \
    "$tag" "$cfg" "$k" "$div" "$guard" "$lib" "$blk" "$end" "$want" "$legstat" > $RES/$tag.verdict
  echo $legstat > $RES/$tag.status
  cp $R/err $RES/$tag.err
  case $tag in derr|derr-emu|forced) cp $R/out $RES/$tag.out;; esac
  grep -h "^StrHooks: QUEST[12]/" $R/err | grep -v "max_claim" | grep -v "bind=0 rebind=0 unmap=0 claim=0 release=0 frame_exit=1 " | sed "s/^/$tag hooks: /" > $RES/$tag.hooks || true
  grep -h "max_claim" $R/err | grep "QUEST1/master" | sed "s/^/$tag /" > $RES/$tag.maxclaim || true
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
launch k1fo      failopen /tmp/drive048.py         8897 1  1000  clean,I.STOP   book "${STR[@]}"  QUEST_FAIL_OPEN=USER_DATA_FILE
launch fo-st     failopen /tmp/drive048.py         8898 50 1000  clean,I.STOP   stock "${STR[@]}" QUEST_FAIL_OPEN=USER_DATA_FILE
wait || true

# Collate in canonical order.
: > $RES/verdicts.txt
FAILS=0
ORDER="k1fo fo-st"
for tag in $ORDER; do
  cat $RES/$tag.verdict >> $RES/verdicts.txt 2>/dev/null || { echo "$tag: MISSING VERDICT" >> $RES/verdicts.txt; FAILS=$((FAILS+1)); }
  [ "$(cat $RES/$tag.status 2>/dev/null)" = "OK" ] || FAILS=$((FAILS+1))
done
echo "== Task 048 layout smoke (Work/emulation rename; main) (JOBS=$JOBS) ==" | tee -a $RES/verdicts.txt
for tag in $ORDER; do cat $RES/$tag.verdict | tee -a $RES/verdicts.txt; done
echo "wall_clock_seconds=$(( $(date +%s) - T0 ))" | tee -a $RES/verdicts.txt
if [ "$FAILS" = "0" ]; then echo "TASK 048 GREEN" | tee -a $RES/verdicts.txt; else echo "TASK 048 RED ($FAILS fails)" | tee -a $RES/verdicts.txt; exit 1; fi
