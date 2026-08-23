#!/bin/bash
# Task 027 — P19 coverage supplement. Task 026 was GREEN (div=0 x7 legs,
# 566 loaded) but VACUOUS for C&D: no driver reached a nested proc
# (Xsites=0) and the rm leg's ?SOPEN failure took the ?LIB_ERROR->?FATAL
# path (clean detach, div=0 — incidental error-path regression) instead
# of the inline-checked RETURN_MESSAGE. This task adds the coverage:
#
#  lp  — driver mode failopen (L then P), NO fault env: LIST_PLAYERS'
#        P branch calls LIST_PLAYERS.3 via nine decorated XCALL 0,1
#        sites, once per stat column per player record. Expect Xsites>0,
#        nested write-mode WSAVS, div=0.
#  kp  — driver mode kp (K, name, password): KILL_PLAYER path toward the
#        XCALL 0,1 @7016E576. Attempt; report coverage either way.
#  rmD — QUEST_FAIL_SSHPT=1 (new knob): SYSCALL 044 fails at client boot;
#        INIT_SHARED_DATA's INLINE check (7015BE59) routes to
#        RETURN_MESSAGE,6 @7015BE74 — write-mode WSAVS, 5 ARGWR incl.
#        the 2-wide WPSH @7015BE6B (740075C2/C4 ascending), marker LCALL,
#        body checkpoints clean, 0310 retire, clone halt.
#        Expect wWSAVS == wWRTN + 1 and div=0.
#
# Unreachable-by-design, reported as such: RETURN_MESSAGE,3 @70169B82
# (LOCK_FILE lock-consistency fatal — entered only from corrupt-lock
# checks); BARGAIN/BOAT/CAST/OP_EDIT sites (gameplay-conditional).
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook
DRV=$W/docs/Project13/drive.py
RES=$ROOT/results/027-p19-coverage; mkdir -p $RES
PM=$W/c_src/quest.pushmap.ABCD
: > $RES/verdicts.txt

leg(){ # tag mode [env...]
  tag=$1; mode=$2; shift 2
  R=/tmp/run027-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
  pkill -f "[e]mulator .*QUEST" 2>/dev/null || true; sleep 1
  env QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$PM "$@" stdbuf -o0 -e0 $EMU \
      -lockstep -silent -trace $R/trace -types lockstep,redirect,gcalls \
      QUEST QUEST_SERVER @QUEST @QUEST > $R/out 2> $R/err &
  EP=$!; sleep 6; python3 $DRV $mode $R/session.log >/dev/null 2>&1 || true
  sleep 6; kill $EP 2>/dev/null || true; sleep 3; kill -9 $EP 2>/dev/null || true
  div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out 2>/dev/null || true)
  i2=$(grep -c 'MAPPER I2' $R/err 2>/dev/null || true)
  prb=$(grep -c 'MAPPER PROBE' $R/err 2>/dev/null || true)
  m4ab=$(grep -c 'M4B ABORT\|m4b_abort' $R/err 2>/dev/null || true)
  mab=$(grep -c 'MAPPER.*abort\|mapper_abort' $R/err 2>/dev/null || true)
  argwr=$(grep -c 'ARGWR' $R/trace 2>/dev/null || true)
  wsavsw=$(grep -c 'WSAVS.*mode=W' $R/trace 2>/dev/null || true)
  wrtnw=$(grep -c 'WRTN.*mode=W' $R/trace 2>/dev/null || true)
  xsites=$(grep -oE 'XCALL pc=[0-9A-F]+' $R/trace 2>/dev/null | sort -u | wc -l || echo 0)
  rmf=$(grep -c 'WSAVS RETURN_MESSAGE.*mode=W' $R/trace 2>/dev/null || true)
  printf "%-5s div=%-3s i2=%-3s probes=%-3s m4b_ab=%-3s map_ab=%-3s argwr=%-6s wWSAVS=%-6s wWRTN=%-6s Xsites=%-4s rmW=%-3s\n" \
    "$tag" "$div" "$i2" "$prb" "$m4ab" "$mab" "$argwr" "$wsavsw" "$wrtnw" "$xsites" "$rmf" | tee -a $RES/verdicts.txt
  cp $R/out $RES/$tag.out; cp $R/err $RES/$tag.err 2>/dev/null || true
  grep -E 'XCALL pc=' $R/trace > $RES/$tag.xcall_marker 2>/dev/null || true
  grep -E 'WSAVS (BARGAIN|BOAT|CAST|KILL_PLAYER|LIST_PLAYERS|OP_EDIT)[^ ]*.*mode=W' $R/trace > $RES/$tag.nested_wsavs 2>/dev/null || true
  # full first nested-XCALL window: pushes -> XCALL marker -> callee WSAVS
  if [ -s $RES/$tag.xcall_marker ]; then
    grep -nE 'ARGWR|XCALL|WSAVS' $R/trace | grep -B 4 -A 2 -m2 'XCALL pc=' > $RES/$tag.xcall_window 2>/dev/null || true
  fi
  if [ "$rmf" != "0" ]; then
    grep -nE 'ARGWR|LCALL|XCALL|WSAVS|WRTN' $R/trace | grep -B 12 -A 6 'WSAVS RETURN_MESSAGE' > $RES/$tag.rm_window 2>/dev/null || true
  fi
  if [ "$div" != "0" ]; then grep -A 30 'LOCKSTEP DIVERGENCE' $R/out | head -80 > $RES/$tag.divdump; fi
}

leg lp   failopen
leg kp   kp
leg rmD  m        QUEST_FAIL_SSHPT=1

echo "--- verdicts ---"; cat $RES/verdicts.txt
echo "--- lp: XCALL markers (unique) ---"
sort -u $RES/lp.xcall_marker 2>/dev/null | head -12 || true
echo "--- lp: first nested-XCALL window ---"
cat $RES/lp.xcall_window 2>/dev/null || echo "(none)"
echo "--- lp: nested write-mode WSAVS (unique) ---"
sed 's/^.*WSAVS/WSAVS/' $RES/lp.nested_wsavs 2>/dev/null | sort -u | head || true
echo "--- kp: XCALL markers ---"
sort -u $RES/kp.xcall_marker 2>/dev/null | head -4 || echo "(KILL_PLAYER.4 not reached)"
echo "--- rmD: RETURN_MESSAGE window ---"
cat $RES/rmD.rm_window 2>/dev/null || echo "(RETURN_MESSAGE not exercised — trigger failed, report)"
echo "--- rmD: FAIL_SSHPT + retire evidence ---"
grep -a "FAIL_SSHPT" $RES/rmD.err | head -4 || true
grep -aE "retire|DETACH|halt" $RES/rmD.err | head -6 || true
echo "--- C coverage across 027 legs (of 26) ---"
cat $RES/*.xcall_marker 2>/dev/null | grep -oE 'pc=[0-9A-F]+' | sort -u | wc -l
echo "TASK 027 DONE"
