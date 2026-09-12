#!/usr/bin/env python3
"""
compiler/irmatch.py — Project 56.

Carries our naive ir 8 toward the book's ir 7 under an oracle, normalises the
book side, and reports the first divergence of a lockstep walk.

Deliberately NOT a port of `compiler/ircmp.py`: that parses ir 6, carries seven
equivalences and a slot-bijection dependency built for a different question.
P55 reused the idea and not the code and this does the same.

There is no search and no cost model here.  The one thing beyond a lockstep
walk is `distance()`, which exists only to answer DESIGN 7.2b and is defined
only where a block bijection exists.

THE FOUR NORMALISATIONS (of the TARGET -- a001 grants the category, and the
rule for it is: a normalisation must CHECK something, not merely delete)

  N1  strip the entry block's `@<entry> WSAVS 0x00NN`, verifying NN against
      the addrbook frame column.  100/102 live entries agree, 0 mismatches.
  N2  `wp/bp(acN, d)` and `R[acN + d]` -> absolute, where acN PROVABLY holds
      the frame.  A base that is not the frame with a displacement in frame
      territory is a HARD ERROR (DESIGN 5.2's inverted check).
  N3  delete `acN = wfp` once N2 leaves it with no reader -- TESTED per site,
      never on sight.  a001 Q3(c): 147 of 1774 sites book-wide read acN as a
      VALUE and must not be deleted.  See compiler/ldafp_census.py.
  N4  drop `site=` / `marker=` / `ret=`; map `goto` labels through the block
      bijection.  Successor structure is checked by the CFG isomorphism, not
      by statement text.

THE REWRITES (of OURS -- sound, precondition hard-checked)

  R1  placement: `v` -> address.  Oracle-supplied.  Merged `v`s must have
      disjoint live ranges.
  R2  arg-cell bridge: `E.aN = X` -> `M32[<argcell>] = X`; `E.arg_count = n`
      deleted into the call decoration; `call E` -> `call <addr>`.
  R3  packed varying immediate: a <=2-byte constant varying assignment becomes
      a register load and one wide store.  Oracle-supplied (site + register).
  R4  literal relocation: `[@bp(v,0), n]` on an initialised read-only `char`
      becomes the located-literal form `[@0xW:b, "text"]`.
"""

import sys
import os
import re
import argparse
from collections import OrderedDict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import irparse as P


class HardError(Exception):
    """A precondition did not hold.  DESIGN 7: every precondition is a hard
    error, because a transformation firing where its precondition fails would
    still produce a match -- the one failure mode with no signature."""


# ==========================================================================
# frame tracking (N2's precondition)
# ==========================================================================

def frame_holders(rb):
    """(block, stmt_index) -> set of registers provably holding the frame.

    Entry: ac3 holds the frame (WSAVS convention, IR.md 6).
    `acN = wfp` makes acN the frame; any other definition of acN unmakes it.
    A string statement writes ac0..ac3 (IR.md 5.8) -- q001 4.1's binding rule,
    and the whole reason this is not read off the statement text.
    Calls PRESERVE ac0..ac3 (measured: ldafp_census.py 4).
    Joins intersect.
    """
    order = list(rb)
    if not order:
        return {}, {}
    entry_in = {order[0]: {'ac3'}}
    preds = P.predecessors(rb)
    # A MUST analysis (join = intersection) has to start at TOP and narrow.
    # Starting at bottom looks fine on straight-line code and silently poisons
    # every block reached by a back edge: the loop predecessor contributes the
    # empty set on the first pass and intersection can never grow again.
    out_state = {bn: set(P.REGS) for bn in order}

    for _ in range(len(order) + 2):            # small CFGs; iterate to fixpoint
        changed = False
        for bn in order:
            if bn == order[0]:
                st = set(entry_in[bn])
            else:
                ps = [out_state[p] for p in preds[bn] if p in out_state]
                st = set.intersection(*ps) if ps else set()
            for s in rb[bn].stmts:
                if s.kind in ('call', 'rt_call'):
                    pass                        # preserved
                elif s.kind == 'string':
                    st -= set(P.REGS)
                elif s.defs:
                    if s.text.split('=')[-1].strip() == 'wfp':
                        st |= {r for r in s.defs if r.startswith('ac')}
                    else:
                        st -= {r for r in s.defs if r.startswith('ac')}
            if out_state.get(bn) != st:
                out_state[bn] = st
                changed = True
        if not changed:
            break

    # second pass: per-statement state.  `at` is the frame set; `known` is the
    # set of registers whose value has a VISIBLE definition in this routine.
    #
    # DESIGN §5.2's inverted check -- "a wp(r,d) against a provably non-frame
    # base with a frame-territory displacement should stop the build" -- needs
    # this second set, and as written it does not have it.  A record base with
    # a small positive displacement is textually indistinguishable from a frame
    # reference (`ac2 = M32[0x70000210]; ac0 = sx16(M16[wp(ac2, 43)])` is
    # SD_PTR, not the frame), and Salvage F23 counts 19,344 base uses of which
    # most are exactly that.  So displacement territory is only evidence when
    # the base's provenance is UNKNOWN; against a base we can see being loaded
    # with something else it is evidence of nothing.
    at = {}
    known_at = {}
    kout = {bn: set(P.REGS) for bn in order}
    for _ in range(len(order) + 2):
        changed = False
        for bn in order:
            k = {'ac3'} if bn == order[0] else None
            if k is None:
                ps = [kout[p] for p in preds[bn] if p in kout]
                k = set.intersection(*ps) if ps else set()
            for s in rb[bn].stmts:
                if s.kind == 'string':
                    k |= set(P.REGS)
                elif s.defs:
                    k |= {r for r in s.defs if r.startswith('ac')}
            if kout[bn] != k:
                kout[bn] = k
                changed = True
        if not changed:
            break

    for bn in order:
        if bn == order[0]:
            st, k = set(entry_in[bn]), {'ac3'}
        else:
            ps = [out_state[p] for p in preds[bn] if p in out_state]
            st = set.intersection(*ps) if ps else set()
            pk = [kout[p] for p in preds[bn] if p in kout]
            k = set.intersection(*pk) if pk else set()
        for i, s in enumerate(rb[bn].stmts):
            at[(bn, i)] = set(st)               # state BEFORE the statement
            known_at[(bn, i)] = set(k)
            if s.kind in ('call', 'rt_call'):
                pass
            elif s.kind == 'string':
                st -= set(P.REGS)
                k |= set(P.REGS)
            elif s.defs:
                k |= {r for r in s.defs if r.startswith('ac')}
                if s.text.split('=')[-1].strip() == 'wfp':
                    st |= {r for r in s.defs if r.startswith('ac')}
                else:
                    st -= {r for r in s.defs if r.startswith('ac')}
    return at, known_at


# ==========================================================================
# address spelling
# ==========================================================================

def word_const(a):
    return '0x%08X' % (a & 0xFFFFFFFF)


def byte_ptr(word, sel):
    return '0x%08X:%d' % (word & 0xFFFFFFFF, sel)


def bp_from_disp(wfp, d):
    """bp(base, d): base is a WORD address scaled to bytes, d already in bytes
    (IR.md 5.2 -- that asymmetry is the hardware's)."""
    total = wfp * 2 + d
    return byte_ptr(total // 2, total % 2)


# ==========================================================================
# N1 - N3 : normalise the book
# ==========================================================================

_WSAVS_RE = re.compile(r'^@([0-9A-F]{8})\s+WSAVS 0x([0-9A-F]+)')


def normalize_book(rb, entry, report):
    """Apply N1, N2, N3.  Returns a new OrderedDict of Blocks."""
    at, known = frame_holders(rb)
    lo = -(10 + 2 * entry.argc)
    hi = 2 * entry.frame

    def in_frame_territory(d):
        return lo <= d <= hi

    out = OrderedDict()
    first = True
    for bn, blk in rb.items():
        nb = P.Block(bn, blk.seg)
        for i, s in enumerate(blk.stmts):
            text = s.text

            # ---- N1: the entry prologue -------------------------------
            if first and i == 0:
                m = _WSAVS_RE.match(text)
                if m:
                    got = int(m.group(2), 16)
                    if got != entry.frame:
                        raise HardError(
                            'N1: %s entry WSAVS 0x%02X but addrbook frame 0x%02X'
                            % (entry.name, got, entry.frame))
                    report.append(('N1', bn, 'prologue WSAVS 0x%02X == addrbook frame'
                                   % got))
                    continue

            # ---- raw lines we cannot produce: NAMED residual, not hidden
            if s.kind in ('raw', 'raw_terminator'):
                report.append(('RESIDUAL', bn, 'unlifted instruction: %s' % text))
                nb.stmts.append(P.classify(text))
                continue

            held = at.get((bn, i), set())
            kn = known.get((bn, i), set())

            # ---- N2: frame spelling -----------------------------------
            def sub_wpbp(m):
                kind, reg, disp = m.group(1), m.group(2), m.group(3)
                try:
                    d = int(disp, 0)
                except ValueError:
                    return m.group(0)               # a register displacement
                if reg not in held:
                    if reg not in kn and in_frame_territory(
                            d if kind == 'wp' else d // 2):
                        raise HardError(
                            'N2: %s %s(%s, %s) at %s:%d -- base provenance is '
                            'UNKNOWN and the displacement is in frame territory'
                            % (entry.name, kind, reg, disp, bn, i))
                    return m.group(0)
                return word_const(entry.wfp + d) if kind == 'wp' \
                    else bp_from_disp(entry.wfp, d)

            def sub_idx(m):
                reg, disp = m.group(1), m.group(2)
                try:
                    d = int(disp, 0)
                except ValueError:
                    return m.group(0)
                if reg not in held:
                    if reg not in kn and in_frame_territory(d):
                        raise HardError(
                            'N2: %s index [%s + %s] at %s:%d -- base provenance '
                            'is UNKNOWN and the displacement is in frame '
                            'territory' % (entry.name, reg, disp, bn, i))
                    return m.group(0)
                return m.group(0).replace('%s + %s' % (reg, disp),
                                          word_const(entry.wfp + d))

            text = P._BASE_RE.sub(sub_wpbp, text)
            text = P._RAWIDX_RE.sub(sub_idx, text)

            # ---- N3: the LDAFP, TESTED ---------------------------------
            if s.kind == 'assign' and s.text.split('=')[-1].strip() == 'wfp':
                reg = next((r for r in s.defs if r.startswith('ac')), None)
                if reg:
                    _b, value_uses, _e, _c = _walk_uses(rb, bn, i + 1, reg)
                    if value_uses:
                        vb, vi, vt = value_uses[0]
                        raise HardError(
                            'N3: %s `%s = wfp` at %s:%d is read as a VALUE at '
                            '%s:%d (%s) -- refusing to delete'
                            % (entry.name, reg, bn, i, vb, vi, vt[:40]))
                    report.append(('N3', bn, 'deleted `%s = wfp` (no value read '
                                             'reaches it)' % reg))
                    continue

            nb.stmts.append(P.classify(text))
        out[bn] = nb
        first = False
    return out


def _walk_uses(rb, start_block, start_idx, reg):
    """Forward walk; shared with ldafp_census.py's question."""
    base_uses, value_uses, reached_exit, crossed = 0, [], False, False
    work, seen = [(start_block, start_idx)], set()
    while work:
        bn, idx = work.pop()
        if (bn, idx) in seen:
            continue
        seen.add((bn, idx))
        blk, killed, i = rb[bn], False, idx
        while i < len(blk.stmts):
            s = blk.stmts[i]
            if reg in s.uses:
                value_uses.append((bn, i, s.text))
            for (r, _d, _k) in s.base_uses:
                if r == reg:
                    base_uses += 1
            if s.kind in ('call', 'rt_call', 'raw_terminator'):
                crossed = True
            if reg in s.defs:
                killed = True
                break
            i += 1
        if killed:
            continue
        succ = P.successors(rb, bn)
        if not succ:
            reached_exit = True
        work.extend((t, 0) for t in succ)
    return base_uses, value_uses, reached_exit, crossed


# ==========================================================================
# the oracle
# ==========================================================================

class Oracle:
    def __init__(self):
        self.routine = None
        self.places = OrderedDict()      # name -> spelling
        self.rewrites = []               # (rule, {k: v})

    @property
    def length(self):
        """DESIGN 5.1: oracle length counts PLACEMENT decisions."""
        return len(self.places)


def load_oracle(path):
    o = Oracle()
    for raw in open(path):
        line = raw.split('#')[0].strip()
        if not line:
            continue
        f = line.split()
        if f[0] == 'routine':
            o.routine = f[1]
        elif f[0] == 'place':
            o.places[f[1]] = f[2]
        elif f[0] == 'rewrite':
            args = {}
            for kv in f[2:]:
                k, _, v = kv.partition('=')
                args[k] = v
            o.rewrites.append((f[1], args))
        else:
            raise HardError('oracle: unknown directive %r' % f[0])
    if not o.routine:
        raise HardError('oracle: no `routine` line')
    return o


def resolve_place(spelling, entry, ab, name):
    """`frame+N` | `argcell` | a literal.  Returns ('word', addr) or
    ('byteptr', word, sel)."""
    if spelling.startswith('frame+'):
        return ('word', entry.wfp + int(spelling[6:], 0))
    if spelling.startswith('frame-'):
        return ('word', entry.wfp - int(spelling[6:], 0))
    if spelling == 'argcell':
        owner, _, cell = name.rpartition('.')
        m = re.fullmatch(r'a(\d+)', cell)
        if not m or owner not in ab:
            raise HardError('R1: `argcell` needs an <ENTRY>.aN name, got %r' % name)
        return ('word', ab[owner].arg_cell(int(m.group(1))))
    m = re.fullmatch(r'(0x[0-9A-Fa-f]+):([01])', spelling)
    if m:
        return ('byteptr', int(m.group(1), 16), int(m.group(2)))
    return ('word', int(spelling, 0))


# ==========================================================================
# R1 - R4 : transform ours
# ==========================================================================

def transform_ours(blocks, decls, oracle, entry, ab, report, enable=None):
    enable = enable if enable is not None else {'R1', 'R2', 'R3', 'R4'}
    qual = lambda n: n if '.' in n else '%s.%s' % (oracle.routine, n)

    # ---- resolve placements, and check the merge precondition -----------
    placed = OrderedDict()
    for name, spelling in oracle.places.items():
        placed[qual(name)] = resolve_place(spelling, entry, ab, qual(name))
    _check_merges(blocks, placed, report)

    # ---- R3 sites ------------------------------------------------------
    packs = {}
    for rule, args in oracle.rewrites:
        if rule != 'pack_varying_imm':
            raise HardError('unknown rewrite rule %r' % rule)
        packs[qual(args['at'])] = args.get('reg', 'ac1')

    out = OrderedDict()
    for bn, blk in blocks.items():
        nb = P.Block(bn)
        for s in blk.stmts:
            for t in _rewrite_stmt(s, placed, decls, packs, ab, report, enable):
                nb.stmts.append(P.classify(t))
        out[bn] = nb
    return out


def _check_merges(blocks, placed, report):
    """R1's precondition: two `v`s may share an address only when their live
    ranges are disjoint (DESIGN 5.3).  The compiler proves it or HARD-ERRORS."""
    bytarget = {}
    for name, tgt in placed.items():
        bytarget.setdefault(tgt, []).append(name)
    for tgt, names in bytarget.items():
        if len(names) < 2:
            continue
        ranges = {n: _live_range(blocks, n) for n in names}
        for i in range(len(names)):
            for j in range(i + 1, len(names)):
                a, b = names[i], names[j]
                if ranges[a] and ranges[b] and _overlap(ranges[a], ranges[b]):
                    raise HardError(
                        'R1: %s and %s are merged onto the same address but '
                        'their live ranges overlap (%s vs %s)'
                        % (a, b, ranges[a], ranges[b]))
        report.append(('R1', '-', 'merge %s onto one address: live ranges disjoint'
                       % ' + '.join(names)))


def _live_range(blocks, name):
    """(first, last) linear statement index mentioning `name`.  Linear order is
    sound here only because the first case is straight-line; a routine with a
    back edge must use a real liveness analysis, and this HARD-ERRORS rather
    than guess."""
    idx, first, last = 0, None, None
    has_backedge = False
    order = list(blocks)
    for bn in order:
        for t in P.successors(blocks, bn):
            if order.index(t) <= order.index(bn):
                has_backedge = True
    for bn in order:
        for s in blocks[bn].stmts:
            if name in s.text:
                first = idx if first is None else first
                last = idx
            idx += 1
    if has_backedge and first is not None:
        raise HardError('R1: %s lives in a routine with a back edge; the linear '
                        'live-range approximation is not sound here' % name)
    return (first, last) if first is not None else None


def _overlap(a, b):
    return not (a[1] < b[0] or b[1] < a[0])


_STRASSIGN_RE = re.compile(
    r'^\[@wp\((\S+?),\s*0\),\s*(\d+) varying\]\s*=\s*\[@bp\((\S+?),\s*0\),\s*(\d+)\]$')


def _spell(tgt):
    return word_const(tgt[1]) if tgt[0] == 'word' else byte_ptr(tgt[1], tgt[2])


def _rewrite_stmt(s, placed, decls, packs, ab, report, enable={'R1','R2','R3','R4'}):
    text = s.text

    # ---- R3: packed varying immediate ----------------------------------
    m = _STRASSIGN_RE.match(text)
    if m:
        dst, cap, src, n = m.group(1), int(m.group(2)), m.group(3), int(m.group(4))
        if dst in packs and 'R3' in enable:
            reg = packs[dst]
            lit = _decl_literal(decls, src)
            if lit is None:
                raise HardError('R3: %s has no constant initialiser' % src)
            if n > 2:
                raise HardError('R3: %s is %d bytes; the packed form takes <= 2'
                                % (src, n))
            if n > cap:
                raise HardError('R3: %s capacity %d < source %d' % (dst, cap, n))
            word = (n << 16) | int.from_bytes(lit.ljust(2, b'\0')[:2], 'big')
            addr = _spell(placed[dst]) if dst in placed else 'wp(%s, 0)' % dst
            report.append(('R3', '-', 'packed %d-byte literal into one wide store'
                           % n))
            return ['%s = 0x%08X' % (reg, word),
                    'M32[%s] = %s' % (addr, reg)]

    # ---- R4: literal relocation ----------------------------------------
    if m:
        src = m.group(3)
        if 'R4' in enable and src in placed and placed[src][0] == 'byteptr':
            lit = _decl_literal(decls, src)
            if lit is None:
                raise HardError('R4: %s has no constant initialiser' % src)
            text = '[@%s, %s varying] = [@%s, "%s"]' % (
                _placed_or_keep(m.group(1), placed), m.group(2),
                _spell(placed[src]), _escape(lit))
            report.append(('R4', '-', 'relocated literal %s into the code image' % src))
            return [text]

    # ---- R2: the arg-cell bridge ---------------------------------------
    m2 = re.match(r'^(\S+\.arg_count)\s*=\s*(\d+)$', text)
    if m2 and 'R2' in enable:
        report.append(('R2', '-', 'arg_count folded into the call decoration'))
        return []
    m2 = re.match(r'^(\S+\.a(\d+))\s*=\s*(.*)$', text)
    if m2 and 'R2' in enable:
        owner = m2.group(1).rpartition('.')[0]
        if owner in ab:
            cell = ab[owner].arg_cell(int(m2.group(2)))
            text = 'M32[%s] = %s' % (word_const(cell), m2.group(3))
            report.append(('R2', '-', 'argument %s written to its addrbook cell'
                           % m2.group(1)))
    m2 = re.match(r'^call (\S+) (.*)$', text)
    if m2 and 'R2' in enable and m2.group(1) in ab:
        text = 'call %08X %s' % (ab[m2.group(1)].addr, m2.group(2))

    # ---- R1: placement --------------------------------------------------
    def sub_named(mm):
        fn, name, disp = mm.group(1), mm.group(2), mm.group(3)
        if name not in placed:
            return mm.group(0)
        tgt = placed[name]
        d = int(disp, 0) if re.fullmatch(r'-?(0x)?[0-9A-Fa-f]+', disp) else None
        if d is None:
            return mm.group(0)
        if fn == 'wp':
            base = tgt[1] if tgt[0] == 'word' else tgt[1]
            return word_const(base + d)
        base = tgt[1] * 2 + (tgt[2] if tgt[0] == 'byteptr' else 0)
        return byte_ptr((base + d) // 2, (base + d) % 2)

    if 'R1' in enable:
        text = re.sub(r'\b(wp|bp)\(\s*([A-Za-z_][\w.]*)\s*,\s*([^()]*?)\s*\)',
                      sub_named, text)
    return [text]


def _placed_or_keep(name, placed):
    return _spell(placed[name]) if name in placed else 'wp(%s, 0)' % name


def _decl_literal(decls, name):
    """The initialiser bytes of a `char n = "..."` declaration."""
    d = decls.get(name)
    if not d:
        return None
    m = re.search(r'=\s*"(.*)"\s*$', d[1])
    if not m:
        return None
    return _unescape(m.group(1))


def _unescape(s):
    out, i = bytearray(), 0
    while i < len(s):
        if s[i] == '\\' and i + 3 < len(s) + 1 and s[i + 1] == 'x':
            out.append(int(s[i + 2:i + 4], 16))
            i += 4
        else:
            out.append(ord(s[i]))
            i += 1
    return bytes(out)


def _escape(b):
    out = []
    for ch in b:
        if 0x20 <= ch <= 0x7E and chr(ch) not in '"\\;':
            out.append(chr(ch))
        else:
            out.append('\\x%02X' % ch)
    return ''.join(out)


# ==========================================================================
# the bijection, canonicalisation, and the lockstep walk
# ==========================================================================

def cfg_shape(blocks):
    order = list(blocks)
    idx = {n: i for i, n in enumerate(order)}
    return [[idx[t] for t in P.successors(blocks, n) if t in idx] for n in order]


def bijection(ours, book):
    """Positional, then VERIFIED.  Refuses rather than guessing an alignment --
    q001 F2: a distance over a non-bijection is an alignment guess dressed up
    as a number."""
    if len(ours) != len(book):
        raise HardError('no block bijection: %d blocks to the book\'s %d'
                        % (len(ours), len(book)))
    a, b = cfg_shape(ours), cfg_shape(book)
    if a != b:
        bad = next(i for i in range(len(a)) if a[i] != b[i])
        raise HardError('no block bijection: CFGs differ at block %d '
                        '(ours -> %s, book -> %s)' % (bad, a[bad], b[bad]))
    return list(zip(ours, book))


_NUM_RE = re.compile(r'\b(?:0x[0-9A-Fa-f]+|\d+)\b')


def canon(text, labels_ours, labels_book, which):
    """N4 plus numeric canonicalisation."""
    t = re.sub(r'\s*\b(?:site|marker|ret)=\S+', '', text).strip()
    table = labels_ours if which == 'ours' else labels_book

    def lab(m):
        inner = [x.strip() for x in m.group(1).split(',')]
        return 'goto [%s]' % ', '.join('#%d' % table[x] if x in table else x
                                       for x in inner)
    t = re.sub(r'goto \[([^\]]*)\]', lab, t)

    def num(m):
        s = m.group(0)
        try:
            return '0x%X' % int(s, 0)
        except ValueError:
            return s
    # do not renumber the byte-select of a byte pointer literal
    parts = re.split(r'(0x[0-9A-Fa-f]+:[01])', t)
    t = ''.join(p if i % 2 else _NUM_RE.sub(num, p) for i, p in enumerate(parts))
    return re.sub(r'\s+', ' ', t).strip()


def canon_blocks(blocks, table):
    return OrderedDict((n, [canon(s.text, table, table, 'ours')
                            for s in b.stmts]) for n, b in blocks.items())


def levenshtein(a, b):
    if a == b:
        return 0
    prev = list(range(len(b) + 1))
    for i, x in enumerate(a, 1):
        cur = [i]
        for j, y in enumerate(b, 1):
            cur.append(min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (x != y)))
        prev = cur
    return prev[-1]


def compare(ours, book):
    """Lockstep walk.  Returns (distance, [divergences])."""
    pairs = bijection(ours, book)
    to_ours = {n: i for i, (n, _) in enumerate(pairs)}
    to_book = {n: i for i, (_, n) in enumerate(pairs)}
    dist, diffs = 0, []
    for k, (on, bn) in enumerate(pairs):
        oo = [canon(s.text, to_ours, to_book, 'ours') for s in ours[on].stmts]
        bb = [canon(s.text, to_ours, to_book, 'book') for s in book[bn].stmts]
        dist += levenshtein(oo, bb)
        if oo != bb:
            for i in range(max(len(oo), len(bb))):
                o = oo[i] if i < len(oo) else '<nothing>'
                b = bb[i] if i < len(bb) else '<nothing>'
                if o != b:
                    diffs.append((k, on, bn, i, o, b))
                    break
    return dist, diffs


def distance(ours, book):
    return compare(ours, book)[0]
