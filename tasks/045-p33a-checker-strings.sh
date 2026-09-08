#!/bin/bash
# Task 045 — P33-A: the checker half of the string design (docs/Project33/{PROMPT-A,REPORT}.md).
# 040's 15 legs, ALL with the string checker ON (QUEST_STRINGS_CHECK=1 +
# QUEST_STRHOOKS=emulation/quest.strhooks), plus:
#   - k1fo-off : book K=1 failopen with the flag OFF (byte-identity gate; want 0 div, clean)
#   - forced   : book K=1 failopen with QUEST_POKE=7015C48B:2:0x75000000:CLONE — the clone's
#                ac2 becomes an arena address of an UNMAPPED row at a rendezvous; want exactly
#                one LOCKSTEP DIVERGENCE whose dump names row block=70166144 (DIED) as unmapped.
#   - derr     : now wants WORLD-ABORT — F2-b pairs the clone's IR assert with the master's
#                kind-2 DERR.TRP arrival: one TERMINAL-ABORT line naming BOTH pcs.
#   - derr-emu : the readout fix — TERMINAL-ABORT ... (top stack wides: 00000011 7015C48E ...).
# Preconditions: helpers self-test GREEN; strings self-test GREEN + broken RED; strhooks
# self-test GREEN + broken (-DP33_BROKEN_DELTA_SIGN) RED (teeth).
# Verdict lines per leg: the StrHooks counters (bind/rebind/unmap/claim/release/frame_exit/
# unwind/onpop/discarded_claims/max_delta) for master and clone.
# Bar: 15/15 040 legs green (0 div), k1fo-off green, forced = 1 div with the row named,
# derr TERMINAL-ABORT with both pcs, derr-emu readout 00000011 7015C48E.
set -eu
exec 9>/tmp/quest-parallel-battery.lock
flock -n 9 || { echo "another battery attempt is still running; refusing overlap"; exit 1; }
cd "$(dirname "$0")/.."
ROOT=$(pwd)
SRC=$(bin/task_source.sh p33a-checker-strings 045-p33a-checker-strings); W=$SRC/Work
cd $W/emulation && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/emulation/emulator; BOOK=$W/emulation/quest.addrbook; PMAP=$W/emulation/quest.pushmap.M4
IRB=$W/emulation/quest.ir2.book; IRS=$W/emulation/quest.ir2.stock
BLK=$W/emulation/quest.blocks.split; SYN=$W/emulation/quest.synclist.p27
RES=$ROOT/results/045-p33a-checker-strings; mkdir -p $RES
STR=(QUEST_STRINGS_CHECK=1 QUEST_STRHOOKS=$W/emulation/quest.strhooks)
# preconditions: the three self-tests (each with its teeth build)
( cd $W/emulation && bash tests/run_helpers_selftest.sh ) > /tmp/selftest045 2>&1 || { echo "helpers selftest RED"; cat /tmp/selftest045; exit 1; }
( cd $W/emulation && bash tests/run_strings_selftest.sh ) > /tmp/strings045 2>&1 || { echo "strings selftest RED"; cat /tmp/strings045; exit 1; }
( cd $W/emulation && bash tests/run_strhooks_selftest.sh ) > /tmp/strhooks045 2>&1 || { echo "strhooks selftest RED"; cat /tmp/strhooks045; exit 1; }
grep -q "teeth confirmed" /tmp/strhooks045 || { echo "strhooks teeth NOT confirmed"; exit 1; }
JOBS=${JOBS:-3}
T0=$(date +%s)
for prt in $(seq 8831 8847); do fuser -k -TERM $prt/tcp 2>/dev/null || true; done; sleep 2

sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project13/drive.py > /tmp/drive045.py
sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project14/drive_patient.py > /tmp/drive_patient034.py

cat > /tmp/blk045.py <<'PYEOF'
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
  local R=/tmp/run045-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
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
  local blk bstat=OK; blk=$(python3 /tmp/blk045.py $R/trace $k $floor) || bstat=FAIL
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
  grep -h "^StrHooks: QUEST[12]/" $R/err | grep -v "bind=0 rebind=0 unmap=0 claim=0" | sed "s/^/$tag hooks: /" > $RES/$tag.hooks || true
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
launch k1fo-off  failopen /tmp/drive045.py         8846 1  1000  clean,I.STOP   book  QUEST_FAIL_OPEN=USER_DATA_FILE
launch forced    failopen /tmp/drive045.py         8847 1  10    DIVERGENCE     book  "${STR[@]}" QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_POKE=7015C48B:2:0x75000000:CLONE
launch play      play     /tmp/drive_patient034.py 8835 50 1000  clean,I.STOP   book  "${STR[@]}"
launch play-st   play     /tmp/drive045.py         8839 50 1000  clean,I.STOP   stock "${STR[@]}"
launch fo        failopen /tmp/drive045.py         8831 50 1000  clean,I.STOP   book  "${STR[@]}"  QUEST_FAIL_OPEN=USER_DATA_FILE
launch m         m        /tmp/drive045.py         8832 50 1000  I.STOP         book  "${STR[@]}"
launch inj       play     /tmp/drive045.py         8833 50 1000  FATAL          book  "${STR[@]}"  QUEST_INJECT=7016A896:-1:0x2006
launch abort     m        /tmp/drive045.py         8834 50 10    WORLD-ABORT    book  "${STR[@]}"  QUEST_TERMINAL=7016871D:ABORT
launch inj3      m        /tmp/drive045.py         8836 50 1000  clean,I.STOP   book  "${STR[@]}"  QUEST_INJECT=70176AA7:-1:0x2006:RESUME
launch k1fo      failopen /tmp/drive045.py         8837 1  1000  clean,I.STOP   book  "${STR[@]}"  QUEST_FAIL_OPEN=USER_DATA_FILE
launch derr      failopen /tmp/drive045.py         8844 1  1000  WORLD-ABORT    book  "${STR[@]}"  QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_POKE=7015C48B:0:11
launch derr-emu  failopen /tmp/drive045.py         8845 1  1000  WORLD-ABORT    emu   "${STR[@]}"  QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_POKE=7015C48B:0:11
launch fo-st     failopen /tmp/drive045.py         8838 50 1000  clean,I.STOP   stock "${STR[@]}" QUEST_FAIL_OPEN=USER_DATA_FILE
launch fo-emu    failopen /tmp/drive045.py         8840 50 1000  clean,I.STOP   emu   "${STR[@]}"  QUEST_FAIL_OPEN=USER_DATA_FILE
launch m-emu     m        /tmp/drive045.py         8841 50 1000  I.STOP         emu   "${STR[@]}"
launch inj-emu   play     /tmp/drive045.py         8842 50 1000  FATAL          emu   "${STR[@]}"  QUEST_INJECT=7016A896:-1:0x2006
launch abort-emu m        /tmp/drive045.py         8843 50 10    WORLD-ABORT    emu   "${STR[@]}"  QUEST_TERMINAL=7016871D:ABORT
wait || true

# Collate in canonical order.
: > $RES/verdicts.txt
FAILS=0
ORDER="fo m inj abort play inj3 k1fo derr derr-emu fo-st play-st fo-emu m-emu inj-emu abort-emu k1fo-off forced"
for tag in $ORDER; do
  cat $RES/$tag.verdict >> $RES/verdicts.txt 2>/dev/null || { echo "$tag: MISSING VERDICT" >> $RES/verdicts.txt; FAILS=$((FAILS+1)); }
  [ "$(cat $RES/$tag.status 2>/dev/null)" = "OK" ] || FAILS=$((FAILS+1))
done
echo "== Task 045 P33-A string checker (dark behind QUEST_STRINGS_CHECK; ON for every leg but k1fo-off) (JOBS=$JOBS) ==" | tee -a $RES/verdicts.txt
echo "-- 040 baseline for comparison --" | tee -a $RES/verdicts.txt
grep -E "^(fo|m|inj|abort|play|inj3|k1fo|derr|derr-emu|fo-st|play-st|fo-emu|m-emu|inj-emu|abort-emu) " $ROOT/results/040-p27-derr-clusters/verdicts.txt | tee -a $RES/verdicts.txt || true
echo "-- hooks (per leg, master and clone) --" | tee -a $RES/verdicts.txt
for tag in $ORDER; do cat $RES/$tag.hooks 2>/dev/null; done | tee -a $RES/verdicts.txt
echo "-- table verified against the program (want every checker-ON leg) --" | tee -a $RES/verdicts.txt
TV=0; for tag in $ORDER; do [ $tag = k1fo-off ] && continue; grep -q "hooked pcs decode as their instruction" $RES/$tag.err && TV=$((TV+1)); done
echo "table_verified_legs=$TV (want 16)" | tee -a $RES/verdicts.txt
[ "$TV" = "16" ] || FAILS=$((FAILS+1))
OFF=$(grep -c "StrHooks" $RES/k1fo-off.err || true)
echo "k1fo-off: StrHooks lines=$OFF (want 0: the checker is dark with the flag off)" | tee -a $RES/verdicts.txt
[ "$OFF" = "0" ] || FAILS=$((FAILS+1))
# F2-b: the derr leg's final verified pair names both pcs.
TA=$(grep -c 'TERMINAL-ABORT at 7017ED1C, verified on both engines: master at the kind-2 terminal, clone IR assert at 7015C48B' $RES/derr.err || true)
CA=$(grep -c 'IR ASSERT FAILED \[block 7015C48B stmt 0\]: .*"DERR 17 @7015C48E"' $RES/derr.err || true)
PK=$(grep -c 'POKE firing at 7015C48B' $RES/derr.err || true)
echo "derr (F2-b): poke_fired=$PK (want 2)  clone_assert=$CA (want 1)  terminal_abort_both_pcs=$TA (want 1)" | tee -a $RES/verdicts.txt
grep -m1 'TERMINAL-ABORT' $RES/derr.err | tee -a $RES/verdicts.txt
[ "$PK" = "2" ] && [ "$CA" = "1" ] && [ "$TA" = "1" ] || FAILS=$((FAILS+1))
# the readout: number, faulting pc
RO=$(grep -c 'TERMINAL-ABORT at 7017ED1C, verified on both engines (top stack wides: 00000011 7015C48E' $RES/derr-emu.err || true)
echo "derr-emu (readout): terminal_abort_with_00000011_7015C48E=$RO (want 1)  $(grep -m1 'TERMINAL-ABORT' $RES/derr-emu.err | sed 's/.*(top/(top/')" | tee -a $RES/verdicts.txt
[ "$RO" = "1" ] || FAILS=$((FAILS+1))
# the forced unmapped-row mismatch
FR=$(grep -c 'hits UNMAPPED arena row block=70166144' $RES/forced.out || true)
FP=$(grep -c 'POKE firing at 7015C48B: ac2' $RES/forced.err || true)
echo "forced: poke_fired=$FP (want 1: clone only)  divergence_names_unmapped_row_70166144=$FR (want 1)" | tee -a $RES/verdicts.txt
grep -m1 'hits UNMAPPED arena row' $RES/forced.out | tee -a $RES/verdicts.txt
[ "$FP" = "1" ] && [ "$FR" = "1" ] || FAILS=$((FAILS+1))
echo "-- self-tests --" | tee -a $RES/verdicts.txt
tail -1 /tmp/selftest045 | tee -a $RES/verdicts.txt; tail -1 /tmp/strings045 | tee -a $RES/verdicts.txt; tail -1 /tmp/strhooks045 | tee -a $RES/verdicts.txt
echo "wall_clock_seconds=$(( $(date +%s) - T0 ))" | tee -a $RES/verdicts.txt
cat $RES/verdicts.txt
[ "$FAILS" = "0" ] || { echo "TASK 045 RED ($FAILS fails)"; exit 1; }
echo "TASK 045 GREEN"
