#!/usr/bin/env python3
"""
compiler/onesite.py — Project 59.

Settle ONE string site by exhaustion: DISPLAY_INVENTORY's WCMV at 70167F05
(block 70167EF9, statement 7), whose counts are `ac0 = ac1 = min(3, 30 - len)`
with `len` the length word at frame slot 6.

What it checks, in order (docs/Project59/PROMPT.md, "the procedure the
assumptions license"):

  1. the routine's CFG is CLOSED: every block attributed to the routine is
     reachable from its WSAVS entry, no block of another routine has an edge
     into it, and the addrbook has no second entry (`.N@`) in its range;
  2. pointer ESCAPES: every call / rt_call in the routine, with every
     argument that is slot 6 (`wp(ac3, 6)`), a byte pointer into word 6, or a
     register — i.e. anything that could let a callee write the slot;
  3. every WRITE that can reach word 6 anywhere in the routine: 16/32-bit
     stores to a frame address, string destinations at a frame address, and
     stores/destinations through a register (with the register's in-block
     value, when the block defines it before use);
  4. the SITE: the nearest dominating assignment of slot 6, the set of blocks
     on any path from it to the site, that this set is acyclic, every write
     to word 6 inside it, and — by enumerating EVERY path and running the
     append recurrence `len' = len + min(k, 30 - len)` along it — the set of
     values `len` and `ac1` can take at the site.

Reuses countflow.py's parser/CFG/dominators unchanged.  Nothing executes.

Usage:  onesite.py [--book ...] [--blocks ...] [--addrbook ...]
                   [--routine DISPLAY_INVENTORY] [--site-block 70167EF9]
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import countflow as cf  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


# --------------------------------------------------------------------------
# expression helpers
# --------------------------------------------------------------------------

def frame_word(e):
    """`wp(ac3, d)` -> d ; `bp(ac3, d)` -> word d//2 (byte d) ; else None.
    Returns (kind, offset) with kind in {'w','b'}."""
    if e[0] == 'call' and e[1] in ('wp', 'bp') and len(e[2]) == 2:
        b, d = e[2]
        if b == ('reg', 'ac3') and d[0] == 'const':
            return (e[1][0], d[1])
    return None


def words_touched(kind, off, nbytes):
    """Frame words covered by a write of `nbytes` bytes at wp/bp offset."""
    if kind == 'w':
        lo = 2 * off
    else:
        lo = off
    hi = lo + nbytes - 1
    return set(range(lo // 2, hi // 2 + 1))


def const_of(e):
    if e[0] == 'const':
        return e[1]
    if e[0] == 'paren':
        return const_of(e[1])
    return None


def mentions_reg(e):
    return bool(cf.regs_in(e))


# --------------------------------------------------------------------------
# 1. closed CFG
# --------------------------------------------------------------------------

def check_closed(blocks, addrbook, routine, entry):
    R = [b for b in blocks.values() if b.routine == routine]
    Rn = {b.name for b in R}
    idom, reach = cf.dominators(blocks, entry)
    unreachable = sorted(Rn - reach)
    foreign_reach = sorted(reach - Rn)
    edges_in = []
    for b in blocks.values():
        if b.routine != routine:
            for s in b.succ:
                if s in Rn:
                    edges_in.append((b.name, b.routine, s))
    lo = min(int(n, 16) for n in Rn)
    hi = max(int(n, 16) for n in Rn)
    other_entries = [(('%08X' % a), n) for a, n in addrbook if lo <= a <= hi and ('%08X' % a) != entry]
    return R, Rn, idom, reach, unreachable, foreign_reach, edges_in, other_entries


# --------------------------------------------------------------------------
# 2. pointer escapes
# --------------------------------------------------------------------------

def call_args(st):
    """rt_call: parsed args.  call: the by-reference cells are pushed before
    the LCALL by XPEF/LPEF that the lifter folds into the preceding
    statements; the book keeps `args=N marker=`; the pushed cells are the
    WPSH/XPEF statements in the same block.  We report both."""
    return st.args


def escapes(R, slot):
    rows = []
    for b in R:
        for st in b.stmts:
            if st.kind in ('call', 'rt_call'):
                rows.append((b.name, st.idx, st))
            elif st.kind == 'raw' and st.raw_op in cf.RAW_CALL:
                rows.append((b.name, st.idx, st))
    return rows


def classify_arg(e, slot):
    fw = frame_word(e)
    if fw:
        kind, off = fw
        w = off if kind == 'w' else off // 2
        return ('SLOT%d' % w, w == slot)
    if e[0] == 'const':
        return ('static', False)
    if e[0] == 'bplit':
        return ('literal', False)
    if mentions_reg(e):
        return ('REGISTER-DERIVED', None)
    return ('other', None)


# --------------------------------------------------------------------------
# 3. writers of word `slot`
# --------------------------------------------------------------------------

def store_width(lhs):
    return {'M16': 2, 'M32': 4, 'M8': 1}.get(lhs[1], None)


def block_reg_value(b, upto, reg):
    """The last in-block definition of `reg` before statement index `upto`,
    if it is a plain assign of an expression without registers other than
    ac3 (which we do not substitute).  Returns tree or None."""
    val = None
    for st in b.stmts[:upto]:
        if st.kind == 'assign' and st.lhs == ('reg', reg):
            val = st.rhs
        elif reg in st.defs:
            val = ('opaque', st.text)
    return val


def subst(e, reg, val):
    if e == ('reg', reg):
        return val
    k = e[0]
    if k in ('paren', 'neg', 'not'):
        return (k, subst(e[1], reg, val))
    if k == 'mem':
        return (k, e[1], subst(e[2], reg, val))
    if k == 'call':
        return (k, e[1], [subst(a, reg, val) for a in e[2]])
    if k == 'bin':
        return (k, e[1], subst(e[2], reg, val), subst(e[3], reg, val))
    return e


def resolve_in_block(b, idx, e):
    """Substitute in-block register values (one level, iterated) so that
    `wp(ac2, 0)` after `ac2 = wp(ac3, 6)` reads as `wp(ac3, 6)`."""
    for _ in range(4):
        regs = cf.regs_in(e) - {'ac3'}
        if not regs:
            break
        changed = False
        for r in sorted(regs):
            v = block_reg_value(b, idx, r)
            if v is None or v[0] == 'opaque':
                continue
            e = subst(e, r, v)
            changed = True
        if not changed:
            break
    return e


def writers(R, slot):
    """Every statement that may write frame word `slot`.  Each row:
    (block, idx, class, detail, definite)."""
    rows = []
    for b in R:
        for st in b.stmts:
            if st.kind == 'assign' and st.lhs[0] == 'mem':
                lhs = st.lhs
                addr = resolve_in_block(b, st.idx, lhs[2])
                fw = frame_word(addr)
                w = store_width(lhs)
                if fw and w:
                    ws = words_touched(fw[0], fw[1], w)
                    if slot in ws:
                        rows.append((b.name, st.idx, 'STORE', st.text, True))
                elif mentions_reg(addr):
                    rows.append((b.name, st.idx, 'STORE-VIA-REGISTER', st.text, None))
            elif st.kind == 'string' and st.op == 'WCMV':
                d = st.operands['dst']
                addr = resolve_in_block(b, st.idx, d['addr'])
                fw = frame_word(addr)
                cnt = d['count']
                n = const_of(cnt) if cnt is not None else None
                if fw:
                    kind, off = fw
                    # a varying destination writes the length word at `off`
                    # AND `n` data bytes from byte 2*off+2
                    if d['form'] == 'dst-varying':
                        ws = {off} if kind == 'w' else {off // 2}
                        if n is not None:
                            ws |= words_touched('b', 2 * off + 2, n) if kind == 'w' else set()
                            rows.append((b.name, st.idx, 'WCMV-DST-VARYING', st.text, slot in ws))
                        else:
                            rows.append((b.name, st.idx, 'WCMV-DST-VARYING(len=expr)', st.text, slot in ws or None))
                        continue
                    if n is not None:
                        ws = words_touched(kind, off, n)
                        if slot in ws:
                            rows.append((b.name, st.idx, 'WCMV-DST', st.text, True))
                    else:
                        # count is an expression: bytes from `off` upward;
                        # word `slot` is reachable iff slot >= first word
                        first = off if kind == 'w' else off // 2
                        if slot >= first:
                            rows.append((b.name, st.idx, 'WCMV-DST(count=expr)', st.text, None))
                elif mentions_reg(addr):
                    rows.append((b.name, st.idx, 'WCMV-DST-VIA-REGISTER', st.text, None))
            elif st.kind == 'string' and st.op == 'WBLM':
                d = st.operands['dst']
                addr = resolve_in_block(b, st.idx, d['addr'])
                fw = frame_word(addr)
                if fw or mentions_reg(addr):
                    rows.append((b.name, st.idx, 'WBLM-DST', st.text, None))
            elif st.kind == 'raw' and st.raw_op not in cf.RAW_CALL and not cf.RAW_NO_DEF.match(st.raw_op):
                rows.append((b.name, st.idx, 'RAW', st.text, None))
    return rows


# --------------------------------------------------------------------------
# 4. the site: paths and the recurrence
# --------------------------------------------------------------------------

def region(blocks, A, S):
    """Blocks on some path A ->* S (inclusive)."""
    fwd = set()
    stack = [A]
    while stack:
        n = stack.pop()
        if n in fwd:
            continue
        fwd.add(n)
        if n == S:
            continue
        stack.extend(blocks[n].succ)
    back = set()
    stack = [S]
    while stack:
        n = stack.pop()
        if n in back:
            continue
        back.add(n)
        if n == A:
            continue
        stack.extend(blocks[n].pred)
    return fwd & back


def acyclic(blocks, reg, A, S):
    """True iff the induced subgraph on `reg` (edges leaving S ignored) has no cycle."""
    colour = {}
    def dfs(n):
        colour[n] = 1
        if n != S:
            for s in blocks[n].succ:
                if s in reg:
                    c = colour.get(s, 0)
                    if c == 1:
                        return False
                    if c == 0 and not dfs(s):
                        return False
        colour[n] = 2
        return True
    return dfs(A)


def all_paths(blocks, reg, A, S):
    out = []
    def walk(n, path):
        path.append(n)
        if n == S:
            out.append(list(path))
        else:
            for s in blocks[n].succ:
                if s in reg:
                    walk(s, path)
        path.pop()
    walk(A, [])
    return out


def match_append(b, slot):
    """Recognise the compiler's clamped concatenation head:
         ac2 = wp(ac3, slot); ac1 = CAP; ac0 = K; ac1 = nsub(ac1, M16[wp(ac2, 0)]);
         goto [F, T] (ac0 >=s ac1)
       Returns (CAP, K, F, T) or None."""
    s = b.stmts
    if len(s) != 5:
        return None
    if not (s[0].kind == 'assign' and s[0].lhs == ('reg', 'ac2')):
        return None
    fw = frame_word(s[0].rhs)
    if fw is None or fw[0] != 'w' or (slot is not None and fw[1] != slot):
        return None
    if not (s[1].kind == 'assign' and s[1].lhs == ('reg', 'ac1') and s[1].rhs[0] == 'const'):
        return None
    if not (s[2].kind == 'assign' and s[2].lhs == ('reg', 'ac0') and s[2].rhs[0] == 'const'):
        return None
    exp = ('call', 'nsub', [('reg', 'ac1'), ('mem', 'M16', ('call', 'wp', [('reg', 'ac2'), ('const', 0)]))])
    if not (s[3].kind == 'assign' and s[3].lhs == ('reg', 'ac1') and s[3].rhs == exp):
        return None
    g = s[4]
    if not (g.kind == 'goto' and len(g.labels) == 2 and cf.strip_paren(g.cond) == ('bin', '>=s', ('reg', 'ac0'), ('reg', 'ac1'))):
        return None
    return (s[1].rhs[1], s[2].rhs[1], g.labels[0], g.labels[1], fw[1])


def match_append_tail(b):
    """The body after the clamp:
         ac0 = sx16(M16[wp(ac2,0)]); ac0 = add(ac0, ac1); M16[wp(ac2,0)] = trunc16(ac0);
         ac2 = bp(ac2, 2); ac2 = add(ac2, ac0); ac2 = sub(ac2, ac1); ac0 = ac1;
         [@<dst>, ac0] = [@<lit>, ac1]; ac3 = wfp; goto [N] 0
       Returns the WCMV statement or None."""
    s = b.stmts
    want = [
        'ac0 = sx16(M16[wp(ac2, 0)])',
        'ac0 = add(ac0, ac1)',
        'M16[wp(ac2, 0)] = trunc16(ac0)',
        'ac2 = bp(ac2, 2)',
        'ac2 = add(ac2, ac0)',
        'ac2 = sub(ac2, ac1)',
        'ac0 = ac1',
    ]
    if len(s) != 10:
        return None
    for st, w in zip(s, want):
        if st.text.strip() != w:
            return None
    if not (s[7].kind == 'string' and s[7].op == 'WCMV'):
        return None
    if s[7].operands['dst']['count'] != ('reg', 'ac0') or s[7].operands['src']['count'] != ('reg', 'ac1'):
        return None
    if s[8].text.strip() != 'ac3 = wfp':
        return None
    if not (s[9].kind == 'goto' and len(s[9].labels) == 1):
        return None
    return s[7]


def sx16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def nsub(a, b):
    return sx16(sx16(a) - sx16(b))


def simulate(blocks, path, slot, len0, trace=False):
    """Run the append recurrence along one path.  Returns list of
    (block, len_before, k, cap, ac1, len_after) events, plus the final len
    and the (len, ac1) seen at the site block (last on the path).
    Any statement that is not part of a recognised append and could write
    slot `slot` aborts (returns None) — the caller has already listed those.
    """
    ln = len0
    events = []
    i = 0
    site_vals = None
    while i < len(path):
        n = path[i]
        b = blocks[n]
        head = match_append(b, slot)
        if head:
            cap, k, F, T, _ = head
            ac1 = nsub(cap, ln)                # 30 - len, 16-bit, sign-extended
            taken_true = (k >= ac1)            # signed compare, both small
            nxt = path[i + 1]
            if taken_true:
                if nxt != T:                   # the path takes the arm the value forbids
                    return None
                piece = ac1
                body = nxt
            else:
                if nxt != F:
                    return None
                piece = k
                # F block is `ac1 = ac0; goto [T] 0`
                fb = blocks[F]
                assert [s.text.strip() for s in fb.stmts] == ['ac1 = ac0', 'goto [%s] 0' % T], fb.stmts
                body = path[i + 2]
                assert body == T
            tb = blocks[body]
            wc = match_append_tail(tb)
            assert wc is not None, body
            newlen = sx16(ln + piece)
            events.append((n, ln, k, cap, piece, newlen, wc.text.strip()))
            if body == path[-1]:
                site_vals = (ln, piece, newlen)
            ln = newlen
            i = path.index(body) + 1
            continue
        i += 1
    return events, ln, site_vals


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------

def find_slot(blocks, S, default):
    """The slot of the append whose body is S: the head block's `ac2 = wp(ac3, d)`."""
    for p in blocks[S].pred:
        h = match_append(blocks[p], None)
        if h:
            return h[4]
    return default


def analyse_site(blocks, idom, R, W, routine, S, slot, show_paths=False):
    print('\n== 4. the site: block %s, slot %d ==' % (S, slot))
    n = S
    A = None
    while True:
        cands = [(bn, idx) for bn, idx, cls, text, d in W if bn == n and cls.startswith('WCMV-DST-VARYING') and d]
        if cands and n != S:
            A = n
            break
        if idom.get(n, n) == n:
            break
        n = idom[n]
    if A is None:
        print('no dominating varying assignment of slot %d found' % slot)
        return
    print('nearest dominating varying assignment of slot %d: block %s' % (slot, A))
    print('  ', [st.text.strip()[:100] for st in blocks[A].stmts if st.kind == 'string'])
    print('A dominates S: %s' % cf.dominates(idom, A, S))
    reg = region(blocks, A, S)
    print('blocks on paths A -> S: %d : %s' % (len(reg), ' '.join(sorted(reg))))
    acyc = acyclic(blocks, reg, A, S)
    print('region acyclic: %s' % acyc)
    print('predecessors of S: %s' % blocks[S].pred)
    print('predecessors of A: %s' % blocks[A].pred)
    print('writers of word %d inside the region (excluding A itself):' % slot)
    inreg = [(bn, idx, cls, text, d) for bn, idx, cls, text, d in W if bn in reg and bn != A]
    for bn, idx, cls, text, d in inreg:
        print('  %s:%d %-24s %s' % (bn, idx, cls, text[:110]))
    print('calls inside the region: %s' % ([(bn, idx, st.callee) for bn, idx, st in escapes(R, slot) if bn in reg] or 'none'))
    print('append heads recognised in the region (head: cap, k, F, T; T\'s predecessors):')
    heads = {}
    for bn in sorted(reg):
        h = match_append(blocks[bn], slot)
        if h:
            heads[bn] = h
            print('  %s: cap=%d k=%d F=%s T=%s  pred(T)=%s pred(F)=%s' % (bn, h[0], h[1], h[2], h[3], blocks[h[3]].pred, blocks[h[2]].pred))
    print('region statements that write memory / call and are NOT inside a recognised append (A excluded):')
    covered = set(heads)
    for bn, h in heads.items():
        covered.add(h[2]); covered.add(h[3])
    stray = 0
    for bn in sorted(reg):
        if bn in covered or bn == A:
            continue
        for st in blocks[bn].stmts:
            if (st.kind == 'assign' and st.lhs[0] == 'mem') or st.kind == 'string' or st.kind in ('call', 'rt_call') \
                    or (st.kind == 'raw' and not cf.RAW_NO_DEF.match(st.raw_op)):
                print('  %s:%d %s' % (bn, st.idx, st.text[:110]))
                stray += 1
    if not stray:
        print('  none')
    aw = [st for st in blocks[A].stmts if st.kind == 'string' and st.op == 'WCMV' and frame_word(st.operands['dst']['addr']) == ('w', slot)]
    len0 = const_of(aw[0].operands['dst']['count'])
    print('initial len at A: %d  (%s)' % (len0, aw[0].text.strip()[:80]))
    if not acyc:
        print('region has a cycle: path enumeration skipped (the clamp invariant still applies if every writer is an append)')
        return
    paths = all_paths(blocks, reg, A, S)
    print('CFG paths A -> S: %d' % len(paths))
    site_lens = {}
    infeasible = 0
    for p in paths:
        r = simulate(blocks, p, slot, len0)
        if r is None:
            infeasible += 1
            continue
        ev, ln, sv = r
        site_lens.setdefault(sv, []).append([e[0] for e in ev])
        if show_paths:
            print('  path %s' % ' '.join(p))
            for e in ev:
                print('     append @%s: len %d + min(%d, %d-%d)= +%d -> %d   [%s]' % (e[0], e[1], e[2], e[3], e[1], e[4], e[5], e[6][:60]))
    print('CFG paths whose clamp arm contradicts the computed length (infeasible): %d ; feasible: %d' % (infeasible, len(paths) - infeasible))
    print('(len before site append, ac1 = piece at site, len after) -> append sequences:')
    for k in sorted(site_lens):
        print('  %s : %s' % (k, sorted(set(tuple(x) for x in site_lens[k]))))
    lens = sorted({k[0] for k in site_lens})
    print('len at site in %s ; ac1 at site in %s ; max len after site %d' % (lens, sorted({k[1] for k in site_lens}), max(k[2] for k in site_lens)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--book', default=os.path.join(ROOT, 'emulation', 'quest.ir2.book'))
    ap.add_argument('--blocks', default=os.path.join(ROOT, 'emulation', 'quest.blocks.split'))
    ap.add_argument('--addrbook', default=os.path.join(ROOT, 'emulation', 'quest.addrbook'))
    ap.add_argument('--routine', default='DISPLAY_INVENTORY')
    ap.add_argument('--site-block', default=['70167EF9'], nargs='+')
    ap.add_argument('--slot', type=int, default=None, help='frame word; default: from the append head')
    ap.add_argument('--paths', action='store_true', help='print every path')
    a = ap.parse_args()

    blocks = cf.load_book(a.book)
    split = cf.load_split(a.blocks)
    addrbook = cf.load_addrbook(a.addrbook)
    unresolved = cf.build_cfg(blocks, split, addrbook)
    entry = {n: '%08X' % ad for ad, n in addrbook}[a.routine]
    slot = a.slot if a.slot is not None else find_slot(blocks, a.site_block[0], 6)

    print('== routine %s entry %s, slot %d ==' % (a.routine, entry, slot))
    R, Rn, idom, reach, unreachable, foreign_reach, edges_in, other_entries = \
        check_closed(blocks, addrbook, a.routine, entry)
    print('blocks attributed: %d  reachable from entry: %d' % (len(Rn), len(reach & Rn)))
    print('unreachable-from-entry blocks: %s' % (unreachable or 'none'))
    print('blocks of OTHER routines reachable from entry: %s' % (foreign_reach or 'none'))
    print('edges INTO the routine from other routines: %s' % (edges_in or 'none'))
    print('other addrbook entries in the address range: %s' % (other_entries or 'none'))
    unres_here = [u for u in unresolved if u[0] in Rn]
    print('CFG-builder unresolved rows in this routine: %s' % (unres_here or 'none'))

    print('\n== 2. calls and their arguments (!! = slot %d) ==' % slot)
    for bn, idx, st in escapes(R, slot):
        if st.kind == 'rt_call':
            cls = [classify_arg(e, slot) for e in st.args]
            print('  %s:%d rt_call %s args=[%s]' % (bn, idx, st.callee, ', '.join('%s%s' % (c, '!!' if f else '') for c, f in cls)))
            for e, (c, f) in zip(st.args, cls):
                if c == 'REGISTER-DERIVED':
                    print('      register-derived arg: %s  (in-block value: %s)' % (cf.show(e), cf.show(resolve_in_block(blocks[bn], idx, e))))
        elif st.kind == 'call':
            print('  %s:%d call %s  (%s)' % (bn, idx, st.callee, st.text[:80]))
            for s2 in blocks[bn].stmts[:idx]:
                if s2.kind == 'raw' or 'wsp' in s2.text:
                    print('      push: %s' % s2.text.strip()[:100])
        else:
            print('  %s:%d RAW %s' % (bn, idx, st.text[:80]))

    print('\n== 3. every statement that may write frame word %d, whole routine ==' % slot)
    W = writers(R, slot)
    for bn, idx, cls, text, definite in W:
        print('  %s:%d  %-28s %s  %s' % (bn, idx, cls, {True: 'WRITES', False: 'misses', None: 'MAY'}[definite], text[:110]))

    for S in a.site_block:
        analyse_site(blocks, idom, R, W, a.routine, S, slot, a.paths)


if __name__ == '__main__':
    main()
