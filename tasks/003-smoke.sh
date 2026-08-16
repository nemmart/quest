#!/bin/bash
# Task 002 — smoke rerun: build emulator, book sanity; Tools build optional
set -eu
cd "$(dirname "$0")/.."
echo "== toolchain =="
g++ --version | head -1; nproc
echo "== emulator build =="
cd Work/c_src && make -j"$(nproc)" 2>&1 | tail -1 && ls -l emulator
echo "== tools build =="
cd ../../Tools
if command -v javac >/dev/null 2>&1; then
  javac -nowarn *.java */*.java && echo "tools built"
else
  echo "SKIPPED (no javac on runner; install a JDK when a task needs the Java tools)"
fi
echo "== book sanity =="
grep -c "^7" ../Work/c_src/quest.addrbook
echo "SMOKE OK"
