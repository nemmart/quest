#!/bin/bash
# Task 001 — smoke: prove the loop. Build the emulator, build the tools,
# verify the book loads (launch aborts cleanly without a server; we only
# check the build + book-parse path here).
set -eu
cd "$(dirname "$0")/.."
echo "== toolchain =="
g++ --version | head -1
nproc
echo "== emulator build =="
cd Work/c_src && make -j"$(nproc)" 2>&1 | tail -2 && ls -l emulator
echo "== tools build =="
cd ../../Tools && javac -nowarn *.java */*.java 2>&1 | head -3 || true
ls Follow.class ArgWindows.class
echo "== book sanity =="
grep -c "^7" ../Work/c_src/quest.addrbook
echo "SMOKE OK"
