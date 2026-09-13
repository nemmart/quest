#!/usr/bin/env python3
"""
compiler/countflow.py — Project 58 (Stage A).

Reaching definitions of ac0..ac3 over `emulation/quest.ir2.book`, and the
string-count census built on top of it.  Nothing here executes and nothing
here changes an artifact.

What it does, in order (docs/Project58/PROMPT.md):

  1. parses the book into expression TREES (compiler/irparse.py is statement-
     level; the count proof needs the operands);
  2. builds the CFG: `goto` labels, `call ret=`, `rt_call site+4`, and for a
     block that ends in an unlifted instruction the successor list of the
     quest.blocks.split block that contains it;
  3. runs reaching definitions per register to a fixpoint (union meet, no
     iteration cap — emulation/tools/dataflow.py's cap is the reason it was
     not extended);
  4. traces every string count operand back through copies and arithmetic to
     its ROOTS, where a root is one of: a constant, a literal's byte count,
     a length-word read `sx16(M16[a])`, a wide read `M32[a]`, a string
     statement's residue, a call's ac3 (= wfp), a routine entry, or opaque
     arithmetic;
  5. judges each root against the closure lemma (see docs/Project58/
     q001-plan-gate.md §2): WCMV leaves ac0 = 0 and ac1 = len - min(|n|,|len|)
     (>= 0 whenever len >= 0); WCMP leaves ac0 in [0, n] and ac1 in {-1,0,1};
     WBLM leaves ac1 = 0.
  6. matches the BASE CLASS: an operand whose count is the 16-bit word at A
     and whose pointer is byte 2A+2 — the layout of a CHAR(n) VARYING — on
     either side of the statement;
  7. asks, for every varying read of a frame slot, whether a write of that
     slot's length word DOMINATES the read within the routine (PROMPT §b);
  8. measures which residues are LIVE after each string statement (P6);
  9. (Part 2, a001) applies a DOMINATING-GUARD rule — facts from `assert`s and
     two-way branches every path to the site executes, matched structurally
     against the traced count; reported separately from dataflow (Q3);
 10. (Part 2, a001 Q2 policy (a), `--infer-caps`) optionally bounds an
     unbounded scratch copy by the largest constant copy into the same buffer
     — circular by construction, so flagged per row;
 11. writes the assert site list (`--asserts`), the deliverable a later project
     emits from.

Register model (the part that carries weight — stated, not inferred):

  * `acN = <expr>` defines acN.  Effectful ops (`add`, `sub`, ...) define
    their lvalue only.  `c`/`ovr`/`tN`/memory lvalues define no register.
  * A string statement defines the registers the LIBRARY writes
    (EagleString.cpp:152-163, :199-202, :206-210): WCMV ac0..ac3 (+c),
    WCMP ac0..ac3, WBLM ac1..ac3.  Its operands are USES.
  * `call` / `rt_call` / raw LCALL / XCALL / LJSR PRESERVE ac0..ac2 and set
    ac3 = wfp (EagleStack.cpp WRTN pops ac2, ac1, ac0 and sets ac3 = wfp;
    RTBridge.cpp:88-90 restores saved_ac[0..2] and sets ac3 = wfp).
  * A raw instruction that is not a call: float ops, WPSH, DERR, WSAVS
    write no accumulator we care about (WSAVS opens a routine, where
    everything is `entry` anyway); anything else is treated as defining all
    four (opaque) so that a trace through it comes out UNKNOWN, never
    "proven".

Self-check: `--selftest` runs a synthetic book with hand-computed answers
plus program-wide invariants (every def in an IN set is a def of that
register; every string site's operand roots are reachable; the site count is
the book's 1,689).

Usage:
  countflow.py [--book emulation/quest.ir2.book] [--blocks emulation/quest.blocks.split]
               [--addrbook emulation/quest.addrbook] [--ledgers p31.tsv p32.tsv ...]
               [--out docs/Project58/sites.tsv] [--dump-site 7016816B] [--selftest]
"""

import re
import sys
import os
import bisect
import argparse
from collections import OrderedDict, defaultdict, deque

REGS = ('ac0', 'ac1', 'ac2', 'ac3')

# --------------------------------------------------------------------------
# tokenizer / parser for IR expressions (IR.md 5.1, 5.8)
# --------------------------------------------------------------------------

_TOK = re.compile(r'''
    (?P<ws>\s+)
  | (?P<str>"(?:[^"\\]|\\x[0-9A-F]{2})*")
  | (?P<bplit>-?0x[0-9A-Fa-f]+:[01])
  | (?P<num>-?0x[0-9A-Fa-f]+|-?[0-9]+)
  | (?P<op><=s|<=u|>=s|>=u|<s|<u|>s|>u|==|!=|&&|\|\||/s|/u|%s|%u|[-+*&|^~!])
  | (?P<ident>[A-Za-z_?][A-Za-z0-9_.@?]*)
  | (?P<punct>[()\[\],@=:])
''', re.X)

BINOPS = {'+', '-', '*', '&', '|', '^', '/s', '/u', '%s', '%u',
          '==', '!=', '<s', '<=s', '>s', '>=s', '<u', '<=u', '>u', '>=u',
          '&&', '||'}


def tokenize(s):
    out = []
    i = 0
    prev = None
    while i < len(s):
        m = _TOK.match(s, i)
        if not m:
            raise ValueError('tokenize: cannot lex %r at %d in %r' % (s[i:i + 12], i, s))
        i = m.end()
        kind = m.lastgroup
        if kind == 'ws':
            continue
        text = m.group(kind)
        # `a - 12` is binary minus; `-12` after an operand-less position is a
        # negative literal.  The emitter writes binary minus with spaces and
        # negative constants attached, but be exact: a leading '-' number
        # right after an operand is a binary op followed by a literal.
        if kind in ('num', 'bplit') and text.startswith('-') and prev is not None \
                and prev[0] in ('num', 'bplit', 'ident', 'close', 'str'):
            out.append(('op', '-'))
            text = text[1:]
        if kind == 'punct' and text in ')]':
            kind2 = 'close'
        else:
            kind2 = kind
        tok = (kind2, text)
        out.append(tok)
        prev = tok
    return out


class Parser:
    def __init__(self, s):
        self.toks = tokenize(s)
        self.i = 0
        self.src = s

    def peek(self, k=0):
        j = self.i + k
        return self.toks[j] if j < len(self.toks) else (None, None)

    def take(self, text=None):
        t = self.peek()
        if t[0] is None:
            raise ValueError('parse: unexpected end in %r' % self.src)
        if text is not None and t[1] != text:
            raise ValueError('parse: expected %r got %r in %r' % (text, t[1], self.src))
        self.i += 1
        return t

    def at_end(self):
        return self.i >= len(self.toks)

    # expr := unary (binop unary)*   — one flat chain; nested left-assoc here
    def expr(self):
        a = self.unary()
        while self.peek()[0] == 'op' and self.peek()[1] in BINOPS:
            op = self.take()[1]
            b = self.unary()
            a = ('bin', op, a, b)
        return a

    def unary(self):
        k, t = self.peek()
        if k == 'op' and t in ('~', '!'):
            self.take()
            return ('neg' if t == '~' else 'not', self.unary())
        if k == 'punct' and t == '(':
            self.take()
            e = self.expr()
            self.take(')')
            return ('paren', e)
        if k == 'num':
            self.take()
            return ('const', int(t, 0))
        if k == 'bplit':
            self.take()
            w, b = t.split(':')
            return ('bplit', int(w, 0), int(b))
        if k == 'ident':
            self.take()
            nk, nt = self.peek()
            if nk == 'punct' and nt == '[':
                self.take()
                e = self.expr()
                self.take(']')
                return ('mem', t, e)           # M8/M16/M32/R
            if nk == 'punct' and nt == '(':
                self.take()
                args = []
                if not (self.peek()[0] == 'close' and self.peek()[1] == ')'):
                    args.append(self.expr())
                    while self.peek()[1] == ',':
                        self.take()
                        args.append(self.expr())
                self.take(')')
                return ('call', t, args)
            if t in REGS:
                return ('reg', t)
            if re.fullmatch(r't\d+', t):
                return ('t', t)
            if t in ('c', 'ovr'):
                return ('flag', t)
            if t in ('wfp', 'wsp', 'wsb', 'wsl'):
                return ('sreg', t)
            return ('name', t)
        raise ValueError('parse: unexpected token %r in %r' % ((k, t), self.src))

    # located / piece:  [@addr, n] | [@addr, n varying] | [@addr, varying] | [@0xW:b, "text"]
    def piece(self):
        self.take('[')
        self.take('@')
        addr = self.expr()
        self.take(',')
        k, t = self.peek()
        if k == 'ident' and t == 'varying':
            self.take()
            self.take(']')
            return {'addr': addr, 'count': None, 'form': 'varying'}
        if k == 'str':
            self.take()
            self.take(']')
            text = t[1:-1]
            n = len(text) - 3 * text.count('\\x')
            return {'addr': addr, 'count': ('const', n), 'form': 'literal', 'text': text}
        cnt = self.expr()
        form = 'fixed'
        if self.peek()[0] == 'ident' and self.peek()[1] == 'varying':
            self.take()
            form = 'dst-varying'
        self.take(']')
        return {'addr': addr, 'count': cnt, 'form': form}


def show(e):
    """Render a tree back to (near) IR spelling, for reports."""
    k = e[0]
    if k == 'const':
        v = e[1]
        return ('-0x%X' % -v) if v < 0 else ('0x%X' % v if v > 9 else str(v))
    if k == 'bplit':
        return '0x%X:%d' % (e[1], e[2])
    if k in ('reg', 't', 'flag', 'sreg', 'name'):
        return e[1]
    if k == 'mem':
        return '%s[%s]' % (e[1], show(e[2]))
    if k == 'call':
        return '%s(%s)' % (e[1], ', '.join(show(a) for a in e[2]))
    if k == 'bin':
        return '%s %s %s' % (show(e[2]), e[1], show(e[3]))
    if k == 'paren':
        return '(%s)' % show(e[1])
    if k == 'neg':
        return '~' + show(e[1])
    if k == 'not':
        return '!' + show(e[1])
    if k == 'root':
        return '<%s %s>' % (e[1], e[2] if e[1] != 'residue' else '%s.%s@%s:%d' % e[2])
    if k == 'union':
        return '{' + ' | '.join(show(x) for x in e[1]) + '}'
    return repr(e)


def regs_in(e, acc=None):
    if acc is None:
        acc = set()
    k = e[0]
    if k == 'reg':
        acc.add(e[1])
    elif k in ('mem', 'paren', 'neg', 'not'):
        regs_in(e[-1], acc)
    elif k == 'call':
        for a in e[2]:
            regs_in(a, acc)
    elif k == 'bin':
        regs_in(e[2], acc)
        regs_in(e[3], acc)
    return acc


# --------------------------------------------------------------------------
# statements and blocks
# --------------------------------------------------------------------------

# raw instructions that do NOT write ac0..ac3 (float unit, pushes, faults)
RAW_NO_DEF = re.compile(r'^(F[A-Z]+|WFLAD|WFFAD|XFLDS|XFAMS|LFLDS|LFAMS|WPSH|DERR|WSAVS|WSAVR)$')
RAW_CALL = ('LCALL', 'XCALL', 'LJSR')


class Stmt:
    __slots__ = ('text', 'kind', 'lhs', 'rhs', 'defs', 'uses', 'labels', 'ret',
                 'raw_op', 'raw_pc', 'callee', 'args', 'site', 'op', 'operands',
                 'blk', 'idx', 'cond', 'sdefs', 'sdefs_def')

    def __init__(self, text):
        self.text = text
        self.kind = None
        self.lhs = None
        self.rhs = None
        self.defs = set()
        self.uses = set()
        self.labels = []
        self.ret = None
        self.raw_op = None
        self.raw_pc = None
        self.callee = None
        self.args = []
        self.site = None
        self.op = None          # WCMV / WCMP / WBLM
        self.operands = None    # dict: WCMV dst/src, WCMP s1/s2, WBLM dst/src
        self.blk = None
        self.idx = None
        self.cond = None
        self.sdefs = {}         # frame slot k -> ('write', tree, width) | ('clobber', reason, strength)
        self.sdefs_def = {}     # the same, definite effects only

    def is_terminator(self):
        return self.kind in ('ret', 'goto', 'call', 'rt_call') or \
            (self.kind == 'raw' and (self.raw_op in RAW_CALL or self.raw_op in ('WRTN', 'SYSCALL', 'DERR')))

    def __repr__(self):
        return '<%s %s:%s %r>' % (self.kind, self.blk, self.idx, self.text[:60])


def parse_stmt(text):
    st = Stmt(text)
    s = text.strip()
    m = re.match(r'^@([0-9A-F]{8})\s+(\S+)', s)
    if m:
        st.kind = 'raw'
        st.raw_pc = int(m.group(1), 16)
        st.raw_op = m.group(2).rstrip(';,')
        if st.raw_op in RAW_CALL:
            st.defs = {'ac3'}
        elif RAW_NO_DEF.match(st.raw_op):
            pass
        elif st.raw_op == 'WRTN':
            pass
        else:
            st.defs = set(REGS)          # opaque: DIVX, WLOB, SYSCALL, ...
        return st
    if s == 'ret':
        st.kind = 'ret'
        return st
    m = re.match(r'^goto \[([^\]]*)\]\s*(.*)$', s)
    if m:
        st.kind = 'goto'
        st.labels = [x.strip() for x in m.group(1).split(',') if x.strip()]
        if m.group(2).strip():
            st.cond = Parser(m.group(2)).expr()
            st.uses = regs_in(st.cond)
        return st
    m = re.match(r'^rt_call\s+(\S+?)\((.*)\)\s*site=([0-9A-F]{8})\s*$', s)
    if m:
        st.kind = 'rt_call'
        st.callee = m.group(1)
        st.site = int(m.group(3), 16)
        if m.group(2).strip():
            p = Parser(m.group(2))
            st.args.append(p.expr())
            while not p.at_end():
                p.take(',')
                st.args.append(p.expr())
        for a in st.args:
            regs_in(a, st.uses)
        st.defs = {'ac3'}
        st.labels = ['%08X' % (st.site + 4)]
        return st
    m = re.match(r'^call (\S+)\s+(.*)$', s)
    if m:
        st.kind = 'call'
        st.callee = m.group(1)
        r = re.search(r'\bret=(\S+)', m.group(2))
        st.ret = r.group(1)
        st.labels = [st.ret]
        st.defs = {'ac3'}
        return st
    if s.startswith('assert('):
        st.kind = 'assert'
        inner = s[len('assert('):-1]
        # strip a trailing , "message"
        mm = re.match(r'^(.*?),\s*"[^"]*"\s*$', inner)
        if mm:
            inner = mm.group(1)
        st.cond = Parser(inner).expr()
        st.uses = regs_in(st.cond)
        return st
    if s.startswith('words('):
        st.kind = 'string'
        st.op = 'WBLM'
        lhs, rhs = s.split('=', 1)
        d = _parse_words(lhs.strip())
        sr = _parse_words(rhs.strip())
        st.operands = {'dst': d, 'src': sr}
        st.defs = {'ac1', 'ac2', 'ac3'}
        for x in (d, sr):
            regs_in(x['addr'], st.uses)
            regs_in(x['count'], st.uses)
        return st
    if s.startswith('ac1 = cmp('):
        st.kind = 'string'
        st.op = 'WCMP'
        p = Parser(s[len('ac1 = cmp('):-1])
        s1 = p.piece()
        p.take(',')
        s2 = p.piece()
        st.operands = {'s1': s1, 's2': s2}
        st.defs = set(REGS)
        for x in (s1, s2):
            regs_in(x['addr'], st.uses)
            if x['count'] is not None:
                regs_in(x['count'], st.uses)
        return st
    if s.startswith('[@'):
        st.kind = 'string'
        st.op = 'WCMV'
        p = Parser(s)
        dst = p.piece()
        p.take('=')
        src = p.piece()
        st.operands = {'dst': dst, 'src': src}
        st.defs = set(REGS)
        for x in (dst, src):
            regs_in(x['addr'], st.uses)
            if x['count'] is not None:
                regs_in(x['count'], st.uses)
        return st
    m = re.match(r'^claim (s@\S+),\s*(ac[0-3])$', s)
    if m:                                   # IR.md 5.9: reads acN, moves nothing
        st.kind = 'claim'
        st.uses = {m.group(2)}
        return st
    m = re.match(r'^release (s@\S+),\s*(ac[0-3])$', s)
    if m:                                   # IR.md 5.9: asserts acN, then acN := wsp
        st.kind = 'release'
        st.uses = {m.group(2)}
        st.defs = {m.group(2)}
        return st
    if '=' in s:
        lhs, rhs = s.split('=', 1)
        lhs, rhs = lhs.strip(), rhs.strip()
        st.kind = 'assign'
        st.lhs = Parser(lhs).expr()
        st.rhs = Parser(rhs).expr()
        if st.lhs[0] == 'reg':
            st.defs = {st.lhs[1]}
        else:
            regs_in(st.lhs, st.uses)
        regs_in(st.rhs, st.uses)
        return st
    raise ValueError('unrecognised statement: %r' % text)


def _parse_words(t):
    m = re.match(r'^words\(@(.*)\)$', t)
    p = Parser(m.group(1))
    addr = p.expr()
    p.take(',')
    cnt = p.expr()
    return {'addr': addr, 'count': cnt, 'form': 'words'}


class Block:
    __slots__ = ('name', 'seg', 'stmts', 'succ', 'pred', 'routine')

    def __init__(self, name, seg):
        self.name = name
        self.seg = seg
        self.stmts = []
        self.succ = []
        self.pred = []
        self.routine = None


def load_book(path):
    blocks = OrderedDict()
    cur = None
    for line in open(path):
        line = line.rstrip('\n')
        m = re.match(r'^block ([0-9A-F]{8})\s+seg\s+(\S+)', line)
        if m:
            cur = Block(m.group(1), m.group(2))
            blocks[cur.name] = cur
            continue
        if cur is None or not line.strip() or line.lstrip().startswith(';'):
            if line.startswith('blocks '):
                cur = None
            continue
        if line.startswith('blocks '):
            cur = None
            continue
        i = line.find(' ; ')
        t = (line[:i] if i >= 0 else line).strip()
        if not t:
            continue
        st = parse_stmt(t)
        st.blk = cur.name
        st.idx = len(cur.stmts)
        cur.stmts.append(st)
    return blocks


def load_split(path):
    """quest.blocks.split: start -> successor list (the `n` part)."""
    succ = {}
    cur = None
    for line in open(path, errors='replace'):
        line = line.rstrip('\r\n')
        m = re.match(r'^([0-9A-F]{8}):$', line)
        if m:
            cur = m.group(1)
            succ[cur] = []
            continue
        if cur is None:
            continue
        m = re.match(r'^[ncsju]\b(.*)$', line)
        if m and (line[0] != 'n' or not line.startswith('n' + 'o')):
            t = re.search(r'\bn\s+(.*)$', line)
            if t:
                succ[cur] = [x.upper() for x in t.group(1).split()]
            cur = None
    return succ


def load_addrbook(path):
    """(entry pc, name); commented-out rows (nested routines that stay
    stacked under M4a) are included — they are routines all the same."""
    entries = []
    for line in open(path):
        if not line.strip():
            continue
        if line.startswith('#'):
            line = line[1:]
        f = line.split()
        if len(f) < 6 or not re.fullmatch(r'[0-9A-F]{8}', f[0]):
            continue
        entries.append((int(f[0], 16), f[1]))
    entries.sort()
    return entries


def load_ledgers(paths):
    """pc/idiom/func per (block, ir text) from the P31/P32/P33 site ledgers."""
    out = {}
    for p in paths:
        cols = None
        for line in open(p):
            if line.startswith('# pc\t'):
                cols = line[2:].rstrip('\n').split('\t')
                continue
            if line.startswith('#') or not line.strip() or cols is None:
                continue
            f = line.rstrip('\n').split('\t')
            row = dict(zip(cols, f))
            if row.get('verdict') != 'EMIT':
                continue
            out[(row['block'], row['ir'])] = row
    return out


# --------------------------------------------------------------------------
# CFG
# --------------------------------------------------------------------------

def build_cfg(blocks, split, addrbook):
    starts = sorted(int(k, 16) for k in split)
    unresolved = []
    for b in blocks.values():
        last = b.stmts[-1] if b.stmts else None
        if last is None:
            unresolved.append((b.name, 'empty'))
            continue
        if last.kind in ('goto', 'call', 'rt_call'):
            b.succ = list(last.labels)
        elif last.kind == 'ret':
            b.succ = []
        elif last.kind == 'raw':
            i = bisect.bisect_right(starts, last.raw_pc) - 1
            key = '%08X' % starts[i]
            b.succ = list(split.get(key, []))
        else:
            unresolved.append((b.name, 'open block: ' + last.text[:40]))
        for s in b.succ:
            if s not in blocks:
                unresolved.append((b.name, 'successor %s not a book block' % s))
        b.succ = [s for s in b.succ if s in blocks]
    for b in blocks.values():
        for s in b.succ:
            blocks[s].pred.append(b.name)
    for b in blocks.values():
        if not b.pred and not (b.stmts and b.stmts[0].kind == 'raw' and b.stmts[0].raw_op == 'WSAVS'):
            unresolved.append((b.name, 'no predecessor and does not open with WSAVS: ' + (b.stmts[0].text[:40] if b.stmts else '')))
    # routine attribution by address
    addrs = [a for a, _ in addrbook]
    names = [n for _, n in addrbook]
    for b in blocks.values():
        i = bisect.bisect_right(addrs, int(b.name, 16)) - 1
        b.routine = names[i] if i >= 0 else None
    return unresolved


# --------------------------------------------------------------------------
# reaching definitions
# --------------------------------------------------------------------------

def stmt_defs(st, key, definite=False):
    """Does statement st define `key`?  Registers come from st.defs; frame
    slots ('slot', k) from st.sdefs (filled by SlotModel.effects), or from
    st.sdefs_def when only DEFINITE effects count."""
    if isinstance(key, tuple):
        return key[1] in (st.sdefs_def if definite else st.sdefs)
    return key in st.defs


def block_gen(b, keys, definite=False):
    gen = {}
    for st in b.stmts:
        for k in keys:
            if stmt_defs(st, k, definite):
                gen[k] = (b.name, st.idx)
    return gen


def reaching_defs(blocks, keys=REGS, subset=None, entry_blocks=None, definite=False):
    """IN[b] : key -> set of (block, idx) | ('entry', block).
    `subset` restricts the analysis to those block names (a routine);
    predecessors outside the subset are ignored.  A block with no
    predecessor inside the subset (or listed in entry_blocks) starts with
    an `entry` definition of every key."""
    names = list(subset) if subset is not None else list(blocks)
    nameset = set(names)
    gen = {n: block_gen(blocks[n], keys, definite) for n in names}
    IN = {n: {k: set() for k in keys} for n in names}
    OUT = {n: {k: set() for k in keys} for n in names}
    for n in names:
        preds = [p for p in blocks[n].pred if p in nameset]
        if not preds or (entry_blocks and n in entry_blocks):
            for k in keys:
                IN[n][k].add(('entry', n))
    work = deque(names)
    inq = set(work)
    while work:
        n = work.popleft()
        inq.discard(n)
        b = blocks[n]
        preds = [p for p in b.pred if p in nameset]
        if preds:
            newin = {k: set() for k in keys}
            for p in preds:
                for k in keys:
                    newin[k] |= OUT[p][k]
            if entry_blocks and n in entry_blocks:
                for k in keys:
                    newin[k].add(('entry', n))
            IN[n] = newin
        newout = {}
        for k in keys:
            if k in gen[n]:
                newout[k] = {gen[n][k]}
            else:
                newout[k] = IN[n][k]
        if newout != OUT[n]:
            OUT[n] = newout
            for sc in b.succ:
                if sc in nameset and sc not in inq:
                    work.append(sc)
                    inq.add(sc)
    return IN, OUT


def defs_at(blocks, IN, blk, idx, key, definite=False):
    """Definitions of `key` reaching statement `idx` of block `blk`
    (i.e. just before it executes)."""
    b = blocks[blk]
    for j in range(idx - 1, -1, -1):
        if stmt_defs(b.stmts[j], key, definite):
            return {(blk, j)}
    if blk not in IN or key not in IN[blk]:
        return {('entry', blk)}
    return IN[blk][key]


# --------------------------------------------------------------------------
# value tracing
# --------------------------------------------------------------------------
# A traced VALUE is a tree in which every register has been replaced by a
# ('root', kind, detail) leaf or by the defining expression.  Roots:
#   const        a literal constant (as ('const', v))
#   lenload      sx16(M16[A]) — a 16-bit read; A is kept as a tree
#   wide         M32[A]
#   residue      ('root', 'residue', (op, reg, blk, idx))
#   call         ('root', 'call', (blk, idx))           ac3 := wfp
#   entry        ('root', 'entry', blk)
#   opaque       ('root', 'opaque', text)               raw instruction, cycle, effop we do not model
#   union        ('union', [values])                    several reaching definitions

class Tracer:
    def __init__(self, blocks, IN, SIN=None, through_len=False):
        self.blocks = blocks
        self.IN = IN
        self.SIN = SIN or {}        # block -> {('slot', k): defs}
        self.through_len = through_len
        self.memo = {}

    # -- frame identity ----------------------------------------------------
    def is_frame(self, base, blk, idx, stack):
        """Is the address base the routine's own frame pointer?  wfp itself,
        or ac3 traced to wfp / a call (ac3 := wfp) / the routine entry."""
        base = strip_paren(base)
        if base[0] == 'sreg' and base[1] == 'wfp':
            return True
        if base[0] != 'reg':
            return False
        v = self.trace_reg(base[1], blk, idx, stack)
        for x in flatten_union(v):
            x = strip_paren(x)
            if x[0] == 'sreg' and x[1] == 'wfp':
                continue
            return False
        return True

    def trace_slot(self, k, width, blk, idx, stack):
        key = ('slot', k)
        mkey = (key, width, blk, idx)
        if mkey in self.memo:
            return self.memo[mkey]
        if mkey in stack:
            return ('root', 'cycle', mkey)
        stack = stack | {mkey}
        vals = []
        for d in defs_at(self.blocks, self.SIN, blk, idx, key):
            if d[0] == 'entry':
                # negative offsets are the argument cells (wfp-10-2N): written by
                # the caller, an interprocedural fact this tool does not chase
                vals.append(('root', 'argument' if k < 0 else 'slot-entry', (k, d[1])))
                continue
            st = self.blocks[d[0]].stmts[d[1]]
            eff = st.sdefs[k]
            if eff[0] == 'clobber':
                vals.append(('root', 'opaque', 'slot %d clobbered: %s @%s:%d' % (k, eff[1], d[0], d[1])))
            elif False:
                pass
            else:
                v = self.subst(eff[1], d[0], d[1], stack)
                if eff[2] == 32 and width == 16:
                    v = ('call', 'trunc16', [v])
                elif eff[2] == 16 and width == 32:
                    v = ('root', 'opaque', 'wide read of a narrow write @%s:%d' % (d[0], d[1]))
                vals.append(v)
        v = dedupe_union(vals)
        if not any_cycle(v):
            self.memo[mkey] = v
        return v

    def trace_reg(self, reg, blk, idx, stack=None):
        """Value of `reg` just before (blk, idx)."""
        if stack is None:
            stack = frozenset()
        key = (reg, blk, idx)
        if key in self.memo:
            return self.memo[key]
        if key in stack:
            return ('root', 'cycle', key)
        stack = stack | {key}
        vals = []
        for d in defs_at(self.blocks, self.IN, blk, idx, reg):
            vals.append(self.value_of_def(reg, d, stack))
        v = dedupe_union(vals)
        if not any_cycle(v):
            self.memo[key] = v
        return v

    def value_of_def(self, reg, d, stack):
        if d[0] == 'entry':
            # WSAVS opens every routine with ac3 = wfp (EagleStack.cpp WSAVS);
            # ac0..ac2 hold the caller's values.  build_cfg checks that every
            # predecessor-less block opens with WSAVS.
            if reg == 'ac3':
                return ('sreg', 'wfp')
            return ('root', 'entry', d[1])
        blk, idx = d
        st = self.blocks[blk].stmts[idx]
        if st.kind == 'assign':
            return self.subst(st.rhs, blk, idx, stack)
        if st.kind == 'string':
            return ('root', 'residue', (st.op, reg, blk, idx))
        if st.kind in ('call', 'rt_call') or (st.kind == 'raw' and st.raw_op in RAW_CALL):
            # the only register a call defines is ac3, and its value is wfp
            # (WRTN: ac3 = wfp; RTBridge.cpp:90 the same)
            return ('sreg', 'wfp') if reg == 'ac3' else ('root', 'call', (blk, idx, st.callee or st.raw_op))
        if st.kind == 'raw':
            return ('root', 'opaque', 'raw %s @%08X' % (st.raw_op, st.raw_pc))
        if st.kind == 'release':
            return ('root', 'opaque', 'release (wsp) @%s:%d' % (blk, idx))
        return ('root', 'opaque', st.text[:40])

    def subst(self, e, blk, idx, stack):
        k = e[0]
        if k == 'reg':
            return self.trace_reg(e[1], blk, idx, stack)
        if k in ('const', 'bplit', 'flag', 'sreg', 'name', 't', 'root'):
            return e
        if k == 'mem':
            if e[1] in ('M32', 'M16') and self.SIN:
                a = norm_addr(strip_paren(self.subst(e[2], blk, idx, stack)))
                if a[0] == 'call' and a[1] == 'wp' and len(a[2]) == 2 and strip_paren(a[2][1])[0] == 'const' \
                        and strip_paren(a[2][0]) == ('sreg', 'wfp'):
                    return self.trace_slot(to_signed(strip_paren(a[2][1])[1]), 32 if e[1] == 'M32' else 16, blk, idx, stack)
                return ('mem', e[1], a)
            return ('mem', e[1], self.subst(e[2], blk, idx, stack))
        if k == 'call':
            if e[1] == 'sx16' and not self.through_len and len(e[2]) == 1 and strip_paren(e[2][0])[0] == 'mem':
                # a length-word read stays a length-word read (the base class);
                # through_len=True traces it like any other slot
                m = strip_paren(e[2][0])
                return ('call', 'sx16', [('mem', m[1], norm_addr(strip_paren(self.subst(m[2], blk, idx, stack))))])
            return ('call', e[1], [self.subst(a, blk, idx, stack) for a in e[2]])
        if k == 'bin':
            return ('bin', e[1], self.subst(e[2], blk, idx, stack), self.subst(e[3], blk, idx, stack))
        if k in ('paren', 'neg', 'not'):
            return (k, self.subst(e[1], blk, idx, stack))
        if k == 'union':
            return ('union', [self.subst(x, blk, idx, stack) for x in e[1]])
        raise ValueError(e)


def dedupe_union(vals):
    flat = []
    seen = set()
    for v in vals:
        for x in flatten_union(v):
            k = show(x)
            if k not in seen:
                seen.add(k)
                flat.append(x)
    return flat[0] if len(flat) == 1 else ('union', flat)


def norm_addr(a):
    """wp(wp(b, k1), k2) -> wp(b, k1 + k2); bp(bp(b, j1), j2) -> bp(b, j1 + j2);
    a register base already traced to its value is folded the same way."""
    a = strip_paren(a)
    if a[0] == 'call' and a[1] in ('wp', 'bp') and len(a[2]) == 2:
        base, disp = norm_addr(strip_paren(a[2][0])), strip_paren(a[2][1])
        if disp[0] == 'const' and base[0] == 'call' and base[1] == a[1] and len(base[2]) == 2 \
                and strip_paren(base[2][1])[0] == 'const':
            return ('call', a[1], [base[2][0], ('const', to_signed(strip_paren(base[2][1])[1]) + to_signed(disp[1]))])
        return ('call', a[1], [base, disp])
    return a


def any_cycle(v):
    k = v[0]
    if k == 'root':
        return v[1] == 'cycle'
    if k == 'union':
        return any(any_cycle(x) for x in v[1])
    if k in ('mem', 'paren', 'neg', 'not'):
        return any_cycle(v[-1])
    if k == 'call':
        return any(any_cycle(a) for a in v[2])
    if k == 'bin':
        return any_cycle(v[2]) or any_cycle(v[3])
    return False


def flatten_union(v):
    if v[0] == 'union':
        out = []
        for x in v[1]:
            out.extend(flatten_union(x))
        return out
    return [v]



# --------------------------------------------------------------------------
# frame-slot effects (what each statement does to M32/M16[wp(frame, k)])
# --------------------------------------------------------------------------
# Runtime callees' writes-through, from docs/Project28/RTConventions.md 64-80
# (measured by P28): position -> 'wide' (one wide = 2 words) or 'upward' (a
# buffer of unknown length).  ?UNSIGNED_TO_CHAR writes a varying at the ac2
# word address, at most 17 words.  ?GET_SHARED_PAGE's arg-2 write is
# quest.assumptions' finding.  An integer is a bounded word extent.
RT_WRITES = {
    '?WRITE_SCREEN': {3: 'wide', 4: 'wide'},
    '?RANDOM_NUMBER': {3: 'wide'},
    '?READ': {3: 'wide', 4: 'upward'},
    '?OPEN_FILE': {1: 'wide'},
    '?OPEN_SHARED_IO_FILE': {1: 'wide'},
    '?WRITE': {3: 'wide'},
    '?LOOKUP_PORT': {2: 'wide'},
    '?GET_SHARED_PAGE': {2: 'wide'},
    '?UNSIGNED_TO_CHAR': {'ac2': 17},      # length word + up to 32 digits (runtime/unsigned_to_char.cpp:136 `k<=width<=32 always`)
    '?DELAY': {}, '?CHAR_TO_UNSIGNED': {}, '?CLOSE_FILE': {}, '?CREATE_TASK': {},
    '?AWAIT_CONSOLE_INTERRUPT': {}, '?LIB_ERROR_CODE': {}, '?CURRENT_PID': {},
}
NF = 'non-frame'


class SlotModel:
    def __init__(self, blocks, tracer, call_policy='upward', infer_caps=False):
        self.blocks = blocks
        self.tracer = tracer
        self.call_policy = call_policy
        self.infer_caps = infer_caps      # a001 Q2 policy (a): bound an unbounded copy by the
        self.caps = {}                    # largest CONSTANT copy into the same buffer start
        self.memo = {}
        self.stats = defaultdict(int)

    def infer_capacities(self, names):
        """(a001 Q2 (a)) per buffer start word: the largest constant extent any
        string statement of the routine writes there.  A LOWER bound on the
        buffer's declared size; using it to bound an unbounded copy into the
        same buffer is the circular case a001 asked to be marked."""
        caps = {}
        for n in names:
            for st in self.blocks[n].stmts:
                if st.kind != 'string' or st.op == 'WCMP':
                    continue
                d = st.operands['dst']
                r = self.frame_range(d['addr'], st.blk, st.idx)
                cnt = strip_paren(d['count'])
                if r in (NF, None) or cnt[0] != 'const':
                    continue
                nb = to_signed(cnt[1])
                if st.op == 'WBLM':
                    ext = nb
                elif d['form'] == 'dst-varying':
                    ext = (nb + 1) // 2 + 1
                else:
                    ext = max(nb - 1, 0) // 2 + 1
                caps[r[0]] = max(caps.get(r[0], 0), ext)
        self.caps = caps

    def frame_range(self, addr, blk, idx, stack=frozenset()):
        """(start_word, end_word|None) of a frame address tree, NF for an
        address that is not in the routine's frame, None when unknown."""
        a = strip_paren(addr)
        if a[0] == 'call' and a[1] in ('wp', 'bp') and len(a[2]) == 2:
            base, disp = strip_paren(a[2][0]), strip_paren(a[2][1])
            if disp[0] != 'const':
                r = self.frame_range(base, blk, idx, stack)
                return r if r in (NF, None) else (r[0], None)
            d = to_signed(disp[1])
            w = d if a[1] == 'wp' else d // 2
            if base[0] in ('reg', 'sreg'):
                if self.tracer.is_frame(base, blk, idx, stack):
                    return (w, w)
                r = self.frame_range(base, blk, idx, stack)
                return r if r in (NF, None) else (r[0] + w, None)
            r = self.frame_range(base, blk, idx, stack)
            return r if r in (NF, None) else (r[0] + w, None)
        if a[0] in ('bplit', 'const', 'name'):
            return NF
        if a[0] == 'mem':
            return NF                     # a pointer loaded from memory: assumed not into the own frame
        if a[0] == 'sreg':
            return (0, None) if a[1] == 'wfp' else None
        if a[0] == 'call':                # arithmetic on pointers: add/sub/ind/lsh/...
            parts = [self.frame_range(x, blk, idx, stack) for x in a[2]]
            if all(pt == NF for pt in parts):
                return NF
            if any(pt is None for pt in parts):
                return None
            if a[1] in ('add', 'nadd', 'ind', 'paren'):
                starts = [pt[0] for pt in parts if pt != NF]
                return (min(starts), None)
            return None
        if a[0] == 'bin':
            if a[1] in ('+', '-'):
                l = self.frame_range(a[2], blk, idx, stack)
                r = self.frame_range(a[3], blk, idx, stack)
                if l == NF and r == NF:
                    return NF
                if l is None or r is None:
                    return None
                if l == NF:
                    return (r[0], None)
                return (l[0], None)
            return None
        if a[0] == 'reg':
            v = self.tracer.trace_reg(a[1], blk, idx, stack)
            return self.range_of_value(v, stack)
        if a[0] == 'union':
            return self.range_of_value(a, stack)
        if a[0] == 'root':
            return self.range_of_root(a, stack)
        return None

    def range_of_value(self, v, stack):
        outs = []
        for x in flatten_union(v):
            x = strip_paren(x)
            if x[0] == 'root':
                outs.append(self.range_of_root(x, stack))
            elif x[0] == 'sreg' and x[1] == 'wfp':
                outs.append((0, None))
            else:
                outs.append(self.frame_range(x, '?', 0, stack) if x[0] != 'reg' else None)
        if all(o == NF for o in outs):
            return NF
        if any(o is None for o in outs):
            return None
        starts = [o[0] for o in outs if o != NF]
        return (min(starts), None)

    def range_of_root(self, r, stack):
        kind, det = r[1], r[2]
        if kind == 'residue':
            op, reg, blk, idx = det
            key = ('res', op, reg, blk, idx)
            if key in self.memo:
                return self.memo[key]
            if key in stack:
                return None
            st = self.blocks[blk].stmts[idx]
            opd = None
            if op == 'WCMV':
                opd = st.operands['dst'] if reg == 'ac2' else st.operands['src'] if reg == 'ac3' else None
            elif op == 'WCMP':
                opd = st.operands['s2'] if reg == 'ac2' else st.operands['s1'] if reg == 'ac3' else None
            elif op == 'WBLM':
                opd = st.operands['src'] if reg == 'ac2' else st.operands['dst'] if reg == 'ac3' else None
            if opd is None:
                return None
            rr = self.frame_range(ptr_tree(opd), blk, idx, stack | {key})
            out = rr if rr in (NF, None) else (rr[0], None)
            self.memo[key] = out
            return out
        return None

    def effects(self, st, tracked):
        """Fill st.sdefs for the tracked slots of its routine."""
        blk, idx = st.blk, st.idx
        eff = {}
        def clobber_range(lo, hi, why):
            # a bounded range is a DEFINITE effect; an open-ended one is a MAY
            strength = 'definite' if hi is not None else 'maybe'
            for k in tracked:
                if k >= lo and (hi is None or k <= hi):
                    eff.setdefault(k, ('clobber', why, strength))
        def apply_range(r, why):
            if r == NF:
                return
            if r is not None and r[1] is None and self.infer_caps and r[0] in self.caps:
                # policy (a): a bounded MAY, flagged 'inferred' (circular by construction)
                for k in tracked:
                    if r[0] <= k < r[0] + self.caps[r[0]]:
                        eff.setdefault(k, ('clobber', why + ' [extent INFERRED: %d words, the largest constant copy into this buffer]' % self.caps[r[0]], 'inferred'))
                self.stats['inferred-bound:' + why.split(' ')[0]] += 1
                return
            if r is None:
                self.stats['clobber-all:' + why.split(' ')[0]] += 1
                for k in tracked:
                    eff.setdefault(k, ('clobber', why + ' (frame range unknown)', 'maybe'))
                return
            clobber_range(r[0], r[1], why)
        if st.kind == 'assign' and st.lhs[0] == 'mem':
            w = st.lhs[1]
            r = self.frame_range(st.lhs[2], blk, idx)
            if r == NF:
                pass
            elif r is None:
                apply_range(None, 'store %s' % show(st.lhs))
            elif r[1] is None:
                apply_range(r, 'store %s' % show(st.lhs))
            else:
                k = r[0]
                if w == 'M32':
                    eff[k] = ('write', st.rhs, 32)
                    if k + 1 in tracked:
                        eff[k + 1] = ('clobber', 'upper word of M32 store', 'definite')
                elif w == 'M16':
                    eff[k] = ('write', st.rhs, 16)
                elif w == 'M8':
                    eff[k] = ('clobber', 'byte store', 'definite')
        elif st.kind == 'string':
            if st.op == 'WCMV':
                d = st.operands['dst']
                r = self.frame_range(d['addr'], blk, idx)
                n = strip_paren(d['count'])
                if r not in (NF, None) and n[0] == 'const':
                    nb = to_signed(n[1])
                    if d['form'] == 'dst-varying':
                        r = (r[0], r[0] + (nb + 1) // 2)
                    else:
                        r = (r[0], r[0] + max(nb - 1, 0) // 2)
                elif r not in (NF, None):
                    r = (r[0], None)
                if d['form'] == 'dst-varying' and r not in (NF, None) and r[0] in tracked:
                    # assign_varying stores min(len, n) in the length word, and the
                    # varying destination form is emitted only where the two counts
                    # are equal (IR.md 5.8, P32), so the length word := the count
                    eff[r[0]] = ('write', d['count'], 16)
                    r = (r[0] + 1, r[1])
                apply_range(r, 'WCMV destination')
            elif st.op == 'WBLM':
                d = st.operands['dst']
                r = self.frame_range(d['addr'], blk, idx)
                n = strip_paren(d['count'])
                if r not in (NF, None):
                    r = (r[0], r[0] + to_signed(n[1]) - 1) if n[0] == 'const' else (r[0], None)
                apply_range(r, 'WBLM destination')
        elif st.kind == 'rt_call':
            tab = RT_WRITES.get(st.callee)
            if tab is None:
                for a in st.args:
                    apply_range(self._up(self.frame_range(a, blk, idx)), 'unlisted runtime callee %s' % st.callee)
            else:
                for pos, mode in tab.items():
                    if pos == 'ac2':
                        r = self.frame_range(('reg', 'ac2'), blk, idx)
                    else:
                        if pos > len(st.args):
                            continue
                        r = self.frame_range(st.args[pos - 1], blk, idx)
                    if r in (NF, None):
                        apply_range(r, 'rt_call %s arg %s' % (st.callee, pos))
                    elif mode == 'wide':
                        clobber_range(r[0], r[0] + 1, 'rt_call %s writes arg %s' % (st.callee, pos))
                    elif isinstance(mode, int):
                        clobber_range(r[0], r[0] + mode - 1, 'rt_call %s writes %d words at arg %s' % (st.callee, mode, pos))
                    else:
                        apply_range((r[0], None), 'rt_call %s writes through arg %s' % (st.callee, pos))
        elif st.kind == 'call' or (st.kind == 'raw' and st.raw_op in RAW_CALL):
            # the arguments are the `M32[<argslot>] = <ea>` stores earlier in the block
            args = []
            for j in range(idx - 1, -1, -1):
                s2 = self.blocks[blk].stmts[j]
                if s2.kind == 'assign' and s2.lhs[0] == 'mem' and s2.lhs[1] == 'M32' and strip_paren(s2.lhs[2])[0] == 'const':
                    args.append(s2.rhs)
                elif s2.kind in ('call', 'rt_call'):
                    break
            for a in args:
                r = self.frame_range(a, blk, idx)
                if r in (NF, None):
                    apply_range(r, 'call %s by-reference argument' % st.callee)
                elif self.call_policy == 'wide':
                    clobber_range(r[0], r[0] + 1, 'call %s writes through argument (policy: one wide)' % st.callee)
                else:
                    apply_range((r[0], None), 'call %s writes through argument (policy: upward)' % st.callee)
            if st.kind == 'raw':
                apply_range(None, 'raw %s (arguments not lifted)' % st.raw_op)
        elif st.kind == 'raw':
            if not RAW_NO_DEF.match(st.raw_op) and st.raw_op not in RAW_CALL:
                apply_range(None, 'raw %s' % st.raw_op)
        st.sdefs = eff
        st.sdefs_def = {k: e for k, e in eff.items() if e[0] == 'write' or e[2] == 'definite'}

    @staticmethod
    def _up(r):
        return r if r in (NF, None) else (r[0], None)


def tracked_slots(blocks, names):
    """Frame word offsets read as M32/M16[wp(ac3, k)] anywhere in the routine."""
    T = set()
    def walk(e):
        k = e[0]
        if k == 'mem' and e[1] in ('M32', 'M16'):
            a = strip_paren(e[2])
            if a[0] == 'call' and a[1] == 'wp' and len(a[2]) == 2 and strip_paren(a[2][0]) == ('reg', 'ac3') \
                    and strip_paren(a[2][1])[0] == 'const':
                T.add(to_signed(strip_paren(a[2][1])[1]))
            walk(e[2])
        elif k in ('paren', 'neg', 'not'):
            walk(e[1])
        elif k == 'call':
            for a in e[2]:
                walk(a)
        elif k == 'bin':
            walk(e[2]); walk(e[3])
    for n in names:
        for st in blocks[n].stmts:
            for e in ([st.rhs] if st.rhs else []) + ([st.lhs] if st.lhs else []) + ([st.cond] if st.cond else []) + st.args:
                walk(e)
            if st.kind == 'string':
                for role, opd in site_operands(st):
                    walk(opd['addr'])
                    if opd['count'] is not None:
                        walk(opd['count'])
                    if opd['form'] == 'varying':
                        walk(('mem', 'M16', opd['addr']))
    return T


def slot_reaching_defs(blocks, tracer, call_policy='upward', infer_caps=False):
    """Per routine: compute slot effects and run RD over ('slot', k) keys.
    -> SIN (block -> key -> defs), model."""
    model = SlotModel(blocks, tracer, call_policy, infer_caps)
    by_routine = defaultdict(list)
    for n, b in blocks.items():
        by_routine[b.routine].append(n)
    SIN = {}
    SIN_DEF = {}
    for rname, names in by_routine.items():
        T = tracked_slots(blocks, names)
        if infer_caps:
            model.infer_capacities(names)
        for n in names:
            for st in blocks[n].stmts:
                model.effects(st, T)
        if not T:
            continue
        keys = [('slot', k) for k in sorted(T)]
        IN, OUT = reaching_defs(blocks, keys, subset=names)
        SIN.update(IN)
        IN2, OUT2 = reaching_defs(blocks, keys, subset=names, definite=True)
        SIN_DEF.update(IN2)
    return SIN, SIN_DEF, model

# --------------------------------------------------------------------------
# judging non-negativity
# --------------------------------------------------------------------------
# verdict: ('yes', reason) | ('cond', reason) | ('no', reason) | ('unknown', reason)
#   yes   : >= 0 by construction
#   cond  : >= 0 IF the named length words are >= 0 (the base-class assertion)
#   no    : can be negative by construction (cmp's ac1)
#   unknown: opaque / entry / wide read / subtraction

def is_lenload(v):
    return v[0] == 'call' and v[1] == 'sx16' and len(v[2]) == 1 and v[2][0][0] == 'mem' and v[2][0][1] == 'M16'


def strip_paren(v):
    while v[0] == 'paren':
        v = v[1]
    return v


def judge(v, closure=True):
    """-> (verdict, needs) where needs is the list of length-word addresses
    (as trees) whose non-negativity the verdict is conditional on."""
    v = strip_paren(v)
    k = v[0]
    if k == 'const':
        return ('yes' if to_signed(v[1]) >= 0 else 'no', 'constant %d' % to_signed(v[1])), []
    if k == 'union':
        needs = []
        worst = 'yes'
        reasons = []
        order = {'yes': 0, 'cond': 1, 'unknown': 2, 'no': 3}
        for x in flatten_union(v):
            (vd, r), n = judge(x, closure)
            needs.extend(n)
            reasons.append(r)
            if order[vd] > order[worst]:
                worst = vd
        return (worst, 'join{' + ' | '.join(reasons) + '}'), needs
    if k == 'root':
        kind, det = v[1], v[2]
        if kind == 'residue':
            op, reg, blk, idx = det
            if op == 'WCMV' and reg == 'ac0':
                return ('yes', 'WCMV residue ac0 = 0 @%s:%d' % (blk, idx)), []
            if op == 'WCMV' and reg == 'ac1':
                return ('cond', 'WCMV residue ac1 = len - min(|n|,|len|) @%s:%d (closure: >= 0 iff its len >= 0)' % (blk, idx)), [('residue', blk, idx)]
            if op == 'WCMP' and reg == 'ac0':
                return ('cond', 'WCMP residue ac0 (dst bytes left) @%s:%d (closure: in [0, n] iff n >= 0)' % (blk, idx)), [('residue', blk, idx)]
            if op == 'WCMP' and reg == 'ac1':
                return ('no', 'WCMP residue ac1 = result in {-1,0,1} @%s:%d' % (blk, idx)), []
            if op == 'WBLM' and reg == 'ac1':
                return ('yes', 'WBLM residue ac1 = 0 @%s:%d' % (blk, idx)), []
            return ('unknown', '%s residue %s (a pointer) @%s:%d' % (op, reg, blk, idx)), []
        if kind == 'call':
            return ('unknown', 'ac3 after call (= wfp) @%s:%d' % (det[0], det[1])), []
        if kind == 'entry':
            return ('unknown', 'routine entry value @%s' % det), []
        if kind == 'argument':
            return ('unknown', 'argument cell wp(wfp, %d) (caller-supplied)' % det[0]), []
        if kind == 'slot-entry':
            return ('unknown', 'frame slot %d read with no write on some path (entry @%s)' % det), []
        return ('unknown', 'opaque: %s' % (det,)), []
    if is_lenload(v):
        return ('cond', 'length word %s' % show(v[2][0][2])), [('len', v[2][0][2])]
    if k == 'mem':
        return ('unknown', '%s read %s' % (v[1], show(v))), []
    if k == 'call':
        f = v[1]
        if f in ('add', 'nadd') or (k == 'bin' and v[1] == '+'):
            pass
        if f in ('add', 'nadd', 'sub', 'nsub') and strip_paren(v[2][0])[0] == 'const' and strip_paren(v[2][1])[0] == 'const':
            a, b = to_signed(strip_paren(v[2][0])[1]), to_signed(strip_paren(v[2][1])[1])
            r = a + b if f in ('add', 'nadd') else a - b
            return ('yes' if r >= 0 else 'no', 'constant %d' % r), []
        if f in ('add', 'nadd'):
            (va, ra), na = judge(v[2][0], closure)
            (vb, rb), nb = judge(v[2][1], closure)
            return combine_add(va, ra, vb, rb), na + nb
        if f in ('sub', 'nsub'):
            if show(strip_paren(v[2][0])) == show(strip_paren(v[2][1])):
                return ('yes', 'x - x = 0'), []          # WSUB n,n: the zeroing idiom
            (va, ra), na = judge(v[2][0], closure)
            b = strip_paren(v[2][1])
            if b[0] == 'const' and to_signed(b[1]) <= 0:
                return combine_add(va, ra, 'yes', 'minus %d' % to_signed(b[1])), na
            a = strip_paren(v[2][0])
            if a[0] == 'const':
                members = [strip_paren(x) for x in flatten_union(b)]
                members = [('const', 0) if (m[0] == 'call' and m[1] in ('sub', 'nsub') and show(strip_paren(m[2][0])) == show(strip_paren(m[2][1]))) else m for m in members]
                if all(m[0] == 'const' and to_signed(m[1]) <= to_signed(a[1]) for m in members):
                    return ('yes', '%d - {constants <= %d}' % (to_signed(a[1]), to_signed(a[1]))), []
            return ('unknown', 'subtraction %s' % show(v)), []
        if f in ('zx16', 'zx8'):
            return ('yes', 'zero-extended'), []
        if f in ('trunc16', 'trunc8'):
            return ('yes', 'truncated (unsigned view)'), []
        if f == 'cvwn':
            return judge(v[2][0], closure)
        if f in ('mul', 'nmul'):
            (va, ra), na = judge(v[2][0], closure)
            (vb, rb), nb = judge(v[2][1], closure)
            if va == 'yes' and vb == 'yes':
                return ('yes', 'product of non-negatives (no overflow assumed)'), []
            return ('unknown', 'product %s' % show(v)), []
        if f == 'sx16':
            inner = strip_paren(v[2][0])
            if inner[0] == 'call' and inner[1] == 'trunc16':
                return judge(inner[2][0], closure)      # assumes the value fit in 16 bits
            if inner[0] == 'const':
                return ('yes' if to_signed(inner[1]) & 0x8000 == 0 else 'no', 'sx16 of constant'), []
            (vi, ri), ni = judge(inner, closure)
            if vi in ('yes', 'cond'):
                return (vi, 'sx16(%s)' % ri), ni          # assumes the value fit in 16 bits
            return ('unknown', 'sx16(%s)' % ri), []
        if f in ('wp', 'bp', 'lsh', 'ash', 'div', 'ind', 'tf'):
            return ('unknown', '%s(...)' % f), []
        return ('unknown', 'call %s' % f), []
    if k == 'bin':
        op = v[1]
        if op == '+':
            (va, ra), na = judge(v[2], closure)
            (vb, rb), nb = judge(v[3], closure)
            return combine_add(va, ra, vb, rb), na + nb
        if op == '-':
            (va, ra), na = judge(v[2], closure)
            b = strip_paren(v[3])
            if b[0] == 'const' and to_signed(b[1]) <= 0:
                return combine_add(va, ra, 'yes', 'minus %d' % to_signed(b[1])), na
            return ('unknown', 'subtraction %s' % show(v)), []
        if op == '*':
            (va, ra), na = judge(v[2], closure)
            (vb, rb), nb = judge(v[3], closure)
            if va == 'yes' and vb == 'yes':
                return ('yes', 'product of non-negatives'), []
            return ('unknown', 'product %s' % show(v)), []
        if op == '&':
            return ('unknown', 'mask %s' % show(v)), []
        return ('unknown', 'operator %s' % op), []
    return ('unknown', show(v)), []


def combine_add(va, ra, vb, rb):
    order = {'yes': 0, 'cond': 1, 'unknown': 2, 'no': 3}
    if va == 'no' or vb == 'no':
        return ('unknown', 'sum with a possibly-negative term: %s + %s' % (ra, rb))
    w = va if order[va] >= order[vb] else vb
    return (w, '%s + %s' % (ra, rb))


def to_signed(v):
    v &= 0xFFFFFFFF
    return v - 0x100000000 if v & 0x80000000 else v


# --------------------------------------------------------------------------
# the base class: count == sx16(M16[A]) and pointer == byte 2A+2
# --------------------------------------------------------------------------

def byte_addr_of(e):
    """Normalise an address tree to (base_tree, byte_offset) when it is a
    wp/bp of a base plus constant, or a bare constant word/byte address."""
    e = strip_paren(e)
    if e[0] == 'call' and e[1] in ('wp', 'bp') and len(e[2]) == 2:
        base, disp = strip_paren(e[2][0]), strip_paren(e[2][1])
        if disp[0] == 'const':
            d = to_signed(disp[1])
            return (show(base), d * 2 if e[1] == 'wp' else d)
        return None
    if e[0] == 'bplit':
        return ('abs', e[1] * 2 + e[2])
    if e[0] == 'const':
        return ('abs-word', to_signed(e[1]) * 2)   # a bare constant is a WORD address in a located
    if e[0] == 'bin' and e[1] in ('+', '-'):
        a, b = strip_paren(e[2]), strip_paren(e[3])
        if b[0] == 'const':
            inner = byte_addr_of(a)
            if inner is not None:
                d = to_signed(b[1]) if e[1] == '+' else -to_signed(b[1])
                # a word-address sum (`X + k` in a varying/word context)
                return (inner[0], inner[1] + 2 * d) if inner[0] == 'abs-word' or inner[0].startswith('W:') else (inner[0], inner[1] + d)
        return ('expr:' + show(e), 0)
    return ('expr:' + show(e), 0)


def word_addr_key(e):
    """A key for a WORD address tree: (base, word_offset)."""
    e = strip_paren(e)
    if e[0] == 'call' and e[1] == 'wp' and len(e[2]) == 2 and strip_paren(e[2][1])[0] == 'const':
        return (show(strip_paren(e[2][0])), to_signed(strip_paren(e[2][1])[1]))
    if e[0] == 'const':
        return ('abs', to_signed(e[1]))
    return ('expr:' + show(e), 0)


def base_class_match(count_v, ptr_v):
    """count_v is the TRACED count value, ptr_v the traced pointer.  True when
    count_v is sx16(M16[A]) and ptr_v is bp(base, 2*off+2) for A = wp(base, off)
    (or the literal constant-address forms)."""
    c = strip_paren(count_v)
    if not is_lenload(c):
        return False
    A = strip_paren(c[2][0][2])
    p = strip_paren(ptr_v)
    ak = word_addr_key(A)
    if p[0] == 'call' and p[1] == 'bp' and len(p[2]) == 2 and strip_paren(p[2][1])[0] == 'const':
        pk = (show(strip_paren(p[2][0])), to_signed(strip_paren(p[2][1])[1]))
        return ak[0] == pk[0] and pk[1] == 2 * ak[1] + 2 and not ak[0].startswith('expr:')
    if p[0] == 'bplit' and ak[0] == 'abs':
        return p[1] == ak[1] + 1 and p[2] == 0
    # general: A + 1 as a byte pointer of a computed word address
    if p[0] == 'call' and p[1] == 'bp' and len(p[2]) == 2:
        pb, pd = strip_paren(p[2][0]), strip_paren(p[2][1])
        if pd[0] == 'const' and to_signed(pd[1]) == 2 and show(pb) == show(A):
            return True
    return False



# --------------------------------------------------------------------------
# path sensitivity: dominating guards (a001 Q3) — reported SEPARATELY
# --------------------------------------------------------------------------
# A guard is a fact known at a program point: an `assert(cond)` that every path
# to the site executes, or the outcome of a two-way `goto` whose one edge every
# path to the site takes.  Facts are traced at the guard point and matched
# STRUCTURALLY against the site's traced count.  Two caveats, stated: a fact
# about a record field read (`M16[<non-frame>]`) is trusted across the guard-
# to-site interval without checking for a store to that field in between;
# and matching is syntactic, so an equal value spelled differently is missed
# (sound: a miss is an `unknown`, never a `yes`).

CMP_OPS = {'<s', '<=s', '>s', '>=s', '==', '!=', '<u', '<=u', '>u', '>=u'}


def cond_facts(cond, truth):
    """-> list of (lhs, op, rhs) known TRUE, from a condition tree with a known
    truth value.  `&&` true gives each conjunct; `||` false gives each disjunct
    false; `!` flips; a comparison flips its operator."""
    c = strip_paren(cond)
    if c[0] == 'not':
        return cond_facts(c[1], not truth)
    if c[0] == 'bin' and c[1] == '&&':
        return (cond_facts(c[2], True) + cond_facts(c[3], True)) if truth else []
    if c[0] == 'bin' and c[1] == '||':
        return (cond_facts(c[2], False) + cond_facts(c[3], False)) if not truth else []
    if c[0] == 'bin' and c[1] in CMP_OPS:
        op = c[1]
        if not truth:
            op = {'<s': '>=s', '<=s': '>s', '>s': '<=s', '>=s': '<s', '==': '!=', '!=': '==',
                  '<u': '>=u', '<=u': '>u', '>u': '<=u', '>=u': '<u'}[op]
        return [(c[2], op, c[3])]
    return []


def entry_dom_trees(blocks):
    """dominator tree per predecessor-less block; -> block -> (idom, entry)."""
    owner = {}
    for n, b in blocks.items():
        if b.pred:
            continue
        idom, reach = dominators(blocks, n)
        for r in reach:
            owner.setdefault(r, (idom, n))
    return owner


class Guards:
    def __init__(self, blocks, tracer, owner):
        self.blocks = blocks
        self.tracer = tracer
        self.owner = owner

    def facts_at(self, blk, idx):
        """Traced facts that hold on every path to statement (blk, idx)."""
        if blk not in self.owner:
            return []
        idom, entry = self.owner[blk]
        out = []
        d = blk
        chain = []
        while True:
            chain.append(d)
            if idom.get(d, d) == d:
                break
            d = idom[d]
        for d in chain:
            b = self.blocks[d]
            limit = idx if d == blk else len(b.stmts)
            for j in range(limit):
                st = b.stmts[j]
                if st.kind == 'assert':
                    for lhs, op, rhs in cond_facts(st.cond, True):
                        out.append((self.tracer.subst(lhs, d, j, frozenset()), op,
                                    self.tracer.subst(rhs, d, j, frozenset()), 'assert@%s:%d' % (d, j)))
            # the edge into d: a unique predecessor ending in a two-way goto
            preds = b.pred
            if len(preds) == 1:
                pb = self.blocks[preds[0]]
                last = pb.stmts[-1] if pb.stmts else None
                if last is not None and last.kind == 'goto' and last.cond is not None and len(last.labels) == 2 \
                        and last.labels.count(d) == 1:
                    truth = last.labels.index(d) == 1
                    for lhs, op, rhs in cond_facts(last.cond, truth):
                        out.append((self.tracer.subst(lhs, pb.name, last.idx, frozenset()), op,
                                    self.tracer.subst(rhs, pb.name, last.idx, frozenset()), 'edge %s->%s' % (pb.name, d)))
        return out

    def prove_nonneg(self, v, facts):
        """-> (True, why) if the facts give v >= 0; (False, None) otherwise."""
        v = strip_paren(v)
        if v[0] == 'union':
            whys = []
            for x in flatten_union(v):
                ok, why = self.prove_nonneg(x, facts)
                if not ok:
                    return False, None
                whys.append(why)
            return True, ' & '.join(whys)
        (vd, _), _ = judge(v)
        if vd == 'yes':
            return True, 'by construction'
        key = show(v)
        vcx = self._as_c_minus_x(v)
        # an UNSIGNED upper bound is a non-negativity proof: v <=u K (K < 2^31)
        # means 0 <= v <= K as a signed value.  This is the shape of the
        # compiler's own DERR bounds check on a SUBSTR length.
        for lhs, op, rhs, where in facts:
            rc = strip_paren(rhs)
            if op in ('<=u', '<u') and rc[0] == 'const' and 0 <= to_signed(rc[1]) < 0x80000000:
                same = show(lhs) == key
                if not same and vcx is not None:
                    lcx = self._as_c_minus_x(lhs)
                    same = lcx is not None and lcx[0] == vcx[0] and show(lcx[1]) == show(vcx[1])
                if same:
                    return True, '%s %s %d unsigned (%s)' % (key[:40], op, to_signed(rc[1]), where)
        # direct lower bound: v >= c, v > c, c <= v, c < v
        for lhs, op, rhs, where in facts:
            L, R = show(lhs), show(rhs)
            rc = strip_paren(rhs)
            lc = strip_paren(lhs)
            if L == key and rc[0] == 'const':
                c = to_signed(rc[1])
                if (op == '>=s' and c >= 0) or (op == '>s' and c >= -1) or (op == '==' and c >= 0):
                    return True, '%s %s %d (%s)' % (key[:40], op, c, where)
            if R == key and lc[0] == 'const':
                c = to_signed(lc[1])
                if (op == '<=s' and c >= 0) or (op == '<s' and c >= -1) or (op == '==' and c >= 0):
                    return True, '%d %s %s (%s)' % (c, op, key[:40], where)
            if L == key and op in ('>=s', '>s') and self.prove_nonneg(rhs, facts)[0]:
                return True, '%s %s <non-negative> (%s)' % (key[:40], op, where)
        # C - X  (sub / nsub / (0 - X) + C): need X <= C
        cx = self._as_c_minus_x(v)
        if cx is not None:
            C, X = cx
            xk = show(X)
            for lhs, op, rhs, where in facts:
                L, R = show(lhs), show(rhs)
                rc, lc = strip_paren(rhs), strip_paren(lhs)
                if L == xk and rc[0] == 'const':
                    c = to_signed(rc[1])
                    if (op == '<=s' and c <= C) or (op == '<s' and c <= C + 1) or (op == '==' and c <= C):
                        return True, '%d - X with X %s %d (%s)' % (C, op, c, where)
                if R == xk and lc[0] == 'const':
                    c = to_signed(lc[1])
                    if (op == '>=s' and c <= C) or (op == '>s' and c <= C + 1) or (op == '==' and c <= C):
                        return True, '%d - X with %d %s X (%s)' % (C, c, op, where)
        # X + k with k >= 0: need X >= -k
        if v[0] == 'call' and v[1] in ('add', 'nadd') or (v[0] == 'bin' and v[1] == '+'):
            a, b = (v[2][0], v[2][1]) if v[0] == 'call' else (v[2], v[3])
            a, b = strip_paren(a), strip_paren(b)
            for X, K in ((a, b), (b, a)):
                if K[0] == 'const' and to_signed(K[1]) >= 0:
                    xk = show(X)
                    for lhs, op, rhs, where in facts:
                        rc = strip_paren(rhs)
                        if show(lhs) == xk and rc[0] == 'const':
                            c = to_signed(rc[1])
                            if (op == '>=s' and c >= -to_signed(K[1])) or (op == '>s' and c >= -to_signed(K[1]) - 1):
                                return True, 'X + %d with X %s %d (%s)' % (to_signed(K[1]), op, c, where)
                    ok, why = self.prove_nonneg(X, facts)
                    if ok:
                        return True, 'X + %d with X >= 0: %s' % (to_signed(K[1]), why)
        return False, None

    @staticmethod
    def _as_c_minus_x(v):
        v = strip_paren(v)
        if v[0] == 'call' and v[1] in ('sub', 'nsub'):
            a, b = strip_paren(v[2][0]), strip_paren(v[2][1])
            if a[0] == 'const':
                return to_signed(a[1]), b
        if v[0] == 'bin' and v[1] == '-':
            a, b = strip_paren(v[2]), strip_paren(v[3])
            if a[0] == 'const':
                return to_signed(a[1]), b
        # (0 - X) + C  and  C + (0 - X)
        if (v[0] == 'call' and v[1] in ('add', 'nadd')) or (v[0] == 'bin' and v[1] == '+'):
            a, b = (v[2][0], v[2][1]) if v[0] == 'call' else (v[2], v[3])
            a, b = strip_paren(a), strip_paren(b)
            for X, K in ((a, b), (b, a)):
                if K[0] == 'const':
                    inner = Guards._as_c_minus_x(X)
                    if inner is not None:
                        return inner[0] + to_signed(K[1]), inner[1]
        return None

# --------------------------------------------------------------------------
# per-site census
# --------------------------------------------------------------------------

def string_sites(blocks):
    for b in blocks.values():
        for st in b.stmts:
            if st.kind == 'string':
                yield st


def operand_count_class(opd):
    """Textual class of one operand's count."""
    if opd['form'] == 'literal':
        return 'literal'
    if opd['form'] == 'varying':
        return 'varying'
    c = strip_paren(opd['count'])
    if c[0] == 'const':
        return 'const'
    if c[0] == 'reg':
        return 'register'
    if is_lenload(c):
        return 'lenload-expr'
    if c[0] == 'mem' and c[1] == 'M32':
        return 'wide-expr'
    return 'expr'


def site_operands(st):
    """-> list of (role, operand) where role names the count register."""
    if st.op == 'WCMV':
        return [('dst/ac0', st.operands['dst']), ('src/ac1', st.operands['src'])]
    if st.op == 'WCMP':
        return [('s1/ac1', st.operands['s1']), ('s2/ac0', st.operands['s2'])]
    return [('k/ac1', st.operands['src'])]


def count_tree(opd):
    """The count as a tree in the site's context (varying -> sx16(M16[A]))."""
    if opd['form'] == 'varying':
        return ('call', 'sx16', [('mem', 'M16', opd['addr'])])
    return opd['count']


def ptr_tree(opd):
    """The byte pointer as a tree (varying -> bp(A, 2) i.e. word A+1)."""
    if opd['form'] in ('varying', 'dst-varying'):
        return ('call', 'bp', [opd['addr'], ('const', 2)])
    return opd['addr']


def analyse_site(st, tracer):
    rows = []
    for role, opd in site_operands(st):
        cls = operand_count_class(opd)
        ct = count_tree(opd)
        cv = tracer.subst(ct, st.blk, st.idx, frozenset())
        pv = tracer.subst(ptr_tree(opd), st.blk, st.idx, frozenset())
        roots = flatten_union(cv)
        (verdict, reason), needs = judge(cv)
        # head vs residue: does the count come from a residue at all?
        rootkinds = set()
        for r in roots:
            rootkinds |= root_kinds(r)
        fed_by_residue = bool(rootkinds) and rootkinds <= {'residue'}
        pattern = base_class_match(cv, pv)
        if opd['form'] == 'varying':
            pattern = True      # by the lifter's own recognition rule (string_sites.py:1696/2177)
        rows.append({
            'role': role, 'form': opd['form'], 'class': cls,
            'count': show(ct), 'traced': show(cv), 'roots': sorted(rootkinds),
            'fed_by_residue': fed_by_residue, 'verdict': verdict, 'reason': reason,
            'needs': needs, 'pattern': pattern,
        })
    return rows


def root_kinds(v, acc=None):
    if acc is None:
        acc = set()
    v = strip_paren(v)
    k = v[0]
    if k == 'root':
        acc.add(v[1] if v[1] != 'opaque' else 'opaque')
    elif k == 'const':
        acc.add('const')
    elif is_lenload(v):
        acc.add('lenload')
    elif k == 'mem':
        acc.add('mem:' + v[1])
    elif k == 'call':
        for a in v[2]:
            root_kinds(a, acc)
    elif k == 'bin':
        root_kinds(v[2], acc)
        root_kinds(v[3], acc)
    elif k in ('neg', 'not'):
        root_kinds(v[1], acc)
    elif k == 'union':
        for x in v[1]:
            root_kinds(x, acc)
    return acc


# --------------------------------------------------------------------------
# dominators and dominating writes (PROMPT §b)
# --------------------------------------------------------------------------

def routine_entry_blocks(blocks, addrbook):
    """entry name -> its book block (the addrbook entry pc), when present."""
    return {name: '%08X' % addr for addr, name in addrbook if ('%08X' % addr) in blocks}


def dominators(blocks, entry):
    """Cooper-Harvey-Kennedy over the blocks reachable from `entry`."""
    order = []
    seen = set()
    def dfs(n):
        seen.add(n)
        for s in blocks[n].succ:
            if s not in seen:
                dfs(s)
        order.append(n)
    sys.setrecursionlimit(100000)
    dfs(entry)
    rpo = list(reversed(order))
    idx = {n: i for i, n in enumerate(rpo)}
    idom = {entry: entry}
    changed = True
    while changed:
        changed = False
        for n in rpo[1:]:
            preds = [p for p in blocks[n].pred if p in idom]
            if not preds:
                continue
            new = preds[0]
            for p in preds[1:]:
                a, b = p, new
                while a != b:
                    while idx[a] > idx[b]:
                        a = idom[a]
                    while idx[b] > idx[a]:
                        b = idom[b]
                new = a
            if idom.get(n) != new:
                idom[n] = new
                changed = True
    return idom, set(seen)


def dominates(idom, a, b):
    """block a dominates block b?"""
    n = b
    while True:
        if n == a:
            return True
        if idom.get(n, n) == n:
            return False
        n = idom[n]


def frame_word_writes(blocks, routine_blocks):
    """(blk, idx) -> set of frame word offsets DEFINITELY written by that
    statement, plus a separate MAY set: word offsets a by-reference argument or
    a covering copy could write."""
    definite = defaultdict(set)
    may = defaultdict(set)
    for n in routine_blocks:
        for st in blocks[n].stmts:
            if st.kind == 'assign' and st.lhs[0] == 'mem':
                w = st.lhs[1]
                k = word_addr_key(st.lhs[2])
                if k[0] == 'ac3':
                    definite[(n, st.idx)].add(k[1])
                    if w == 'M32':
                        definite[(n, st.idx)].add(k[1] + 1)
            elif st.kind == 'string':
                if st.op == 'WCMV' and st.operands['dst']['form'] == 'dst-varying':
                    k = word_addr_key(st.operands['dst']['addr'])
                    if k[0] == 'ac3':
                        definite[(n, st.idx)].add(k[1])
                # a fixed copy / fill covering the length word: MAY (it writes
                # bytes, which is a length only if the copy targets the varying)
                for role, opd in site_operands(st):
                    if role.startswith('dst') or role.startswith('k'):
                        pass
            elif st.kind in ('rt_call',):
                for a in st.args:
                    k = word_addr_key(a)
                    if k[0] == 'ac3':
                        may[(n, st.idx)].add(k[1])
    return definite, may


# --------------------------------------------------------------------------
# residue liveness (P6)
# --------------------------------------------------------------------------

def stmt_reads(st, reg, observing=True):
    """Does st read `reg`?  With observing=True, a value-independent read
    (`acN = acN` — WMOV n,n — and `acN = sub(acN, acN)` — WSUB n,n, the
    zeroing idiom, whose carry is 1 for every x) does not count."""
    if reg not in st.uses:
        return False
    if observing and st.kind == 'assign' and st.lhs == ('reg', reg):
        r = strip_paren(st.rhs)
        if r == ('reg', reg) or r == ('call', 'sub', [('reg', reg), ('reg', reg)]):
            return False
    return True


def live_after(blocks, st, reg, observing=True):
    """Is `reg` read before being redefined on some path after statement st?
    -> (True, where) | (False, None)"""
    b = blocks[st.blk]
    for j in range(st.idx + 1, len(b.stmts)):
        s2 = b.stmts[j]
        if stmt_reads(s2, reg, observing):
            return True, (st.blk, j)
        if reg in s2.defs:
            return False, None
    seen = set()
    work = deque(b.succ)
    while work:
        n = work.popleft()
        if n in seen:
            continue
        seen.add(n)
        bb = blocks[n]
        stop = False
        for s2 in bb.stmts:
            if stmt_reads(s2, reg, observing):
                return True, (n, s2.idx)
            if reg in s2.defs:
                stop = True
                break
        if not stop:
            work.extend(bb.succ)
    return False, None


# --------------------------------------------------------------------------
# main census
# --------------------------------------------------------------------------

def run(args):
    blocks = load_book(args.book)
    split = load_split(args.blocks)
    addrbook = load_addrbook(args.addrbook)
    ledger = load_ledgers(args.ledgers) if args.ledgers else {}
    unresolved = build_cfg(blocks, split, addrbook)
    IN, OUT = reaching_defs(blocks)
    tracer0 = Tracer(blocks, IN)
    SIN, SIN_DEF, model = slot_reaching_defs(blocks, tracer0, args.call_policy, args.infer_caps)
    tracer = Tracer(blocks, IN, SIN)
    tracer_len = Tracer(blocks, IN, SIN, through_len=True)
    print('slot model: call policy %s; clobber-all events: %s' % (args.call_policy, dict(model.stats)))

    sites = list(string_sites(blocks))
    print('book: %d blocks, %d string sites (WCMV %d, WCMP %d, WBLM %d)' % (
        len(blocks), len(sites), sum(1 for s in sites if s.op == 'WCMV'),
        sum(1 for s in sites if s.op == 'WCMP'), sum(1 for s in sites if s.op == 'WBLM')))
    print('cfg: %d unresolved successor situations' % len(unresolved))
    for u in unresolved[:10]:
        print('   ', u)
    nopred = [n for n, b in blocks.items() if not b.pred]
    entries = routine_entry_blocks(blocks, addrbook)
    entry_set = set(entries.values())
    print('cfg: %d blocks without predecessors; %d of them are addrbook entries; %d are not: %s' % (
        len(nopred), sum(1 for n in nopred if n in entry_set), sum(1 for n in nopred if n not in entry_set),
        ' '.join(n for n in nopred if n not in entry_set)[:400]))

    # ---- per-site analysis
    results = []
    for st in sites:
        rows = analyse_site(st, tracer)
        rows2 = analyse_site(st, tracer_len)
        for r, r2 in zip(rows, rows2):
            r['verdict2'] = r2['verdict']
            r['reason2'] = r2['reason']
            r['roots2'] = r2['roots']
        key = (st.blk, st.text)
        led = ledger.get(key)
        results.append((st, rows, led))

    # ---- path sensitivity (a001 Q3), reported separately
    owner = entry_dom_trees(blocks)
    guards = Guards(blocks, tracer, owner)
    gcount = defaultdict(int)
    for st, rows, led in results:
        facts = None
        for r in rows:
            r['guard'] = ''
            if r['verdict'] in ('unknown', 'cond'):
                if facts is None:
                    facts = guards.facts_at(st.blk, st.idx)
                v = tracer.subst(count_tree(dict(site_operands(st))[r['role']]), st.blk, st.idx, frozenset())
                ok, why = guards.prove_nonneg(v, facts)
                if ok:
                    r['guard'] = why
                    gcount[(r['class'], r['verdict'])] += 1
    print('\n=== Q3. Dominating-guard rule: operands it settles (from unknown / cond) ===')
    for k, v in sorted(gcount.items()):
        print('  %-14s %-8s %4d' % (k[0], k[1], v))

    # ---- write the ledger
    if args.out:
        with open(args.out, 'w') as f:
            f.write('# docs/Project58/sites.tsv — compiler/countflow.py census of every string count operand\n')
            f.write('# block\tstmt\tpc\top\troutine\tidiom\trole\tform\tclass\tfed_by_residue\tpattern\tverdict\troots\tcount\treason\tverdict_through_len\troots_through_len\treason_through_len\tguard\n')
            for st, rows, led in results:
                for r in rows:
                    f.write('\t'.join([
                        st.blk, str(st.idx), led['pc'] if led else '-', st.op,
                        blocks[st.blk].routine or '-', led['idiom'] if led else '-',
                        r['role'], r['form'], r['class'], 'Y' if r['fed_by_residue'] else 'N',
                        'Y' if r['pattern'] else 'N', r['verdict'], ','.join(r['roots']),
                        r['count'][:120], r['reason'][:200], r['verdict2'], ','.join(r['roots2']), r['reason2'][:200], r['guard'][:160]]) + '\n')
        print('wrote', args.out)

    # ---- the assert site list
    if args.asserts:
        write_asserts(args.asserts, blocks, results)

    # ---- summaries
    summarise(blocks, results)
    dominating_write_pass(blocks, results, tracer, SIN, SIN_DEF)
    residue_liveness(blocks, sites)

    if args.dump_site:
        for st, rows, led in results:
            if led and led['pc'] == args.dump_site.upper() or st.blk == args.dump_site.upper():
                print('\n== site', led['pc'] if led else st.blk, st.text)
                for r in rows:
                    for k2, v2 in r.items():
                        print('   %-14s %s' % (k2, v2))


def assert_text(st, role, opd, tier, pc):
    """The exact IR statement that discharges one operand at runtime
    (IR.md 3: assert(e, "message"); 5.1: `>=s` is the signed comparison)."""
    ct = count_tree(opd)
    return 'assert((%s) >=s 0, "P58 %s %s @%s")' % (show(ct) if opd['form'] != 'varying' else 'sx16(M16[%s])' % show(opd['addr']), tier, role.replace('/', '-'), pc)


def write_asserts(path, blocks, results):
    n = 0
    with open(path, 'w') as f:
        f.write('# docs/Project58/asserts.tsv — every string count operand NOT proven non-negative by construction,\n')
        f.write('# with the exact IR assert that discharges it at runtime.  Emit it immediately BEFORE the statement\n')
        f.write('# (same block; the count expression is evaluated in the statement\'s own context, IR.md 5.8).\n')
        f.write('# tier: base-class = a varying read (count = length word at A, data at A+1); cond = non-negative iff the\n')
        f.write('#       named length word(s) are; guard = proven by a dominating guard (no assert NEEDED, listed for completeness);\n')
        f.write('#       unknown = the tool could not decide (arithmetic / opaque / interprocedural); the assert still discharges it.\n')
        f.write('# pc\tblock\tstmt\top\troutine\trole\tclass\ttier\tneeded\tassert\n')
        for st, rows, led in results:
            pc = led['pc'] if led else st.blk
            opds = dict(site_operands(st))
            for r in rows:
                if r['verdict'] == 'yes':
                    continue
                if r.get('guard'):
                    tier, needed = 'guard', 'no'
                elif r['pattern']:
                    tier, needed = 'base-class', 'yes'
                elif r['verdict'] == 'cond':
                    tier, needed = 'cond', 'yes'
                else:
                    tier, needed = 'unknown', 'yes'
                f.write('\t'.join([pc, st.blk, str(st.idx), st.op, blocks[st.blk].routine or '-', r['role'], r['class'],
                                    tier, needed, assert_text(st, r['role'], opds[r['role']], tier, pc)]) + '\n')
                n += 1
    print('wrote %s (%d operand rows)' % (path, n))


def summarise(blocks, results):
    print('\n=== 1. Site classification by count operands (textual, from the IR) ===')
    bysite = defaultdict(int)
    byopd = defaultdict(int)
    for st, rows, led in results:
        classes = tuple(sorted(r['class'] for r in rows))
        bysite[(st.op, classes)] += 1
        for r in rows:
            byopd[(st.op, r['role'], r['class'])] += 1
    for k in sorted(bysite):
        print('  %-5s %-32s %5d' % (k[0], '+'.join(k[1]), bysite[k]))
    print('\n  per operand:')
    for k in sorted(byopd):
        print('  %-5s %-8s %-14s %5d' % (k[0], k[1], k[2], byopd[k]))

    # P58-style buckets: both const | any register | any varying | other
    print('\n=== 1b. The PROMPT\'s four buckets, recomputed per site ===')
    b = defaultdict(int)
    for st, rows, led in results:
        if st.op == 'WBLM':
            b['WBLM'] += 1
            continue
        cl = [r['class'] for r in rows]
        if all(c == 'const' for c in cl):
            b['both constant'] += 1
        elif 'register' in cl:
            b['at least one register'] += 1
        elif 'varying' in cl:
            b['a varying length (source read)'] += 1
        else:
            b['other (literal counts / expressions)'] += 1
    for k, v in sorted(b.items(), key=lambda kv: -kv[1]):
        print('  %-40s %5d' % (k, v))

    print('\n=== 3. Chain heads vs residue-fed (register operands only) ===')
    reg_rows = [(st, r, led) for st, rows, led in results for r in rows if r['class'] == 'register']
    fed = [x for x in reg_rows if x[1]['fed_by_residue']]
    print('  register operands: %d; fed ONLY by string residues: %d; chain heads (some non-residue root): %d' % (
        len(reg_rows), len(fed), len(reg_rows) - len(fed)))
    rk = defaultdict(int)
    for st, r, led in reg_rows:
        rk[tuple(r['roots'])] += 1
    for k, v in sorted(rk.items(), key=lambda kv: -kv[1]):
        print('    %-60s %4d' % ('+'.join(k), v))
    print('\n  residue-fed register operands by (op, role, residue op):')
    rr = defaultdict(int)
    for st, r, led in fed:
        rr[(st.op, r['role'], r['reason'][:22])] += 1
    for k, v in sorted(rr.items()):
        print('    %-5s %-8s %-24s %4d' % (k[0], k[1], k[2], v))

    print('\n=== 5/6/7. Verdicts per operand ===')
    vd = defaultdict(int)
    for st, rows, led in results:
        for r in rows:
            vd[(r['class'], r['verdict'])] += 1
    for k in sorted(vd):
        print('  %-14s %-8s %5d' % (k[0], k[1], vd[k]))
    print('\n  verdicts when length words are TRACED THROUGH their writes (the induction):')
    vd2 = defaultdict(int)
    for st, rows, led in results:
        for r in rows:
            vd2[(r['class'], r['verdict'], r['verdict2'])] += 1
    for k in sorted(vd2):
        print('  %-14s %-8s -> %-8s %5d' % (k[0], k[1], k[2], vd2[k]))
    print('\n  per SITE (worst operand):')
    order = {'yes': 0, 'cond': 1, 'unknown': 2, 'no': 3}
    sv = defaultdict(int)
    for st, rows, led in results:
        w = max((r['verdict'] for r in rows), key=lambda x: order[x])
        sv[(st.op, w)] += 1
    for k in sorted(sv):
        print('  %-5s %-8s %5d' % (k[0], k[1], sv[k]))

    print('\n=== 6. Base-class pattern (count = sx16(M16[A]), pointer = byte 2A+2) ===')
    pat = defaultdict(int)
    for st, rows, led in results:
        for r in rows:
            if r['pattern']:
                pat[(st.op, r['role'], r['form'], r['class'])] += 1
    tot = 0
    for k in sorted(pat):
        print('  %-5s %-8s %-12s %-12s %5d' % (k[0], k[1], k[2], k[3], pat[k]))
        tot += pat[k]
    print('  total operands matching: %d' % tot)
    varsites = [st for st, rows, led in results if any(r['form'] == 'varying' for r in rows)]
    print('  sites with a `[@A, varying]` read: %d' % len(varsites))
    # varying reads by address shape
    shape = defaultdict(int)
    for st, rows, led in results:
        for role, opd in site_operands(st):
            if opd['form'] == 'varying':
                shape[addr_shape(opd['addr'])] += 1
    print('  varying reads by address shape:')
    for k, v in sorted(shape.items(), key=lambda kv: -kv[1]):
        print('    %-40s %4d' % (k, v))

    print('\n=== 7. The residue: operands NOT proven and NOT the base class ===')
    res = [(st, r, led) for st, rows, led in results for r in rows
           if r['verdict'] in ('unknown', 'no') or (r['verdict'] == 'cond' and not r['pattern'])]
    kinds = defaultdict(int)
    for st, r, led in res:
        kinds[(r['class'], r['verdict'], '+'.join(r['roots']))] += 1
    for k, v in sorted(kinds.items(), key=lambda kv: -kv[1]):
        print('  %-14s %-8s %-50s %4d' % (k[0], k[1], k[2], v))
    print('  residue operands: %d at %d sites' % (len(res), len({st.blk + str(st.idx) for st, r, led in res})))
    print('\n  unknown/no operands, listed:')
    for st, r, led in res:
        if r['verdict'] in ('unknown', 'no'):
            print('    %s %-8s %-5s %-8s %-22s %s' % (led['pc'] if led else st.blk, blocks[st.blk].routine, st.op, r['role'],
                                                       (led['idiom'] if led else '-')[:22], r['reason'][:110]))


def addr_shape(e):
    e = strip_paren(e)
    if e[0] == 'call' and e[1] == 'wp':
        base = strip_paren(e[2][0])
        d = strip_paren(e[2][1])
        if base[0] == 'reg' and d[0] == 'const':
            return 'wp(%s, k) frame slot' % base[1] if base[1] == 'ac3' else 'wp(%s, k)' % base[1]
        return 'wp(...)'
    if e[0] == 'const':
        return 'static word %s' % show(e)
    if e[0] == 'mem' and e[1] == 'R':
        return 'R[...] by-reference argument'
    if e[0] == 'bin':
        s = show(e)
        if '0x70000210' in s:
            return 'SD_PTR record field (+ arithmetic)'
        if '0x70000212' in s:
            return 'OBJ_PTR record field (+ arithmetic)'
        if '0x700007A0' in s:
            return 'PLAYER cache field (+ arithmetic)'
        return 'arithmetic: ' + s[:30]
    return show(e)[:40]


def dominating_write_pass(blocks, results, tracer, SIN, SIN_DEF):
    """PROMPT (b): for every `[@wp(ac3, k), varying]` read of the routine's own
    frame, what reaches its length word?  Uses the slot model's reaching
    definitions: an `entry` among them means a path from the routine entry
    with no write (definite or may) of word k."""
    print('\n=== 7b. Varying reads of FRAME slots: what reaches the length word? (slot model) ===')
    rows = []
    for st, srows, led in results:
        for role, opd in site_operands(st):
            if opd['form'] != 'varying':
                continue
            k = word_addr_key(opd['addr'])
            if k[0] != 'ac3':
                continue
            if not tracer.is_frame(('reg', 'ac3'), st.blk, st.idx, frozenset()):
                rows.append((st, role, k[1], 'ac3-not-frame', ''))
                continue
            # definite effects only: an entry here = a path with no definite write
            ddefs = defs_at(blocks, SIN_DEF, st.blk, st.idx, ('slot', k[1]), definite=True)
            # all effects: an entry here = a path with no write of any kind, not even a may
            adefs = defs_at(blocks, SIN, st.blk, st.idx, ('slot', k[1]))
            why = []
            for d in ddefs:
                if d[0] != 'entry':
                    eff = blocks[d[0]].stmts[d[1]].sdefs[k[1]]
                    why.append(('write%d' % eff[2] if eff[0] == 'write' else eff[1][:36]) + '@%s:%d' % d)
            d_entry = any(d[0] == 'entry' for d in ddefs)
            a_entry = any(d[0] == 'entry' for d in adefs)
            if not d_entry:
                status = 'DEFINITE write on every path'
            elif not a_entry:
                status = 'definite write missing on some path; a MAY-write covers it'
            elif all(d[0] == 'entry' for d in adefs):
                status = 'NO write of any kind reaches on any path'
            else:
                status = 'no write of any kind on SOME path'
            rows.append((st, role, k[1], status, ' '.join(why)[:100]))
    c = defaultdict(int)
    for st, role, w, status, why in rows:
        c[status] += 1
    for k, v in sorted(c.items(), key=lambda kv: -kv[1]):
        print('  %-70s %4d' % (k, v))
    print('  (%d frame-slot varying reads in all)' % len(rows))
    print('\n  reads with an UNWRITTEN path (routine, block, role, slot word, status, reaching writes):')
    for st, role, w, status, why in rows:
        if not status.startswith('DEFINITE'):
            print('    %-22s %s %-7s wp(ac3,%d) %s | %s' % (blocks[st.blk].routine, st.blk, role, w, status[:40], why))


def reader_shape(st, reg):
    if st.kind == 'string':
        for role, opd in site_operands(st):
            if opd['count'] is not None and strip_paren(opd['count']) == ('reg', reg):
                return 'string %s count %s' % (st.op, role)
            if reg in regs_in(opd['addr']):
                return 'string %s address %s' % (st.op, role)
        return 'string %s' % st.op
    if st.kind == 'goto':
        return 'goto test'
    if st.kind == 'assign':
        if st.lhs[0] == 'mem':
            if reg in regs_in(st.lhs):
                return 'store address %s[...]' % st.lhs[1]
            return 'stored: %s = %s' % (show(st.lhs)[:28], show(st.rhs)[:28])
        return 'assign: %s = %s' % (show(st.lhs), show(st.rhs)[:40])
    if st.kind == 'rt_call':
        return 'rt_call %s argument' % st.callee
    return st.kind


def residue_liveness(blocks, sites):
    print('\n=== P6. Residues live after string statements ===')
    c = defaultdict(int)
    detail = defaultdict(list)
    readers = defaultdict(lambda: defaultdict(int))
    for st in sites:
        for reg in sorted(st.defs):
            live_syn, _ = live_after(blocks, st, reg, observing=False)
            live, where = live_after(blocks, st, reg)
            c[(st.op, reg, 'live' if live else 'dead')] += 1
            if live_syn and not live:
                c[(st.op, reg, 'read-but-not-observed')] += 1
            if live:
                detail[(st.op, reg)].append((st.blk, st.idx, where))
                rd = blocks[where[0]].stmts[where[1]]
                readers[(st.op, reg)][reader_shape(rd, reg)] += 1
    for k in sorted(c):
        print('  %-5s %-4s %-22s %5d' % (k[0], k[1], k[2], c[k]))
    print('\n  reader shapes (first observing read on some path):')
    for k in sorted(readers):
        for shape, n in sorted(readers[k].items(), key=lambda kv: -kv[1]):
            print('    %-5s %-4s %-60s %4d' % (k[0], k[1], shape, n))
    for k in sorted(detail):
        if k[0] == 'WCMP' or len(detail[k]) <= 40:
            print('  live %s %s at:' % k, ' '.join('%s:%d->%s:%d' % (b, i, w[0], w[1]) for b, i, w in detail[k])[:1500])
    # what reads a WCMP's ac1?  (should be the goto test only)
    print('\n  WCMP ac1 readers by statement kind:')
    kinds = defaultdict(int)
    for st in sites:
        if st.op != 'WCMP':
            continue
        live, where = live_after(blocks, st, 'ac1')
        if live:
            rd = blocks[where[0]].stmts[where[1]]
            kinds[(rd.kind, rd.text[:40])] += 1
    for k, v in sorted(kinds.items(), key=lambda kv: -kv[1]):
        print('    %-8s %-42s %3d' % (k[0], k[1], v))


# --------------------------------------------------------------------------
# self-test
# --------------------------------------------------------------------------

SELFTEST_BOOK = '''ir 7
mode book

block 70000000 seg 0x70000000
  @70000000 WSAVS 0x0000 ; entry
  ac3 = wfp ; LDAFP 3;
  ac0 = 0x00000005 ; NLDAI 5,0;
  ac1 = ac0 ; WMOV 0,1;
  goto [70000010, 70000020] (ac0 >s 0) ; test

block 70000010 seg 0x70000000
  ac1 = sx16(M16[wp(ac3, 12)]) ; XNLDA 1,[ac3+0xC];
  goto [70000030] 0 ; join

block 70000020 seg 0x70000000
  ac1 = sub(ac1, 0x00000009) ; WSBI
  goto [70000030] 0 ; join

block 70000030 seg 0x70000000
  [@bp(ac3, 100), ac0] = [@bp(ac3, 26), ac1] ; WCMV;
  ac3 = wfp ; LDAFP 3;
  [@ac2, ac1] = [@wp(ac3, 40), varying] ; WCMV;
  ac1 = cmp([@wp(ac3, 50), varying], [@0x70000100:0, "abc"]) ; WCMP;
  goto [70000040, 70000050] (ac1 != 0) ; WSNE

block 70000040 seg 0x70000000
  [@bp(ac3, 200), ac0] = [@bp(ac3, 300), ac1] ; WCMV;
  ret

block 70000050 seg 0x70000000
  ac0 = ac1 ; WMOV 1,0;
  goto [70000030] 0 ; loop

blocks 6
'''


def closure_check():
    """Transcription of EagleString.cpp copy() :60-90 and residues_after_copy()
    :152-163 (and the WCMP loop :167-202), run over every pair of counts in
    [-6, 6] and every source length.  Corroborates, by exhaustion over small
    operands, the closure lemma of q001 2: after WCMV ac0 == 0 and
    (len >= 0) => (ac1 >= 0); after WCMP (n >= 0) => (0 <= ac0 <= n) and
    ac1 in {-1, 0, 1}.  This is a transcription, not the C++ itself."""
    def direction(cnt):
        return -1 if cnt > 0 else (1 if cnt < 0 else 0)
    def sgn(v):
        return 1 if v > 0 else (-1 if v < 0 else 0)
    bad = 0
    for n in range(-6, 7):
        for ln in range(-6, 7):
            # the loop
            dst_count, src_count = n, ln
            dd, sd = direction(dst_count), direction(src_count)
            dst, src = 1000, 2000
            fetched = 0
            while dst_count != 0:
                if src_count != 0:
                    src_count += sd
                    src -= sd
                    fetched += 1
                dst_count += dd
                dst -= dd
            # the residue rule
            t = min(abs(n), abs(ln))
            r_ac0, r_ac1, r_ac3, r_ac2 = 0, ln - sgn(ln) * t, 2000 + sgn(ln) * t, 1000 + n
            if (dst_count, src_count, src, dst) != (r_ac0, r_ac1, r_ac3, r_ac2):
                bad += 1
            if ln >= 0 and r_ac1 < 0:
                bad += 1
            # WCMP over blank-equal bytes: the loop runs until both counts hit 0
            dc, sc = n, ln
            dd, sd = direction(dc), direction(sc)
            while dc != 0 or sc != 0:
                if sc != 0:
                    sc += sd
                if dc != 0:
                    dc += dd
            if n >= 0 and not (0 <= dc <= n):
                bad += 1
    return bad


def selftest():
    assert closure_check() == 0
    import tempfile
    d = tempfile.mkdtemp()
    p = os.path.join(d, 'book')
    open(p, 'w').write(SELFTEST_BOOK)
    blocks = load_book(p)
    unresolved = build_cfg(blocks, {}, [(0x70000000, 'T')])
    assert not unresolved, unresolved
    IN, OUT = reaching_defs(blocks)
    tr = Tracer(blocks, IN)
    sites = list(string_sites(blocks))
    assert len(sites) == 4
    # site 1: dst count ac0 -> const 5 on the first visit, and the loop
    # brings ac0 = ac1 = cmp result back in via 70000050.
    rows = analyse_site(sites[0], tr)
    assert rows[0]['class'] == 'register'
    assert 'const' in rows[0]['roots'] and 'residue' in rows[0]['roots'], rows[0]
    assert rows[0]['verdict'] == 'no', rows[0]           # WCMP's ac1 feeds ac0 round the loop
    # src count ac1: a length load on one arm, a subtraction of a copy of 5 on the other
    # ...and round the loop the untouched ac1 is cmp's result -> 'no'
    assert rows[1]['verdict'] == 'no', rows[1]
    assert 'lenload' in rows[1]['roots'] and 'const' in rows[1]['roots'] and 'residue' in rows[1]['roots'], rows[1]
    assert not rows[1]['fed_by_residue']
    assert rows[1]['pattern'] is False
    # site 2: `[@ac2, ac1] = [@wp(ac3,40), varying]` — ac1 is the WCMV residue
    # of site 1 (closure), the source is the base class by form
    rows = analyse_site(sites[1], tr)
    assert rows[0]['fed_by_residue'] and rows[0]['verdict'] == 'cond', rows[0]
    assert rows[1]['form'] == 'varying' and rows[1]['pattern'] and rows[1]['verdict'] == 'cond'
    # site 4: after the cmp, ac0 is cmp's residue (cond), ac1 is the result (no)
    rows = analyse_site(sites[3], tr)
    assert rows[0]['verdict'] == 'cond' and rows[0]['fed_by_residue'], rows[0]
    assert rows[1]['verdict'] == 'no', rows[1]
    assert tr.trace_reg('ac3', '70000000', 0) == ('sreg', 'wfp')
    # liveness: site 1's ac2 is read by site 2 (the append cursor), its ac3 is
    # overwritten by LDAFP first
    assert live_after(blocks, sites[0], 'ac2')[0]
    assert not live_after(blocks, sites[0], 'ac3')[0]
    # invariants: every reaching def in an IN set defines that register
    for n in blocks:
        for r in REGS:
            for dd in IN[n][r]:
                if dd[0] != 'entry':
                    assert r in blocks[dd[0]].stmts[dd[1]].defs
    # dominators: 70000030 is dominated by 70000000, not by 70000010
    idom, reach = dominators(blocks, '70000000')
    assert dominates(idom, '70000000', '70000030') and not dominates(idom, '70000010', '70000030')
    # tokenizer edges
    assert Parser('M32[0x70000210] - 0x280').expr()[0] == 'bin'
    assert Parser('wp(ac3, -12)').expr()[2][1] == ('const', -12)
    assert Parser('R[ac3 + -12]').expr()[2][3] == ('const', -12)
    lit = Parser('[@0x7015BD9B:0, "a\\x0Bb"]').piece()
    assert lit['count'] == ('const', 3), lit
    print('selftest OK')


def main():
    ap = argparse.ArgumentParser()
    here = os.path.dirname(os.path.abspath(__file__))
    work = os.path.dirname(here)
    ap.add_argument('--book', default=os.path.join(work, 'emulation/quest.ir2.book'))
    ap.add_argument('--blocks', default=os.path.join(work, 'emulation/quest.blocks.split'))
    ap.add_argument('--addrbook', default=os.path.join(work, 'emulation/quest.addrbook'))
    ap.add_argument('--ledgers', nargs='*', default=[os.path.join(work, 'docs/Project%d/p%d.tsv' % (n, n)) for n in (31, 32, 33)])
    ap.add_argument('--out', default=None)
    ap.add_argument('--dump-site', default=None)
    ap.add_argument('--asserts', default=None, help='write the assert site list (a001: a deliverable in its own right)')
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--call-policy', default='upward', choices=['upward', 'wide'])
    ap.add_argument('--infer-caps', action='store_true', help='a001 Q2 policy (a): bound unbounded copies by the largest constant copy into the buffer')
    args = ap.parse_args()
    if args.selftest:
        selftest()
        return
    run(args)


if __name__ == '__main__':
    main()
