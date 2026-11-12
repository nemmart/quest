#!/bin/bash
# Task 177-w25-datalist -- meshsim sim/ WORKER25 (branch sim-w25; P7b). Read-only listing of the runner's data, looking
# for item 113's guest delivery (v1.3 run_tests.sh's DELIVERY=: a replay-inputs directory with programs lrd_x0, scd_x0,
# nop_hint, jalr_self, jalr_zero). Lists $DATA (/home/questrunner/data and the queue checkout's data/) to depth 3 with
# sizes, and every path under them, under the checkout's data and under /tmp whose name mentions item113, a guest or
# replay-inputs. Reads only names, sizes and the .COMPLETE markers; writes only results/177-w25-datalist/. Not timing.
# Safe to re-run.
set -u
cd "$(dirname "$0")/.."
REPO=$PWD; RES=$REPO/results/177-w25-datalist; mkdir -p "$RES"
echo "host $(hostname); $(date -u)"
PAT='item113|item-113|lrd_x0|scd_x0|nop_hint|jalr_self|jalr_zero|replay-inputs|guest'
for D in /home/questrunner/data "$REPO/data"; do
  echo; echo "==== $D"
  [ -d "$D" ] || { echo "(absent)"; continue; }
  du -sh "$D" 2>/dev/null
  find "$D" -maxdepth 3 -printf '%y %10s %TY-%Tm-%Td %p\n' 2>/dev/null | sort -k4 | head -400 | tee "$RES/$(basename "$D")-depth3.txt" | head -150
  echo "-- names matching ($PAT):"
  find "$D" -maxdepth 6 -regextype egrep -iregex ".*($PAT).*" -printf '%y %10s %p\n' 2>/dev/null | head -200 | tee "$RES/$(basename "$D")-matches.txt"
  for c in "$D"/*/*.COMPLETE "$D"/*/.COMPLETE "$D"/*.COMPLETE; do [ -f "$c" ] && echo "marker $c: $(head -c 200 "$c")"; done
done
echo; echo "==== /tmp (not persistent): names matching"
find /tmp -maxdepth 4 -regextype egrep -iregex ".*($PAT).*" -printf '%y %10s %p\n' 2>/dev/null | head -100 | tee "$RES/tmp-matches.txt"
echo; echo "==== /tmp to depth 1"
find /tmp -maxdepth 1 -printf '%y %10s %TY-%Tm-%Td %p\n' 2>/dev/null | sort -k4 | head -100
echo; echo "done $(date -u)"
exit 0
