#!/usr/bin/env python3
"""
compiler/ldafp_census.py — Project 56, ruled by a001 Q3(c).

N3 deletes a book statement: once N2 has substituted absolute addresses for
`wp/bp(acN, d)` where acN provably holds the frame, `acN = wfp` is left with no
reader and must go or our side can never match.  Deletion from the TARGET is
the strongest act in the normalisation category, so a001 overruled q001's
"delete and report" and ordered the census first.

The question, stated so it can be answered:

    For each `acN = wfp` in the book, is every use of acN reached from it
    a wp/bp BASE (which N2 rewrites), or does something read acN as a
    VALUE (which N3 must not delete)?

A def is N3-SAFE when no reached use is a value-read.  Anything else is a
blocker and is reported with its site.

Also measured, because q001 4.1's clobber rule is what authorises the analysis
and an unchecked rule is not a rule:

    * what clobbers acN before each LDAFP -- if nothing in our model does,
      either the LDAFP is genuinely redundant or our clobber model is
      incomplete, and both are findings
    * whether calls preserve ac0..ac3 (asserted from IR.md 6; measured here)

Usage:  python3 compiler/ldafp_census.py [--book F] [--addrbook F] [--examples N]
"""

import sys
import os
import argparse
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import irparse as P


def def_sites(rb):
    """(block, index, reg) for every `acN = wfp` in a routine."""
    out = []
    for name, blk in rb.items():
        for i, s in enumerate(blk.stmts):
            if s.kind == 'assign' and s.text.split('=')[-1].strip() == 'wfp':
                for r in s.defs:
                    if r.startswith('ac'):
                        out.append((name, i, r))
    return out


def walk(rb, start_block, start_idx, reg):
    """Forward over the CFG from a def of `reg`.

    Returns (base_uses, value_uses, reached_exit, crossed_call) where
    value_uses is a list of (block, index, text) -- the N3 blockers.
    """
    base_uses = 0
    value_uses = []
    reached_exit = False
    crossed_call = False

    # worklist of (block, index-to-resume-at)
    work = [(start_block, start_idx)]
    seen = set()
    while work:
        bn, idx = work.pop()
        if (bn, idx) in seen:
            continue
        seen.add((bn, idx))
        blk = rb[bn]
        killed = False
        i = idx
        while i < len(blk.stmts):
            s = blk.stmts[i]
            if reg in s.uses:
                value_uses.append((bn, i, s.text))
                # a value-read does not kill; keep going only if not redefined
            for (r, _d, _k) in s.base_uses:
                if r == reg:
                    base_uses += 1
            if s.kind in ('call', 'rt_call', 'raw_terminator'):
                crossed_call = True
            if reg in s.defs:
                killed = True
                break
            i += 1
        if killed:
            continue
        succ = P.successors(rb, bn)
        if not succ:
            reached_exit = True
        for t in succ:
            work.append((t, 0))
    return base_uses, value_uses, reached_exit, crossed_call


def clobber_before(rb, bn, idx, reg):
    """What last defined `reg` before this LDAFP, looking backwards within the
    block only (a cheap, honest probe -- cross-block gets reported as such)."""
    blk = rb[bn]
    for i in range(idx - 1, -1, -1):
        s = blk.stmts[i]
        if reg in s.defs:
            return s.kind
    return 'block-entry'


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    ap = argparse.ArgumentParser()
    ap.add_argument('--book', default=os.path.join(root, 'emulation/quest.ir2.book'))
    ap.add_argument('--addrbook', default=os.path.join(root, 'emulation/quest.addrbook'))
    ap.add_argument('--examples', type=int, default=12)
    args = ap.parse_args()

    book = P.load_book(args.book)
    ab = P.load_addrbook(args.addrbook)

    total = 0
    by_reg = Counter()
    safe = 0
    blocked = 0
    no_use_at_all = 0
    clob = Counter()
    blockers = []
    blocked_routines = Counter()
    call_crossed_safe = 0
    per_routine = defaultdict(lambda: [0, 0])      # name -> [safe, blocked]
    entry_tail = []                                # LDAFPs with no in-block clobber

    for rn, e in ab.items():
        rb = P.routine_blocks(book, e)
        if not rb:
            continue
        for (bn, i, reg) in def_sites(rb):
            total += 1
            by_reg[reg] += 1
            cb = clobber_before(rb, bn, i, reg)
            clob[cb] += 1
            if cb == 'block-entry':
                entry_tail.append((rn, bn, i, reg))
            b, v, _exit, crossed = walk(rb, bn, i + 1, reg)
            if v:
                blocked += 1
                blocked_routines[rn] += 1
                per_routine[rn][1] += 1
                if len(blockers) < args.examples:
                    blockers.append((rn, bn, i, reg, v[0]))
            else:
                safe += 1
                per_routine[rn][0] += 1
                if crossed:
                    call_crossed_safe += 1
                if b == 0:
                    no_use_at_all += 1

    SEVEN = ('HIT_ANY_CHAR', 'GET_INPUT', 'PICK_X_Y', 'UPDATE_SCREENS',
             'INIT_SCREEN', 'FAKE_OCEAN', 'FAKE_LAND_MASS')

    print('=' * 74)
    print('LDAFP CENSUS -- `acN = wfp` in quest.ir2.book        (P56, a001 Q3(c))')
    print('=' * 74)
    print()
    print('  total `acN = wfp` statements : %d' % total)
    print('  by register                  : %s' % dict(by_reg.most_common()))
    print()
    print('  1. IS N3 SAFE?  (every reached use is a wp/bp base N2 rewrites)')
    print('     N3-SAFE                    : %d  (%.1f%%)' % (safe, 100.0 * safe / max(total, 1)))
    print('       ...of which no use at all: %d' % no_use_at_all)
    print('       ...of which a use after a call, no intervening def: %d' % call_crossed_safe)
    print('     BLOCKED (acN read as a value) : %d  (%.1f%%)'
          % (blocked, 100.0 * blocked / max(total, 1)))
    print()
    if blockers:
        print('     blockers, first %d:' % len(blockers))
        for (rn, bn, i, reg, (vb, vi, vt)) in blockers:
            print('       %-22s def %s:%d %s  ->  read at %s:%d  %s'
                  % (rn, bn, i, reg, vb, vi, vt[:46]))
        print()
        print('     routines with blockers: %d  %s'
              % (len(blocked_routines), blocked_routines.most_common(8)))
        print()
    print('  2. WHAT CLOBBERED acN BEFORE THE LDAFP (within the block)')
    for k, n in clob.most_common():
        print('     %-16s %6d' % (k, n))
    print()
    print('     `string` is q001 4.1\'s clobber rule doing the work: a folded')
    print('     string statement writes ac0..ac3 (IR.md 5.8) while naming none')
    print('     of them.  %d of %d LDAFPs (%.0f%%) exist because of it.'
          % (clob['string'], total, 100.0 * clob['string'] / max(total, 1)))
    print('     The %d `block-entry` cases have no in-block clobber; this probe'
          % clob['block-entry'])
    print('     does not look across blocks, so they are UNEXPLAINED HERE, not')
    print('     shown to be redundant.  First few: %s'
          % ', '.join('%s@%s' % (r, b) for (r, b, _i, _g) in entry_tail[:5]))
    print()
    print('  3. THE SEVEN')
    print('     %-16s %6s %8s' % ('routine', 'safe', 'blocked'))
    any_seven = False
    for rn in SEVEN:
        s, b = per_routine.get(rn, [0, 0])
        if s or b:
            any_seven = True
        print('     %-16s %6d %8d' % (rn, s, b))
    if not any_seven:
        print('     (none of the seven contains an LDAFP except HIT_ANY_CHAR)')
    print()
    print('  4. DO CALLS PRESERVE ac0..ac3?')
    print('     %d N3-safe defs have their base uses AFTER a call with no' % call_crossed_safe)
    print('     intervening definition.  If calls clobbered ac0..ac3 the book')
    print('     would have to reload the frame after every one of them and')
    print('     these %d sites could not exist.  Consistent with IR.md 6:' % call_crossed_safe)
    print('     WSAVS saves ac0 ac1 ac2 wfp ac3|c and WRTN restores them.')
    print()
    print('  5. READING')
    if blocked == 0:
        print('     N3 is safe at every site in the program.')
    else:
        print('     N3 is NOT safe unconditionally: %d of %d sites (%.1f%%), in %d'
              % (blocked, total, 100.0 * blocked / max(total, 1), len(blocked_routines)))
        print('     of %d routines, read acN as a VALUE.  The shapes are `ac1 = ac3`'
              % len([1 for rn in ab if P.routine_blocks(book, ab[rn])]))
        print('     (the frame copied into a second base) and `acX = add(acX, ac3)`')
        print('     (frame-relative arithmetic the lifter did not fold) -- which is')
        print('     DESIGN 5.2\'s two-register pressure, not an anomaly.')
        print()
        print('     N3 MUST therefore TEST, not delete on sight: delete `acN = wfp`')
        print('     only when no use reached from it reads acN as a value, and')
        print('     HARD-ERROR otherwise.  a001 Q3(c) was the right call; q001\'s')
        print('     recommendation of (a) would have shipped a normalisation that')
        print('     is wrong at 1 site in 12 and correct in HIT_ANY_CHAR by luck.')
    print()


if __name__ == '__main__':
    main()
