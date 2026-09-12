#!/bin/bash
# Task 051 — P49 STAGE A: the stale-expectation refresh that 047b/050/052 all owe.
# This is task 052 VERBATIM except for three `want` lines that predate P33 and are wrong on
# any post-P33 tree. Same source branch (p46-ir7) and therefore the same artifacts, so 051 vs
# 052 is a clean A/B on exactly those three lines. Every replacement number below is derived
# from an artifact, not read off the tree (P49 PROMPT ruling 6).
#
# The P33 lowering emitted 229 sites (docs/Project33/p33.tsv, all EMIT, 0 REFUSE) which
# decompose exactly as:  96 assigns + 57 bases (WADI) + 57 claims + 19 releases = 229.
# That decomposition is what justifies each number:
#
#   1. embeds_book 729 -> 557, embeds_stock 2608 -> 2436.
#      Justified independently of P33 arithmetic: the SAME FILE's P33-B section already
#      wants 557/2436 and gets them. 052 asserted 729 and 557 for one quantity.
#   2. string_statements 1593 -> 1689, literal assignments 857 -> 871.
#      The check counts `^  (\[@|ac1 = cmp\(|words\(@)`. Exactly 96 of the 229 P33 sites emit
#      `[@...]` assign forms (bases/claims/releases do not match that regex): 1593 + 96 = 1689.
#      Exactly 14 of those 96 match the literal-assignment form: 857 + 14 = 871.
#      cmp stays 40 and words stays 12 — P33 emitted none of either.
#   3. The ledger cross-check was STRUCTURALLY stale, not mis-numbered: it diffed lower.py's
#      ledger against p31.tsv + p32.tsv and never consulted p33.tsv, so it could not pass on
#      any post-P33 tree. WIDENED to three-way, not retuned. SE 1593 -> 1822 = 1593 + 229.
#      p33.tsv's $1 is the ledger pc (229/229 match; $10 matches 0/107), so the p31/p32 awk
#      shape carries over unchanged. This is not a tautology: the ledger is lower.py's output
#      and p33.tsv is tools/string_sites.py's, so the check still compares two independently
#      generated artifacts and still fails if either drifts.
#
# KNOWN, NOT FIXED HERE (see docs/Project49/q002): 052 exited RED with FOUR fails, not three.
# The fourth is a real leg failure — inj-emu ended I.STOP against want=FATAL — which passed in
# 047b and 050 and regressed in 052. It is a race between the injected fault and the play
# driver's stray ESC-after-D quitting the session early (docs/Project49/q001-plan-gate §1.2),
# and it is P49 Stage B's to fix. It is deliberately left RED here: a red check with a reason
# is worth more than a green one without. If it recurs, 051 exits 1 for THAT reason alone and
# the three refreshed lines above should still read green in verdicts.txt.
set -eu
exec 9>/tmp/quest-parallel-battery.lock
flock -n 9 || { echo "another battery attempt is still running; refusing overlap"; exit 1; }
cd "$(dirname "$0")/.."
ROOT=$(pwd)
SRC=$(bin/task_source.sh p46-ir7 051-p49-stage-a); W=$SRC/Work
cd $W/emulation && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/emulation/emulator; BOOK=$W/emulation/quest.addrbook; PMAP=$W/emulation/quest.pushmap.M4
IRB=$W/emulation/quest.ir2.book; IRS=$W/emulation/quest.ir2.stock
BLK=$W/emulation/quest.blocks.split; SYN=$W/emulation/quest.synclist.p27
RES=$ROOT/results/051-p49-stage-a; mkdir -p $RES
ARENA=$W/emulation/quest.arena; HOOKS=$W/emulation/quest.strhooks
STR=(QUEST_STRINGS_CHECK=1 QUEST_STRHOOKS=$HOOKS QUEST_ARENA=$ARENA)
( cd $W/emulation && bash tests/run_helpers_selftest.sh ) > /tmp/selftest051 2>&1 || { echo "helpers selftest RED"; cat /tmp/selftest051; exit 1; }
( cd $W/emulation && bash tests/run_strings_selftest.sh ) > /tmp/strings051 2>&1 || { echo "strings selftest RED"; cat /tmp/strings051; exit 1; }
( cd $W/emulation && bash tests/run_strhooks_selftest.sh ) > /tmp/strhooks051 2>&1 || { echo "strhooks selftest RED"; cat /tmp/strhooks051; exit 1; }
grep -q "teeth confirmed" /tmp/strhooks051 || { echo "strhooks teeth NOT confirmed"; exit 1; }
( cd $W/emulation && bash tests/run_vform_selftest.sh ) > /tmp/vform051 2>&1 || { echo "vform selftest RED"; cat /tmp/vform051; exit 1; }
grep -q "teeth confirmed" /tmp/vform051 || { echo "vform teeth NOT confirmed"; exit 1; }
JOBS=${JOBS:-3}
T0=$(date +%s)
for prt in $(seq 8891 8907); do fuser -k -TERM $prt/tcp 2>/dev/null || true; done; sleep 2

sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project13/drive.py > /tmp/drive051.py
sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project14/drive_patient.py > /tmp/drive_patient051.py

cat > /tmp/blk051.py <<'PYEOF'
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
  local R=/tmp/run051-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
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
  local blk bstat=OK; blk=$(python3 /tmp/blk051.py $R/trace $k $floor) || bstat=FAIL
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
launch play      play     /tmp/drive_patient051.py 8895 50 1000  clean,I.STOP   book "${STR[@]}"
launch play-st   play     /tmp/drive051.py         8899 50 1000  clean,I.STOP   stock "${STR[@]}"
launch fo        failopen /tmp/drive051.py         8891 50 1000  clean,I.STOP   book "${STR[@]}"  QUEST_FAIL_OPEN=USER_DATA_FILE
launch m         m        /tmp/drive051.py         8892 50 1000  I.STOP         book "${STR[@]}"
launch inj       play     /tmp/drive051.py         8893 50 1000  FATAL          book "${STR[@]}"  QUEST_INJECT=7016A896:-1:0x2006
launch abort     m        /tmp/drive051.py         8894 50 10    WORLD-ABORT    book "${STR[@]}" QUEST_TERMINAL=7016871D:ABORT
launch inj3      m        /tmp/drive051.py         8896 50 1000  clean,I.STOP   book "${STR[@]}"  QUEST_INJECT=70176AA7:-1:0x2006:RESUME
launch k1fo      failopen /tmp/drive051.py         8897 1  1000  clean,I.STOP   book "${STR[@]}"  QUEST_FAIL_OPEN=USER_DATA_FILE
launch derr      failopen /tmp/drive051.py         8904 1  1000  WORLD-ABORT    book "${STR[@]}"  QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_POKE=7015C48B:0:11
launch derr-emu  failopen /tmp/drive051.py         8905 1  1000  WORLD-ABORT    emu "${STR[@]}"   QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_POKE=7015C48B:0:11
launch fo-st     failopen /tmp/drive051.py         8898 50 1000  clean,I.STOP   stock "${STR[@]}" QUEST_FAIL_OPEN=USER_DATA_FILE
launch fo-emu    failopen /tmp/drive051.py         8900 50 1000  clean,I.STOP   emu "${STR[@]}"   QUEST_FAIL_OPEN=USER_DATA_FILE
launch m-emu     m        /tmp/drive051.py         8901 50 1000  I.STOP         emu "${STR[@]}"
launch inj-emu   play     /tmp/drive051.py         8902 50 1000  FATAL          emu "${STR[@]}"   QUEST_INJECT=7016A896:-1:0x2006
launch abort-emu m        /tmp/drive051.py         8903 50 10    WORLD-ABORT    emu "${STR[@]}"   QUEST_TERMINAL=7016871D:ABORT
launch forced    failopen /tmp/drive051.py         8907 1  10    DIVERGENCE     book "${STR[@]}" QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_POKE=7015C48B:2:0x75000000:CLONE
wait || true

# Collate in canonical order.
: > $RES/verdicts.txt
FAILS=0
ORDER="fo m inj abort play inj3 k1fo derr derr-emu fo-st play-st fo-emu m-emu inj-emu abort-emu forced"
for tag in $ORDER; do
  cat $RES/$tag.verdict >> $RES/verdicts.txt 2>/dev/null || { echo "$tag: MISSING VERDICT" >> $RES/verdicts.txt; FAILS=$((FAILS+1)); }
  [ "$(cat $RES/$tag.status 2>/dev/null)" = "OK" ] || FAILS=$((FAILS+1))
done
A3=0   # P49 Stage A: fails among ONLY the three refreshed lines
echo "== Task 051 P49 STAGE A: task 052 verbatim with the three stale P33 expectations refreshed (JOBS=$JOBS) ==" | tee -a $RES/verdicts.txt
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
SYNI=$(grep -c '^[0-9A-F]' $W/emulation/quest.synclist.split || true)
echo "derr_embeds_remaining=$DERR_EMB (want 2: the LDSP-fed sinks 701604D4 7016D707, verified terminal pairs — P28 A1)" | tee -a $RES/verdicts.txt
echo "asserts=$ASSERTS (want 2273 = 2271 P27 folds, artifact lists $FOLDED, + 2 LDSP range asserts)  unfoldable=2" | tee -a $RES/verdicts.txt
echo "synclist_delisted=$((SYNI-SYNN)) (want 4499)  synclist_entries=$SYNN (want 13510)" | tee -a $RES/verdicts.txt
[ "$DERR_EMB" = "2" ] && [ "$ASSERTS" = "2273" ] && [ "$SYNN" = "13510" ] || FAILS=$((FAILS+1))
echo "-- P28 rt_call decoration (docs/Project28/Census.md; rulings F1 terminator, F2 argc-set, LDSP A1) --" | tee -a $RES/verdicts.txt
EMBS=$(grep -c '^  @' $IRS || true)
RTB=$(grep -c '^  rt_call ' $IRB || true); RTS=$(grep -c '^  rt_call ' $IRS || true)
echo "embeds_book=$EMB (want 557: 729 P28-era minus the 172 the P33 lowering consumed; the P33-B section of this file wants 557 independently)  embeds_stock=$EMBS (want 2436 = 2608 - 172)  rt_call_book=$RTB rt_call_stock=$RTS (want 987/987)" | tee -a $RES/verdicts.txt
[ "$EMB" = "557" ] && [ "$EMBS" = "2436" ] && [ "$RTB" = "987" ] && [ "$RTS" = "987" ] || { FAILS=$((FAILS+1)); A3=$((A3+1)); }
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
echo "-- P31 located strings + P32 append chains (docs/Project31/Census.md §11 correction; docs/Project32/Census.md) --" | tee -a $RES/verdicts.txt
STRB=$(grep -cE '^  (\[@|ac1 = cmp\(|words\(@)' $IRB || true); STRS=$(grep -cE '^  (\[@|ac1 = cmp\(|words\(@)' $IRS || true)
LITB=$(grep -cE '^  \[@[^;]*\] = \[@0x[0-9A-F]+:[01], "' $IRB || true); CMPB=$(grep -c '^  ac1 = cmp(' $IRB || true); WDB=$(grep -c '^  words(@' $IRB || true)
echo "string_statements: book=$STRB stock=$STRS (want 1689/1689 = 649 P31 + 944 P32 + 96 P33 assign forms, the only 96 of the 229 P33 sites this regex matches); literal assignments=$LITB (want 871 = 534 + 323 + 14, the 14 of those 96 in literal-assignment form) cmp=$CMPB (want 40 = 31 + 9; P33 emitted none) words=$WDB (want 12; P33 emitted none)" | tee -a $RES/verdicts.txt
[ "$STRB" = "1689" ] && [ "$STRS" = "1689" ] && [ "$LITB" = "871" ] && [ "$CMPB" = "40" ] && [ "$WDB" = "12" ] || { FAILS=$((FAILS+1)); A3=$((A3+1)); }
SLED=$W/docs/Project31/strings.ledger; STSV=$W/docs/Project31/p31.tsv; STSV32=$W/docs/Project32/p32.tsv; STSV33=$W/docs/Project33/p33.tsv
SE=$(grep -c '^[0-9A-F]\{8\} [A-Z]* emitted ' $SLED || true); SR=$(grep -c '^[0-9A-F]\{8\} [A-Z]* REFUSED ' $SLED || true)
TE=$(awk -F'\t' '$7=="EMIT"' $STSV | wc -l); TR=$(awk -F'\t' '$7=="REFUSE"' $STSV | wc -l)
TE32=$(awk -F'\t' '!/^#/ && $7=="EMIT"' $STSV32 | wc -l); TR32=$(awk -F'\t' '!/^#/ && $7=="REFUSE"' $STSV32 | wc -l)
TE33=$(awk -F'\t' '!/^#/ && $7=="EMIT"' $STSV33 | wc -l); TR33=$(awk -F'\t' '!/^#/ && $7=="REFUSE"' $STSV33 | wc -l)
SAME=$(diff <(grep '^[0-9A-F]\{8\} [A-Z]* emitted ' $SLED | cut -d' ' -f1 | sort) <(cat <(awk -F'\t' '$7=="EMIT"{print $1}' $STSV) <(awk -F'\t' '!/^#/ && $7=="EMIT"{print $1}' $STSV32) <(awk -F'\t' '!/^#/ && $7=="EMIT"{print $1}' $STSV33) | sort) >/dev/null && echo yes || echo NO)
# text byte-compare: the ledger's ir column against the artifacts' ir column, per pc
SAMETXT=$(diff <(grep '^[0-9A-F]\{8\} [A-Z]* emitted ' $SLED | sed 's/^\([0-9A-F]*\) [A-Z]* emitted [^\t]*\t//' | sort) <(cat <(awk -F'\t' '$7=="EMIT"{sub(/ ; .*/, "", $11); print $11}' $STSV) <(awk -F'\t' '!/^#/ && $7=="EMIT"{sub(/ ; .*/, "", $11); print $11}' $STSV32) <(awk -F'\t' '!/^#/ && $7=="EMIT"{sub(/ ; .*/, "", $11); print $11}' $STSV33) | sort) >/dev/null && echo yes || echo NO)
echo "strings ledger: lower.py emitted=$SE refused=$SR; census EMIT p31=$TE/$TR p32=$TE32/$TR32 p33=$TE33/$TR33 (want 1822/0 = 1593 + 229 P33; 649/127; 944/0; 229/0); same pc set=$SAME same text=$SAMETXT (both now three-way against p31+p32+p33)" | tee -a $RES/verdicts.txt
[ "$SE" = "1822" ] && [ "$SR" = "0" ] && [ "$TE" = "649" ] && [ "$TR" = "127" ] && [ "$TE32" = "944" ] && [ "$TR32" = "0" ] && [ "$TE33" = "229" ] && [ "$TR33" = "0" ] && [ "$SAME" = "yes" ] && [ "$SAMETXT" = "yes" ] || { FAILS=$((FAILS+1)); A3=$((A3+1)); }
SYN2=$(grep -c '^[0-9A-F]' $SYN || true)
echo "synclist_entries=$SYN2 (want 13510: unchanged by P31/P32)" | tee -a $RES/verdicts.txt
[ "$SYN2" = "13510" ] || FAILS=$((FAILS+1))
LM=$(grep -h 'IR literal mismatch' $RES/*.err 2>/dev/null | wc -l)
echo "literal_mismatches=$LM (want 0)" | tee -a $RES/verdicts.txt
[ "$LM" = "0" ] || FAILS=$((FAILS+1))
python3 - $RES $W/docs/Project32/p32.tsv <<'PYEOF' | tee -a $RES/verdicts.txt
import sys, re, glob, collections
res = sys.argv[1]
p32 = {}
for l in open(sys.argv[2]):
    if l.startswith('#'): continue
    f = l.rstrip('\n').split('\t')
    p32.setdefault(f[2], []).append((int(f[12]), f[11]))     # block -> [(slice, form)]
tot = collections.Counter(); perleg = {}
for f in glob.glob(res + '/*.err'):
    leg = f.split('/')[-1][:-4]
    kinds = collections.Counter(re.findall(r'first execution of string statement (\w+)', open(f, errors='replace').read()))
    if kinds: perleg[leg] = kinds
    tot |= kinds   # max over legs is not what we want; use union of (kind, block, stmt)
uniq = set()
for f in glob.glob(res + '/*.err'):
    for m in re.finditer(r'first execution of string statement (\w+) in block ([0-9A-F]{8}) stmt (\d+)', open(f, errors='replace').read()):
        uniq.add(m.groups())
byk = collections.Counter(k for k, b, s in uniq)
print("string statement coverage (distinct statements, all IR legs): total %d; %s" % (len(uniq), "  ".join("%s %d" % kv for kv in sorted(byk.items()))))
for leg in ("play", "play-st", "k1fo", "fo", "fo-st"):
    k = perleg.get(leg, {})
    print("  %-8s assign_varying=%d assign_fixed=%d cmp=%d words=%d" % (leg, k.get('assign_varying', 0), k.get('assign_fixed', 0), k.get('cmp', 0), k.get('words', 0)))
ok = all(perleg.get(l, {}).get(k, 0) > 0 for l in ("play", "play-st") for k in ("assign_varying", "cmp", "words"))
print("string statements live in play/play-st: assign_varying, cmp, words all > 0: %s" % ("OK" if ok else "FAIL"))
# P32: live blocks by slice and form; HELP's chain block
live_blocks = set()
for f in glob.glob(res + '/*.err'):
    for m in re.finditer(r'first execution of block ([0-9A-F]{8})', open(f, errors='replace').read()):
        live_blocks.add(m.group(1))
bys = collections.Counter(); byf = collections.Counter(); tot_s = collections.Counter(); tot_f = collections.Counter()
for b, sites in p32.items():
    for sl, form in sites:
        tot_s[sl] += 1; tot_f[form] += 1
        if b in live_blocks: bys[sl] += 1; byf[form] += 1
print("P32 site coverage (block first-executed in any IR leg): by slice %s; by form %s" % (
      "  ".join("%d: %d/%d" % (k, bys[k], tot_s[k]) for k in sorted(tot_s)),
      "  ".join("%s %d/%d" % (k, byf[k], tot_f[k]) for k in sorted(tot_f))))
help_live = {leg: ('7016D5E3' in set(re.findall(r'first execution of block ([0-9A-F]{8})', open(res + '/' + leg + '.err', errors='replace').read())))
             for leg in ("play", "play-st") if glob.glob(res + '/' + leg + '.err')}
ok2 = all(bys[k] > 0 for k in (4, 5, 6))
print("P32: slices 4/5/6 live (want all > 0): %s -> %s; HELP chain block 7016D5E3 live in play/play-st: %s (INFORMATIONAL: the play driver's post-auto-move keys do not reach the prompt — driver fix owed)" % (
      all(bys[k] > 0 for k in (4, 5, 6)), "OK" if ok2 else "FAIL", help_live))
sys.exit(0 if (ok and ok2) else 1)
PYEOF
[ "${PIPESTATUS[0]}" = "0" ] || FAILS=$((FAILS+1))
echo "-- P28 leftover block coverage (LNDO 7015C0C5; LDSP 701604C4 7016D702; Nova SKP 7015BE37 7015BE59 70175F31 7017609C 70176102 7017D7BC) --" | tee -a $RES/verdicts.txt
for b in 7015C0C5 701604C4 7016D702 7015BE37 7015BE59 70175F31 7017609C 70176102 7017D7BC; do
  c=$(grep -h "first execution of block $b" $RES/*.err 2>/dev/null | wc -l)
  printf "%s:%s " $b $c
done | tee -a $RES/verdicts.txt
echo | tee -a $RES/verdicts.txt
# derr leg (P33-A F2-b): the clone's assert (anchored) AND the master's kind-2
# terminal make ONE verified TERMINAL-ABORT pair naming both pcs.
CA=$(grep -c '^IR ASSERT FAILED \[block 7015C48B stmt 0\]: .*"DERR 17 @7015C48E"' $RES/derr.err || true)
TAB=$(grep -c 'TERMINAL-ABORT at 7017ED1C, verified on both engines: master at the kind-2 terminal, clone IR assert at 7015C48B' $RES/derr.err || true)
PK=$(grep -c 'POKE firing at 7015C48B' $RES/derr.err || true)
echo "derr (F2-b): poke_fired=$PK (want 2: both roles)  clone_assert_at_7015C48E=$CA (want 1, anchored)  terminal_abort_both_pcs=$TAB (want 1)" | tee -a $RES/verdicts.txt
[ "$PK" = "2" ] && [ "$CA" = "1" ] && [ "$TAB" = "1" ] || FAILS=$((FAILS+1))
# derr-emu control: both engines execute the real DERR -> the entry-keyed
# DERR.TRP kind-2 terminal forms the final verified pair (REPORT §3).
TA=$(grep -c 'TERMINAL-ABORT at 7017ED1C, verified on both engines' $RES/derr-emu.err || true)
PKE=$(grep -c 'POKE firing at 7015C48B' $RES/derr-emu.err || true)
RO=$(grep -c 'TERMINAL-ABORT at 7017ED1C, verified on both engines (top stack wides: 00000011 7015C48E' $RES/derr-emu.err || true)
echo "derr-emu: poke_fired=$PKE (want 2)  terminal_abort_at_DERR.TRP=$TA (want 1)  readout_00000011_7015C48E=$RO (want 1)  $(grep -m1 'TERMINAL-ABORT' $RES/derr-emu.err | sed 's/.*(top/(top/')" | tee -a $RES/verdicts.txt
[ "$PKE" = "2" ] && [ "$TA" = "1" ] && [ "$RO" = "1" ] || FAILS=$((FAILS+1))
echo "-- P33-B: the arena twins --" | tee -a $RES/verdicts.txt
HB=$(head -1 $IRB); HS=$(head -1 $IRS)
EB=$(grep -c '^  @' $IRB || true); ES=$(grep -c '^  @' $IRS || true)
WB=$(grep -c '^  @[0-9A-F]* WCMV' $IRB || true); MB=$(grep -c '^  @[0-9A-F]* WMSP' $IRB || true); SB=$(grep -c '^  @[0-9A-F]* STASP' $IRB || true)
WS=$(grep -c '^  @[0-9A-F]* WCMV' $IRS || true); MS=$(grep -c '^  @[0-9A-F]* WMSP' $IRS || true); SS=$(grep -c '^  @[0-9A-F]* STASP' $IRS || true)
CL=$(grep -c '^  claim s@' $IRB || true); RL=$(grep -c '^  release s@' $IRB || true); BA=$(grep -c '= s@[0-9A-F]*\.[0-9]* ; WADI' $IRB || true)
TW=$(grep -c '^temp ' $ARENA || true); GR=$(grep -c '^== group .* EMIT$' $W/docs/Project33/p33.ledger || true)
echo "ir headers: book '$HB' stock '$HS' (want ir 7)  embeds book=$EB (want 557) stock=$ES (want 2436)" | tee -a $RES/verdicts.txt
echo "string family embeds remaining WCMV/WMSP/STASP: book $WB/$MB/$SB stock $WS/$MS/$SS (want 0/0/0)" | tee -a $RES/verdicts.txt
echo "twins $TW (want 57); groups EMIT $GR/19; IR bases=$BA claims=$CL releases=$RL (want 57/57/19)" | tee -a $RES/verdicts.txt
[ "$HB" = "ir 7" ] && [ "$HS" = "ir 7" ] && [ "$WB$MB$SB$WS$MS$SS" = "000000" ] && [ "$TW" = "57" ] && [ "$GR" = "19" ] && [ "$BA" = "57" ] && [ "$CL" = "57" ] && [ "$RL" = "19" ] || FAILS=$((FAILS+1))
TB=$(grep -o 't@[0-9A-F]\{8\}' $IRB | wc -l); SB2=$(grep -o 's@[0-9A-F]\{8\}' $IRB | wc -l); TS2=$(grep -o 't@[0-9A-F]\{8\}' $IRS | wc -l); SS2=$(grep -o 's@[0-9A-F]\{8\}' $IRS | wc -l)
TA2=$(grep -o 't@[0-9A-F]\{8\}' $ARENA | wc -l); SA2=$(grep -o 's@[0-9A-F]\{8\}' $ARENA | wc -l)
echo "P46 rename census: book t@=$TB s@=$SB2 stock t@=$TS2 s@=$SS2 arena t@=$TA2 s@=$SA2 (want 0/236 0/236 0/57)" | tee -a $RES/verdicts.txt
[ "$TB$TS2$TA2" = "000" ] && [ "$SB2" = "236" ] && [ "$SS2" = "236" ] && [ "$SA2" = "57" ] || FAILS=$((FAILS+1))
echo "-- hooks (per leg; clone claim must be 0 everywhere) --" | tee -a $RES/verdicts.txt
for tag in $ORDER; do cat $RES/$tag.hooks 2>/dev/null; done | tee -a $RES/verdicts.txt
CC=$(cat $RES/*.hooks 2>/dev/null | grep -v "derr-emu\|fo-emu\|m-emu\|inj-emu\|abort-emu" | grep "/clone" | grep -vc "claim=0 " || true)
echo "IR legs with a claiming clone: $CC (want 0 — delta_clone 0 everywhere; the emu legs cancel the master's insertions instead)" | tee -a $RES/verdicts.txt
[ "$CC" = "0" ] || FAILS=$((FAILS+1))
echo "-- twins: max claim per twin over all legs (bytes; the capacity datum for quest.arena) --" | tee -a $RES/verdicts.txt
cat $RES/*.maxclaim 2>/dev/null | sed 's/.*max_claim //' | awk '{split($0,a," "); t=a[1]; v=a[3]; if(v+0>m[t]+0) m[t]=v; cap[t]=a[6]} END{for(t in m) printf "%s max=%s %s\n", t, m[t], cap[t]}' | sort | tee -a $RES/verdicts.txt
echo "-- groups exercised (master claims per block, IR legs) --" | tee -a $RES/verdicts.txt
grep -h "^strings.*QUEST1 claim" /tmp/run051-*/trace 2>/dev/null | sed 's/.*block=\([0-9A-F]*\).*/\1/' | sort | uniq -c | sort -rn | tee -a $RES/verdicts.txt
RS=$(grep -h "first execution of.*release\|release s@" $RES/*.err 2>/dev/null | wc -l)
echo "release statements first-executed (any leg): $RS" | tee -a $RES/verdicts.txt
FR=$(grep -c 'hits UNMAPPED arena row s@70166144.1' $RES/forced.out || true)
FP=$(grep -c 'POKE firing at 7015C48B: ac2' $RES/forced.err || true)
echo "forced: poke_fired=$FP (want 1: clone only)  divergence_names_unmapped_twin=$FR (want 1)" | tee -a $RES/verdicts.txt
[ "$FP" = "1" ] && [ "$FR" = "1" ] || FAILS=$((FAILS+1))
V76=$(grep -h '0x76 space' $RES/*.err 2>/dev/null | wc -l)
echo "P46 Stage C/D: 0x76 mapping lines across legs=$V76 (want 0: the book declares no v)" | tee -a $RES/verdicts.txt
[ "$V76" = "0" ] || FAILS=$((FAILS+1))
echo "-- self-tests --" | tee -a $RES/verdicts.txt
tail -1 /tmp/selftest051 | tee -a $RES/verdicts.txt; tail -1 /tmp/strings051 | tee -a $RES/verdicts.txt; tail -1 /tmp/strhooks051 | tee -a $RES/verdicts.txt; tail -1 /tmp/vform051 | tee -a $RES/verdicts.txt
echo "-- P26 newly-lowered mnemonic coverage (book+stock legs; regression reference) --" | tee -a $RES/verdicts.txt
python3 $W/docs/Project26/p26cov.py $IRB $RES/*.err 2>&1 | tail -3 | tee -a $RES/verdicts.txt
echo "-- P49 STAGE A verdict (the three refreshed lines, scored on their own) --" | tee -a $RES/verdicts.txt
echo "stage_a_refreshed_lines_failing=$A3 (want 0)  other_fails=$((FAILS-A3))" | tee -a $RES/verdicts.txt
if [ "$A3" = "0" ] && [ "$FAILS" != "0" ]; then
  echo "STAGE A GREEN but the battery is RED for $((FAILS-A3)) unrelated fail(s) — read the leg table above; q002 predicts inj-emu (play-driver race, Stage B owns it)" | tee -a $RES/verdicts.txt
fi
echo "wall_clock_seconds=$(( $(date +%s) - T0 ))" | tee -a $RES/verdicts.txt
cat $RES/verdicts.txt
[ "$FAILS" = "0" ] || { echo "TASK 051 RED ($FAILS fails)"; exit 1; }
echo "TASK 051 GREEN"
