#!/bin/bash
# Build + run the helper self-test against the current hw/ objects.
set -eu
cd "$(dirname "$0")/.."
make -j"$(nproc)" >/dev/null
g++ -std=c++17 -O2 -I. tests/helpers_selftest.cpp $(ls hw/*.o os/*.o debug/*.o runtime/*.o | grep -v Launch) -lpthread -o /tmp/helpers_selftest
/tmp/helpers_selftest
