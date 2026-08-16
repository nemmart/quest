#!/bin/bash
# run.sh <tag> <mode> [env assignments...]
#   e.g. run.sh b1_m m   |  run.sh b1_fo failopen QUEST_FAIL_OPEN=USER_DATA_FILE
#   Book: $BOOK (default c_src/quest.addrbook). Emulator: $EMU. Scratch-copies QUEST/.
#   Prints divergence count, redirect line count (total + per routine), detach/abort lines.
TAG=$1; MODE=$2; shift 2
W=/home/claude/work/Work
EMU=${EMU:-$W/c_src/emulator}
BOOK=${BOOK:-$W/c_src/quest.addrbook}
DRV=${DRV:-$W/docs/Project13/drive.py}
RUN=/home/claude/runs/$TAG; rm -rf $RUN; mkdir -p $RUN
cp -r /home/claude/work/QUEST $RUN/QUEST
cd $RUN
pkill -f "[e]mulator .*QUEST" 2>/dev/null; sleep 1
env QUEST_ADDRESS_BOOK=$BOOK "$@" stdbuf -o0 -e0 $EMU -lockstep -silent -trace $RUN/trace -types scalls,rtcalls,redirect,gcalls QUEST QUEST_SERVER @QUEST @QUEST > $RUN/stdout 2> $RUN/stderr &
EPID=$!
sleep 6
python3 $DRV $MODE $RUN/session.log
sleep 5
kill $EPID 2>/dev/null; sleep 3; kill -9 $EPID 2>/dev/null
grep "^redirect " $RUN/trace > $RUN/redirect.log 2>/dev/null
grep "^gcalls " $RUN/trace > $RUN/gcalls.log 2>/dev/null
echo "== $TAG/$MODE: divergences=$(grep -c 'LOCKSTEP DIVERGENCE' $RUN/stdout) redirect_lines=$(grep -c '' $RUN/redirect.log) detach=$(grep -c 'DETACHED' $RUN/stderr) abort=$(grep -ci 'WORLD ABORT' $RUN/stdout $RUN/stderr | awk -F: '{s+=$2} END{print s}')"
echo "-- coverage: gcalls(clone=QUEST2) vs redirect WSAVS per routine (LIVE routines must be equal; STACKED show gcalls only):"
python3 $W/docs/Project13/coverage.py $RUN $BOOK
echo "-- exceptions:"; grep -i "exception\|terminate\|abort\|what()\|DIVERGENCE\|DETACH" $RUN/stderr | grep -v "RESERVED\|Segment fault" | head -8
