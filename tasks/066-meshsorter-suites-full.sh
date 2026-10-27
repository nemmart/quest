#!/bin/bash
# Task 066 -- mesh-sorter: meshsim v1.1's full-size suites (CtrlTest full, EngineTest full, the skew table,
# the full sweep). Input: /tmp/runner.tgz, copied to the box by hand; checked by SHA-256 first.
# Runs runner/suites/run_suites.sh -j 4 in /tmp and commits only VERDICT.txt and the last 30 lines of each
# suite's log; the full logs stay in /tmp/ms-066/out. Expected 10-15 min, about 18 GB.
set -u
cd "$(dirname "$0")/.."
RES=$PWD/results/066-meshsorter-suites-full; mkdir -p "$RES"
SRC=/tmp/runner.tgz
WANT=e0e646e76c5862a4cf36b8ab6271defec834307d2a8ffb575254eab3514fa533
[ -f "$SRC" ] || { echo "missing $SRC"; exit 2; }
GOT=$(sha256sum "$SRC" | cut -d' ' -f1)
echo "runner.tgz sha256 $GOT"
[ "$GOT" = "$WANT" ] || { echo "sha256 mismatch (want $WANT)"; exit 2; }
W=/tmp/ms-066; rm -rf "$W"; mkdir -p "$W"
tar -xzf "$SRC" -C "$W" || exit 2
cd "$W/runner/suites" || exit 2
echo "start $(date -u) on $(hostname), $(nproc) threads"; java -version 2>&1 | head -1
./run_suites.sh -j 4 "$W/out"; RC=$?
echo "run_suites.sh exit $RC at $(date -u)"
cp "$W/out/VERDICT.txt" "$RES/" 2>/dev/null
mkdir -p "$RES/tails"
for f in "$W"/out/*.log; do [ -f "$f" ] && tail -30 "$f" > "$RES/tails/$(basename "$f")"; done
cat "$W/out/VERDICT.txt" 2>/dev/null
exit $RC
