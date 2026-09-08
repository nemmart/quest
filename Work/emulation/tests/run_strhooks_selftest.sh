#!/bin/bash
# Build + run the P33-A hook self-test against the current objects, then
# rebuild it with ClaimDelta compiled -DP33_BROKEN_DELTA_SIGN (claims
# subtract instead of add) and require THAT run to go RED — the STASP's
# "delta == 0 after release" check must catch a wrong sign. Exit 0 only when
# the hooks are GREEN and the broken build is RED.
set -eu
cd "$(dirname "$0")/.."
make -j"$(nproc)" >/dev/null
OBJS=$(ls hw/*.o hw/strings/*.o os/*.o debug/*.o runtime/*.o | grep -v Launch)
g++ -std=c++17 -O2 -I. tests/strhooks_selftest.cpp $OBJS -lpthread -o /tmp/strhooks_selftest
/tmp/strhooks_selftest | tee /tmp/strhooks_selftest.out
grep -q "STRHOOKS SELFTEST GREEN" /tmp/strhooks_selftest.out
BROKEN=$(echo "$OBJS" | grep -v hw/strings/ClaimDelta.o)
g++ -std=c++17 -O2 -I. -DP33_BROKEN_DELTA_SIGN tests/strhooks_selftest.cpp hw/strings/ClaimDelta.cpp $BROKEN -lpthread -o /tmp/strhooks_selftest_broken
set +e
/tmp/strhooks_selftest_broken > /tmp/strhooks_selftest_broken.out 2>/dev/null
set -e
tail -1 /tmp/strhooks_selftest_broken.out | sed 's/^/broken build: /'
grep -q "STRHOOKS SELFTEST RED" /tmp/strhooks_selftest_broken.out
echo "STRHOOKS SELFTEST: hooks GREEN, broken build RED (teeth confirmed)"
