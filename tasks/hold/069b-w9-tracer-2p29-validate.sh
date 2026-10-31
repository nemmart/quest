#!/bin/bash
# Task 069b (re-run of 069; 069 found no tarball) -- meshsim WORKER9: the standalone jolt-cpp tracer at jolt-cpp 821e1bd (jolt-asic-planning
# branch tracer-standalone-main, commit 502dc8a), on the 2^29 workloads, with --validate.
# Input: /tmp/w9-tracer.tgz, copied to the box by hand (not stored in this repo); checked by SHA-256 first.
# It holds tracer/ (git archive of 502dc8a), PR #1's old EVM Zone program with its trace-29 input, and
# Ubuntu 24.04's libelf-dev/libelf1t64 unpacked (used only if the box has no libelf.h).
# Builds with CMake (fetches fmt, nlohmann_json, xbyak from GitHub), runs `make runs`, then capture_writes
# --validate on all three workloads (full expansion: ~30 GB RAM each, one at a time).
# Commits only logs and the small JSON summaries; the multi-GB outputs stay in /tmp/w9-069.
# Expected ~10 min.
set -u
cd "$(dirname "$0")/.."
RES=$PWD/results/069b-w9-tracer-2p29-validate; mkdir -p "$RES"
SRC=/tmp/w9-tracer.tgz
WANT=0d46b00806b9529cb1d4e65b7354c5aab3fc54b1ba0db9bcc05ecf83b347f8df
[ -f "$SRC" ] || { echo "missing $SRC"; exit 2; }
GOT=$(sha256sum "$SRC" | cut -d' ' -f1); echo "w9-tracer.tgz sha256 $GOT"
[ "$GOT" = "$WANT" ] || { echo "sha256 mismatch (want $WANT)"; exit 2; }
W=/tmp/w9-069b; rm -rf "$W"; mkdir -p "$W"; tar -xzf "$SRC" -C "$W" || exit 2
cd "$W/w9/tracer" || exit 2
echo "start $(date -u) on $(hostname), $(nproc) threads"; free -g | head -2
EXTRA=()
if [ ! -e /usr/include/libelf.h ] && [ ! -e /usr/include/libelf/libelf.h ]; then
  echo "no system libelf.h: using the bundled libelf"
  EXTRA=(-DCMAKE_PREFIX_PATH="$W/w9/libelf/usr" -DCMAKE_LIBRARY_PATH="$W/w9/libelf/usr/lib/x86_64-linux-gnu")
  export LD_LIBRARY_PATH="$W/w9/libelf/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
fi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "${EXTRA[@]}" > "$RES/configure.log" 2>&1 || { tail -30 "$RES/configure.log"; exit 3; }
cmake --build build -j8 > "$RES/build.log" 2>&1 || { tail -30 "$RES/build.log"; exit 3; }
grep -i warning "$RES/build.log" | sort | uniq -c | head
RC=0
TIMEV=(); [ -x /usr/bin/time ] && TIMEV=(/usr/bin/time -v)
run() { # name, then the command; logs wall time and peak RSS
  local name=$1; shift
  "${TIMEV[@]}" "$@" > "$RES/$name.log" 2>&1; local rc=$?
  grep -E 'INFO|ERROR|VALIDATION|Elapsed|Maximum resident' "$RES/$name.log" | grep -v 'Compiled\|Loaded' | tail -12
  echo "$name exit $rc"; [ $rc = 0 ] || RC=$rc
}
run make-runs make -C build runs
cp build/out/*.json "$RES/" 2>/dev/null
ls -la build/out
CW=build/capture_writes; V=$W/validate; mkdir -p "$V"
run validate-evm-zone-29 $CW evm-zone-4e3451c1d8c5 evm-zone-4e3451c1d8c5/inputs/trace-29-4599adc50712 $V/evm-zone-29 --validate
run validate-cyclone-29  $CW cyclone cyclone/input/29/input-15961266 $V/cyclone-29 --validate
run validate-old-evm-zone-29 $CW ../old-evm-zone ../old-evm-zone/inputs/trace-29-e23d2a1c5937 $V/old-evm-zone-29 --validate
cp "$V/old-evm-zone-29.json" "$RES/"
cmp "$V/evm-zone-29.bin" build/out/evm-zone-29.bin && echo "evm-zone-29: validate run .bin == make runs .bin"
cmp "$V/cyclone-29.bin" build/out/cyclone-29.bin && echo "cyclone-29: validate run .bin == make runs .bin"
sha256sum build/out/*.bin $V/*.bin | tee "$RES/sha256.txt"
echo "end $(date -u), exit $RC"
exit $RC
