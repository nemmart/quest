#!/bin/bash
# Task 064 -- mesh-sorter: probe the runner's environment before sending real jobs.
# Read-only and quick (well under a minute). Reports cores, the memory cap the task actually
# runs under (cgroup), Java, compilers, disk and network, to size the meshsim kit's p4 runs and
# full-program replay. Writes results/064-meshsorter-runner-probe/env.txt.
set -u
cd "$(dirname "$0")/.."
RES=results/064-meshsorter-runner-probe; mkdir -p "$RES"
OUT=$RES/env.txt
sec(){ echo; echo "===== $* ====="; }
{
sec "host and time"; hostname; date -u; uname -a
sec "cpu"; nproc; lscpu 2>/dev/null | grep -E 'Model name|^CPU\(s\)|Thread|Core|Socket|MHz|NUMA' || cat /proc/cpuinfo | grep -m4 -E 'model name|cpu MHz'
sec "memory (host)"; free -g
sec "memory (this task's cgroup cap)"
CG=$(awk -F: '$1=="0"{print $3}' /proc/self/cgroup 2>/dev/null)
for f in /sys/fs/cgroup${CG}/memory.max /sys/fs/cgroup${CG}/memory.high /sys/fs/cgroup${CG}/pids.max /sys/fs/cgroup/memory.max; do
  [ -r "$f" ] && echo "$f: $(cat "$f")"
done
sec "limits"; ulimit -a | grep -E 'processes|open files|virtual|max memory'
sec "disk"; df -h /tmp . 2>/dev/null
sec "java"; (java -version 2>&1; javac -version 2>&1) || echo "no java"
sec "compilers"; gcc --version 2>&1 | head -1; (riscv64-unknown-elf-gcc --version 2>&1 | head -1) || echo "no riscv64-unknown-elf-gcc"; (cmake --version 2>&1 | head -1) || echo "no cmake"
sec "python and git"; python3 --version 2>&1; git --version
sec "network"; for u in https://github.com https://repo1.maven.org https://archive.ubuntu.com; do printf '%s ' "$u"; curl -s -o /dev/null -m 10 -w '%{http_code}\n' "$u" || echo "unreachable"; done
sec "quick single-core speed (java, 2^27 adds)"
if command -v java >/dev/null; then
  T=/tmp/msprobe.$$; mkdir -p "$T"
  printf 'public class B{public static void main(String[] a){long t=System.nanoTime();long s=0;for(int i=0;i<(1<<27);i++)s+=i^(s>>>3);System.out.println("ms="+(System.nanoTime()-t)/1000000+" s="+s);}}\n' > "$T/B.java"
  (cd "$T" && javac B.java && java B) 2>&1; rm -rf "$T"
fi
} > "$OUT" 2>&1
cat "$OUT"
