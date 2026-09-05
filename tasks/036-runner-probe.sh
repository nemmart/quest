#!/bin/bash
# Task 036 — runner probe (Sep 5 2026, new runner box, no systemd).
# Reports host + toolchain and does a clean build of the emulator.
# No game runs. DONE = the runner is alive and the box can build.
set -u
cd "$(dirname "$0")/.."
echo "== host";     hostname; uname -a; nproc; free -h | head -2; df -h . | tail -1
echo "== cpu";      grep -m1 'model name' /proc/cpuinfo
echo "== git";      git rev-parse --short HEAD; git status --short | head
echo "== g++";      g++ --version | head -1
echo "== make";     make --version | head -1
echo "== python3";  python3 --version
echo "== java";     java -version 2>&1 | head -1 || echo "no java"
echo "== javac";    javac -version 2>&1 | head -1 || echo "no javac"
echo "== build";    cd Work/c_src && make clean >/dev/null 2>&1; time make -j"$(nproc)" >/dev/null && ls -l emulator
