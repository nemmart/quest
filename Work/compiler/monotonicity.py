#!/usr/bin/env python3
"""
compiler/monotonicity.py — Project 56, DESIGN §7.2b.

"Is `ircmp` distance monotone under single rewrites?"  Nobody had measured it.
q001 §7 pre-registered a prediction with a mechanism; this runs it.

The question is about SINGLE REWRITES, not about the order an oracle happens to
be written in, so both orders are replayed:

    shipped    -- the order the oracle file states
    reordered  -- the statement-count-changing rewrite (R3) moved to the front

Distance is Levenshtein over canonicalised statements, summed per block, and is
defined only where the block bijection holds (q001 F2).

    python3 compiler/monotonicity.py --entry HIT_ANY_CHAR --ir F --oracle F
"""

import sys
import os
import copy
import argparse
from collections import OrderedDict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import irparse as P
import irmatch as M


def steps_for(oracle):
    """One step per decision.  R2 and R4 are automatic rules rather than oracle
    lines, but they are still rewrites, so the replay steps them too."""
    out = [('R2  arg-cell bridge', [], {'R2'})]
    places = list(oracle.places.items())
    for i, (name, _sp) in enumerate(places):
        rules = {'R1', 'R2'}
        # R4 fires as soon as the literal it relocates has an address
        if any(s.endswith(':0') or s.endswith(':1') for _n, s in places[:i + 1]):
            rules.add('R4')
        out.append(('R1  place %s' % name, places[:i + 1], rules))
    out.append(('R3  packed varying immediate', places, {'R1', 'R2', 'R3', 'R4'}))
    return out


def reorder(steps):
    """R3 to the front -- the ordering q001 §7 predicts will break monotonicity."""
    r3 = [s for s in steps if s[0].startswith('R3')]
    rest = [s for s in steps if not s[0].startswith('R3')]
    # R3 needs its own oracle line but NOT the placements
    r3_first = [(r3[0][0], [], {'R2', 'R3'})]
    out = r3_first + [(lbl, pl, rules | {'R3'}) for (lbl, pl, rules) in rest]
    return out


def replay(label, steps, entry_name, ir_path, oracle, book, ab):
    entry = ab[entry_name]
    ours_all = P.load_ours(ir_path)
    ours = OrderedDict((n, b) for n, b in ours_all.items()
                       if n.startswith(entry_name + '.b'))
    decls = P.load_decls(ir_path)
    nbook = M.normalize_book(P.routine_blocks(book, entry), entry, [])

    print('  %s' % label)
    print('    %-36s %8s %8s' % ('after applying', 'distance', 'delta'))

    base = M.distance(ours, nbook)
    print('    %-36s %8d %8s' % ('(nothing -- the naive form)', base, '-'))

    prev, increases, plateaus = base, 0, 0
    for (lbl, places, rules) in steps:
        sub = M.Oracle()
        sub.routine = oracle.routine
        sub.places = OrderedDict(places)
        sub.rewrites = oracle.rewrites if 'R3' in rules else []
        t = M.transform_ours(ours, decls, sub, entry, ab, [], enable=rules)
        d = M.distance(t, nbook)
        delta = d - prev
        flag = ''
        if delta > 0:
            increases += 1
            flag = '  <-- INCREASE'
        elif delta == 0:
            plateaus += 1
            flag = '  <-- plateau'
        print('    %-36s %8d %+8d%s' % (lbl, d, delta, flag))
        prev = d
    print('    final distance %d;  increases %d, plateaus %d'
          % (prev, increases, plateaus))
    print()
    return increases, plateaus, prev


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    ap = argparse.ArgumentParser()
    ap.add_argument('--entry', required=True)
    ap.add_argument('--ir', required=True)
    ap.add_argument('--oracle', required=True)
    ap.add_argument('--book', default=os.path.join(root, 'emulation/quest.ir2.book'))
    ap.add_argument('--addrbook', default=os.path.join(root, 'emulation/quest.addrbook'))
    args = ap.parse_args()

    book = P.load_book(args.book)
    ab = P.load_addrbook(args.addrbook)
    oracle = M.load_oracle(args.oracle)

    print('=' * 74)
    print('MONOTONICITY -- DESIGN §7.2b, pre-registered in q001 §7')
    print('=' * 74)
    print()

    steps = steps_for(oracle)
    ia, pa, fa = replay('ORDER A -- shipped (placement first, R3 last)',
                        steps, args.entry, args.ir, oracle, book, ab)
    ib, pb, fb = replay('ORDER B -- reordered (R3 first, before its target is placed)',
                        reorder(steps), args.entry, args.ir, oracle, book, ab)

    print('  AGAINST THE PREDICTION (q001 §7)')
    print('    claim 4: shipped order has 0 increases and 0 plateaus')
    print('             -> increases %d, plateaus %d : %s'
          % (ia, pa, 'CORRECT' if (ia, pa) == (0, 0) else 'WRONG'))
    print('    claim 5: reordered has >=1 increase')
    print('             -> increases %d : %s'
          % (ib, 'CORRECT' if ib >= 1 else 'WRONG'))
    print('    both orders reach the same final distance (%d, %d): %s'
          % (fa, fb, 'yes' if fa == fb else 'NO'))
    print()


if __name__ == '__main__':
    main()
