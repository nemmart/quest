#!/bin/bash
# Build + run the P46 (ir 7) v-form self-test against the current objects:
# a hand-written naive IR program (declared v's, symbolic blocks) loads, is
# placed (every v at 0x76, every block at 0x77) and executes correctly
# through Machine::run_steps; the teeth leg checks every loader refusal of
# docs/IR.md §5.10. Then rebuild IRExec with -DP46_BROKEN_ALLOC (the v
# allocator never advances) and require THAT run to go RED on the
# disjointness assertion — the assertion cannot fire in ir 7 by construction
# (docs/IR.md §5.10.3), so this is the only evidence it is live. Exit 0 only
# when the real build is GREEN and the broken build is RED for that reason.
set -eu
cd "$(dirname "$0")/.."
make -j"$(nproc)" >/dev/null
OBJS=$(ls hw/*.o hw/strings/*.o os/*.o debug/*.o runtime/*.o | grep -v Launch)
g++ -std=c++17 -O2 -I. tests/vform_selftest.cpp $OBJS -lpthread -o /tmp/vform_selftest
/tmp/vform_selftest 2>/tmp/vform_selftest.err | tee /tmp/vform_selftest.out
grep -q "VFORM SELFTEST GREEN" /tmp/vform_selftest.out
BROKEN=$(echo "$OBJS" | grep -v hw/IRExec.o)
g++ -std=c++17 -O2 -I. -DP46_BROKEN_ALLOC tests/vform_selftest.cpp hw/IRExec.cpp $BROKEN -lpthread -o /tmp/vform_selftest_broken
set +e
/tmp/vform_selftest_broken > /tmp/vform_selftest_broken.out 2>/dev/null
set -e
grep "FAIL load program" /tmp/vform_selftest_broken.out | sed 's/^/broken build: /'
grep -q "VFORM SELFTEST RED" /tmp/vform_selftest_broken.out
grep -q "two v's share an address (allocator bug)" /tmp/vform_selftest_broken.out
echo "VFORM SELFTEST: GREEN ($(grep -c '^IRExec: v ' /tmp/vform_selftest.err) v placements, $(grep -c '^IRExec: block ' /tmp/vform_selftest.err) block placements logged; $(grep -c 'first execution of block .* at 77' /tmp/vform_selftest.err) symbolic blocks first-executed), broken-allocator build RED on the disjointness assertion (teeth confirmed)"
