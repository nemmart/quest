#!/bin/bash
# Task 071 -- meshsim WORKER9 part 3: the full-program replay delivery of cyclone-29 (jolt-cpp 821e1bd), written by
# delivery_writer (jolt-asic-planning tracer-standalone-main) and proven window by window by its functional replay
# from the delivered files (all three state-record variants). The delivery stays on the box in /tmp/w9-deliveries/cyclone-29
# (about 31 GB; nothing large is committed); results/ gets the log, a summary and the manifest's SHA-256.
# Input: /tmp/w9-delivery.tgz, copied to the box by hand, checked by SHA-256 (the tracer tree, the universal replayer
# tables from meshsim wip/tables/replayer, a libelf fallback). Measured on a 1-CPU sandbox: 17 min without writing files.
set -u
cd "$(dirname "$0")/.."
RES=$PWD/results/071-w9-delivery-cyclone-29; mkdir -p "$RES"
SRC=/tmp/w9-delivery.tgz; WANT=4000c1acf70e6d3795a345b592fa7d7987a23e4230631e456c8d472c3da1ce10
[ -f "$SRC" ] || { echo "missing $SRC"; exit 2; }
GOT=$(sha256sum "$SRC" | cut -d' ' -f1); echo "sha256 $GOT"; [ "$GOT" = "$WANT" ] || { echo "sha256 mismatch"; exit 2; }
W=/tmp/w9-071; rm -rf "$W"; mkdir -p "$W"; tar -xzf "$SRC" -C "$W" || exit 2
cd "$W/w9/tracer" || exit 2
echo "start $(date -u) on $(hostname), $(nproc) threads"; df -h /tmp | tail -1
EXTRA=()
if [ ! -e /usr/include/libelf.h ] && [ ! -e /usr/include/libelf/libelf.h ]; then
  EXTRA=(-DCMAKE_PREFIX_PATH="$W/w9/libelf/usr" -DCMAKE_LIBRARY_PATH="$W/w9/libelf/usr/lib/x86_64-linux-gnu")
  export LD_LIBRARY_PATH="$W/w9/libelf/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
  INC="-I$W/w9/libelf/usr/include -L$W/w9/libelf/usr/lib/x86_64-linux-gnu"
else INC=""; fi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "${EXTRA[@]}" > "$RES/configure.log" 2>&1 || { tail -30 "$RES/configure.log"; exit 3; }
cmake --build build -j8 --target tracer > "$RES/build.log" 2>&1 || { tail -30 "$RES/build.log"; exit 3; }
CXXFLAGS_EXTRA="$INC" bash -c "sed -i 's#-lelf#$INC -lelf#' delivery/build.sh; bash delivery/build.sh $PWD $PWD/build $W/dw" >> "$RES/build.log" 2>&1 || { tail -30 "$RES/build.log"; exit 3; }
OUT=/tmp/w9-deliveries; mkdir -p "$OUT"; rm -rf "$OUT/cyclone-29" "$OUT/cyclone-29.manifest.json"
T0=$(date +%s)
"$W/dw" cyclone cyclone/input/29/input-15961266 "$W/w9/tables" "$OUT" cyclone-29 --windows=all --keep=all > "$RES/delivery.log" 2>&1; RC=$?
echo "delivery_writer exit $RC after $(( $(date +%s) - T0 )) s"
grep -v INFO "$RES/delivery.log" | tail -5
du -sh "$OUT/cyclone-29" | tee "$RES/size.txt"; ls "$OUT/cyclone-29" | wc -l >> "$RES/size.txt"
sha256sum "$OUT/cyclone-29.manifest.json" | tee "$RES/manifest.sha256"
python3 -c "import json;m=json.load(open('$OUT/cyclone-29.manifest.json'));W=m['programs'][0]['window_entries'];print(len(W),'windows,',sum(w['replay']=='ok' for w in W),'replayed ok')" | tee "$RES/summary.txt"
echo "end $(date -u), exit $RC"
exit $RC
