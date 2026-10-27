#!/bin/bash
# Task 065 -- mesh-sorter: the sort's 43 full-size P4 runs (64 x 65,536 keys) on meshsim v1.1 (spec 26r).
# Input: /tmp/runner.tgz, copied to the box by hand (not stored in this repo); checked by SHA-256 first.
# Runs runner/p4/run_p4.sh -j 8 in /tmp and commits only the small outputs (VERDICT, SUMMARY, CSVs);
# the per-run logs stay in /tmp/ms-065/out. Expected 15-25 min, about 28 GB.
set -u
cd "$(dirname "$0")/.."
RES=$PWD/results/065-meshsorter-p4-full; mkdir -p "$RES"
SRC=/tmp/runner.tgz
WANT=e0e646e76c5862a4cf36b8ab6271defec834307d2a8ffb575254eab3514fa533
[ -f "$SRC" ] || { echo "missing $SRC"; exit 2; }
GOT=$(sha256sum "$SRC" | cut -d' ' -f1)
echo "runner.tgz sha256 $GOT"
[ "$GOT" = "$WANT" ] || { echo "sha256 mismatch (want $WANT)"; exit 2; }
W=/tmp/ms-065; rm -rf "$W"; mkdir -p "$W"
tar -xzf "$SRC" -C "$W" || exit 2
cd "$W/runner/p4" || exit 2
echo "start $(date -u) on $(hostname), $(nproc) threads"; java -version 2>&1 | head -1
./run_p4.sh -j 8 "$W/out"; RC=$?
echo "run_p4.sh exit $RC at $(date -u)"
cp "$W/out/VERDICT.txt" "$W/out/SUMMARY.md" "$W/out/all.csv" "$RES/" 2>/dev/null
mkdir -p "$RES/csv"; cp "$W"/out/*.csv "$RES/csv/" 2>/dev/null
cat "$W/out/VERDICT.txt" 2>/dev/null
exit $RC
