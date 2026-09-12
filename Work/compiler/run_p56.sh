#!/bin/sh
# compiler/run_p56.sh — reproduce every Project 56 result from a clean tree.
#
#   cd Work && sh compiler/run_p56.sh
#
# Nothing here executes game code.  This is a text-to-text match.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/p56
mkdir -p "$T"

echo "== 1. compile the first case (P53's compilation unit) =================="
python3 compiler/lower_c.py game/routines/HIT_ANY_CHAR.c game/routines/GET_INPUT.c \
    --routine HIT_ANY_CHAR --routine GET_INPUT -o "$T/HIT_ANY_CHAR.ir" >/dev/null
echo "   ok"
echo

echo "== 2. the LDAFP census (a001 Q3(c)) ===================================="
python3 compiler/ldafp_census.py --examples 4
echo

echo "== 3. the match ========================================================"
python3 compiler/match_routine.py --entry HIT_ANY_CHAR \
    --ir "$T/HIT_ANY_CHAR.ir" \
    --oracle docs/Project56/oracles/HIT_ANY_CHAR.oracle --verbose
echo

echo "== 4. teeth — the comparator must FAIL when the oracle is wrong ========"
O=docs/Project56/oracles/HIT_ANY_CHAR.oracle
t() {
    printf '   %-34s ' "$1"
    python3 compiler/match_routine.py --entry HIT_ANY_CHAR --ir "$T/HIT_ANY_CHAR.ir" \
        --oracle "$2" 2>&1 | grep -E 'RESULT|HARD ERROR' | head -1 | sed 's/^ *//'
}
sed 's/place  v0            frame+2/place  v0            frame+3/'      "$O" > "$T/t1"
sed 's/place  v4            frame+4/place  v4            frame+6/'      "$O" > "$T/t2"
sed 's|place  v1            0x7016DE07:0|place  v1            frame+4|' "$O" > "$T/t3"
grep -v pack_varying_imm "$O" > "$T/t4"
grep -v 'place  v2'      "$O" > "$T/t5"
t "v0 misplaced by one word"      "$T/t1"
t "the merge broken"              "$T/t2"
t "an illegal merge (live ranges)" "$T/t3"
t "the R3 rewrite removed"        "$T/t4"
t "a placement omitted"           "$T/t5"
echo

echo "== 5. monotonicity (DESIGN 7.2b, pre-registered in q001 7) ============="
python3 compiler/monotonicity.py --entry HIT_ANY_CHAR \
    --ir "$T/HIT_ANY_CHAR.ir" --oracle docs/Project56/oracles/HIT_ANY_CHAR.oracle
