#!/bin/bash
# P42 Stage 3 -- the port verification harness.
#
# The bed's four matched routines cannot verify a template they never
# exercise (P42 finding; user ruling: do NOT port those on the strength of a
# green bed run).  But behaviour preservation is not "matches the book" -- it
# is "emits exactly what it emitted before the port".  So a routine that does
# NOT match the book, or that refuses partway, is still a perfectly good
# behaviour-preservation witness for any production it fires.
#
#   BED   (4 matched)  -- byte-identical output AND 349/349 + 242/242
#   EXTRA (unmatched)  -- byte-identical output only; these routines are not
#                         expected to match the book and are not asked to
#
# Usage:  ./witness.sh snap      capture the "before"
#         ./witness.sh check     re-translate and compare
cd "$(dirname "$0")/.." || exit 1
D=/tmp/p42_witness; mkdir -p $D
BED="PICK_X_Y UPDATE_SCREENS REFRESH_SCREEN OWNS"
EXTRA="DIED:DIED HIT_ANY_CHAR:HIT_ANY_CHAR LIST_PLAYERS.3:LIST_PLAYERS.3@7016F556 QUEST.1:QUEST.1@7015C5E1"
MODE=${1:-check}
[ "$MODE" = snap ] && S=$D/snap || S=$D/now
rm -rf $S; mkdir -p $S

for R in $BED; do
  timeout 120 python3 compiler/translate.py game/routines/$R.c --routine $R \
      -o $S/$R.ir --mem ../Disassembled/quest.mem --ledger $S/$R.ledger \
      >/dev/null 2>$S/$R.err || echo "  !! $R FAILED TO TRANSLATE"
done
for E in $EXTRA; do
  f=${E%%:*}; n=${E##*:}
  timeout 120 python3 compiler/translate.py game/routines/$f.c --routine "$n" \
      -o $S/$f.ir --mem ../Disassembled/quest.mem --ledger $S/$f.ledger \
      >/dev/null 2>$S/$f.err
done

[ "$MODE" = snap ] && { echo "snapshot taken ($(ls $S/*.ir 2>/dev/null|wc -l) routines)"; exit 0; }

fail=0
echo "-- behaviour preservation (byte-identical to the pre-port snapshot) --"
for f in $D/snap/*.ir; do
  b=$(basename $f)
  if ! cmp -s "$f" "$S/$b"; then echo "  CHANGED: $b"; fail=1; fi
done
[ $fail -eq 0 ] && echo "  all $(ls $D/snap/*.ir|wc -l) routines byte-identical"

echo "-- the bed still matches the book --"
T=0; G=0
for R in $BED; do
  a=$(timeout 120 python3 compiler/ircmp.py emulation/quest.ir2.book $S/$R.ir --routine $R 2>&1|grep -o 'MATCH [0-9]*'|head -1|grep -o '[0-9]*')
  g=$(timeout 120 python3 compiler/ircmp.py emulation/quest.ir2.book $S/$R.ir --routine $R --folded 2>&1|grep -o 'MATCH [0-9]*'|head -1|grep -o '[0-9]*')
  T=$((T+a)); G=$((G+g))
done
echo "  $T/349 primary, $G/242 folded"
[ "$T" != 349 ] || [ "$G" != 242 ] && fail=1

echo "-- path coverage (every declared path fired or excused with a reason) --"
python3 compiler/pathcheck.py "$S" | sed 's/^/  /' || fail=1
exit $fail
