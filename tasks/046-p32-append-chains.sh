#!/bin/bash
# Task 046 — P32 append chains (docs/Project32/{PROMPT,Census,REPORT}.md).
# 044's 15 legs (034 template + derr + derr-emu, JOBS=3) on branch p32-append-chains:
#   - the P31 correction (docs/Project31/Census.md §11): 17 statements that rendered a
#     frame slot ?UNSIGNED_TO_CHAR had overwritten — p31 649 EMIT / 127 REFUSE;
#   - ir 5 slice 6: 944 more WCMV/WCMP sites are located-string statements — chain
#     first pieces and continuations ([@ac2, n] = piece), copy-outs with expression
#     counts, bounded copies, SUBSTR pieces, CALLRESULT / residue counts, the 9 compares
#     P31 refused; per-operand expression-or-register rendering, 0 refused.  Embeds
#     1,673 -> 729 (book), 3,552 -> 2,608 (stock); 1,593 string statements in both modes.
#   - the play drivers grew a HELP step (H, 1, 0): HELP's two-stage chain
#     (7016D5E3..7016D602) is a named check.
#   - sync list unchanged (quest.synclist.p27, 13,510); P27/P28 lines as in 042/044.
# Verdict lines: embeds_book/stock == 729/2608; string statements == 1593 in both artifacts
# (literal assignments 857 = 534 + 323, cmp 40, words 12); lower.py's strings.ledger ==
# p31.tsv ∪ p32.tsv EMIT sets (1593, same pcs, same text); string-statement coverage by
# slice (4/5/6 each > 0 live) and form; HELP chain block 7016D5E3 live in play/play-st;
# 0 literal mismatches.  Source tree via bin/task_source.sh.
# Landing bar: 15/15 green, 0 div, embeds 729/2608, string statements 1593/1593,
# slices 4/5/6 live, HELP live, WRITE_SCREEN and RANDOM_NUMBER rt_call blocks live,
# derr pair at 7015C48E as in 042.
set -eu
exec 9>/tmp/quest-parallel-battery.lock
flock -n 9 || { echo "another battery attempt is still running; refusing overlap"; exit 1; }
cd "$(dirname "$0")/.."
ROOT=$(pwd)
SRC=$(bin/task_source.sh p32-append-chains 046-p32-append-chains); W=$SRC/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook; PMAP=$W/c_src/quest.pushmap.M4
IRB=$W/c_src/quest.ir2.book; IRS=$W/c_src/quest.ir2.stock
BLK=$W/c_src/quest.blocks.split; SYN=$W/c_src/quest.synclist.p27
RES=$ROOT/results/046-p32-append-chains; mkdir -p $RES
JOBS=${JOBS:-3}
T0=$(date +%s)
for prt in $(seq 8851 8865); do fuser -k -TERM $prt/tcp 2>/dev/null || true; done; sleep 2

sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project13/drive.py > /tmp/drive046.py
sed 's|("127.0.0.1", 8781)|("127.0.0.1", int(__import__("os").environ.get("QUEST_PORT", "8781")))|' \
  $W/docs/Project14/drive_patient.py > /tmp/drive_patient046.py

cat > /tmp/blk046.py <<'PYEOF'
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
  local R=/tmp/run046-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
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
  local blk bstat=OK; blk=$(python3 /tmp/blk046.py $R/trace $k $floor) || bstat=FAIL
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
launch play      play     /tmp/drive_patient046.py 8855 50 1000  clean,I.STOP   book
launch play-st   play     /tmp/drive046.py         8859 50 1000  clean,I.STOP   stock
launch fo        failopen /tmp/drive046.py         8851 50 1000  clean,I.STOP   book  QUEST_FAIL_OPEN=USER_DATA_FILE
launch m         m        /tmp/drive046.py         8852 50 1000  I.STOP         book
launch inj       play     /tmp/drive046.py         8853 50 1000  FATAL          book  QUEST_INJECT=7016A896:-1:0x2006
launch abort     m        /tmp/drive046.py         8854 50 10    WORLD-ABORT    book  QUEST_TERMINAL=7016871D:ABORT
launch inj3      m        /tmp/drive046.py         8856 50 1000  clean,I.STOP   book  QUEST_INJECT=70176AA7:-1:0x2006:RESUME
launch k1fo      failopen /tmp/drive046.py         8857 1  1000  clean,I.STOP   book  QUEST_FAIL_OPEN=USER_DATA_FILE
launch derr      failopen /tmp/drive046.py         8864 1  1000  IR-ASSERT      book  QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_POKE=7015C48B:0:11
launch derr-emu  failopen /tmp/drive046.py         8865 1  1000  WORLD-ABORT    emu   QUEST_FAIL_OPEN=USER_DATA_FILE QUEST_POKE=7015C48B:0:11
launch fo-st     failopen /tmp/drive046.py         8858 50 1000  clean,I.STOP   stock QUEST_FAIL_OPEN=USER_DATA_FILE
launch fo-emu    failopen /tmp/drive046.py         8860 50 1000  clean,I.STOP   emu   QUEST_FAIL_OPEN=USER_DATA_FILE
launch m-emu     m        /tmp/drive046.py         8861 50 1000  I.STOP         emu
launch inj-emu   play     /tmp/drive046.py         8862 50 1000  FATAL          emu   QUEST_INJECT=7016A896:-1:0x2006
launch abort-emu m        /tmp/drive046.py         8863 50 10    WORLD-ABORT    emu   QUEST_TERMINAL=7016871D:ABORT
wait || true

# Collate in canonical order.
: > $RES/verdicts.txt
FAILS=0
ORDER="fo m inj abort play inj3 k1fo derr derr-emu fo-st play-st fo-emu m-emu inj-emu abort-emu"
for tag in $ORDER; do
  cat $RES/$tag.verdict >> $RES/verdicts.txt 2>/dev/null || { echo "$tag: MISSING VERDICT" >> $RES/verdicts.txt; FAILS=$((FAILS+1)); }
  [ "$(cat $RES/$tag.status 2>/dev/null)" = "OK" ] || FAILS=$((FAILS+1))
done
echo "== Task 046 P32 append chains battery (JOBS=$JOBS) ==" | tee -a $RES/verdicts.txt
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
ok2 = all(bys[k] > 0 for k in (4, 5, 6)) and all(help_live.values()) and len(help_live) == 2
print("P32: slices 4/5/6 live (want all > 0): %s; HELP chain block 7016D5E3 live in play/play-st: %s -> %s" % (
      all(bys[k] > 0 for k in (4, 5, 6)), help_live, "OK" if ok2 else "FAIL"))
sys.exit(0 if (ok and ok2) else 1)
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
