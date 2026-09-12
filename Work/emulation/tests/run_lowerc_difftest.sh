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
# ---------------------------------------------------------------------------
# EXPECTED RED between P52 and P53 (P52/q001 R10, granted in P52/a002).
# ir 8 (docs/IR.md §5.10.4) changed what a `v` NAME means: in ir 7 it was the
# cell's ADDRESS, in ir 8 it is the cell's CONTENTS. `compiler/lower_c.py`
# still emits `ir 7` with v declarations in the OLD meaning (lower_c.py:1025),
# so the ir 8 loader REFUSES its output — loudly, which is the correct
# failure and far better than reinterpreting it silently. Updating the
# emitter IS Project 53.
#
# Two independent causes, both for P53:
#   1. lower_c.py emits `ir 7` + `v` -> refused by the §5.10.9 compatibility
#      window (which exists so quest.ir2.{book,stock}, declaring no v and no
#      a, keep loading untouched).
#   2. compiler/difftest/cases/{sub_traps,shortcircuit}.c still call SUB();
#      P52's boundary (P52/a002 R8) covered lower_c.py, cgen.py and
#      make_update_screens_fixture.py only — 6 tokens in 2 case files are
#      outside it. Renaming them would not restore green anyway, because (1)
#      stands until P53.
# Set QUEST_DIFFTEST_EXPECT_RED=0 once P53 has landed the ir 8 emitter.
# ---------------------------------------------------------------------------
if [ "${QUEST_DIFFTEST_EXPECT_RED:-1}" = "1" ]; then
  echo "run_lowerc_difftest.sh: SKIPPED — expected red between P52 and P53 (see the banner"
  echo "  in this script; docs/Project52/REPORT.md §4). Re-enable with QUEST_DIFFTEST_EXPECT_RED=0."
  exit 0
fi
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
echo "=== UPDATE_SCREENS end to end ==="
# Stage C: the C this project wrote, compiled, loaded and RUN as a standalone
# v-form program.  Expectations come from an independent model of the
# disassembly (make_update_screens_fixture.py), never from the compiler.
MK=../compiler/difftest/make_update_screens_fixture.py
US=../game/routines/UPDATE_SCREENS.c
python3 ../compiler/lower_c.py "$US" --routine UPDATE_SCREENS \
        --entry UPDATE_SCREENS -o /tmp/p48/us.ir
python3 ../compiler/lower_c.py "$US" --routine UPDATE_SCREENS \
        --entry UPDATE_SCREENS --mutate no_sign_extend -o /tmp/p48/us_bad.ir
for SC in "" "--neg" "--wrap"; do
  python3 "$MK" $SC > /tmp/p48/us.fixture
  R=$("$RIGDIR/lowerc_rig" --program /tmp/p48/us.ir --vmap /tmp/p48/us.vmap \
        --addrbook ./quest.addrbook --fixture /tmp/p48/us.fixture 2>/dev/null \
        | grep '^EXPECT:')
  echo "  ${SC:-positive}: $R"
  case "$R" in *", 0 failed") ;; *) echo "  UPDATE_SCREENS FAILED"; exit 1;; esac
done
# the DERR 17 leg: player_count past PLAYER's bound must abort at the first SUB
python3 "$MK" --trap > /tmp/p48/us_trap.fixture
T=$("$RIGDIR/lowerc_rig" --program /tmp/p48/us.ir --vmap /tmp/p48/us.vmap \
      --addrbook ./quest.addrbook --fixture /tmp/p48/us_trap.fixture 2>/dev/null)
case "$T" in TRAP\ DERR17*) echo "  trap leg: $T";;
             *) echo "  trap leg did not fire: $T"; exit 1;; esac
# teeth: only the --wrap scenario can tell sx16 from zx16 in this routine,
# because every 16-bit datum it reads is used inside a DIFFERENCE of two
# 16-bit data, and such a difference is invariant under +65536.
python3 "$MK" --wrap > /tmp/p48/us.fixture
B=$("$RIGDIR/lowerc_rig" --program /tmp/p48/us_bad.ir --vmap /tmp/p48/us_bad.vmap \
      --addrbook ./quest.addrbook --fixture /tmp/p48/us.fixture 2>/dev/null \
      | grep '^EXPECT:')
case "$B" in *", 0 failed") echo "  teeth: a no_sign_extend build PASSED — no teeth"; exit 1;;
             *) echo "  teeth: no_sign_extend build caught ($B)";; esac

echo
echo "LOWERC DIFFTEST: GREEN"
