#!/bin/bash
# Task 040 — P27 DERR cluster compression (docs/Project27/{PROMPT,Census,REPORT}.md).
# 034 template (13 legs, JOBS=3) + the `derr` leg, on branch p27-derr-clusters:
#   - IR regenerated with tools/lower.py --assumed-foldable (2,271 clusters
#     folded to assert+goto in their guard blocks; 4,499 interior blocks
#     gone); shipped sync list = c_src/quest.synclist.p27 (13,510 entries,
#     identity minus interiors). DERR embeds 2,273 -> 2 (the LDSP pair, P28).
#   - `derr` leg: book K=1 failopen with QUEST_POKE=7015C48B:0:11 (P27 test
#     knob: ac0 := 11 on arrival at the QUEST-main loop-body guard, both
#     roles, one shot). Expected (ruling F2-a): clone `IR ASSERT FAILED …
#     "DERR 17 @7015C48E"`; master DERR -> DERR.TRP -> ?FATAL, non-clean,
#     START_TURN never reached (tripwire silent). TWO lines matched.
# Source tree via bin/task_source.sh (Sep 5 finding: the old `git checkout
# origin/<branch> -- Work Disassembled` staged the branch into the queue
# checkout and the results commit carried it onto main).
#   - `derr-emu` leg: the same poke on the all-emulated pair — control:
#     TERMINAL-ABORT at 7017ED1C verified on both engines (WORLD-ABORT).
# Landing bar: 15/15 green, 0 div, embeds_book == 6258 (2 DERR embeds
# remaining), synclist 13510, derr leg paired at 7015C48E.
set -eu
exec 9>/tmp/quest-parallel-battery.lock
flock -n 9 || { echo "another battery attempt is still running; refusing overlap"; exit 1; }
cd "$(dirname "$0")/.."
ROOT=$(pwd)
SRC=$(bin/task_source.sh p27-derr-clusters 040-p27-derr-clusters); W=$SRC/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook; PMAP=$W/c_src/quest.pushmap.M4
IRB=$W/c_src/quest.ir2.book; IRS=$W/c_src/quest.ir2.stock
BLK=$W/c_src/quest.blocks.split; SYN=$W/c_src/quest.synclist.p27
RES=$ROOT/results/040-p27-derr-clusters; mkdir -p $RES
JOBS=${JOBS:-3}
T0=$(date +%s)
for prt in $(seq 8831 8845); do fuser -k -TERM $prt/tcp 2>/dev/null || true; done; sleep 2

sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project13/drive.py > /tmp/drive040.py
sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project14/drive_patient.py > /tmp/drive_patient034.py

cat > /tmp/blk040.py <<'PYEOF'
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
  local R=/tmp/run040-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
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
  local blk bstat=OK; blk=$(python3 /tmp/blk040.py $R/trace $k $floor) || bstat=FAIL
  local end="clean"
  grep -q 'DETACHED at 7017FCE8' $R/err && end="I.STOP"
  grep -q 'DETACHED at 7017F036' $R/err && end="FATAL"
  grep -q 'DETACHED at IR assert' $R/err && end="IR-ASSERT"
  grep -q 'WORLD ABORT' $R/out $R/err 2>/dev/null && end="WORLD-ABORT"
  local estat=FAIL; case ",$want," in *",$end,"*) estat=OK;; esac
  local legstat=OK
  [ "$div" = "0" ] && [ "$bstat" = "OK" ] && [ "$estat" = "OK" ] || legstat=FAIL
  local lib=$(grep -c '?LIB_ERROR(native)' $R/trace||true)
  printf "%-9s cfg=%-5s K=%-3s div=%-3s guard=%-2s liberr_native=%-2s %s end=%s want=%s leg=%s\n" \
    "$tag" "$cfg" "$k" "$div" "$guard" "$lib" "$blk" "$end" "$want" "$legstat" > $RES/$tag.verdict
  echo $legstat > $RES/$tag.status
  cp $R/err $RES/$tag.err
  case $tag in derr|derr-emu) cp $R/out $RES/$tag.out;; esac
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
launch play      play     /tmp/drive_patient034.py 8835 50 1000  clean,I.STOP   book
launch play-st   play     /tmp/drive040.py         8839 50 1000  clean,I.STOP   stock
launch fo        failopen /tmp/drive040.py         8831 50 1000  clean,I.STOP   book  QUEST_FAIL_OPEN=USER_DATA_FILE
launch m         m        /tmp/drive040.py         8832 50 1000  I.STOP         book
launch inj       play     /tmp/drive040.py         8833 50 1000  FATAL          book  QUEST_INJECT=7016A896:-1:0x2006
launch abort     m        /tmp/drive040.py         8834 50 10    WORLD-ABORT    book  QUEST_TERMINAL=7016871D:ABORT
launch inj3      m        /tmp/drive040.py         8836 50 1000  clean,I.STOP   book  QUEST_INJECT=70176AA7:-1:0x2006:RESUME
launch k1fo      failopen /tmp/drive040.py         8837 1  1000  clean,I.STOP   book  QUEST_FAIL_OPEN=USER_DATA_FILE
launch derr      failopen /tmp/drive040.py         8844 1  1000  IR-ASSERT      book  QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_POKE=7015C48B:0:11
launch derr-emu  failopen /tmp/drive040.py         8845 1  1000  WORLD-ABORT    emu   QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_POKE=7015C48B:0:11
launch fo-st     failopen /tmp/drive040.py         8838 50 1000  clean,I.STOP   stock QUEST_FAIL_OPEN=USER_DATA_FILE
launch fo-emu    failopen /tmp/drive040.py         8840 50 1000  clean,I.STOP   emu   QUEST_FAIL_OPEN=USER_DATA_FILE
launch m-emu     m        /tmp/drive040.py         8841 50 1000  I.STOP         emu
launch inj-emu   play     /tmp/drive040.py         8842 50 1000  FATAL          emu   QUEST_INJECT=7016A896:-1:0x2006
launch abort-emu m        /tmp/drive040.py         8843 50 10    WORLD-ABORT    emu   QUEST_TERMINAL=7016871D:ABORT
wait || true

# Collate in canonical order.
: > $RES/verdicts.txt
FAILS=0
ORDER="fo m inj abort play inj3 k1fo derr derr-emu fo-st play-st fo-emu m-emu inj-emu abort-emu"
for tag in $ORDER; do
  cat $RES/$tag.verdict >> $RES/verdicts.txt 2>/dev/null || { echo "$tag: MISSING VERDICT" >> $RES/verdicts.txt; FAILS=$((FAILS+1)); }
  [ "$(cat $RES/$tag.status 2>/dev/null)" = "OK" ] || FAILS=$((FAILS+1))
done
echo "== Task 040 P26 math-grammar battery (JOBS=$JOBS) ==" | tee -a $RES/verdicts.txt
for tag in inj abort; do
  d=$(grep -c 'mid-block — dropping' $RES/$tag.err || true)
  echo "$tag: armed-pc block drops=$d (want 1)" | tee -a $RES/verdicts.txt
  [ "$d" = "1" ] || FAILS=$((FAILS+1))
done
for tag in inj-emu abort-emu; do
  d=$(grep -c 'mid-block — dropping' $RES/$tag.err || true)
  echo "$tag: armed-pc block drops=$d (want 0)" | tee -a $RES/verdicts.txt
  [ "$d" = "0" ] || FAILS=$((FAILS+1))
done
echo "-- carry-consumer-site block coverage (CarryCensus.md; book/stock legs) --" | tee -a $RES/verdicts.txt
for b in 70160E64 70160E65 70160E73 70160E74 7016E75B 7016E75C 7016E76A 7016E76B \
         7015E701 701644C5 70168107 7016A5E5 7016B852 7016DB9A 7016DCD0 7016DCEF \
         70171734 70171984 70171E2B 7017200B 701723F2 701727AB 70177336 701785C8 \
         7017860D 70178738 70179BFC 70169B56; do
  c=$(grep -h "first execution of block $b" $RES/*.err 2>/dev/null | wc -l)
  printf "%s:%s " $b $c
done | tee -a $RES/verdicts.txt
echo | tee -a $RES/verdicts.txt
echo "-- P27 DERR cluster compression (docs/Project27/Census.md; ruling F1=A, F2-a) --" | tee -a $RES/verdicts.txt
EMB=$(grep -c '^  @' $IRB || true)
DERR_EMB=$(grep -c '^  @[0-9A-F]* DERR' $IRB || true)
ASSERTS=$(grep -c '^  assert(' $IRB || true)
FOLDED=$(grep -c '^[0-9A-F]' $W/docs/Project27/assumed-foldable.txt || true)
SYNN=$(grep -c '^[0-9A-F]' $SYN || true)
SYNI=$(grep -c '^[0-9A-F]' $W/c_src/quest.synclist.split || true)
echo "embeds_book=$EMB (want 6258)  derr_embeds_remaining=$DERR_EMB (want 2: the LDSP pair 701604D4 7016D707 -> P28)" | tee -a $RES/verdicts.txt
echo "clusters_folded=$ASSERTS (artifact lists $FOLDED; want 2271)  unfoldable=2" | tee -a $RES/verdicts.txt
echo "synclist_delisted=$((SYNI-SYNN)) (want 4499)  synclist_entries=$SYNN (want 13510)" | tee -a $RES/verdicts.txt
[ "$DERR_EMB" = "2" ] && [ "$ASSERTS" = "2271" ] && [ "$SYNN" = "13510" ] || FAILS=$((FAILS+1))
# derr leg: TWO lines matched (user ruling, Sep 5): the clone's assert at the
# predicted pc AND a non-clean master (DERR.TRP on its backtrace, the
# detached-master START_TURN tripwire silent, end != clean).
CA=$(grep -c 'IR ASSERT FAILED \[block 7015C48B stmt 0\]: .*"DERR 17 @7015C48E"' $RES/derr.err || true)
MT=$(grep -c 'DERR.TRP' $RES/derr.out || true)
TW=$(grep -c 'Master did not terminate after clone detach' $RES/derr.out $RES/derr.err | awk -F: '{s+=$NF}END{print s+0}')
PK=$(grep -c 'POKE firing at 7015C48B' $RES/derr.err || true)
echo "derr: poke_fired=$PK (want 2: both roles)  clone_assert_at_7015C48E=$CA (want 1)  master_DERR.TRP_frame=$MT (want >=1)  start_turn_tripwire=$TW (want 0)" | tee -a $RES/verdicts.txt
echo "derr: master_end=$(grep -m1 -A1 'Lockstep: ordinal 0 DETACHED at IR assert' $RES/derr.err | tail -1)" | tee -a $RES/verdicts.txt
[ "$PK" = "2" ] && [ "$CA" = "1" ] && [ "$MT" -ge 1 ] && [ "$TW" = "0" ] || FAILS=$((FAILS+1))
# derr-emu control: both engines execute the real DERR -> the entry-keyed
# DERR.TRP kind-2 terminal forms the final verified pair (REPORT §3).
TA=$(grep -c 'TERMINAL-ABORT at 7017ED1C, verified on both engines' $RES/derr-emu.err || true)
PKE=$(grep -c 'POKE firing at 7015C48B' $RES/derr-emu.err || true)
echo "derr-emu: poke_fired=$PKE (want 2)  terminal_abort_at_DERR.TRP=$TA (want 1)  $(grep -m1 'TERMINAL-ABORT' $RES/derr-emu.err | sed 's/.*(top/(top/')" | tee -a $RES/verdicts.txt
[ "$PKE" = "2" ] && [ "$TA" = "1" ] || FAILS=$((FAILS+1))
echo "-- P26 newly-lowered mnemonic coverage (book+stock legs; regression reference) --" | tee -a $RES/verdicts.txt
python3 $W/docs/Project26/p26cov.py $IRB $RES/*.err 2>&1 | tail -3 | tee -a $RES/verdicts.txt
echo "wall_clock_seconds=$(( $(date +%s) - T0 ))" | tee -a $RES/verdicts.txt
cat $RES/verdicts.txt
[ "$FAILS" = "0" ] || { echo "PARALLEL BATTERY RED ($FAILS fails)"; exit 1; }
echo "PARALLEL BATTERY GREEN"
