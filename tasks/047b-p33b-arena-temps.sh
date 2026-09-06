#!/bin/bash
# Task 047b — P33-B re-run after P33-C (docs/Project33/REPORT-C.md): 047's two play
# "divergences" were the pair compare of batches cut by the graceful SIGTERM shutdown
# (the play legs are killed mid-game: the driver never reaches the prompt after the
# auto-move). Fix on the branch: Lockstep::halting set by the shutdown path, compare_pair
# returns before comparing a halt-truncated pair. Legs, verdicts and bar as 047.
# Coverage caveat (recorded): play/play-st end by the kill (`end=clean`), verifying their
# 2-3.8M pairs; the post-auto-move screens are not exercised until the driver is fixed
# and the verdict requires I.STOP (Gen 6.2 items, next weekend).
# Task 047 — P33-B: the 19 WMSP claim groups as arena twins (docs/Project33/{PROMPT-B,REPORT-B}.md).
# 046's 15 legs on branch p33b-arena-temps, ALL with the string checker ON
# (QUEST_STRINGS_CHECK=1 + QUEST_STRHOOKS + QUEST_ARENA: the clone's temps live
# in the arena, the master is unchanged), plus P33-A's forced-mismatch leg.
# Carries the two 046 corrections: derr wants WORLD-ABORT (P33-A's F2-b makes the
# folded DERR a TERMINAL-ABORT pair) with the assert line counted anchored
# (^IR ASSERT FAILED); the HELP line is INFORMATIONAL (the play driver's post-
# auto-move keys do not reach the command prompt — a pre-existing driver defect,
# owed in NextSession).
# Preconditions: helpers / strings / strhooks self-tests GREEN with their teeth.
# Verdict lines: groups lowered / WCMV-WMSP-STASP embeds remaining (want 0/0/0),
# embeds 557 book / 2436 stock, ir 6 header, twins bound / unmapped counts, max
# live delta_master, delta_clone == 0 everywhere (clone claim=0 on every leg),
# releases executed, per-twin max claim (the capacity datum), the groups the
# legs exercise, forced = 1 div naming t@70166144.1 unmapped.
# Bar: 15/15 (+forced), 0 div on the 16 non-forced legs, embeds 0/0/0.
set -eu
exec 9>/tmp/quest-parallel-battery.lock
flock -n 9 || { echo "another battery attempt is still running; refusing overlap"; exit 1; }
cd "$(dirname "$0")/.."
ROOT=$(pwd)
SRC=$(bin/task_source.sh p33b-arena-temps 047b-p33b-arena-temps); W=$SRC/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook; PMAP=$W/c_src/quest.pushmap.M4
IRB=$W/c_src/quest.ir2.book; IRS=$W/c_src/quest.ir2.stock
BLK=$W/c_src/quest.blocks.split; SYN=$W/c_src/quest.synclist.p27
RES=$ROOT/results/047b-p33b-arena-temps; mkdir -p $RES
ARENA=$W/c_src/quest.arena; HOOKS=$W/c_src/quest.strhooks
STR=(QUEST_STRINGS_CHECK=1 QUEST_STRHOOKS=$HOOKS QUEST_ARENA=$ARENA)
( cd $W/c_src && bash tests/run_helpers_selftest.sh ) > /tmp/selftest047b 2>&1 || { echo "helpers selftest RED"; cat /tmp/selftest047b; exit 1; }
( cd $W/c_src && bash tests/run_strings_selftest.sh ) > /tmp/strings047b 2>&1 || { echo "strings selftest RED"; cat /tmp/strings047b; exit 1; }
( cd $W/c_src && bash tests/run_strhooks_selftest.sh ) > /tmp/strhooks047b 2>&1 || { echo "strhooks selftest RED"; cat /tmp/strhooks047b; exit 1; }
grep -q "teeth confirmed" /tmp/strhooks047b || { echo "strhooks teeth NOT confirmed"; exit 1; }
JOBS=${JOBS:-3}
T0=$(date +%s)
for prt in $(seq 8891 8907); do fuser -k -TERM $prt/tcp 2>/dev/null || true; done; sleep 2

sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project13/drive.py > /tmp/drive047b.py
sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project14/drive_patient.py > /tmp/drive_patient047b.py

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
launch play      play     /tmp/drive_patient047b.py 8895 50 1000  clean,I.STOP   book "${STR[@]}"
launch play-st   play     /tmp/drive047b.py         8899 50 1000  clean,I.STOP   stock "${STR[@]}"
launch fo        failopen /tmp/drive047b.py         8891 50 1000  clean,I.STOP   book "${STR[@]}"  QUEST_FAIL_OPEN=USER_DATA_FILE
launch m         m        /tmp/drive047b.py         8892 50 1000  I.STOP         book "${STR[@]}"
launch inj       play     /tmp/drive047b.py         8893 50 1000  FATAL          book "${STR[@]}"  QUEST_INJECT=7016A896:-1:0x2006
launch abort     m        /tmp/drive047b.py         8894 50 10    WORLD-ABORT    book "${STR[@]}" QUEST_TERMINAL=7016871D:ABORT
launch inj3      m        /tmp/drive047b.py         8896 50 1000  clean,I.STOP   book "${STR[@]}"  QUEST_INJECT=70176AA7:-1:0x2006:RESUME
launch k1fo      failopen /tmp/drive047b.py         8897 1  1000  clean,I.STOP   book "${STR[@]}"  QUEST_FAIL_OPEN=USER_DATA_FILE
launch derr      failopen /tmp/drive047b.py         8904 1  1000  WORLD-ABORT    book "${STR[@]}"  QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_POKE=7015C48B:0:11
launch derr-emu  failopen /tmp/drive047b.py         8905 1  1000  WORLD-ABORT    emu "${STR[@]}"   QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_POKE=7015C48B:0:11
launch fo-st     failopen /tmp/drive047b.py         8898 50 1000  clean,I.STOP   stock "${STR[@]}" QUEST_FAIL_OPEN=USER_DATA_FILE
launch fo-emu    failopen /tmp/drive047b.py         8900 50 1000  clean,I.STOP   emu "${STR[@]}"   QUEST_FAIL_OPEN=USER_DATA_FILE
launch m-emu     m        /tmp/drive047b.py         8901 50 1000  I.STOP         emu "${STR[@]}"
launch inj-emu   play     /tmp/drive047b.py         8902 50 1000  FATAL          emu "${STR[@]}"   QUEST_INJECT=7016A896:-1:0x2006
launch abort-emu m        /tmp/drive047b.py         8903 50 10    WORLD-ABORT    emu "${STR[@]}"   QUEST_TERMINAL=7016871D:ABORT
launch forced    failopen /tmp/drive047b.py         8907 1  10    DIVERGENCE     book "${STR[@]}" QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_POKE=7015C48B:2:0x75000000:CLONE
wait || true

# Collate in canonical order.
: > $RES/verdicts.txt
FAILS=0
ORDER="fo m inj abort play inj3 k1fo derr derr-emu fo-st play-st fo-emu m-emu inj-emu abort-emu forced"
for tag in $ORDER; do
  cat $RES/$tag.verdict >> $RES/verdicts.txt 2>/dev/null || { echo "$tag: MISSING VERDICT" >> $RES/verdicts.txt; FAILS=$((FAILS+1)); }
  [ "$(cat $RES/$tag.status 2>/dev/null)" = "OK" ] || FAILS=$((FAILS+1))
done
echo "== Task 047b P33-B (post P33-C) arena twins battery (checker ON, QUEST_ARENA) (JOBS=$JOBS) ==" | tee -a $RES/verdicts.txt
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
echo "embeds_book=$EMB (want 729 = 1673 - 944)  embeds_stock=$EMBS (want 2608 = 3552 - 944)  rt_call_book=$RTB rt_call_stock=$RTS (want 987/987)" | tee -a $RES/verdicts.txt
[ "$EMB" = "729" ] && [ "$EMBS" = "2608" ] && [ "$RTB" = "987" ] && [ "$RTS" = "987" ] || FAILS=$((FAILS+1))
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
echo "string_statements: book=$STRB stock=$STRS (want 1593/1593 = 649 P31 + 944 P32); literal assignments=$LITB (want 857 = 534 + 323) cmp=$CMPB (want 40 = 31 + 9) words=$WDB (want 12)" | tee -a $RES/verdicts.txt
[ "$STRB" = "1593" ] && [ "$STRS" = "1593" ] && [ "$LITB" = "857" ] && [ "$CMPB" = "40" ] && [ "$WDB" = "12" ] || FAILS=$((FAILS+1))
SLED=$W/docs/Project31/strings.ledger; STSV=$W/docs/Project31/p31.tsv; STSV32=$W/docs/Project32/p32.tsv
SE=$(grep -c '^[0-9A-F]\{8\} [A-Z]* emitted ' $SLED || true); SR=$(grep -c '^[0-9A-F]\{8\} [A-Z]* REFUSED ' $SLED || true)
TE=$(awk -F'\t' '$7=="EMIT"' $STSV | wc -l); TR=$(awk -F'\t' '$7=="REFUSE"' $STSV | wc -l)
TE32=$(awk -F'\t' '!/^#/ && $7=="EMIT"' $STSV32 | wc -l); TR32=$(awk -F'\t' '!/^#/ && $7=="REFUSE"' $STSV32 | wc -l)
SAME=$(diff <(grep '^[0-9A-F]\{8\} [A-Z]* emitted ' $SLED | cut -d' ' -f1 | sort) <(cat <(awk -F'\t' '$7=="EMIT"{print $1}' $STSV) <(awk -F'\t' '!/^#/ && $7=="EMIT"{print $1}' $STSV32) | sort) >/dev/null && echo yes || echo NO)
# text byte-compare: the ledger's ir column against the artifacts' ir column, per pc
SAMETXT=$(diff <(grep '^[0-9A-F]\{8\} [A-Z]* emitted ' $SLED | sed 's/^\([0-9A-F]*\) [A-Z]* emitted [^\t]*\t//' | sort) <(cat <(awk -F'\t' '$7=="EMIT"{sub(/ ; .*/, "", $11); print $11}' $STSV) <(awk -F'\t' '!/^#/ && $7=="EMIT"{sub(/ ; .*/, "", $11); print $11}' $STSV32) | sort) >/dev/null && echo yes || echo NO)
echo "strings ledger: lower.py emitted=$SE refused=$SR; census EMIT p31=$TE/$TR p32=$TE32/$TR32 (want 1593/0; 649/127; 944/0); same pc set=$SAME same text=$SAMETXT" | tee -a $RES/verdicts.txt
[ "$SE" = "1593" ] && [ "$SR" = "0" ] && [ "$TE" = "649" ] && [ "$TR" = "127" ] && [ "$TE32" = "944" ] && [ "$TR32" = "0" ] && [ "$SAME" = "yes" ] && [ "$SAMETXT" = "yes" ] || FAILS=$((FAILS+1))
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
CL=$(grep -c '^  claim t@' $IRB || true); RL=$(grep -c '^  release t@' $IRB || true); BA=$(grep -c '= t@[0-9A-F]*\.[0-9]* ; WADI' $IRB || true)
TW=$(grep -c '^temp ' $ARENA || true); GR=$(grep -c '^== group .* EMIT$' $W/docs/Project33/p33.ledger || true)
echo "ir headers: book '$HB' stock '$HS' (want ir 6)  embeds book=$EB (want 557) stock=$ES (want 2436)" | tee -a $RES/verdicts.txt
echo "string family embeds remaining WCMV/WMSP/STASP: book $WB/$MB/$SB stock $WS/$MS/$SS (want 0/0/0)" | tee -a $RES/verdicts.txt
echo "twins $TW (want 57); groups EMIT $GR/19; IR bases=$BA claims=$CL releases=$RL (want 57/57/19)" | tee -a $RES/verdicts.txt
[ "$HB" = "ir 6" ] && [ "$HS" = "ir 6" ] && [ "$WB$MB$SB$WS$MS$SS" = "000000" ] && [ "$TW" = "57" ] && [ "$GR" = "19" ] && [ "$BA" = "57" ] && [ "$CL" = "57" ] && [ "$RL" = "19" ] || FAILS=$((FAILS+1))
echo "-- hooks (per leg; clone claim must be 0 everywhere) --" | tee -a $RES/verdicts.txt
for tag in $ORDER; do cat $RES/$tag.hooks 2>/dev/null; done | tee -a $RES/verdicts.txt
CC=$(cat $RES/*.hooks 2>/dev/null | grep -v "derr-emu\|fo-emu\|m-emu\|inj-emu\|abort-emu" | grep "/clone" | grep -vc "claim=0 " || true)
echo "IR legs with a claiming clone: $CC (want 0 — delta_clone 0 everywhere; the emu legs cancel the master's insertions instead)" | tee -a $RES/verdicts.txt
[ "$CC" = "0" ] || FAILS=$((FAILS+1))
echo "-- twins: max claim per twin over all legs (bytes; the capacity datum for quest.arena) --" | tee -a $RES/verdicts.txt
cat $RES/*.maxclaim 2>/dev/null | sed 's/.*max_claim //' | awk '{split($0,a," "); t=a[1]; v=a[3]; if(v+0>m[t]+0) m[t]=v; cap[t]=a[6]} END{for(t in m) printf "%s max=%s %s\n", t, m[t], cap[t]}' | sort | tee -a $RES/verdicts.txt
echo "-- groups exercised (master claims per block, IR legs) --" | tee -a $RES/verdicts.txt
grep -h "^strings.*QUEST1 claim" /tmp/run047b-*/trace 2>/dev/null | sed 's/.*block=\([0-9A-F]*\).*/\1/' | sort | uniq -c | sort -rn | tee -a $RES/verdicts.txt
RS=$(grep -h "first execution of.*release\|release t@" $RES/*.err 2>/dev/null | wc -l)
echo "release statements first-executed (any leg): $RS" | tee -a $RES/verdicts.txt
FR=$(grep -c 'hits UNMAPPED arena row t@70166144.1' $RES/forced.out || true)
FP=$(grep -c 'POKE firing at 7015C48B: ac2' $RES/forced.err || true)
echo "forced: poke_fired=$FP (want 1: clone only)  divergence_names_unmapped_twin=$FR (want 1)" | tee -a $RES/verdicts.txt
[ "$FP" = "1" ] && [ "$FR" = "1" ] || FAILS=$((FAILS+1))
echo "-- self-tests --" | tee -a $RES/verdicts.txt
tail -1 /tmp/selftest047b | tee -a $RES/verdicts.txt; tail -1 /tmp/strings047b | tee -a $RES/verdicts.txt; tail -1 /tmp/strhooks047b | tee -a $RES/verdicts.txt
echo "-- P26 newly-lowered mnemonic coverage (book+stock legs; regression reference) --" | tee -a $RES/verdicts.txt
python3 $W/docs/Project26/p26cov.py $IRB $RES/*.err 2>&1 | tail -3 | tee -a $RES/verdicts.txt
echo "wall_clock_seconds=$(( $(date +%s) - T0 ))" | tee -a $RES/verdicts.txt
cat $RES/verdicts.txt
[ "$FAILS" = "0" ] || { echo "TASK 047 RED ($FAILS fails)"; exit 1; }
echo "TASK 047 GREEN"
