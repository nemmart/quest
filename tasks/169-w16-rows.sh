#!/bin/bash
# Task 169-w16-rows -- meshsim sim/ WORKER16 (branch sim-w16). Not timing; a minute or two.
# T2's acceptance reference for the rows (M3Seg's check against rows.bin; AUDIT-M3 0): for each window 640-703
# (segment 10) of evm-zone-29 and cyclone-29 in $DATA/w9-deliveries, rows.bin's row count (80-byte rows) and the
# replayer's sink hash over every row's ten words (C++ Replayer::row_hash: h = 0; h = mix(h ^ word), mix splitmix64's
# finalizer), so the framework's run is checked against it without moving 670 MB of rows. Delivery checked as W13's
# tasks check it (manifest sha256 and COMPLETE). Reads only $DATA/w9-deliveries; writes only results/169-w16-rows/.
set -u
cd "$(dirname "$0")/.."
REPO=$PWD; RES=$REPO/results/169-w16-rows; mkdir -p "$RES"
DATA=${T1_DATA:-$HOME/data}; [ -d "$DATA/w9-deliveries" ] || DATA=$REPO/data
D9=$DATA/w9-deliveries
echo "persistent data: $DATA"; uptime
declare -A S9=([evm-zone-29]=7142e894c80a0c9d491f21c852a942885e0abd97cfe6aefd0101d716824e138f [cyclone-29]=f8a0622e144a944c51386b5b36f81101dcdeb16e5a8d950c1e9ca0e3097f8982)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
cat > "$T/rows.c" <<'C'
#include <stdint.h>
#include <stdio.h>
static uint64_t mix(uint64_t z) { z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull; z = (z ^ (z >> 27)) * 0x94d049bb133111ebull; return z ^ (z >> 31); }
int main(int argc, char** argv) {
    FILE* f = fopen(argv[1], "rb"); if (!f) return 2;
    uint64_t w[10], h = 0, n = 0; size_t got;
    while ((got = fread(w, 8, 10, f)) == 10) { for (int i = 0; i < 10; i++) h = mix(h ^ w[i]); n++; }
    if (got != 0 || !feof(f)) return 3;
    printf("%llu %016llx\n", (unsigned long long)n, (unsigned long long)h);
    return 0;
}
C
gcc -O2 -o "$T/rows" "$T/rows.c" || exit 1
FAIL=0
for P in evm-zone-29 cyclone-29; do
  A=$(sha256sum "$D9/$P.manifest.json" 2>/dev/null | cut -d' ' -f1)
  [ "$A" = "${S9[$P]}" ] && [ "$(cat "$D9/$P.COMPLETE" 2>/dev/null)" = "$A" ] || { echo "$P: the delivery isn't complete or differs"; FAIL=1; continue; }
  for w in $(seq 640 703); do
    F=$(printf "%s/%s/window_%05d.rows.bin" "$D9" "$P" "$w")
    r=$("$T/rows" "$F") || { echo "$P $w: cannot read $F"; FAIL=1; continue; }
    echo "$P $w $r"
  done > "$RES/$P.rows.txt"
  echo "$P: $(wc -l < "$RES/$P.rows.txt") windows"
done
head -3 "$RES"/*.rows.txt
exit $FAIL
