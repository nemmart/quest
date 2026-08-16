#!/bin/bash
# battery.sh — run the standing regression battery against a book.
#
# Usage:  bin/battery.sh <outdir> [legs...]
#   outdir: where per-leg logs + battery_summary.txt land
#   legs:   any of  m fo play inj abort   (default: all five)
# Env:
#   SERIAL=1        run legs one at a time (default: parallel, one port each)
#   BOOK=<path>     address book (default: Work/c_src/quest.addrbook)
#
# Each leg: fresh scratch copy of QUEST/ (COPY, never symlink — P13 §6.2),
# own port, own drive.py invocation. Verdicts collated into
# battery_summary.txt: div count, cross-check, probe count, endpoint,
# coverage table per leg. Pass criteria per the banded matrix
# (docs/Project14/REPORT.md §6): 0 div; gcalls==redirect strict; probes 0;
# endpoint pinned; world-downstream counts banded.

set -eu
REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:?usage: battery.sh <outdir> [legs...]}"; shift || true
LEGS=("${@:-m fo play inj abort}")
[ $# -gt 0 ] && LEGS=("$@") || LEGS=(m fo play inj abort)
BOOK="${BOOK:-$REPO_DIR/Work/c_src/quest.addrbook}"
BASE_PORT=8781

mkdir -p "$OUT"
cd "$REPO_DIR/Work/c_src"
make -j"$(nproc)" >/dev/null

run_leg() {
  local leg="$1" port="$2" scratch legdir
  legdir="$OUT/$leg"; mkdir -p "$legdir"
  scratch=$(mktemp -d)
  cp -r "$REPO_DIR/QUEST" "$scratch/QUEST"        # real copy
  (
    cd "$REPO_DIR/Work/docs/Project14"            # drivers live with the project
    QUEST_PORT=$port QUEST_ADDRESS_BOOK="$BOOK" QUEST_SCRATCH="$scratch/QUEST" \
      python3 drive.py "$leg" "$legdir" > "$legdir/driver.log" 2>&1
  )
  rm -rf "$scratch"
}

pids=()
i=0
for leg in "${LEGS[@]}"; do
  if [ "${SERIAL:-0}" = "1" ]; then
    run_leg "$leg" $BASE_PORT
  else
    run_leg "$leg" $((BASE_PORT + i)) &
    pids+=($!)
  fi
  i=$((i+1))
done
[ "${SERIAL:-0}" = "1" ] || wait "${pids[@]}"

# Collate. drive.py is expected to leave, per legdir: divergences.txt,
# crosscheck.txt (from coverage.py), probes.txt, endpoint.txt, coverage.txt.
summary="$OUT/battery_summary.txt"
{
  echo "# battery summary — $(date -u '+%Y-%m-%d %H:%M UTC')  book=$(basename "$BOOK")"
  overall=PASS
  for leg in "${LEGS[@]}"; do
    d=$(cat "$OUT/$leg/divergences.txt" 2>/dev/null || echo "?")
    x=$(cat "$OUT/$leg/crosscheck.txt"  2>/dev/null || echo "?")
    p=$(cat "$OUT/$leg/probes.txt"      2>/dev/null || echo "?")
    e=$(cat "$OUT/$leg/endpoint.txt"    2>/dev/null || echo "?")
    v=PASS
    [ "$d" = "0" ] || v=FAIL
    [ "$x" = "OK" ] || v=FAIL
    [ "$p" = "0" ] || v=FAIL
    [ "$v" = "PASS" ] || overall=FAIL
    printf "%-6s %-4s div=%-3s crosscheck=%-4s probes=%-3s endpoint=%s\n" "$leg" "$v" "$d" "$x" "$p" "$e"
  done
  echo "OVERALL: $overall"
  echo
  for leg in "${LEGS[@]}"; do
    echo "== $leg coverage =="; cat "$OUT/$leg/coverage.txt" 2>/dev/null || echo "(none)"; echo
  done
} > "$summary"
cat "$summary"
[ "$(grep -c '^OVERALL: PASS' "$summary")" = "1" ]
