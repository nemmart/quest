#!/bin/bash
# Task 032 — P24 wide-carry correction battery (Parts 2+3 landing gate).
#
# Tree: checks out origin/p24-wide-carry -- Work Disassembled (the
# P23-integrated base + the P24 fix; main does not yet carry P23).
# Template: tasks/hold/031 (single runner loop; flock overlap guard;
# per-leg ports + wait-for-free; normal driver speed; STRICT gate:
# div=0, blk_mismatch=0, gaps_over_k=0, pairs floors, pinned endpoints).
# P24 additions: per-leg IR configuration (book / stock / all-emulated,
# per the P24 prompt battery shape), the SPLIT CFG + synclist (the
# operative P23 pair), and a carry-consumer-site coverage report
# (CarryCensus.md sites) appended to the verdicts.
set -eu
exec 9>/tmp/quest-p24-battery.lock
flock -n 9 || { echo "another battery attempt is still running; refusing overlap"; exit 1; }
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
git fetch --quiet origin p24-wide-carry
git checkout --quiet origin/p24-wide-carry -- Work Disassembled
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook; PMAP=$W/c_src/quest.pushmap.M4
IRB=$W/c_src/quest.ir2.book; IRS=$W/c_src/quest.ir2.stock
BLK=$W/c_src/quest.blocks.split; SYN=$W/c_src/quest.synclist.split
RES=$ROOT/results/032-p24-wide-carry-battery; mkdir -p $RES
: > $RES/verdicts.txt
for prt in 8791 8792 8793 8794 8795 8796 8797 8798 8799 8800 8801; do fuser -k -TERM $prt/tcp 2>/dev/null || true; done; sleep 2

sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project13/drive.py > /tmp/drive032.py
sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project14/drive_patient.py > /tmp/drive_patient032.py

cat > /tmp/blk032.py <<'PYEOF'
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
# leg <tag> <mode> <driver> <port> <k> <floor> <endpoints> <cfg: book|stock|emu> [envs...]
leg(){ local tag=$1 mode=$2 drv=$3 port=$4 k=$5 floor=$6 want=$7 cfg=$8; shift 8
  local R=/tmp/run032-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
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
  QUEST_PORT=$port timeout 360 python3 $drv $mode $R/session.log >/dev/null 2>&1 || true
  sleep 15; kill $EP 2>/dev/null||true; sleep 4; kill -9 $EP 2>/dev/null||true; fuser -k -KILL $port/tcp 2>/dev/null||true; sleep 2
  cd $ROOT
  local div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out||true)
  local guard=$(grep -c 'runaway guard' $R/out $R/err|awk -F: '{s+=$NF}END{print s+0}')
  local blk bstat=OK; blk=$(python3 /tmp/blk032.py $R/trace $k $floor) || bstat=FAIL
  local end="clean"
  grep -q 'DETACHED at 7017FCE8' $R/err && end="I.STOP"
  grep -q 'DETACHED at 7017F036' $R/err && end="FATAL"
  grep -q 'WORLD ABORT' $R/out $R/err 2>/dev/null && end="WORLD-ABORT"
  local estat=FAIL; case ",$want," in *",$end,"*) estat=OK;; esac
  local legstat=OK
  [ "$div" = "0" ] && [ "$bstat" = "OK" ] && [ "$estat" = "OK" ] || { legstat=FAIL; FAILS=$((FAILS+1)); }
  local lib=$(grep -c '?LIB_ERROR(native)' $R/trace||true)
  printf "%-8s cfg=%-5s K=%-3s div=%-3s guard=%-2s liberr_native=%-2s %s end=%s want=%s leg=%s\n" \
    "$tag" "$cfg" "$k" "$div" "$guard" "$lib" "$blk" "$end" "$want" "$legstat" | tee -a $RES/verdicts.txt
  cp $R/err $RES/$tag.err
  if [ "$legstat" != "OK" ]; then
    grep -A 30 'LOCKSTEP DIVERGENCE' $R/out | head -100 > $RES/$tag.divdump 2>/dev/null || true
    tail -40 $R/out > $RES/$tag.out_tail; tail -20 $R/session.log > $RES/$tag.session_tail 2>/dev/null || true
  fi
}

#   tag      mode     driver                   port k  floor endpoints      cfg   [envs...]
leg fo       failopen /tmp/drive032.py         8791 50 1000  clean,I.STOP   book  QUEST_FAIL_OPEN=USER_DATA_FILE
leg m        m        /tmp/drive032.py         8792 50 1000  I.STOP         book
leg inj      play     /tmp/drive032.py         8793 50 1000  FATAL          book  QUEST_INJECT=7016A896:-1:0x2006
leg abort    m        /tmp/drive032.py         8794 50 10    WORLD-ABORT    book  QUEST_TERMINAL=7016871D:ABORT
leg play     play     /tmp/drive_patient032.py 8795 50 1000  clean,I.STOP   book
leg inj3     m        /tmp/drive032.py         8796 50 1000  clean,I.STOP   book  QUEST_INJECT=70176AA7:-1:0x2006:RESUME
leg k1fo     failopen /tmp/drive032.py         8797 1  1000  clean,I.STOP   book  QUEST_FAIL_OPEN=USER_DATA_FILE
leg fo-st    failopen /tmp/drive032.py         8798 50 1000  clean,I.STOP   stock QUEST_FAIL_OPEN=USER_DATA_FILE
leg play-st  play     /tmp/drive032.py         8799 50 1000  clean,I.STOP   stock
leg fo-emu   failopen /tmp/drive032.py         8800 50 1000  clean,I.STOP   emu   QUEST_FAIL_OPEN=USER_DATA_FILE
leg m-emu    m        /tmp/drive032.py         8801 50 1000  I.STOP         emu

echo "== P24 wide-carry battery verdicts ==" | tee -a $RES/verdicts.txt
echo "-- carry-consumer-site block coverage (CarryCensus.md; IRExec first-execution, book/stock legs) --" | tee -a $RES/verdicts.txt
for b in 70160E64 70160E65 70160E73 70160E74 7016E75B 7016E75C 7016E76A 7016E76B \
         7015E701 701644C5 70168107 7016A5E5 7016B852 7016DB9A 7016DCD0 7016DCEF \
         70171734 70171984 70171E2B 7017200B 701723F2 701727AB 70177336 701785C8 \
         7017860D 70178738 70179BFC 70169B56; do
  c=$(grep -h "first execution of block $b" $RES/*.err 2>/dev/null | wc -l)
  printf "%s:%s " $b $c
done | tee -a $RES/verdicts.txt
echo | tee -a $RES/verdicts.txt
cat $RES/verdicts.txt
[ "$FAILS" = "0" ] || { echo "P24 BATTERY RED ($FAILS legs)"; exit 1; }
echo "P24 BATTERY GREEN"
