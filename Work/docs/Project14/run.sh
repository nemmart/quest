#!/bin/bash
# run.sh <tag> <mode> [env assignments...]  — Project 14 Phase B battery runner.
# Same contract as Project13/run.sh plus: PORT=<n> (default 8781) for parallel
# runs — each run gets its own scratch QUEST copy AND its own port; no pkill
# when PORT is non-default (concurrent launches are legitimate).
TAG=$1; MODE=$2; shift 2
W=/home/claude/work/Work
EMU=${EMU:-$W/c_src/emulator}
BOOK=${BOOK:-$W/c_src/quest.addrbook}
DRV=${DRV:-$W/docs/Project13/drive.py}
PORT=${PORT:-8781}
RUN=/home/claude/runs/$TAG; rm -rf $RUN; mkdir -p $RUN
cp -r /home/claude/work/QUEST $RUN/QUEST
cd $RUN
if [ "$PORT" = "8781" ]; then pkill -f "[e]mulator .*QUEST" 2>/dev/null; sleep 1; fi
env QUEST_ADDRESS_BOOK=$BOOK QUEST_PORT=$PORT "$@" stdbuf -o0 -e0 $EMU -lockstep -silent -trace $RUN/trace -types scalls,rtcalls,redirect,gcalls QUEST QUEST_SERVER @QUEST @QUEST > $RUN/stdout 2> $RUN/stderr &
EPID=$!
sleep 6
python3 $DRV $MODE $RUN/session.log $PORT
sleep 5
kill $EPID 2>/dev/null; sleep 3; kill -9 $EPID 2>/dev/null
grep "^redirect " $RUN/trace > $RUN/redirect.log 2>/dev/null
grep "^gcalls " $RUN/trace > $RUN/gcalls.log 2>/dev/null
echo "== $TAG/$MODE: divergences=$(grep -c 'LOCKSTEP DIVERGENCE' $RUN/stdout) redirect_lines=$(grep -c '' $RUN/redirect.log) detach=$(grep -c 'DETACHED' $RUN/stderr) abort=$(grep -ci 'WORLD ABORT' $RUN/stdout $RUN/stderr | awk -F: '{s+=$2} END{print s}') probes=$(grep -c 'MAPPER PROBE' $RUN/stderr)"
echo "-- coverage: gcalls(clone=QUEST2) vs redirect WSAVS per routine:"
python3 $W/docs/Project13/coverage.py $RUN $BOOK
echo "-- exceptions:"; grep -i "exception\|terminate\|abort\|what()\|DIVERGENCE\|DETACH" $RUN/stderr | grep -v "RESERVED\|Segment fault" | head -8
