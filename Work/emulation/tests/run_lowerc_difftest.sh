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
echo "=== the compiler's own invariants ==="
# Naive means naive: zero t-places anywhere in the output (a001 R3), and no
# effectful operator (a001 R2).  Both are greppable properties.
IRS=$(find /tmp/p48-difftest -name 'prog.ir' 2>/dev/null | head -60)
if [ -n "$IRS" ]; then
  BAD=$(grep -hoE '\bt[0-9]+\b' $IRS | wc -l)
  EFF=$(grep -hoE '\b(add|sub|mul|div|cvwn|ash|nadd|nsub|nmul)\(' $IRS | wc -l)
  echo "  t-places emitted: $BAD (must be 0)"
  echo "  effectful ops emitted: $EFF (must be 0)"
  [ "$BAD" = "0" ] && [ "$EFF" = "0" ] || { echo "  INVARIANT VIOLATED"; exit 1; }
fi

echo
echo "=== teeth: every deliberate soundness bug must be CAUGHT ==="
# A differential tester that has never gone red proves nothing (the P46
# -DP46_BROKEN_ALLOC precedent).  Each --mutate is one plausible wrong
# lowering; --expect-red succeeds only when the corpus DISAGREES.
for M in u16_unsigned no_sign_extend eager_bool shift_logical cmp_unsigned abs_argtype; do
  if python3 "$DT" --seeds 6 --mutate "$M" --expect-red --rig "$RIGDIR/lowerc_rig" \
       >/tmp/p48/teeth.$M.txt 2>&1; then
    echo "  caught: $M ($(grep -c '^!! ' /tmp/p48/teeth.$M.txt) of 18 programs)"
  else
    echo "  NOT CAUGHT: $M — the tester has a hole"; exit 1
  fi
done

echo
echo "LOWERC DIFFTEST: GREEN"
