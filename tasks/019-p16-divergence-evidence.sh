#!/bin/bash
# Task 019 — P16 evidence capture: the task-018 divergence dump.
# Hypothesis (boundary-2 stop condition): a 500-instruction batch quantum
# boundary landed INSIDE the decorated arg window — master pushed k args
# (wsp climbed), clone wrote them (wsp flat), shadow_wsp is off by 2k at
# the pair -> wsp_differs. Task 018 saved only err; the dump went to out.
# Recover /tmp/run018-m/out if it survived, and re-run the m leg saving
# everything. NO code changes — evidence only.
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd); W=$ROOT/Work
RES=$ROOT/results/019-p16-divergence-evidence; mkdir -p $RES
# 1) salvage task 018's out if still in /tmp
for f in /tmp/run018-*/out; do
  [ -f "$f" ] && { tag=$(basename $(dirname $f)); grep -B2 -A40 'LOCKSTEP DIVERGENCE' "$f" | head -120 > $RES/salvaged-${tag#run018-}.txt || true; }
done
ls $RES || true
# 2) fresh m leg, keep out+err+trace tail
cd $W/c_src && make -j"$(nproc)" >/dev/null && cd $ROOT
EMU=$W/c_src/emulator; BOOK=$W/c_src/quest.addrbook; PMAP=$W/c_src/quest.pushmap
R=/tmp/run019-m; rm -rf $R; mkdir -p $R; cp -r $ROOT/QUEST $R/QUEST; cd $R
pkill -f "[e]mulator .*QUEST" 2>/dev/null||true; sleep 1
env QUEST_ADDRESS_BOOK=$BOOK QUEST_PUSH_MAP=$PMAP stdbuf -o0 -e0 $EMU -lockstep -silent -trace $R/trace -types lockstep,redirect,gcalls QUEST QUEST_SERVER @QUEST @QUEST >$R/out 2>$R/err &
EP=$!; sleep 6; python3 $W/docs/Project13/drive.py m $R/session.log >/dev/null 2>&1||true
sleep 6; kill $EP 2>/dev/null||true; sleep 3; kill -9 $EP 2>/dev/null||true
grep -B2 -A50 'LOCKSTEP DIVERGENCE' $R/out | head -140 > $RES/m_divergence_dump.txt || true
grep -E '^redirect ' $R/trace | tail -40 > $RES/m_trace_tail.txt || true
grep -cE 'WSAVS DIST +mode=W' $R/trace > $RES/m_write_calls_before_div.txt || true
grep -c 'ARGWR pc=' $R/trace >> $RES/m_write_calls_before_div.txt || true
cp $R/err $RES/m.err
echo "== divergence dump =="; cat $RES/m_divergence_dump.txt
echo "== trace tail =="; cat $RES/m_trace_tail.txt
echo "== write calls / argwr before div =="; cat $RES/m_write_calls_before_div.txt
echo "EVIDENCE OK"
