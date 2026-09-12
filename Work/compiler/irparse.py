#!/usr/bin/env python3
"""
compiler/irparse.py — Project 56.

One parser for both sides of the match: ir 7 (the book) and ir 8 (ours).
Statement-level only; nothing here executes and nothing here compares.

Shared by `ldafp_census.py` and `irmatch.py`.

The register def/use model is the part that carries weight, so it is stated
explicitly rather than inferred from the text:

  * A string statement (IR.md 5.8) writes ac0..ac3 and does NOT name them.
    `assign_fixed` / `assign_varying` leave ac0..ac3 + c; `compare` leaves
    ac0..ac3; `block_move` leaves ac1..ac3.  We take the union and call every
    string statement a definition of ac0..ac3.  This is P56 q001 4.1's binding
    clobber rule: an analysis that reads register facts off the statement TEXT
    is wrong here, and wrong in a way that still matches.

  * `call` / `rt_call` PRESERVE ac0..ac3.  The callee's WSAVS saves
    `ac0 ac1 ac2 wfp ac3|c` and WRTN restores them (IR.md 6, EagleStack.cpp
    :421-425).  Measured rather than assumed: see ldafp_census.py 4.
"""

import re
import bisect
from collections import OrderedDict

REGS = ('ac0', 'ac1', 'ac2', 'ac3')
STRING_CLOBBER = frozenset(REGS) | {'c'}


# --------------------------------------------------------------------------
# statements
# --------------------------------------------------------------------------

class Stmt:
    __slots__ = ('text', 'kind', 'defs', 'uses', 'base_uses', 'labels', 'ret',
                 'raw_op', 'callee')

    def __init__(self, text):
        self.text = text
        self.kind = None
        self.defs = set()        # registers written
        self.uses = set()        # registers read, EXCLUDING wp/bp bases
        self.base_uses = []      # (reg, disp_text, 'wp'|'bp') -- frame-base candidates
        self.labels = []         # successor labels, if a terminator
        self.ret = None          # ret= target, if a call
        self.raw_op = None       # opcode, if an unlifted instruction
        self.callee = None       # callee name, if a call

    def is_terminator(self):
        return self.kind in ('ret', 'goto', 'call', 'rt_call', 'raw_terminator')

    def __repr__(self):
        return '<%s %r>' % (self.kind, self.text[:48])


class Block:
    __slots__ = ('name', 'seg', 'stmts')

    def __init__(self, name, seg=None):
        self.name = name
        self.seg = seg
        self.stmts = []

    def __len__(self):
        return len(self.stmts)


# --------------------------------------------------------------------------
# expression scanning
# --------------------------------------------------------------------------

_REG_RE = re.compile(r'\bac[0-3]\b')
# wp(base, disp) / bp(base, disp) where base is a register
_BASE_RE = re.compile(r'\b(wp|bp)\(\s*(ac[0-3])\s*,\s*([^()]*?)\s*\)')
# the RAW index form: `acN + <literal>` inside a memory index or an R[].
# `R[ac3 + -12]` is argument slot 1 (wfp-10-2N) and is every bit as
# frame-relative as `wp(ac3, -12)`; Indirection.md 2 counts 939 of them.
# An `acN` added to anything that is NOT a literal is arithmetic on the value.
_RAWIDX_RE = re.compile(
    r'(?:R|M8|M16|M32)\[\s*(ac[0-3])\s*\+\s*(-?(?:0x)?[0-9A-Fa-f]+)\s*\]')


def scan_expr(expr, st):
    """Record register reads in `expr`, separating frame-base uses from the rest.

    A "base use" is a spelling N2 rewrites: wp/bp with a register base, or the
    raw `R[acN + lit]` / `M**[acN + lit]` index form.  Everything else that
    mentions a register is a VALUE read -- which is what blocks N3.
    """
    for m in _BASE_RE.finditer(expr):
        kind, reg, disp = m.group(1), m.group(2), m.group(3)
        st.base_uses.append((reg, disp, kind))
        # a register appearing in the DISPLACEMENT is an ordinary use
        for r in _REG_RE.findall(disp):
            st.uses.add(r)
    for m in _RAWIDX_RE.finditer(expr):
        st.base_uses.append((m.group(1), m.group(2), 'idx'))
    # blank out base occurrences so they are not double-counted as value reads
    rest = _BASE_RE.sub(lambda m: '%s(<base>,%s)' % (m.group(1), m.group(3)), expr)
    rest = _RAWIDX_RE.sub('<idx>', rest)
    for r in _REG_RE.findall(rest):
        st.uses.add(r)


_RAW_RE = re.compile(r'^@([0-9A-F]{8})\s+(\S+)')
_GOTO_RE = re.compile(r'^goto \[([^\]]*)\]\s*(.*)$')
_CALL_RE = re.compile(r'^call (\S+)\s*(.*)$')
_RTCALL_RE = re.compile(r'^rt_call\s+(\S+?)\((.*)\)\s*(.*)$')
_LOCATED_RE = re.compile(r'^\[@')
_WORDS_RE = re.compile(r'^words\(')
_RET_FIELD_RE = re.compile(r'\bret=(\S+)')
# raw instructions that END a block (a call or a return in the machine text)
_RAW_TERM = ('LCALL', 'XCALL', 'LJSR', 'WRTN', 'SYSCALL')


def classify(text):
    """Turn one statement's text into a Stmt with defs/uses filled in."""
    st = Stmt(text)
    s = text.strip()

    m = _RAW_RE.match(s)
    if m:
        st.kind = 'raw'
        st.raw_op = m.group(2).rstrip(';,')
        # an unlifted call/return terminates its block
        if st.raw_op in _RAW_TERM:
            st.kind = 'raw_terminator'
        # unlifted instruction: we do not model its registers.  Conservative:
        # a raw call clobbers nothing (WSAVS/WRTN), anything else is opaque.
        return st

    if s == 'ret' or s.startswith('ret '):
        st.kind = 'ret'
        return st

    m = _GOTO_RE.match(s)
    if m:
        st.kind = 'goto'
        st.labels = [x.strip() for x in m.group(1).split(',') if x.strip()]
        scan_expr(m.group(2), st)
        return st

    m = _RTCALL_RE.match(s)
    if m:
        st.kind = 'rt_call'
        st.callee = m.group(1).lstrip('?')
        scan_expr(m.group(2), st)
        r = _RET_FIELD_RE.search(m.group(3))
        if r:
            st.ret = r.group(1)
            st.labels = [st.ret]
        return st

    m = _CALL_RE.match(s)
    if m:
        st.kind = 'call'
        st.callee = m.group(1)
        r = _RET_FIELD_RE.search(m.group(2))
        if r:
            st.ret = r.group(1)
            st.labels = [st.ret]
        return st

    if s.startswith('assert('):
        st.kind = 'assert'
        scan_expr(s, st)
        return st

    # string statements: ac1 = cmp(...), [@a,n] = piece, words(..) = words(..)
    if _LOCATED_RE.match(s) or _WORDS_RE.match(s) or re.match(r'^ac1 = cmp\(', s):
        st.kind = 'string'
        scan_expr(s, st)
        st.defs |= STRING_CLOBBER          # IR.md 5.8 residues -- the binding rule
        return st

    if '=' in s:
        lhs, rhs = s.split('=', 1)
        lhs, rhs = lhs.strip(), rhs.strip()
        st.kind = 'assign'
        if re.fullmatch(r'ac[0-3]|c|t\d+', lhs):
            st.defs.add(lhs)
        else:
            scan_expr(lhs, st)             # M32[...] = ... : the index is a USE
        scan_expr(rhs, st)
        return st

    st.kind = 'other'
    scan_expr(s, st)
    return st


# --------------------------------------------------------------------------
# file loading
# --------------------------------------------------------------------------

def _strip(line):
    """Drop the provenance / note tail after ' ; ' (IR.md 3: audit trail only)."""
    i = line.find(' ; ')
    return (line[:i] if i >= 0 else line).strip()


def load_book(path):
    blocks = OrderedDict()
    cur = None
    for line in open(path):
        line = line.rstrip('\n')
        m = re.match(r'^block ([0-9A-F]+)(?:\s+seg\s+(\S+))?', line)
        if m:
            cur = Block(m.group(1), m.group(2))
            blocks[cur.name] = cur
            continue
        if cur is None or not line.strip():
            continue
        if line.lstrip().startswith(';'):
            continue
        t = _strip(line)
        if t:
            cur.stmts.append(classify(t))
    return blocks


def load_ours(path):
    blocks = OrderedDict()
    cur = None
    for line in open(path):
        line = line.rstrip('\n')
        m = re.match(r'^block (\S+)', line)
        if m:
            cur = Block(m.group(1))
            blocks[cur.name] = cur
            continue
        if line.startswith(('ir ', 'mode ', 'v ', 'a ', 's ', 'blocks ')):
            continue
        if cur is None or not line.strip():
            continue
        if line.lstrip().startswith(';'):
            continue
        t = _strip(line)
        if t:
            cur.stmts.append(classify(t))
    return blocks


def load_decls(path):
    """`v`/`a` declarations from one of our .ir files: name -> (class, vtype)."""
    out = OrderedDict()
    for line in open(path):
        m = re.match(r'^([va])\s+(\S+)\s+(.*?)\s*$', line.rstrip('\n'))
        if m:
            out[m.group(2)] = (m.group(1), m.group(3))
    return out


# --------------------------------------------------------------------------
# addrbook
# --------------------------------------------------------------------------

class Entry:
    __slots__ = ('addr', 'name', 'alloc_base', 'wfp', 'argc', 'frame', 'end')

    def __repr__(self):
        return '<%s @%08X wfp=%08X argc=%d frame=0x%02X>' % (
            self.name, self.addr, self.wfp, self.argc, self.frame)

    def arg_cell(self, n):
        """Argument n lives at wfp-10-2n (IR.md 6)."""
        return self.wfp - 10 - 2 * n

    def marker_cell(self):
        return self.arg_cell(self.argc) + 2 if self.argc else self.wfp - 8


def load_addrbook(path):
    entries = OrderedDict()
    for line in open(path):
        if line.startswith('#') or not line.strip():
            continue
        f = line.split()
        if len(f) < 6 or not re.fullmatch(r'[0-9A-F]{8}', f[0]):
            continue
        e = Entry()
        e.addr = int(f[0], 16)
        e.name = f[1]
        e.alloc_base = int(f[2], 16)
        e.wfp = int(f[3], 16)          # column 4 IS wfp -- q001 4
        e.argc = int(f[4])
        e.frame = int(f[5], 16)
        e.end = None
        entries[e.name] = e
    # end address = the next entry by address
    byaddr = sorted(entries.values(), key=lambda e: e.addr)
    for i, e in enumerate(byaddr):
        e.end = byaddr[i + 1].addr if i + 1 < len(byaddr) else 0x7FFFFFFF
    return entries


def routine_blocks(book, entry):
    """The book blocks belonging to one addrbook entry, in address order."""
    out = OrderedDict()
    for name, blk in book.items():
        a = int(name, 16)
        if entry.addr <= a < entry.end:
            out[name] = blk
    return out


def owner_index(entries):
    """-> (sorted addrs, {addr: name}) for attributing a block to a routine."""
    byaddr = sorted(entries.values(), key=lambda e: e.addr)
    return [e.addr for e in byaddr], {e.addr: e.name for e in byaddr}


def owner_of(addr, addrs, names):
    i = bisect.bisect_right(addrs, addr) - 1
    return names[addrs[i]] if i >= 0 else None


# --------------------------------------------------------------------------
# CFG
# --------------------------------------------------------------------------

def successors(blocks, name):
    """Successor block names, resolved within `blocks`.  Falls through when a
    block's last statement is not a terminator (an unlifted instruction that
    is not a call/return leaves the block open)."""
    blk = blocks[name]
    order = list(blocks)
    if not blk.stmts:
        i = order.index(name)
        return [order[i + 1]] if i + 1 < len(order) else []
    last = blk.stmts[-1]
    labs = [l for l in last.labels if l in blocks]
    if labs:
        return labs
    if last.kind in ('ret',):
        return []
    if last.kind == 'raw_terminator' and getattr(last, 'raw_op', '') == 'WRTN':
        return []
    # unresolved call target / open block -> fall through in listing order
    i = order.index(name)
    return [order[i + 1]] if i + 1 < len(order) else []


def predecessors(blocks):
    preds = {n: [] for n in blocks}
    for n in blocks:
        for s in successors(blocks, n):
            preds[s].append(n)
    return preds
