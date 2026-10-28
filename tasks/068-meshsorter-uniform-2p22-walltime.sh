#!/bin/bash
# Task 068 -- mesh-sorter: wall time of ONE full-size uniform sort on meshsim v1.1: 2^22 records
# (64 tiles x 65,536; 32-bit keys, 32-bit values; kit run p4/uniform-kv32), from a fresh unpack, so the
# timing includes unpacking, key generation and setup as well as the simulated sort itself.
# Input: /tmp/runner.tgz (sha-checked). Commits only timing.txt, VERDICT.txt, SUMMARY.md and all.csv.
set -u
cd "$(dirname "$0")/.."
RES=$PWD/results/068-meshsorter-uniform-2p22-walltime; mkdir -p "$RES"
SRC=/tmp/runner.tgz
WANT=e0e646e76c5862a4cf36b8ab6271defec834307d2a8ffb575254eab3514fa533
[ -f "$SRC" ] || { echo "missing $SRC"; exit 2; }
[ "$(sha256sum "$SRC" | cut -d' ' -f1)" = "$WANT" ] || { echo "sha256 mismatch"; exit 2; }
now(){ date +%s.%N; }
T0=$(now)
W=/tmp/ms-068; rm -rf "$W"; mkdir -p "$W"
tar -xzf "$SRC" -C "$W" || exit 2
T1=$(now)
cd "$W/runner/p4" || exit 2
echo "p4/uniform-kv32" > "$W/runlist.txt"
echo "start $(date -u) on $(hostname), $(nproc) threads"; java -version 2>&1 | head -1
./run_p4.sh -j 1 -l "$W/runlist.txt" "$W/out"; RC=$?
T2=$(now)
{
  echo "run: p4/uniform-kv32, 2^22 records (64 x 65,536), 32-bit keys and values, -j 1, fresh unpack"
  echo "host: $(hostname), $(nproc) threads"
  printf 'unpack:                         %8.1f s\n' "$(echo "$T1 - $T0" | bc)"
  printf 'key generation + setup + sort:  %8.1f s\n' "$(echo "$T2 - $T1" | bc)"
  printf 'total wall time:                %8.1f s\n' "$(echo "$T2 - $T0" | bc)"
  echo "run_p4.sh exit $RC"
} | tee "$RES/timing.txt"
cp "$W/out/VERDICT.txt" "$W/out/SUMMARY.md" "$W/out/all.csv" "$RES/" 2>/dev/null
ls "$W/out" | head -20
cat "$W/out/VERDICT.txt" 2>/dev/null
exit $RC
