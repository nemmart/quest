#!/bin/bash
# run.sh <tag> <mode> [env assignments...]   e.g. run.sh base m   or   run.sh fo failopen QUEST_FAIL_OPEN=USER_DATA_FILE
# Uses emulator in $EMU (default c_src/emulator); scratch-copies QUEST/.
TAG=$1; MODE=$2; shift 2
EMU=${EMU:-/home/claude/Work__29_/Work/c_src/emulator}
RUN=/home/claude/runs/$TAG; rm -rf $RUN; mkdir -p $RUN
cp -r /home/claude/QUEST/QUEST $RUN/QUEST
cd $RUN
pkill -f "emulator .*QUEST" 2>/dev/null; sleep 1
env "$@" stdbuf -o0 -e0 $EMU -lockstep -silent -trace $RUN/trace -types scalls,rtcalls,pagemap,hijack QUEST QUEST_SERVER @QUEST @QUEST > $RUN/stdout 2> $RUN/stderr &
EPID=$!
sleep 6
python3 /home/claude/Work__29_/Work/docs/Project12/drive.py $MODE $RUN/session.log
sleep 5
kill $EPID 2>/dev/null; sleep 3; kill -9 $EPID 2>/dev/null
echo "== $TAG/$MODE: divergences=$(grep -c 'LOCKSTEP DIVERGENCE' $RUN/stdout) detach=$(grep -c 'DETACHED' $RUN/stderr) exceptions:"; grep -i "exception\|terminate\|abort\|what()" $RUN/stderr | grep -v "RESERVED\|Segment fault" | head -5
