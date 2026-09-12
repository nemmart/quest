#!/bin/bash
# Build + run the P48 differential tester: compiler/lower_c.py's output against
# gcc, on programs that are NOT game routines (DESIGN §2 obligation (a); the
# standing rule is that a match failure is never a reason to edit the
# compiler, and nothing here has ever read quest.ir2.book).
#
#   tests/run_lowerc_difftest.sh [seeds] [seed0]
#
# Legs:
#   1. build the rig (tests/lowerc_rig.cpp) against the emulator's objects
#   2. the hand-written edge-case suite (compiler/difftest/cases/)
#   3. N generated programs per class over classes A (flattened), B (nested),
#      C (subscript-heavy), with the construct census printed at the end
#
# Exit 0 only when every case and every generated program AGREES and no
# program was discarded as UB (a non-zero UB count is a cgen.py bug).
set -eu
cd "$(dirname "$0")/.."
SEEDS="${1:-100}"
SEED0="${2:-1}"
RIGDIR=/tmp/p48
mkdir -p "$RIGDIR"

make -j"$(nproc)" >/dev/null
OBJS=$(ls hw/*.o hw/strings/*.o os/*.o debug/*.o runtime/*.o | grep -v Launch)
g++ -std=c++17 -O2 -I. tests/lowerc_rig.cpp $OBJS -lpthread -o "$RIGDIR/lowerc_rig"

DT=../compiler/difftest/difftest.py
echo "=== hand-written edge cases ==="
python3 "$DT" --cases --rig "$RIGDIR/lowerc_rig"
echo
echo "=== $SEEDS seeds x 3 classes ==="
python3 "$DT" --seeds "$SEEDS" --seed0 "$SEED0" --rig "$RIGDIR/lowerc_rig"
echo
echo "LOWERC DIFFTEST: GREEN"
