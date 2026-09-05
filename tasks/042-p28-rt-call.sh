#!/bin/bash
# Task 042 — P28 rt_call decoration (041 re-run with two verdict lines corrected — see below) (docs/Project28/{PROMPT,Census,RTConventions,REPORT}.md).
# 040's 15 legs (034 template + derr + derr-emu, JOBS=3) on branch p28-rt-call:
#   - ir 4 artifacts: the 987 game->runtime LCALL sites are `rt_call ?NAME(args)
#     site=` terminators (pushes folded into PL/I-order argument expressions,
#     pushed right-to-left through Machine::wide_push, LCALL run via the
#     normal instruction path); plus LNDO, the LDSP pair (assert + goto table,
#     option A1) and the 67 Nova LOAD forms.  Embeds 6,258 -> 2,322 (book),
#     8,137 -> 4,201 (stock); 987 rt_call in both modes, 0 refused.
#   - sync list unchanged (quest.synclist.p27, 13,510): rt_call keeps every
#     site's block and its site+4 successor; no block added or removed.
#   - derr / derr-emu legs as in 040 (regression: the P27 pairing still holds
#     with the new grammar).
# Verdict lines added: embeds_book/stock == 2322/4201; rt_call == 987 in both
# artifacts; rt_sites ledger emitted=987 refused=0; rt_call coverage by callee
# from the IRExec first-execution lines (site -> block via docs/Project28/
# rt_sites.tsv) — ?WRITE_SCREEN and ?RANDOM_NUMBER must be LIVE in the
# book/stock play legs; leftover block coverage (LNDO 7015C0C5, LDSP
# 701604C4 / 7016D702, Nova carry-consumer list).
# Source tree via bin/task_source.sh.
# 042 = 041 with two verdict-line fixes (METHOD §10): the ledger count was
# row-anchored to exclude the header (988->987) and the leftover-embed pattern's
# `WPSH 0,0` was anchored to the 3 ?OPEN_SHARED_IO_FILE sites (it matched the
# out-of-scope RETURN_MESSAGE temp at 70169B77).  041: 15/15 legs green, 0 div.
# Landing bar: 15/15 green, 0 div, embeds 2322/4201, rt_call 987/987, WRITE_SCREEN
# and RANDOM_NUMBER rt_call blocks live, derr pair at 7015C48E as in 040.
set -eu
exec 9>/tmp/quest-parallel-battery.lock
flock -n 9 || { echo "another battery attempt is still running; refusing overlap"; exit 1; }
cd "$(dirname "$0")/.."
ROOT=$(pwd)
SRC=$(bin/task_source.sh p28-rt-call 042-p28-rt-call); W=$SRC/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook; PMAP=$W/c_src/quest.pushmap.M4
IRB=$W/c_src/quest.ir2.book; IRS=$W/c_src/quest.ir2.stock
BLK=$W/c_src/quest.blocks.split; SYN=$W/c_src/quest.synclist.p27
RES=$ROOT/results/042-p28-rt-call; mkdir -p $RES
JOBS=${JOBS:-3}
T0=$(date +%s)
for prt in $(seq 8831 8845); do fuser -k -TERM $prt/tcp 2>/dev/null || true; done; sleep 2

sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project13/drive.py > /tmp/drive042.py
sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project14/drive_patient.py > /tmp/drive_patient034.py

cat > /tmp/blk042.py <<'PYEOF'
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
  local R=/tmp/run042-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
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
  local blk bstat=OK; blk=$(python3 /tmp/blk042.py $R/trace $k $floor) || bstat=FAIL
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
launch play-st   play     /tmp/drive042.py         8839 50 1000  clean,I.STOP   stock
launch fo        failopen /tmp/drive042.py         8831 50 1000  clean,I.STOP   book  QUEST_FAIL_OPEN=USER_DATA_FILE
launch m         m        /tmp/drive042.py         8832 50 1000  I.STOP         book
launch inj       play     /tmp/drive042.py         8833 50 1000  FATAL          book  QUEST_INJECT=7016A896:-1:0x2006
launch abort     m        /tmp/drive042.py         8834 50 10    WORLD-ABORT    book  QUEST_TERMINAL=7016871D:ABORT
launch inj3      m        /tmp/drive042.py         8836 50 1000  clean,I.STOP   book  QUEST_INJECT=70176AA7:-1:0x2006:RESUME
launch k1fo      failopen /tmp/drive042.py         8837 1  1000  clean,I.STOP   book  QUEST_FAIL_OPEN=USER_DATA_FILE
launch derr      failopen /tmp/drive042.py         8844 1  1000  IR-ASSERT      book  QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_POKE=7015C48B:0:11
launch derr-emu  failopen /tmp/drive042.py         8845 1  1000  WORLD-ABORT    emu   QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_POKE=7015C48B:0:11
launch fo-st     failopen /tmp/drive042.py         8838 50 1000  clean,I.STOP   stock QUEST_FAIL_OPEN=USER_DATA_FILE
launch fo-emu    failopen /tmp/drive042.py         8840 50 1000  clean,I.STOP   emu   QUEST_FAIL_OPEN=USER_DATA_FILE
launch m-emu     m        /tmp/drive042.py         8841 50 1000  I.STOP         emu
launch inj-emu   play     /tmp/drive042.py         8842 50 1000  FATAL          emu   QUEST_INJECT=7016A896:-1:0x2006
launch abort-emu m        /tmp/drive042.py         8843 50 10    WORLD-ABORT    emu   QUEST_TERMINAL=7016871D:ABORT
wait || true

# Collate in canonical order.
: > $RES/verdicts.txt
FAILS=0
ORDER="fo m inj abort play inj3 k1fo derr derr-emu fo-st play-st fo-emu m-emu inj-emu abort-emu"
for tag in $ORDER; do
  cat $RES/$tag.verdict >> $RES/verdicts.txt 2>/dev/null || { echo "$tag: MISSING VERDICT" >> $RES/verdicts.txt; FAILS=$((FAILS+1)); }
  [ "$(cat $RES/$tag.status 2>/dev/null)" = "OK" ] || FAILS=$((FAILS+1))
done
echo "== Task 042 P28 rt_call battery (JOBS=$JOBS) ==" | tee -a $RES/verdicts.txt
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
echo "derr_embeds_remaining=$DERR_EMB (want 2: the LDSP-fed sinks 701604D4 7016D707, verified terminal pairs — P28 A1)" | tee -a $RES/verdicts.txt
echo "asserts=$ASSERTS (want 2273 = 2271 P27 folds, artifact lists $FOLDED, + 2 LDSP range asserts)  unfoldable=2" | tee -a $RES/verdicts.txt
echo "synclist_delisted=$((SYNI-SYNN)) (want 4499)  synclist_entries=$SYNN (want 13510)" | tee -a $RES/verdicts.txt
[ "$DERR_EMB" = "2" ] && [ "$ASSERTS" = "2273" ] && [ "$SYNN" = "13510" ] || FAILS=$((FAILS+1))
echo "-- P28 rt_call decoration (docs/Project28/Census.md; rulings F1 terminator, F2 argc-set, LDSP A1) --" | tee -a $RES/verdicts.txt
EMBS=$(grep -c '^  @' $IRS || true)
RTB=$(grep -c '^  rt_call ' $IRB || true); RTS=$(grep -c '^  rt_call ' $IRS || true)
echo "embeds_book=$EMB (want 2322)  embeds_stock=$EMBS (want 4201)  rt_call_book=$RTB rt_call_stock=$RTS (want 987/987)" | tee -a $RES/verdicts.txt
[ "$EMB" = "2322" ] && [ "$EMBS" = "4201" ] && [ "$RTB" = "987" ] && [ "$RTS" = "987" ] || FAILS=$((FAILS+1))
LED=$W/docs/Project28/rt_call.ledger
LE=$(grep -c '^[0-9A-F]\{8\} .* emitted ' $LED || true); LR=$(grep -c '^[0-9A-F]\{8\} .* REFUSED ' $LED || true)   # row-anchored: 041 counted the header line
echo "rt_sites ledger: emitted=$LE refused=$LR (want 987/0)" | tee -a $RES/verdicts.txt
[ "$LE" = "987" ] && [ "$LR" = "0" ] || FAILS=$((FAILS+1))
LEFT=$(grep -cE '^  @(7015BE89|7015BEA6|7015BEC3) WPSH|^  @[0-9A-F]+ (LNDO|LDSP|[XL]PEF |(MOV|ADD|SUB|COM|NEG|ADC|INC|AND)(\.[ZOC]?[LRS]?)? )' $IRB || true)   # WPSH anchored to the 3 RT sites: 041 matched RETURN_MESSAGE's temp at 70169B77 (boundary 1)
echo "leftover_embeds_book (LNDO/LDSP/RT pushes/Nova loads) = $LEFT (want 0)" | tee -a $RES/verdicts.txt
[ "$LEFT" = "0" ] || FAILS=$((FAILS+1))
python3 - $W/docs/Project28/rt_sites.tsv $LED $RES/*.err <<'PYEOF' | tee -a $RES/verdicts.txt
import sys, re, collections
tsv = {l.split('\t')[0]: l.split('\t') for l in open(sys.argv[1]) if not l.startswith('#')}
emitted = {l.split()[0]: l.split()[1] for l in open(sys.argv[2]) if not l.startswith('#') and l.split()[3] == 'emitted'}
live = set()
for f in sys.argv[3:]:
    for l in open(f, errors='replace'):
        m = re.search(r'first execution of block ([0-9A-F]{8})', l)
        if m: live.add(m.group(1))
by = collections.defaultdict(lambda: [0, 0])
for site, callee in emitted.items():
    by[callee][1] += 1
    if tsv[site][3] in live: by[callee][0] += 1
tot = (sum(v[0] for v in by.values()), sum(v[1] for v in by.values()))
print("rt_call coverage (all IR legs): %d/%d sites executed; by callee: %s" % (tot[0], tot[1],
      "  ".join("%s %d/%d" % (c, x, n) for c, (x, n) in sorted(by.items(), key=lambda kv: -kv[1][1]))))
ws, rn = by["?WRITE_SCREEN"][0], by["?RANDOM_NUMBER"][0]
print("rt_call live: WRITE_SCREEN=%d RANDOM_NUMBER=%d (want both > 0) %s" % (ws, rn, "OK" if ws > 0 and rn > 0 else "FAIL"))
sys.exit(0 if ws > 0 and rn > 0 else 1)
PYEOF
[ "${PIPESTATUS[0]}" = "0" ] || FAILS=$((FAILS+1))
echo "-- P28 leftover block coverage (LNDO 7015C0C5; LDSP 701604C4 7016D702; Nova SKP 7015BE37 7015BE59 70175F31 7017609C 70176102 7017D7BC) --" | tee -a $RES/verdicts.txt
for b in 7015C0C5 701604C4 7016D702 7015BE37 7015BE59 70175F31 7017609C 70176102 7017D7BC; do
  c=$(grep -h "first execution of block $b" $RES/*.err 2>/dev/null | wc -l)
  printf "%s:%s " $b $c
done | tee -a $RES/verdicts.txt
echo | tee -a $RES/verdicts.txt
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
