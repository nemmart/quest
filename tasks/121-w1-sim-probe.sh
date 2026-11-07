#!/bin/bash
# Task 121-w1-sim-probe -- meshsim sim/ WORKER1: the runner probe for the C++ framework (branch sim-w1, fc27ea1).
# 1. Facts: cores, CPU, memory, cgroup and ulimit limits, disk, compilers and versions, the riscv toolchains (and clang's
# riscv32 target), Java, perf, transparent huge pages, TSan. 2. make check from the embedded kit (gcc and clang, debug with
# ASan and UBSan, release). Writes only results/121-w1-sim-probe/. Scratch in /tmp, removed first and at the end.
# Expected ~2 min. Safe to re-run.
set -u
cd "$(dirname "$0")/.."
REPO=$PWD; RES=$REPO/results/121-w1-sim-probe; mkdir -p "$RES"
W=/tmp/w1-sim-probe; rm -rf "$W"; mkdir -p "$W"
sed -n '/^__KIT_BEGIN__$/,/^__KIT_END__$/p' "$0" | sed '1d;$d' | base64 -d > "$W/kit.tgz" || exit 2
GOT=$(sha256sum "$W/kit.tgz" | cut -d' ' -f1); echo "kit sha256 $GOT"
[ "$GOT" = "8ee763013add06d8b42e542814607a14079b9cbf61c95af590cf74928aa25fef" ] || { echo "kit sha256 mismatch (want 8ee763013add06d8b42e542814607a14079b9cbf61c95af590cf74928aa25fef)"; exit 2; }
mkdir -p "$W/kit" && tar -xzf "$W/kit.tgz" -C "$W/kit" || exit 2
cat "$W/kit/VERSION"

sec() { echo; echo "==== $*"; }
ver() { for c in "$@"; do p=$(command -v "$c" 2>/dev/null); if [ -n "$p" ]; then echo "$c: $p: $("$c" --version 2>&1 | head -1)"; else echo "$c: not found"; fi; done; }
{
sec "date, host, OS"; date -u; hostname; uname -a; grep -E '^(PRETTY_NAME|VERSION_ID)=' /etc/os-release
sec "CPU"; echo "nproc $(nproc)"; lscpu
sec "memory"; free -g; grep -E 'MemTotal|AnonHugePages|HugePages_Total|Hugepagesize' /proc/meminfo
command -v numactl >/dev/null && { sec "numactl -H"; numactl -H; }
sec "cgroup and limits"; cat /proc/self/cgroup
CG=/sys/fs/cgroup$(sed -n 's/^0:://p' /proc/self/cgroup)
for f in memory.max memory.high memory.swap.max pids.max cpu.max cpuset.cpus.effective; do [ -r "$CG/$f" ] && echo "$f: $(cat "$CG/$f")"; done
ulimit -a
sec "disk"; df -h /tmp "$HOME" "$REPO" 2>&1
sec "tools"; ver gcc g++ clang clang++ make cmake ld ld.lld lld java javac perf python3 git numactl
sec "what -march=native is"; g++ -march=native -Q --help=target 2>/dev/null | grep -E '^\s+-march=|^\s+-mtune='
sec "riscv toolchains"
ver riscv64-unknown-elf-gcc riscv32-unknown-elf-gcc riscv-none-elf-gcc riscv64-linux-gnu-gcc riscv64-unknown-elf-objcopy
ls -d /opt/* /opt/riscv* "$HOME"/opt "$HOME"/riscv* /usr/local/riscv* 2>/dev/null
find /opt /usr/local "$HOME" -maxdepth 4 -name 'riscv*-gcc' 2>/dev/null | head
echo "-- clang's riscv32 target (rv32i_zicsr_zmmul, ilp32, as ref's firmware builds):"
printf 'int f(int a, int b) { return a * b + 3; }\nvoid _start(void) { for (;;) f(1, 2); }\n' > "$W/t.c"
if command -v clang >/dev/null; then
  clang --target=riscv32-unknown-elf -march=rv32i_zicsr_zmmul -mabi=ilp32 -O2 -ffreestanding -c "$W/t.c" -o "$W/t.o" 2>&1 && echo "clang riscv32 compile: ok" || echo "clang riscv32 compile: failed"
  clang --target=riscv32-unknown-elf -march=rv32i_zicsr_zmmul -mabi=ilp32 -O2 -ffreestanding -nostdlib -fuse-ld=lld "$W/t.c" -o "$W/t.elf" 2>&1 && echo "clang riscv32 link (lld): ok" || echo "clang riscv32 link (lld): failed"
fi
command -v riscv64-unknown-elf-gcc >/dev/null && { riscv64-unknown-elf-gcc -march=rv32i_zicsr_zmmul -mabi=ilp32 -O2 -ffreestanding -nostdlib -nostartfiles -static "$W/t.c" -o "$W/t2.elf" 2>&1 && echo "riscv64-unknown-elf-gcc rv32 link: ok" || echo "riscv64-unknown-elf-gcc rv32 link: failed"; }
sec "java"; java -version 2>&1
sec "perf"; echo "perf_event_paranoid: $(cat /proc/sys/kernel/perf_event_paranoid 2>&1)"; echo "kptr_restrict: $(cat /proc/sys/kernel/kptr_restrict 2>&1)"
command -v perf >/dev/null && timeout 20 perf stat -e cycles,instructions,cache-misses true 2>&1 | tail -8
sec "transparent huge pages"
for f in enabled defrag khugepaged/defrag; do echo "$f: $(cat /sys/kernel/mm/transparent_hugepage/$f 2>&1)"; done
sec "TSan (for W4)"
printf '#include <thread>\nint x;\nint main() { std::thread t([] { x = 1; }); t.join(); return x - 1; }\n' > "$W/ts.cpp"
for c in g++ clang++; do command -v $c >/dev/null || continue
  $c -std=c++20 -fsanitize=thread -O1 "$W/ts.cpp" -o "$W/ts-$c" 2>&1 | tail -2 && timeout 20 "$W/ts-$c" > "$W/ts.out" 2>&1; echo "$c -fsanitize=thread: exit $? $(head -c 300 "$W/ts.out")"; done
} > "$RES/facts.txt" 2>&1
echo "facts written ($(wc -l < "$RES/facts.txt") lines)"

# ---- 2. make check from the kit, timed
cd "$W/kit/sim" || exit 2
A=$(date +%s)
timeout -s TERM 20m make check > "$RES/make-check.log" 2>&1; RC=$?
echo "make check: exit $RC, $(($(date +%s) - A)) s" | tee "$RES/make-check.summary"
grep -E '^(====|\./build)' "$RES/make-check.log" | tee -a "$RES/make-check.summary"
grep -B2 -A20 'FAIL\|error' "$RES/make-check.log" | head -80 >> "$RES/make-check.summary"
# the release build with -j, for the record
make clean >/dev/null; A=$(date +%s); make -j"$(nproc)" check > /dev/null 2>&1; echo "make -j$(nproc) check: exit $?, $(($(date +%s) - A)) s" | tee -a "$RES/make-check.summary"
cd /; rm -rf "$W"
echo "end $(date -u)"
exit $RC
__KIT_BEGIN__
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
GaGiAEABAA==
__KIT_END__
