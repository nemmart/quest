#!/bin/bash
# Task 124-w6-mesh-bench -- meshsim sim/ WORKER6 (branch sim-w6 490ab2a). TIMING TASK: keep the box quiet while it runs.
# W6's busy-mesh benchmark (sim/bench/mesh.cpp): the link word as (a) two words, a 128-bit payload as a ValuePool index,
# against (b) three words inline; the delivery entry's destination as a word (e1) or a parallel array (e2); runtime against
# compile-time widths; ready marks against polling. 8 x 8 XY routers, credits, links of latency 3. One mesh on CPU 0 for the
# full matrix (both payload mixes, rates 0.1 and 0.35); the saturation sweep; -O3, -march=native, clang; then 128 meshes on
# 1, 6 (CPUs 0,2,..,10) and 12 (0-11) P-core threads and the E-cores (12, 12-15) for the best layout. perf stat per run,
# perf record on the busiest case. 3 repeats. Expected ~6 min.
# From an embedded kit (sim/src/base and sim/bench at 490ab2a). Writes only results/124-w6-mesh-bench/. Scratch in /tmp, removed first
# and at the end. Safe to re-run.
set -u
cd "$(dirname "$0")/.."
REPO=$PWD; RES=$REPO/results/124-w6-mesh-bench; mkdir -p "$RES"; rm -f "$RES/raw.txt"
W=/tmp/w6-124-w6-mesh-bench; rm -rf "$W"; mkdir -p "$W"
sed -n '/^__KIT_BEGIN__$/,/^__KIT_END__$/p' "$0" | sed '1d;$d' | base64 -d > "$W/kit.tgz" || exit 2
GOT=$(sha256sum "$W/kit.tgz" | cut -d' ' -f1); echo "kit sha256 $GOT"
[ "$GOT" = "495334909a09e7216572be0ffb7c4a7e5c94c157bd544637b2fee07beb3e750e" ] || { echo "kit sha256 mismatch (want 495334909a09e7216572be0ffb7c4a7e5c94c157bd544637b2fee07beb3e750e)"; exit 2; }
mkdir -p "$W/kit" && tar -xzf "$W/kit.tgz" -C "$W/kit" || exit 2
K=$W/kit/sim/bench
FAILS=0; FAIL=0
sec() { echo; echo "==== $*  [$(date +%T)]"; }
verdict() { echo "VERDICT $1: $2"; [ "$2" = PASS ] || FAIL=1; }
START=$(date +%s)
{
sec "machine"; uname -r; nproc; lscpu | grep -E 'Model name|^CPU\(s\)'; uptime
# Shared by W6's task bodies (sourced). Needs W (scratch), RES (results dir), K (kit's sim/bench dir).
# run TAG CPUSET cmd...: one run under taskset and perf stat; appends "TAG <bench line> perf: ..." to $RES/raw.txt.
EV=cycles,instructions,cache-misses,L1-dcache-load-misses
run() {
  local tag=$1 cpus=$2; shift 2
  taskset -c "$cpus" perf stat -x, -e $EV -o "$W/perf.csv" -- "$@" > "$W/out.txt" 2> "$W/err.txt"
  local rc=$?
  local line; line=$(grep -E '^(mesh|fp128) ' "$W/out.txt" | tail -1)
  if [ $rc -ne 0 ] || [ -z "$line" ]; then
    echo "RUNFAIL $tag rc=$rc: $*"; head -5 "$W/err.txt"; FAILS=$((FAILS + 1)); return 1
  fi
  local pc; pc=$(awk -F, '
    $3 ~ /cycles/ && $3 !~ /ref/ { split($3, a, "/"); k = (a[1] == "cpu_atom") ? "atom" : "core"; c[k] = $1 }
    $3 ~ /instructions/ { split($3, a, "/"); k = (a[1] == "cpu_atom") ? "atom" : "core"; i[k] = $1 }
    $3 ~ /cache-misses/ { split($3, a, "/"); k = (a[1] == "cpu_atom") ? "atom" : "core"; m[k] = $1 }
    $3 ~ /L1-dcache-load-misses/ { split($3, a, "/"); k = (a[1] == "cpu_atom") ? "atom" : "core"; l[k] = $1 }
    END { printf "core_cycles=%s core_instr=%s core_cmiss=%s core_l1miss=%s atom_cycles=%s atom_instr=%s atom_cmiss=%s",
          c["core"], i["core"], m["core"], l["core"], c["atom"], i["atom"], m["atom"] }' "$W/perf.csv")
  echo "$tag $line perf: $pc" | tee -a "$RES/raw.txt"
}
# field NAME < lines: the value of NAME=...
field() { grep -o "$1=[^ ]*" | cut -d= -f2; }
# Task body: W6's mesh benchmark (sourced by the task after the kit is unpacked; see mk_task.sh).
cd "$K" || exit 2
sec "build (gcc -O2, -O3, -O2 -march=native; clang -O2)"
make -j8 all-builds > "$RES/build.log" 2>&1 || { tail -20 "$RES/build.log"; exit 2; }
ls build
B=build/gcc-O2/mesh
C1="--cycles 300000 --warmup 30000"     # one mesh, rate 0.35
C2="--cycles 1000000 --warmup 100000"   # one mesh, rate 0.1
CS="--cycles 10000 --warmup 1000"       # 128 meshes

sec "1. saturation: offered against accepted (lw=a e1 rt, injection queue 64 deep, 1 run each)"
for r in 0.30 0.33 0.35 0.36 0.37 0.40 0.50; do
  run "sat" 0 $B --rate $r --injdepth 64 --cycles 200000 --warmup 50000 --cpus 0
done

sec "2. the matrix: one mesh, one thread (CPU 0), ready marks, 3 repeats"
for rep in 1 2 3; do for lw in a b; do for dl in e1 e2; do for w in rt fx; do for mix in 128 mixed; do
  run "matrix" 0 $B --lw $lw --dl $dl --width $w --mix $mix --rate 0.35 $C1 --cpus 0
  run "matrix" 0 $B --lw $lw --dl $dl --width $w --mix $mix --rate 0.1 $C2 --cpus 0
done; done; done; done; done

sec "choosing the best destination layout and width for the pair (median, rate 0.35, a and b, both mixes)"
BEST=$(grep '^matrix ' "$RES/raw.txt" | grep 'rate=0.350' | awk '{
    for (i = 1; i <= NF; i++) { split($i, kv, "="); v[kv[1]] = kv[2] }
    k = v["dl"] " " v["width"] " " v["lw"] " " v["mix"]; n[k]++; x[k, n[k]] = v["ns_per_mesh_cycle"] }
  END { for (k in n) { split(k, f, " "); m = med(x, k, n[k]); s[f[1] " " f[2]] += m }
        best = ""; for (c in s) { printf "  %s: sum of medians %.1f ns\n", c, s[c] > "/dev/stderr"; if (best == "" || s[c] < s[best]) best = c }
        print best }
  function med(x, k, c,   a, i, j, t) { for (i = 1; i <= c; i++) a[i] = x[k, i]
        for (i = 1; i <= c; i++) for (j = i + 1; j <= c; j++) if (a[j] < a[i]) { t = a[i]; a[i] = a[j]; a[j] = t }
        return a[int((c + 1) / 2)] }')
DL=${BEST% *}; WD=${BEST#* }
echo "best: dl=$DL width=$WD"

sec "3. ready marks against polling (best layout; one mesh; 3 repeats)"
for rep in 1 2 3; do for lw in a b; do for mix in 128 mixed; do
  run "poll" 0 $B --lw $lw --dl $DL --width $WD --ready poll --mix $mix --rate 0.35 $C1 --cpus 0
  run "poll" 0 $B --lw $lw --dl $DL --width $WD --ready poll --mix $mix --rate 0.1 $C2 --cpus 0
done; done; done

sec "4. compilers and flags (best layout, rate 0.35, one mesh; 3 repeats; -O2 gcc is the matrix)"
for rep in 1 2 3; do for bld in gcc-O3 gcc-native clang-O2; do for lw in a b; do for mix in 128 mixed; do
  run "flags-$bld" 0 build/$bld/mesh --lw $lw --dl $DL --width $WD --mix $mix --rate 0.35 $C1 --cpus 0
done; done; done; done

sec "5. phase shares (rdtsc per phase; not a timing run)"
for lw in a b; do for mix in 128 mixed; do
  run "phases-1" 0 $B --lw $lw --dl $DL --width $WD --mix $mix --rate 0.35 --cycles 100000 --warmup 10000 --phases --cpus 0
  run "phases-128" 0 $B --lw $lw --dl $DL --width $WD --mix $mix --rate 0.35 --meshes 128 --cycles 2000 --warmup 200 --phases --cpus 0
done; done

sec "6. scale: 128 independent meshes (rate 0.35; P: 1 thread, 6 = one per P-core, 12 = with HT; E: 1 and 4; 3 repeats)"
for rep in 1 2 3; do for lw in a b; do for mix in 128 mixed; do
  V="--lw $lw --dl $DL --width $WD --mix $mix --rate 0.35 --meshes 128 $CS"
  run "scale-P1" 0-11 $B $V --cpus 0
  run "scale-P6" 0-11 $B $V --cpus 0,2,4,6,8,10 --threads 6
  run "scale-P12" 0-11 $B $V --cpus 0-11 --threads 12
  run "scale-E1" 12-15 $B $V --cpus 12
  run "scale-E4" 12-15 $B $V --cpus 12-15 --threads 4
done; done; done
sec "7. digests: every variant, rate and mix agree; the scale runs agree across thread counts"
for key in "mix=128 rate=0.350" "mix=mixed rate=0.350" "mix=128 rate=0.100" "mix=mixed rate=0.100"; do
  n=$(grep -E '^(matrix|poll|flags-)' "$RES/raw.txt" | grep "$key" | grep -v 'meshes=128' | field digest | sort -u | wc -l)
  [ "$n" = 1 ] && verdict "digests one-mesh $key" PASS || verdict "digests one-mesh $key ($n distinct)" FAIL
done
for mix in 128 mixed; do
  n=$(grep '^scale-' "$RES/raw.txt" | grep "mix=$mix " | field digest | sort -u | wc -l)
  [ "$n" = 1 ] && verdict "digests 128 meshes mix=$mix, all thread counts" PASS || verdict "digests 128 meshes mix=$mix ($n distinct)" FAIL
done

sec "8. perf record: the busiest case (128 meshes, 128-bit, rate 0.35, 1 thread), a and b"
for lw in a b; do
  taskset -c 0 perf record -q -F 4000 -o "$W/perf-$lw.data" -- $B --lw $lw --dl $DL --width $WD --mix 128 --rate 0.35 \
    --meshes 128 --cycles 4000 --warmup 400 --cpus 0 > /dev/null 2>&1
  perf report -i "$W/perf-$lw.data" --stdio --no-children --sort symbol 2>/dev/null | grep -v '^#' | grep -v '^$' \
    | head -25 > "$RES/perf-report-$lw.txt"
  echo "-- lw=$lw"; cat "$RES/perf-report-$lw.txt"
done
[ "$FAILS" = 0 ] && verdict "all runs completed" PASS || verdict "all runs completed ($FAILS failed)" FAIL
sec "done in $(($(date +%s) - START)) s"
} 2>&1 | tee "$RES/run.log"
grep '^VERDICT' "$RES/run.log" > "$RES/VERDICTS.txt"
rm -rf "$W"
grep -q ': FAIL$' "$RES/VERDICTS.txt" && exit 1
exit 0
__KIT_BEGIN__
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
ALgBAA==
__KIT_END__
