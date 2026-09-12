#!/bin/bash
# Build + run the P54 calling-bridge self-test against the current objects:
# calls OUT OF a symbolic block execute (rt_call -> native and -> emulated,
# naive game->game void/valued/nested) and return INTO a 0x77 block; PICK_X_Y
# end to end with a REPOSITION-shaped caller against a host oracle, with the
# two identity-red legs. Then rebuild IRExec with -DP54_BROKEN_BRIDGE (the
# game->game marker claims n pushed arguments that were never pushed) and
# require THAT run to go RED on the stack-balance check — the only evidence
# the balance check is live. Exit 0 only when the real build is GREEN and the
# broken build is RED for that reason. docs/Project54/.
set -eu
cd "$(dirname "$0")/.."
make -j"$(nproc)" >/dev/null
OBJS=$(ls hw/*.o hw/strings/*.o os/*.o debug/*.o runtime/*.o | grep -v Launch)
g++ -std=c++17 -O2 -I. tests/bridge_selftest.cpp $OBJS -lpthread -o /tmp/bridge_selftest
/tmp/bridge_selftest 2>/tmp/bridge_selftest.err | tee /tmp/bridge_selftest.out
grep -q "BRIDGE SELFTEST GREEN" /tmp/bridge_selftest.out
BROKEN=$(echo "$OBJS" | grep -v hw/IRExec.o)
g++ -std=c++17 -O2 -I. -DP54_BROKEN_BRIDGE tests/bridge_selftest.cpp hw/IRExec.cpp $BROKEN -lpthread -o /tmp/bridge_selftest_broken
set +e
/tmp/bridge_selftest_broken > /tmp/bridge_selftest_broken.out 2>/dev/null
set -e
grep "stack NOT balanced" /tmp/bridge_selftest_broken.out | head -3 | sed 's/^/broken build: /'
grep -q "BRIDGE SELFTEST RED" /tmp/bridge_selftest_broken.out
grep -q "stack NOT balanced" /tmp/bridge_selftest_broken.out
echo "BRIDGE SELFTEST: GREEN ($(grep -c 'first execution of block .* at 77' /tmp/bridge_selftest.err) symbolic blocks first-executed), broken-bridge build RED on the stack-balance check (teeth confirmed)"
