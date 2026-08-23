#!/bin/bash
# Task 026 — P19 tranches C & D: full battery on quest.pushmap.ABCD
# (535 A+B sites + 26 XCALL sites + 5 RETURN_MESSAGE sites = 566) after
# landing (a) the XCALL marker hook (EagleStack.cpp case XCALL — identical
# to the LCALL hook, P18 XPEFB/LPEFB precedent) and (b) the ?SOPEN
# QUEST_FAIL_OPEN scaffolding (OSContextShared.cpp, TEMPORARY) that lets
# a leg drive INIT_SHARED_DATA's inline SYSCALL-failure path into
# RETURN_MESSAGE @7015BE74.
#
# SCOPING.md verification asks (verify, don't rebuild):
#  1. XCALL marker/argc convention == LCALL's (code-identical push word;
#     write-mode WSAVS is opcode-agnostic) -> C sites fire at div=0 with
#     trace evidence (XCALL pc= lines, WSAVS <nested> mode=W).
#  2. RETURN_MESSAGE noreturn: write-mode WSAVS opens, body runs, 0310
#     retires, clone halts; never-popped record moot -> rm leg div=0 AND
#     wWSAVS == wWRTN + 1 on legs where RETURN_MESSAGE fired.
#  3. Mid-body checkpoints inside RETURN_MESSAGE stay clean (div=0 covers;
#     stack_offset already handles open-record mid-window).
#  4. Pass-by-ref @70169B82: pointers redirected, temps stay on the real
#     stack. rm2 leg attempts to reach it; report coverage either way.
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook
DRV=$W/docs/Project13/drive.py; PAT=$W/docs/Project14/drive_patient.py
RES=$ROOT/results/026-p19-CD-battery; mkdir -p $RES
PM=$W/c_src/quest.pushmap.ABCD
: > $RES/verdicts.txt

# the 26 decorated XCALL pcs and 5 RETURN_MESSAGE call pcs, from the maps
awk '$1=="call"{print $2}' $W/c_src/quest.pushmap.C > $RES/c_callpcs
awk '$1=="call"{print $2}' $W/c_src/quest.pushmap.D > $RES/d_callpcs

leg(){ # tag mode driver [env...]
  tag=$1; mode=$2; drv=$3; shift 3
  R=/tmp/run026-$tag; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
  pkill -f "[e]mulator .*QUEST" 2>/dev/null || true; sleep 1
  env QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$PM "$@" stdbuf -o0 -e0 $EMU \
      -lockstep -silent -trace $R/trace -types lockstep,redirect,gcalls \
      QUEST QUEST_SERVER @QUEST @QUEST > $R/out 2> $R/err &
  EP=$!; sleep 6; python3 $drv $mode $R/session.log >/dev/null 2>&1 || true
  sleep 6; kill $EP 2>/dev/null || true; sleep 3; kill -9 $EP 2>/dev/null || true
  div=$(grep -c 'LOCKSTEP DIVERGENCE' $R/out 2>/dev/null || true)
  i2=$(grep -c 'MAPPER I2' $R/err 2>/dev/null || true)
  prb=$(grep -c 'MAPPER PROBE' $R/err 2>/dev/null || true)
  m4ab=$(grep -c 'M4B ABORT\|m4b_abort' $R/err 2>/dev/null || true)
  mab=$(grep -c 'MAPPER.*abort\|mapper_abort' $R/err 2>/dev/null || true)
  argwr=$(grep -c 'ARGWR' $R/trace 2>/dev/null || true)
  wsavsw=$(grep -c 'WSAVS.*mode=W' $R/trace 2>/dev/null || true)
  wrtnw=$(grep -c 'WRTN.*mode=W' $R/trace 2>/dev/null || true)
  lsites=$(grep -oE 'LCALL pc=[0-9A-F]+' $R/trace 2>/dev/null | sort -u | wc -l || echo 0)
  xsites=$(grep -oE 'XCALL pc=[0-9A-F]+' $R/trace 2>/dev/null | sort -u | wc -l || echo 0)
  rmfired=$(grep -cE 'LCALL pc=(7015BE74|70169B82|70175EC8|70175EFF|7017D7D9)' $R/trace 2>/dev/null || true)
  printf "%-6s div=%-3s i2=%-3s probes=%-3s m4b_ab=%-3s map_ab=%-3s argwr=%-6s wWSAVS=%-6s wWRTN=%-6s Lsites=%-4s Xsites=%-4s rm_calls=%-3s\n" \
    "$tag" "$div" "$i2" "$prb" "$m4ab" "$mab" "$argwr" "$wsavsw" "$wrtnw" "$lsites" "$xsites" "$rmfired" | tee -a $RES/verdicts.txt
  cp $R/out $RES/$tag.out; cp $R/err $RES/$tag.err 2>/dev/null || true
  grep -oE '(LCALL|XCALL) pc=[0-9A-F]+' $R/trace 2>/dev/null | sort -u > $RES/$tag.callpcs || true
  # C-site evidence: every redirect line at an XCALL pc or a nested-callee WSAVS
  grep -E 'XCALL pc=' $R/trace > $RES/$tag.xcall_marker 2>/dev/null || true
  grep -E 'WSAVS (BARGAIN|BOAT|CAST|KILL_PLAYER|LIST_PLAYERS|OP_EDIT)[^ ]*.*mode=W' $R/trace > $RES/$tag.nested_wsavs 2>/dev/null || true
  # D-site evidence: full windows around RETURN_MESSAGE write-mode saves
  if grep -qE 'WSAVS RETURN_MESSAGE.*mode=W' $R/trace 2>/dev/null; then
    grep -nE 'ARGWR|LCALL|XCALL|WSAVS|WRTN' $R/trace | grep -B 12 -A 4 'WSAVS RETURN_MESSAGE' > $RES/$tag.rm_window 2>/dev/null || true
  fi
  # pass-by-ref site specifically (WPSH 3-wide at 70169B81)
  grep -E 'ARGWR pc=70169B81' $R/trace > $RES/$tag.pbr_argwr 2>/dev/null || true
  if [ "$div" != "0" ]; then grep -A 30 'LOCKSTEP DIVERGENCE' $R/out | head -80 > $RES/$tag.divdump; fi
}

echo "--- push_map.ABCD load check ---"
R=/tmp/run026-load; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
env QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$PM $EMU -lockstep -silent \
    QUEST QUEST_SERVER @QUEST @QUEST </dev/null >/dev/null 2>$RES/loadcheck.err & LP=$!
sleep 4; kill $LP 2>/dev/null||true; sleep 1; kill -9 $LP 2>/dev/null||true
cd $ROOT
grep 'caller map' $RES/loadcheck.err || true
echo "decorated calls loaded: $(grep -c '^AddressBook: decorated call' $RES/loadcheck.err 2>/dev/null||echo 0) (expect 566)"
grep -i 'not inside\|not a book\|out of range\|bad push-map\|duplicate' $RES/loadcheck.err | head -5 || echo "(no load errors)"

# standard five legs on the full map
leg m     m        $DRV
leg fo    m        $DRV
leg inj   play     $DRV   QUEST_INJECT=7016A896:-1:0x2006
leg abort m        $DRV   QUEST_TERMINAL=7016871D:ABORT
leg play  play     $PAT
# D triggers: rm = INIT_SHARED_DATA path (?SOPEN failure at client boot ->
# RETURN_MESSAGE,6 @7015BE74 incl. the 2-wide WPSH); the driver's login
# will fail because the client terminates — that is the expected outcome.
leg rm    m        $DRV   QUEST_FAIL_OPEN=SHARED_DATA_FILE
# rm2 = user-file failure path, probing whether LOCK_FILE's pass-by-ref
# RETURN_MESSAGE,3 @70169B82 is reachable this way (report either way).
leg rm2   m        $DRV   QUEST_FAIL_OPEN=USER_DATA_FILE

echo "--- C&D verdicts ---"; cat $RES/verdicts.txt
echo "--- XCALL marker lines (all legs, unique, first 30) ---"
cat $RES/*.xcall_marker 2>/dev/null | sort -u | head -30
echo "--- nested-callee write-mode WSAVS (unique) ---"
cat $RES/*.nested_wsavs 2>/dev/null | sed 's/^.*WSAVS/WSAVS/' | sort -u | head -20
echo "--- RETURN_MESSAGE write-mode windows ---"
for f in $RES/*.rm_window; do [ -s "$f" ] && { echo "[$f]"; tail -24 "$f"; }; done 2>/dev/null || echo "(RETURN_MESSAGE not exercised — coverage gap, report)"
echo "--- pass-by-ref WPSH @70169B81 ARGWR lines ---"
cat $RES/*.pbr_argwr 2>/dev/null | sort -u | head -6 || true
[ -s "$(ls $RES/*.pbr_argwr 2>/dev/null | head -1)" ] || echo "(70169B82 not exercised — coverage gap, report)"
echo "--- C-site coverage (decorated XCALL pcs fired, of 26) ---"
cat $RES/*.callpcs 2>/dev/null | grep XCALL | sort -u > $RES/all.xcallpcs || true
wc -l < $RES/all.xcallpcs
comm -12 <(sort $RES/c_callpcs) <(sed 's/XCALL pc=//' $RES/all.xcallpcs | sort) | head -30
echo "--- all decorated call sites fired, of 566 ---"
cat $RES/*.callpcs 2>/dev/null | sed 's/^[A-Z]*CALL pc=//' | sort -u | wc -l
echo "TASK 026 DONE"
