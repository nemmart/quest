#!/bin/bash
# explore.sh start <tag> | explore.sh send <tag> <steps...> | explore.sh stop <tag>
W=/home/claude/work/Work; CMD=$1; TAG=$2; shift 2; RUN=/home/claude/runs/$TAG
case $CMD in
start)
  rm -rf $RUN; mkdir -p $RUN; cp -r /home/claude/work/QUEST $RUN/QUEST; cd $RUN
  pkill -f "[e]mulator .*QUEST" 2>/dev/null; sleep 1
  QUEST_ADDRESS_BOOK=${BOOK:-/tmp/full.addrbook} setsid nohup stdbuf -o0 -e0 $W/c_src/emulator -trace $RUN/trace -types redirect QUEST QUEST_SERVER @QUEST > $RUN/stdout 2> $RUN/stderr < /dev/null &
  sleep 5; : > $RUN/cmd
  setsid nohup python3 $W/docs/Project13/explore.py $RUN > $RUN/explore.out 2>&1 < /dev/null &
  sleep 12; cat $RUN/screen.txt ;;
send)
  n=$(grep -c "" $RUN/screen.txt); for st in "$@"; do echo "$st" >> $RUN/cmd; done
  tot=0; for st in "$@"; do w=${st##*:}; tot=$(python3 -c "print($tot+$w)"); done
  sleep $(python3 -c "print($tot+3)"); tail -n +$((n+1)) $RUN/screen.txt ;;
stop)
  echo QUIT >> $RUN/cmd; sleep 3; pkill -f "[e]mulator .*QUEST"; sleep 2
  grep "^redirect" $RUN/trace > $RUN/redirect.log
  echo "redirect WSAVS per routine:"; grep -o 'WSAVS [A-Za-z0-9_.@]*' $RUN/redirect.log | sort | uniq -c | sort -rn
  grep -i "exception\|what()\|AREA:" $RUN/stderr | grep -v "RESERVED\|Segment fault\|INTWT\|EXIT" | head -5 ;;
esac
