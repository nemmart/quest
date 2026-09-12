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
# P54 reopening (a003 item 1): P53's reproducer — a naive game->game call
# through tests/lowerc_rig.cpp (a symbol-less Machine) used to segfault with
# no diagnostic, 40/40. Pin it: the rig must exit 0 on the read-only
# reproducer in docs/Project53/f1-repro/.
g++ -std=c++17 -O2 -I. tests/lowerc_rig.cpp $OBJS -lpthread -o /tmp/lowerc_rig_p54
# (copied to /tmp first: the rig writes and removes <program>.rigerr beside
# its input, and that directory is P53's and holds a committed .rigerr)
cp ../docs/Project53/f1-repro/call_symbolic_crash.ir ../docs/Project53/f1-repro/call_symbolic_crash.vmap /tmp/
REPRO=/tmp/call_symbolic_crash
/tmp/lowerc_rig_p54 --program $REPRO.ir --vmap $REPRO.vmap --addrbook quest.addrbook >/tmp/lowerc_rig_p54.out 2>/tmp/lowerc_rig_p54.err \
  || { echo "F-1 REPRODUCER STILL FAILS (exit $?)"; tail -3 /tmp/lowerc_rig_p54.err; exit 1; }
echo "F-1 reproducer through lowerc_rig: exit 0"
# P54 reopening (a003 item 2b, P53 q006): RUN the spec's worked examples,
# extracted from docs/IR.md at run time. Teeth: the same leg against a copy
# with 5.10.10's varying destination respelled as the bare cell name (the
# stale form P53 met) must go RED with the aggregate-has-no-CONTENTS refusal.
g++ -std=c++17 -O2 -I. tests/spec_examples_selftest.cpp $OBJS -lpthread -o /tmp/spec_examples_selftest
/tmp/spec_examples_selftest 2>/tmp/spec_examples_selftest.err | tee -a /tmp/bridge_selftest.out
grep -q "SPEC EXAMPLES SELFTEST GREEN" /tmp/bridge_selftest.out
sed 's|\[@wp(QUEST.v2, 0), 8 varying\]|[@QUEST.v2, 8 varying]|' ../docs/IR.md > /tmp/IR_stale_example.md
set +e
QUEST_IR_SPEC=/tmp/IR_stale_example.md /tmp/spec_examples_selftest > /tmp/spec_examples_stale.out 2>/dev/null
set -e
grep -q "SPEC EXAMPLES SELFTEST RED" /tmp/spec_examples_stale.out
grep -q "has no CONTENTS" /tmp/spec_examples_stale.out
echo "spec examples: GREEN against docs/IR.md, RED against the stale spelling (teeth confirmed)"
echo "BRIDGE SELFTEST: GREEN ($(grep -c 'first execution of block .* at 77' /tmp/bridge_selftest.err) symbolic blocks first-executed), broken-bridge build RED on the stack-balance check (teeth confirmed)"
