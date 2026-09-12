#!/usr/bin/env python3
"""
compiler/match_routine.py — Project 56 driver.

    python3 compiler/match_routine.py --entry HIT_ANY_CHAR \
        --ir <compiled.ir> --oracle docs/Project56/oracles/HIT_ANY_CHAR.oracle

Transforms our side under the oracle, normalises the book side, and walks the
two in lockstep.  `--verbose` prints both sides after transformation; `--replay`
runs the DESIGN 7.2b monotonicity measurement.
"""

import sys
import os
import argparse
from collections import OrderedDict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import irparse as P
import irmatch as M


def build(entry_name, ir_path, oracle, book, ab, report):
    entry = ab[entry_name]
    ours_all = P.load_ours(ir_path)
    ours = OrderedDict((n, b) for n, b in ours_all.items()
                       if n.startswith(entry_name + '.b'))
    decls = P.load_decls(ir_path)
    rb = P.routine_blocks(book, entry)
    tours = M.transform_ours(ours, decls, oracle, entry, ab, report)
    nbook = M.normalize_book(rb, entry, report)
    return tours, nbook


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    ap = argparse.ArgumentParser()
    ap.add_argument('--entry', required=True)
    ap.add_argument('--ir', required=True)
    ap.add_argument('--oracle', required=True)
    ap.add_argument('--book', default=os.path.join(root, 'emulation/quest.ir2.book'))
    ap.add_argument('--addrbook', default=os.path.join(root, 'emulation/quest.addrbook'))
    ap.add_argument('--verbose', action='store_true')
    args = ap.parse_args()

    book = P.load_book(args.book)
    ab = P.load_addrbook(args.addrbook)
    oracle = M.load_oracle(args.oracle)
    report = []

    try:
        tours, nbook = build(args.entry, args.ir, oracle, book, ab, report)
    except M.HardError as e:
        print('HARD ERROR: %s' % e)
        return 2

    print('=' * 74)
    print('%s — transform, normalise, lockstep' % args.entry)
    print('=' * 74)
    print()
    print('  oracle length (placement decisions, DESIGN §5.1) : %d' % oracle.length)
    print('  oracle rewrite lines                             : %d' % len(oracle.rewrites))
    print()

    print('  NORMALISATIONS AND REWRITES APPLIED')
    for tag, where, what in report:
        print('    %-9s %-10s %s' % (tag, where, what))
    print()

    residual = [r for r in report if r[0] == 'RESIDUAL']
    if residual:
        print('  NAMED UNMATCHED RESIDUAL (a001 Q1(a): reported, never hidden)')
        for _t, where, what in residual:
            print('    %s  %s' % (where, what))
        print()

    if args.verbose:
        pairs = M.bijection(tours, nbook)
        to_o = {n: i for i, (n, _) in enumerate(pairs)}
        to_b = {n: i for i, (_, n) in enumerate(pairs)}
        for k, (on, bn) in enumerate(pairs):
            print('  --- block %d: %s  <->  %s' % (k, on, bn))
            oo = [M.canon(s.text, to_o, to_b, 'ours') for s in tours[on].stmts]
            bb = [M.canon(s.text, to_o, to_b, 'book') for s in nbook[bn].stmts]
            for i in range(max(len(oo), len(bb))):
                o = oo[i] if i < len(oo) else ''
                b = bb[i] if i < len(bb) else ''
                print('      %s ours | %s' % ('  ' if o == b else '!!', o))
                print('         book | %s' % b)
        print()

    try:
        dist, diffs = M.compare(tours, nbook)
    except M.HardError as e:
        print('  RESULT: no match — %s' % e)
        return 1

    if not diffs:
        print('  RESULT: MATCH — exactly, modulo the normalised prologue line,')
        print('          reported above (a001 Q1(a)).  distance 0.')
        return 0

    print('  RESULT: %d divergence(s), distance %d' % (len(diffs), dist))
    for (k, on, bn, i, o, b) in diffs:
        print('    block %d (ours %s, book %s), statement %d:' % (k, on, bn, i))
        print('      ours | %s' % o)
        print('      book | %s' % b)
    return 1


if __name__ == '__main__':
    sys.exit(main())
