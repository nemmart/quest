#!/bin/bash
# Build + run the P30 strings self-test against the current hw/ objects,
# then rebuild it with the library source compiled -DP30_BROKEN_RESIDUE
# (a deliberately wrong ac3 after a copy) and require THAT to go RED —
# the test has teeth, as helpers_selftest did. Exit 0 only when the
# library is GREEN and the broken build is RED.
set -eu
cd "$(dirname "$0")/.."
make -j"$(nproc)" >/dev/null
OBJS=$(ls hw/*.o hw/strings/*.o os/*.o debug/*.o runtime/*.o | grep -v Launch)
g++ -std=c++17 -O2 -I. tests/strings_selftest.cpp $OBJS -lpthread -o /tmp/strings_selftest
/tmp/strings_selftest | tee /tmp/strings_selftest.out
grep -q "STRINGS SELFTEST GREEN" /tmp/strings_selftest.out
# teeth: same test, the library recompiled broken (its .o excluded)
BROKEN=$(echo "$OBJS" | grep -v hw/strings/EagleString.o)
g++ -std=c++17 -O2 -I. -DP30_BROKEN_RESIDUE tests/strings_selftest.cpp hw/strings/EagleString.cpp $BROKEN -lpthread -o /tmp/strings_selftest_broken
set +e
/tmp/strings_selftest_broken > /tmp/strings_selftest_broken.out
set -e
tail -1 /tmp/strings_selftest_broken.out | sed 's/^/broken build: /'
grep -q "STRINGS SELFTEST RED" /tmp/strings_selftest_broken.out
echo "STRINGS SELFTEST: library GREEN, broken build RED (teeth confirmed)"
