#!/usr/bin/env python3
"""
string_sites.py — Project 29 (strings, Phase A) census tool.

Text-only over quest.dis, quest.blocks.split, quest.tags, quest.mem,
quest.symbols.  For every WCMV / WCMP / WBLM / WMSP / STASP / pass-by-
reference WPSH site it reports the block, the setup window (the
instructions that produced ac0..ac3, found by a forward symbolic
evaluation of the block with dependency tracking), and a PROVENANCE
class for each of the four operands:

    literal   XLEFB/LLEFB of a pc-relative or absolute constant into the
              code segment (text decoded from quest.mem)
    frame     [fp+d] local (byte pointer into the frame, or a frame word)
    static    absolute address in the data segment
    temp      a WMSP-claimed stack area (tied to its claim)
    computed  anything else (the symbolic expression is printed)

Nothing here touches the emulator or any artifact it reads.

Usage (from Work/c_src/tools):
  python3 string_sites.py --dis ../../../Disassembled/quest.dis \
      --blocks ../quest.blocks.split --tags ../../../Disassembled/quest.tags \
      --mem ../../../Disassembled/quest.mem \
      --symbols ../../../Disassembled/quest.symbols \
      --sites ../../docs/Project29/sites.txt \
      --strings ../../docs/Project29/quest.strings \
      --census ../../docs/Project29/census_raw.txt
"""

import argparse
import collections
import hashlib
import re
import sys
import time

# ----------------------------------------------------------------------
# Byte-EA conventions (Project25/ByteEA.md §2, DISASSEMBLER_BYTE_OPERANDS.md)
# ----------------------------------------------------------------------

def sext16(v):
    return v - 0x10000 if v & 0x8000 else v

def sext15(v):
    v &= 0x7FFF
    return v - 0x8000 if v & 0x4000 else v

def bp_to_word(bp):
    """byte pointer -> (word address, byte)  (set_byte_segment packing:
    seg in bits 31:29, byte offset in 28:0)."""
    seg = (bp >> 29) & 7
    off = bp & 0x1FFFFFFF
    return (seg << 28) | (off >> 1), off & 1

def word_to_bp(word, byte=0):
    seg = (word >> 28) & 7
    return (seg << 29) | (((word & 0x0FFFFFFF) << 1) | byte)


# ----------------------------------------------------------------------
# Input parsing
# ----------------------------------------------------------------------

INSTR_RE = re.compile(r'^([0-9a-f]{8}) ([^ ;]+)(?: ([^#]*?))?;?\s*(?:#\s*(.*))?$')

class Instr:
    __slots__ = ('pc', 'mn', 'args', 'comment', 'raw', 'idx', 'block')
    def __init__(self, pc, mn, args, comment, raw):
        self.pc, self.mn, self.args, self.comment, self.raw = pc, mn, args, comment, raw
        self.idx = -1
        self.block = None
    def __repr__(self):
        return '%08X %s' % (self.pc, self.raw)

def load_dis(path):
    ins = []
    with open(path, errors='replace') as f:
        for line in f:
            line = line.rstrip('\r\n')
            m = INSTR_RE.match(line)
            if not m:
                continue
            pc = int(m.group(1), 16)
            mn = m.group(2)
            args = (m.group(3) or '').strip()
            comment = (m.group(4) or '').strip()
            raw = line[9:].strip()
            ins.append(Instr(pc, mn, args, comment, raw))
    for i, x in enumerate(ins):
        x.idx = i
    return ins

class Block:
    __slots__ = ('start', 'func', 'instrs', 'succ_line', 'succs', 'kind')
    def __init__(self, start, func):
        self.start, self.func = start, func
        self.instrs = []
        self.succ_line = ''
        self.succs = []
        self.kind = ''

def load_blocks(path, ins):
    """quest.blocks.split: '# FUNC' headers, 'ADDR:' block starts,
    instruction lines ending in ';', then a successor line."""
    pc_index = {x.pc: x.idx for x in ins}
    blocks = []
    func = None
    cur = None
    texts = []
    with open(path, errors='replace') as f:
        for line in f:
            line = line.rstrip('\r\n')
            if not line:
                continue
            if line.startswith('# '):
                func = line[2:].strip()
                continue
            m = re.match(r'^([0-9A-F]{8}):$', line)
            if m:
                cur = Block(int(m.group(1), 16), func)
                blocks.append(cur)
                texts = []
                continue
            if cur is None:
                continue
            m = re.match(r'^([a-z])(?: (.*))?$', line)
            if m and not line.endswith(';'):
                cur.kind = m.group(1)
                cur.succ_line = line
                cur.succs = [int(t, 16) for t in re.findall(r'\b[0-9A-F]{8}\b', m.group(2) or '')]
                # bind instructions to dis lines
                i0 = pc_index.get(cur.start)
                if i0 is None:
                    sys.exit('block start %08X not in dis' % cur.start)
                for k, t in enumerate(texts):
                    x = ins[i0 + k]
                    mn = t.split(' ')[0].rstrip(';')
                    if x.mn != mn:
                        sys.exit('block %08X: instr %d mismatch dis %s vs split %s'
                                 % (cur.start, k, x.mn, mn))
                    x.block = cur
                    cur.instrs.append(x)
                continue
            texts.append(line)
    return blocks

def load_mem(path):
    """word-addressed memory + named regions from the section headers."""
    mem = {}
    regions = []   # (start, name)
    pending = None
    with open(path, errors='replace') as f:
        for line in f:
            line = line.rstrip('\r\n')
            if line.startswith('# ') and not line.startswith('# ---'):
                pending = line[2:].strip()
                continue
            m = re.match(r'^([0-9A-F]{6,8})\s+((?:[0-9A-F]{4}\s+)*[0-9A-F]{4})\s*\[', line)
            if not m:
                continue
            addr = int(m.group(1), 16)
            if pending is not None:
                regions.append((addr, pending))
                pending = None
            for i, w in enumerate(m.group(2).split()):
                mem[addr + i] = int(w, 16)
    regions.sort()
    return mem, regions

def region_name(regions, addr):
    lo, hi = 0, len(regions)
    best = None
    for start, name in regions:
        if start <= addr:
            best = (start, name)
        else:
            break
    if best is None:
        return None
    off = addr - best[0]
    return best[1] if off == 0 else '%s+0x%X' % (best[1], off)

def load_symbols(path):
    syms = {}
    with open(path, errors='replace') as f:
        for line in f:
            m = re.match(r'^0x([0-9A-F]{8}) (.*)$', line.strip())
            if m:
                syms[int(m.group(1), 16)] = m.group(2).split(' / ')[0]
    return syms

def read_bytes(mem, word, byte, n):
    out = []
    for k in range(n):
        w = mem.get(word)
        if w is None:
            return None
        out.append((w >> 8) & 0xFF if byte == 0 else w & 0xFF)
        byte ^= 1
        if byte == 0:
            word += 1
    return bytes(out)

def escape(b):
    s = []
    for c in b:
        if 0x20 <= c < 0x7F and c not in (0x5C,):
            s.append(chr(c))
        elif c == 0x5C:
            s.append('\\\\')
        else:
            s.append('\\x%02X' % c)
    return ''.join(s)


# ----------------------------------------------------------------------
# Symbolic values
# ----------------------------------------------------------------------

class V:
    """A symbolic value.  kind in:
       const(k)            integer
       fp                  the frame pointer (LDAFP / WSAVS / presumed at entry)
       sp(k=pc)            wsp as read by LDASP at pc (or the block-entry wsp)
       entry(k=ac)         unknown register at block entry
       unk(a=why,k=pc)     unknown (clobbered by a call, unmodelled op)
       lin(a=[(coef,V)],k) linear form: sum coef*atom + k   (atoms are non-lin)
       bp(a=base,k=off)    byte pointer = 2*base + off (base a word value)
       load(a=addr,b=bits) memory read
       op(a=name,b=lhs,k=rhs) opaque binary/unary op
       end(a=op,b=which,k=pc) a string op's end value
    deps = frozenset of instruction indices that produced it."""
    __slots__ = ('kind', 'a', 'b', 'k', 'deps', 'tag')
    def __init__(self, kind, a=None, b=None, k=None, deps=(), tag=None):
        self.kind, self.a, self.b, self.k = kind, a, b, k
        self.deps = frozenset(deps)
        self.tag = tag

    def with_deps(self, more):
        return V(self.kind, self.a, self.b, self.k, self.deps | frozenset(more), self.tag)

    def __repr__(self):
        return self.show()

    def show(self):
        k = self.kind
        if k == 'const':
            return str(self.k) if -256 <= self.k < 256 else ('0x%X' % self.k if self.k >= 0 else '-0x%X' % -self.k)
        if k == 'fp':
            return 'fp'
        if k == 'sp':
            return 'sp@%X' % self.k
        if k == 'entry':
            return 'ac%d@entry' % self.k
        if k == 'unk':
            return 'unk(%s@%X)' % (self.a, self.k)
        if k == 'lin':
            parts = []
            for c, t in self.a:
                if c == 1:
                    parts.append(('+' if parts else '') + t.show())
                elif c == -1:
                    parts.append('-' + t.show())
                else:
                    parts.append(('%+d*' % c) + t.show())
            if self.k or not parts:
                parts.append(('%+d' % self.k) if -256 < self.k < 256 else ('%+#X' % self.k))
            return '(' + ''.join(parts) + ')'
        if k == 'bp':
            if self.a.kind == 'const' and self.b is None:
                w, b = bp_to_word(word_to_bp(self.a.k) + self.k)
                return 'bp[0x%X:%d]' % (w, b)
            base = self.a.show() if self.a.kind != 'const' else '0x%X' % self.a.k
            return 'bp(%s%s%s)' % (base, (' bytes ' + self.b.show()) if self.b is not None else '', ('%+d' % self.k) if self.k else '')
        if k == 'load':
            return '%s[%s]' % ('W' if self.b == 32 else ('N' if self.b == 16 else 'B'), self.a.show())
        if k == 'op':
            if self.a == 'phi':
                return 'phi(%s)' % ', '.join(u.show() for u in self.tag[1])
            if self.b is None:
                return '%s(%s)' % (self.a, self.k.show())
            return '(%s %s %s)' % (self.b.show(), self.a, self.k.show())
        if k == 'end':
            return '%s.%s@%X' % (self.a, self.b, self.k)
        return '?'

def const(n, deps=()):
    return V('const', k=n, deps=deps)

def _terms(v):
    """-> (list of (coef, atom), const, deps)"""
    if v.kind == 'const':
        return [], v.k, v.deps
    if v.kind == 'lin':
        return list(v.a), v.k, v.deps
    return [(1, v)], 0, v.deps

def _mk(terms, k, deps):
    merged = collections.OrderedDict()
    for c, t in terms:
        key = t.show()
        if key in merged:
            merged[key] = (merged[key][0] + c, t)
        else:
            merged[key] = (c, t)
    terms = [(c, t) for c, t in merged.values() if c != 0]
    if not terms:
        return const(k, deps)
    if len(terms) == 1 and terms[0][0] == 1 and k == 0:
        return terms[0][1].with_deps(deps)
    return V('lin', a=terms, k=k, deps=deps)

def bp_bytes(bpv, extra, deps=()):
    """bp with its byte-term field (b) advanced by the symbolic byte count extra."""
    cur = bpv.b if bpv.b is not None else const(0)
    nb = addv(cur, extra) if extra.kind != 'const' or cur.kind != 'const' else const(cur.k + extra.k)
    k = bpv.k
    if nb.kind == 'const':
        k += nb.k
        nb = None
    return V('bp', a=bpv.a, b=nb, k=k, deps=bpv.deps | extra.deps | frozenset(deps), tag=bpv.tag)

def add(a, k, deps=()):
    deps = frozenset(deps)
    if k == 0:
        return a.with_deps(deps)
    if a.kind == 'bp':
        return V('bp', a=a.a, b=a.b, k=a.k + k, deps=a.deps | deps, tag=a.tag)
    t, c, d = _terms(a)
    return _mk(t, c + k, d | deps)

def addv(a, b, deps=()):
    deps = frozenset(deps)
    if a.kind == 'bp' and b.kind != 'bp':
        return bp_bytes(a, b, deps)
    if b.kind == 'bp' and a.kind != 'bp':
        return bp_bytes(b, a, deps)
    ta, ca, da = _terms(a)
    tb, cb, db = _terms(b)
    return _mk(ta + tb, ca + cb, da | db | deps)

def negv(b):
    tb, cb, db = _terms(b)
    return _mk([(-c, t) for c, t in tb], -cb, db)

def subv(a, b, deps=()):       # a - b
    deps = frozenset(deps)
    if a.kind == 'bp' and b.kind == 'bp':
        # difference of two byte pointers: 2*(basea-baseb) + bytes + offs
        ta, ca, da = _terms(a.a)
        tbb, cbb, dbb = _terms(b.a)
        r = _mk([(2 * c, t) for c, t in ta] + [(-2 * c, t) for c, t in tbb], 2 * (ca - cbb) + a.k - b.k, da | dbb | deps)
        if a.b is not None:
            r = addv(r, a.b)
        if b.b is not None:
            r = addv(r, negv(b.b))
        return r
    if a.kind == 'bp':
        return bp_bytes(a, negv(b), deps)
    return addv(a, negv(b), deps)

def shl(a, n, deps=()):
    deps = frozenset(deps)
    if a.kind == 'const':
        return const((a.k << n) & 0xFFFFFFFF, a.deps | deps)
    if n == 1:
        return V('bp', a=a, k=0, deps=a.deps | deps)      # word address -> byte pointer
    t, c, d = _terms(a)
    return _mk([(cc * (1 << n), tt) for cc, tt in t], c << n, d | deps)


# ----------------------------------------------------------------------
# Operand decoding
# ----------------------------------------------------------------------

ADDR_RE = re.compile(r'^(@?)\[(?:(pc|ac[0-3])\+)?0x([0-9A-F]+)\](?: \((0x[0-9A-Fa-f]+)(?::(\d))?\))?$')

def parse_ea(text):
    """-> (indirect, base('pc'|'acN'|None), disp, fold(word or None), fold_byte)"""
    m = ADDR_RE.match(text.strip())
    if not m:
        return None
    ind = m.group(1) == '@'
    base = m.group(2)
    disp = int(m.group(3), 16)
    fold = int(m.group(4), 16) if m.group(4) else None
    fb = int(m.group(5)) if m.group(5) else None
    return ind, base, disp, fold, fb

STRING_OPS = ('WCMV', 'WCMP', 'WBLM', 'WMSP', 'STASP')

class Site:
    def __init__(self, ins, acs, wsp, claims):
        self.ins = ins
        self.acs = acs           # [V, V, V, V] at the op
        self.wsp = wsp           # symbolic wsp at the op
        self.claims = claims     # list of active claims (Claim) at the op
        self.window = None
        self.crosses = False
        self.notes = []
        self.klass = None        # per-operand classes filled later
        self.stores = []         # (idx, addrV, valV) stores in the block before the op

class Claim:
    __slots__ = ('pc', 'base', 'size', 'released', 'slot', 'idx', 'top')
    def __init__(self, pc, base, size, idx):
        self.pc, self.base, self.size, self.idx = pc, base, size, idx
        self.released = None
        self.slot = None
        self.top = None


class Evaluator:
    """Forward symbolic evaluation of one block."""

    def __init__(self, block, ins, syms, inherit=None):
        self.block = block
        self.ins = ins
        self.syms = syms
        self.chain = []        # predecessor blocks whose state we inherited
        if inherit is None:
            self.ac = [V('entry', k=i) for i in range(4)]
            # ac3 at block entry is presumed to be the frame pointer (the
            # compiler reloads it with LDAFP 3 after every clobber); tagged
            # so the classifier can report "presumed".
            self.ac[3] = V('fp', tag='entry')
            self.wsp = V('sp', k=block.start, tag='entry')
            self.mem = {}          # canonical addr string -> V   (frame slots + statics)
            self.claims = []       # Claim objects, in order
            self.pushes = []       # (idx, V) WPSH'd values not yet popped
        else:
            self.ac = list(inherit.ac)
            self.wsp = inherit.wsp
            self.mem = dict(inherit.mem)
            self.claims = list(inherit.claims)
            self.pushes = list(inherit.pushes)
            self.chain = inherit.chain + [inherit.block]
        self.sites = []
        self.stores = []       # (idx, addrV, valV)
        self.c = None

    # -- addressing --------------------------------------------------
    def word_ea(self, text, idx, long_form):
        """Effective WORD address for X/L word-form operands."""
        p = parse_ea(text)
        if p is None:
            return V('unk', a='ea:' + text, k=self.ins[idx].pc, deps=[idx])
        ind, base, disp, fold, fb = p
        if base == 'pc':
            if fold is None:
                return V('unk', a='pcrel', k=self.ins[idx].pc, deps=[idx])
            ea = const(fold, [idx])
        elif base is None:
            ea = const(disp & 0x7FFFFFFF, [idx])
        else:
            if long_form:
                d = disp if disp < 0x40000000 else disp - 0x80000000
            else:
                d = sext15(disp)
            ea = add(self.ac[int(base[2])], d, [idx])
        if ind:
            ea = self.load(ea, 32, idx)
        return ea

    def byte_ea(self, text, idx, long_form):
        """Effective BYTE pointer for XLEFB/LLEFB/XLDB/XSTB (ByteEA.md §2)."""
        p = parse_ea(text)
        if p is None:
            return V('unk', a='bea:' + text, k=self.ins[idx].pc, deps=[idx])
        ind, base, disp, fold, fb = p
        if base == 'pc':
            if fold is None:
                return V('unk', a='pcrel-bp', k=self.ins[idx].pc, deps=[idx])
            return V('bp', a=const(fold), k=fb or 0, deps=[idx])
        if long_form:
            raw = disp | 0x80000000 if ind else disp     # the standing compensation
            if base is None:
                w, b = bp_to_word(raw)
                return V('bp', a=const(w), k=b, deps=[idx])
            # acN*2 + raw32: a byte-pointer constant indexed by acN words
            w, b = bp_to_word(raw)
            return V('bp', a=addv(const(w), self.ac[int(base[2])]), k=b, deps=[idx], tag='indexed')
        # X form
        if ind:
            return V('unk', a='xbyte@', k=self.ins[idx].pc, deps=[idx])
        if base is None:
            w, b = bp_to_word(disp)   # ii=0: raw16 as a byte offset in seg (rare)
            return V('bp', a=const(w), k=b, deps=[idx])
        return V('bp', a=self.ac[int(base[2])], k=sext16(disp), deps=[idx])

    def canon(self, addr):
        return addr.show()

    def load(self, addr, width, idx):
        key = self.canon(addr)
        if key in self.mem:
            v = self.mem[key]
            return v.with_deps([idx])
        return V('load', a=addr, b=width, deps=addr.deps | {idx})

    def store(self, addr, val, idx):
        self.mem[self.canon(addr)] = val
        self.stores.append((idx, addr, val))

    def clobber_all(self, why, idx):
        pc = self.ins[idx].pc
        for i in range(4):
            self.ac[i] = V('unk', a=why, k=pc, deps=[idx])
        # a call may write through any pointer it was handed; statics are
        # dropped, frame slots kept (compiler temporaries are never passed)
        self.mem = {k: v for k, v in self.mem.items() if k.startswith('(fp')}

    # -- the walk ----------------------------------------------------
    def run(self):
        ins = self.ins
        for x in self.block.instrs:
            idx = x.idx
            mn, args = x.mn, x.args
            a = [s.strip() for s in args.split(',', 1)] if args else []
            try:
                self.step(x, idx, mn, a)
            except Exception as e:  # loud, per METHOD §8
                raise RuntimeError('at %08X %s: %s' % (x.pc, x.raw, e))
        return self.sites

    def acnum(self, s):
        return int(s)

    def step(self, x, idx, mn, a):
        ac = self.ac
        pc = x.pc
        if mn in STRING_OPS:
            site = Site(x, list(ac), self.wsp, list(self.claims))
            site.stores = list(self.stores)
            self.sites.append(site)
            if mn == 'WCMV':
                dcount, scount, dst, src = ac
                ac[0] = const(0, [idx])
                ac[1] = V('end', a='wcmv', b='src_left', k=pc, deps=[idx])
                ac[2] = addv(dst, dcount, [idx]) if dst.kind == 'bp' else V('end', a='wcmv', b='dst_end', k=pc, deps={idx} | dst.deps | dcount.deps)
                ac[2].tag = ('cont', pc, dst)
                ac[3] = addv(src, scount, [idx]) if src.kind == 'bp' else V('end', a='wcmv', b='src_end', k=pc, deps={idx} | src.deps | scount.deps)
                ac[3].tag = ('cont', pc, src)
                self.c = ('wcmv', pc)
            elif mn == 'WCMP':
                ac[0] = const(0, [idx])
                ac[1] = V('end', a='wcmp', b='result', k=pc, deps=[idx])
                ac[2] = V('end', a='wcmp', b='dst_end', k=pc, deps=[idx], tag=('cont', pc, ac[2]))
                ac[3] = V('end', a='wcmp', b='src_end', k=pc, deps=[idx], tag=('cont', pc, ac[3]))
            elif mn == 'WBLM':
                cnt = ac[1]
                ac[1] = const(0, [idx])
                ac[2] = addv(ac[2], cnt, [idx]) if cnt.kind == 'const' else V('end', a='wblm', b='src_end', k=pc, deps=[idx], tag=('cont', pc, ac[2]))
                ac[3] = addv(ac[3], cnt, [idx]) if cnt.kind == 'const' else V('end', a='wblm', b='dst_end', k=pc, deps=[idx], tag=('cont', pc, ac[3]))
            elif mn == 'WMSP':
                n = self.acnum(a[0])
                size = ac[n]
                cl = Claim(pc, self.wsp, size, idx)
                self.claims.append(cl)
                site.claim = cl
                if size.kind == 'const':
                    self.wsp = add(self.wsp, 2 * size.k, [idx])
                else:
                    t, c = lin_parts(size)
                    self.wsp = addv(self.wsp, _mk([(2 * cc, tt) for cc, tt in t], 2 * c, size.deps), [idx])
                self.wsp.tag = ('after-wmsp', pc)
                cl.top = self.wsp
            elif mn == 'STASP':
                n = self.acnum(a[0])
                site.value = ac[n]
                self.wsp = ac[n].with_deps([idx])
                for cl in self.claims:
                    if cl.released is None:
                        cl.released = pc
            return

        # ---- loads / effective addresses ----
        if mn == 'NLDAI':
            m = re.match(r'^-?\d+ \(0x([0-9A-F]+)\)$', a[0])
            ac[int(a[1])] = const(sext16(int(m.group(1), 16)), [idx])   # EagleCompute.cpp:311 sign-extends
        elif mn == 'WLDAI':
            v = int(a[1], 16)
            if v & 0x80000000:
                v -= 0x100000000
            ac[int(a[0])] = const(v, [idx])
        elif mn == 'WMOV':
            ac[int(a[1])] = ac[int(a[0])].with_deps([idx])
        elif mn == 'LDAFP':
            ac[int(a[0])] = V('fp', deps=[idx])
        elif mn == 'LDASP':
            ac[int(a[0])] = self.wsp.with_deps([idx])
            ac[int(a[0])].tag = ('ldasp', pc, self.wsp)
        elif mn in ('XLEFB', 'LLEFB'):
            ac[int(a[0])] = self.byte_ea(a[1], idx, mn == 'LLEFB')
        elif mn in ('XLEF', 'LLEF'):
            ac[int(a[0])] = self.word_ea(a[1], idx, mn == 'LLEF')
        elif mn in ('XWLDA', 'LWLDA'):
            ac[int(a[0])] = self.load(self.word_ea(a[1], idx, mn == 'LWLDA'), 32, idx)
        elif mn in ('XNLDA', 'LNLDA'):
            ac[int(a[0])] = self.load(self.word_ea(a[1], idx, mn == 'LNLDA'), 16, idx)
        elif mn in ('XLDB', 'LLDB'):
            ac[int(a[0])] = V('load', a=self.byte_ea(a[1], idx, mn == 'LLDB'), b=8, deps=[idx])
        elif mn == 'WLDB':
            ac[int(a[1])] = V('load', a=ac[int(a[0])], b=8, deps=ac[int(a[0])].deps | {idx})
        # ---- stores ----
        elif mn in ('XWSTA', 'LWSTA'):
            self.store(self.word_ea(a[1], idx, mn == 'LWSTA'), ac[int(a[0])].with_deps([idx]), idx)
        elif mn in ('XNSTA', 'LNSTA'):
            self.store(self.word_ea(a[1], idx, mn == 'LNSTA'), ac[int(a[0])].with_deps([idx]), idx)
        elif mn in ('XSTB', 'LSTB'):
            self.store(self.byte_ea(a[1], idx, mn == 'LSTB'), ac[int(a[0])].with_deps([idx]), idx)
        elif mn == 'WSTB':            # WSTB acs,acd: byte of acd -> [acs]  (EagleGeneral.cpp:120)
            self.store(ac[int(a[0])], ac[int(a[1])].with_deps([idx]), idx)
        # ---- arithmetic ----
        elif mn == 'WADI':            # WADI imm,ac
            ac[int(a[1])] = add(ac[int(a[1])], int(a[0]), [idx])
        elif mn == 'WSBI':            # WSBI imm,ac
            ac[int(a[1])] = add(ac[int(a[1])], -int(a[0]), [idx])
        elif mn in ('WNADI', 'NADDI'):           # ac,imm16 sign-extended (EagleCompute.cpp:317)
            m = re.search(r'\(0x([0-9A-F]+)\)', a[1])
            ac[int(a[0])] = add(ac[int(a[0])], sext16(int(m.group(1), 16)), [idx])
        elif mn in ('LNADI', 'XNADI', 'LWADI', 'XWADI', 'LNSBI', 'XNSBI', 'LWSBI', 'XWSBI'):
            # memory increment by immediate: no AC write; forget the slot
            self.mem.pop(self.canon(self.word_ea(a[1], idx, mn.startswith('L'))), None)
        elif mn in ('XWADD', 'LWADD', 'XNADD', 'LNADD'):
            d = int(a[0])
            ld = self.load(self.word_ea(a[1], idx, mn.startswith('L')), 32 if 'W' in mn[1:3] else 16, idx)
            ac[d] = addv(ac[d], ld, [idx])
        elif mn in ('XWSUB', 'LWSUB', 'XNSUB', 'LNSUB'):
            d = int(a[0])
            ld = self.load(self.word_ea(a[1], idx, mn.startswith('L')), 32 if 'W' in mn[1:3] else 16, idx)
            ac[d] = subv(ac[d], ld, [idx])
        elif mn in ('WMUL', 'NMUL', 'WMULS'):      # d = d * s
            sr, d = int(a[0]), int(a[1])
            if ac[sr].kind == 'const' or ac[d].kind == 'const':
                k = ac[sr].k if ac[sr].kind == 'const' else ac[d].k
                other = ac[d] if ac[sr].kind == 'const' else ac[sr]
                t, c = lin_parts(other)
                ac[d] = _mk([(cc * k, tt) for cc, tt in t], c * k, ac[sr].deps | ac[d].deps | {idx})
            else:
                ac[d] = V('op', a=mn, b=ac[d], k=ac[sr], deps=ac[d].deps | ac[sr].deps | {idx})
        elif mn == 'WADD':            # d = d + s
            s, d = int(a[0]), int(a[1])
            ac[d] = addv(ac[d], ac[s], [idx])
        elif mn == 'WSUB':            # d = d - s
            s, d = int(a[0]), int(a[1])
            ac[d] = const(0, [idx]) if s == d else subv(ac[d], ac[s], [idx])
        elif mn == 'WINC':            # d = s + 1
            s, d = int(a[0]), int(a[1])
            ac[d] = add(ac[s], 1, [idx])
        elif mn == 'WADC':            # d = d + ~s
            s, d = int(a[0]), int(a[1])
            ac[d] = V('op', a='+~', b=ac[d], k=ac[s], deps=ac[d].deps | ac[s].deps | {idx})
        elif mn == 'WLSI':            # WLSI n,ac
            ac[int(a[1])] = shl(ac[int(a[1])], int(a[0]), [idx])
        elif mn == 'WMOVR':           # rotate right through carry, used as >>1
            n = int(a[0])
            v = ac[n]
            if v.kind == 'const':
                ac[n] = const(v.k >> 1, v.deps | {idx})
            elif v.kind == 'op' and v.b is None and v.a.startswith('>>'):
                ac[n] = V('op', a='>>%d' % (int(v.a[2:]) + 1), k=v.k, deps=v.deps | {idx})
            else:
                ac[n] = V('op', a='>>1', k=v, deps=v.deps | {idx})
        elif mn == 'WXCH':
            s, d = int(a[0]), int(a[1])
            ac[s], ac[d] = ac[d].with_deps([idx]), ac[s].with_deps([idx])
        elif mn in ('WMUL', 'WDIV', 'WNEG', 'WCOM', 'WAND', 'WIOR', 'WXOR', 'WASH', 'WLSH',
                    'WANDI', 'WIORI', 'WXORI', 'WASHI', 'WLSHI', 'WDIVS', 'WMULS', 'WNADD', 'WNSUB',
                    'WNMUL', 'WNDIV', 'NADD', 'NSUB', 'NMUL', 'NDIV', 'WSBI', 'CVWN', 'WSSVR', 'WSSVS',
                    'SEX', 'ZEX', 'WHLV', 'WNEGI', 'WLDAI', 'WSKBO', 'WSKBZ', 'XNADD', 'XWADD', 'XNSUB',
                    'XWSUB', 'XNMUL', 'XWMUL', 'XNDIV', 'XWDIV', 'LNADD', 'LWADD', 'LNSUB', 'LWSUB',
                    'LNMUL', 'LWMUL', 'LNDIV', 'LWDIV', 'WMSP', 'WPOPB', 'WUSGT', 'WUSGE', 'WULEI',
                    'XCT', 'WBTO', 'WBTZ', 'WSZB', 'WSNB', 'WLOB', 'WLSN', 'WNADI'):
            # generic: last AC operand is the destination (Eagle convention)
            regs = [int(t) for t in re.findall(r'(?<![x0-9A-F])([0-3])(?=,|$|\s)', ','.join(a))]
            if mn.startswith('X') or mn.startswith('L'):
                d = int(a[0])
                ac[d] = V('op', a=mn, k=self.word_ea(a[1], idx, mn.startswith('L')), b=ac[d], deps=ac[d].deps | {idx})
            elif len(a) >= 2 and a[1].isdigit() and int(a[1]) < 4:
                d = int(a[1])
                srcv = ac[int(a[0])] if a[0].isdigit() and int(a[0]) < 4 else const(0)
                ac[d] = V('op', a=mn, b=ac[d], k=srcv, deps=ac[d].deps | srcv.deps | {idx})
            elif len(a) >= 1 and a[0].isdigit() and int(a[0]) < 4:
                d = int(a[0])
                ac[d] = V('op', a=mn, k=ac[d], deps=ac[d].deps | {idx})
        # ---- stack ----
        elif mn == 'WPSH':
            s, d = int(a[0]), int(a[1])
            r = s
            while True:
                self.pushes.append((idx, ac[r]))
                self.wsp = add(self.wsp, 2, [idx])
                if r == d:
                    break
                r = (r + 1) % 4
        elif mn == 'WPOP':
            s, d = int(a[0]), int(a[1])
            r = s
            while True:
                v = self.pushes.pop()[1].with_deps([idx]) if self.pushes else V('unk', a='wpop', k=pc, deps=[idx])
                ac[r] = v
                self.wsp = add(self.wsp, -2, [idx])
                if r == d:
                    break
                r = (r - 1) % 4
        elif mn in ('XPEF', 'LPEF', 'XPEFB', 'LPEFB', 'WPSHI'):
            self.wsp = add(self.wsp, 2, [idx])
        elif mn in ('WSAVS', 'WSAVR'):
            ac[3] = V('fp', deps=[idx])
            self.wsp = V('sp', k=pc, tag=('after-wsavs', pc), deps=[idx])
            self.pushes = []
        elif mn == 'SYSCALL':
            # AOS/VS convention: ac0..ac2 carry results, ac3 is preserved
            # (the compiler addresses [ac3+d] right after, e.g. 7015BE5B)
            keep = ac[3]
            self.clobber_all('SYSCALL ' + x.args, idx)
            ac[3] = keep
        elif mn in ('LCALL', 'XCALL', 'LJSR', 'XJSR', 'WRTN', 'WPOPJ', 'WRSTR'):
            self.clobber_all(mn + (':' + x.comment if x.comment else ''), idx)
            if mn in ('LCALL', 'XCALL', 'LJSR', 'XJSR'):
                # WRTN (EagleStack.cpp:488) leaves ac3 = the caller's wfp;
                # ac0..ac2 come back from the callee's frame (return values)
                ac[3] = V('fp', deps=[idx])
                self.wsp = V('sp', k=pc, tag=('after-call', pc), deps=[idx])
                self.pushes = []
        elif mn == 'WADDI':
            v = int(re.search(r'\(0x([0-9A-F]+)\)', a[1]).group(1), 16)
            if v & 0x80000000:
                v -= 0x100000000
            ac[int(a[0])] = add(ac[int(a[0])], v, [idx])
        elif mn in ('XWSTA', 'FRH', 'FRDS'):
            pass
        elif mn.startswith('F') or mn.startswith('XF') or mn.startswith('LF'):
            pass      # floating point does not touch the ACs we care about
        else:
            # skips/branches/tests: no AC writes.  Anything else that
            # names an AC as an operand we do not model: clobber it.
            if mn in ('WBR', 'XJMP', 'LJMP', 'WSEQ', 'WSNE', 'WSGT', 'WSGE', 'WSLT', 'WSLE', 'WSEQI', 'WSNEI',
                      'WSGTI', 'WSGEI', 'WSLTI', 'WSLEI', 'WUGTI', 'WULEI', 'WUSGT', 'WUSGE', 'WSKBO', 'WSKBZ',
                      'XNDO', 'XWDO', 'XNISZ', 'XNDSZ', 'XWISZ', 'XWDSZ', 'LNISZ', 'LNDSZ', 'LWISZ', 'LWDSZ',
                      'DERR', 'NSKIP', 'WCLM', 'XVCT', 'ISZTS', 'DSZTS', 'LPSHJ', 'XPSHJ', 'WBTZ', 'WBTO',
                      'LNDO', 'LWDO', 'WSSVS', 'WSSVR', 'HALT', 'NOP', 'XNSTA', 'WSZB', 'WSNB'):
                return
            for t in re.findall(r'(?<![x0-9A-F])([0-3])(?=,|$|\s)', ','.join(a)):
                ac[int(t)] = V('unk', a=mn, k=pc, deps=[idx])


# ----------------------------------------------------------------------
# Classification
# ----------------------------------------------------------------------

CODE_LO = 0x70150000       # literals live in the code segment
DATA_HI = 0x70100000       # statics live below this

def is_code_addr(w):
    return w >= CODE_LO

def is_data_addr(w):
    return 0x70000000 <= w < DATA_HI

def lin_parts(v):
    """-> (terms, const) for any word value."""
    if v.kind == 'const':
        return [], v.k
    if v.kind == 'lin':
        return list(v.a), v.k
    return [(1, v)], 0

def fp_offset(v):
    """v == fp + k  -> k, else None."""
    t, c = lin_parts(v)
    if len(t) == 1 and t[0][0] == 1 and t[0][1].kind == 'fp':
        return c
    return None

def sp_part(v):
    """v contains an sp atom with coef 1 -> (spV, const, other terms) else None."""
    t, c = lin_parts(v)
    sps = [x for x in t if x[1].kind == 'sp' and x[0] == 1]
    if len(sps) == 1:
        others = [x for x in t if x[1].kind != 'sp']
        return sps[0][1], c, others
    return None

def has_kind(v, kinds):
    if v is None or not isinstance(v, V):
        return False
    if v.kind in kinds:
        return True
    if v.kind == 'lin':
        return any(has_kind(t, kinds) for _, t in v.a)
    for c in (v.a, v.b, v.k):
        if isinstance(c, V) and has_kind(c, kinds):
            return True
    return False

def name_static(ctx, w):
    n = region_name(ctx.regions, w)
    return ' (%s)' % n if n else ''

def describe_addr(v, ctx):
    """Describe a WORD address expression for the human reader."""
    k = fp_offset(v)
    if k is not None:
        return 'fp%+d' % k
    if v.kind == 'const':
        if is_data_addr(v.k):
            return '0x%X%s' % (v.k, name_static(ctx, v.k))
        return '0x%X' % v.k
    if v.kind == 'load':
        return '*(%s)' % describe_addr(v.a, ctx)
    t, c = lin_parts(v)
    consts = [x for x in t if x[1].kind == 'const']
    return v.show()

def classify_ptr(v, ctx):
    """Return (class, detail, offset) for a pointer operand (ac2/ac3).
    offset is a symbolic byte-offset string when the pointer carries a
    non-constant byte displacement (the SUBSTR shape), else None."""
    if v.kind == 'end':
        if v.tag and v.tag[0] == 'cont':
            cls, det, off = classify_ptr(v.tag[2], ctx)
            return cls, 'cont: end of %s@%X over %s' % (v.a, v.tag[1], det), 'end@%X' % v.tag[1]
        return 'computed', 'end pointer of %s@%X' % (v.a, v.k), None
    if v.kind == 'unk':
        return 'computed', 'unknown (%s@%X)' % (v.a, v.k), None
    if v.kind == 'entry':
        return 'computed', 'ac%d at block entry (word value as pointer)' % v.k, None
    if v.kind == 'bp':
        base, off = v.a, v.k
        boff = v.b.show() if v.b is not None else None
        cont = ' [cont after %s@%X]' % (v.tag[0], v.tag[1]) if isinstance(v.tag, tuple) and v.tag[0] == 'cont' else ''
        if base.kind == 'const':
            w, b = bp_to_word(word_to_bp(base.k) + off)
            if is_code_addr(w):
                return 'literal', '0x%X:%d%s' % (w, b, cont), boff
            if is_data_addr(w):
                return 'static', '0x%X:%d%s%s' % (w, b, name_static(ctx, w), cont), boff
            return 'computed', 'bp const 0x%X:%d outside data/code%s' % (w, b, cont), boff
        k = fp_offset(base)
        if k is not None:
            # byte pointer 2*(fp+k)+off = frame byte 2k+off
            byte = 2 * k + off
            return 'frame', 'frame byte %+d (word fp%+d%s)%s' % (byte, byte // 2, '' if byte % 2 == 0 else ' +1 byte', cont), boff
        sp = sp_part(base)
        if sp is not None:
            spv, c, others = sp
            det = 'temp: %s%+d words' % (spv.show(), c)
            if others:
                det += ' + ' + _mk(others, 0, frozenset()).show()
            if off:
                det += ' byte %+d' % off
            return 'temp', det + cont, boff
        if base.kind == 'load':
            addr = base.a
            k = fp_offset(addr)
            if k is not None:
                return 'computed', 'deref: pointer in frame word fp%+d, byte %+d%s' % (k, off, cont), boff
            if addr.kind == 'const' and is_data_addr(addr.k):
                return 'computed', 'deref: pointer in static 0x%X%s, byte %+d%s' % (addr.k, name_static(ctx, addr.k), off, cont), boff
            return 'computed', 'deref: pointer at %s, byte %+d%s' % (describe_addr(addr, ctx), off, cont), boff
        t, c = lin_parts(base)
        fps = [x for x in t if x[1].kind == 'fp']
        if fps:
            others = [x for x in t if x[1].kind != 'fp']
            return 'frame', 'frame word fp%+d indexed by %s, byte %+d%s' % (c, _mk(others, 0, frozenset()).show(), off, cont), boff
        if c and is_data_addr(c):
            return 'computed', 'static table 0x%X%s indexed by %s, byte %+d%s' % (c, name_static(ctx, c), _mk(t, 0, frozenset()).show(), off, cont), boff
        ptrs = [x for x in t if x[1].kind == 'load' and x[1].a.kind == 'const' and is_data_addr(x[1].a.k)]
        if ptrs:
            names = ', '.join('W[0x%X%s]' % (x[1].a.k, name_static(ctx, x[1].a.k)) for x in ptrs)
            others = [x for x in t if x not in ptrs]
            return 'computed', 'record via static pointer %s + %s words %+d, byte %+d%s' % (
                names, _mk(others, 0, frozenset()).show() if others else '0', c, off, cont), boff
        if has_kind(base, ('entry',)):
            return 'computed', 'block-entry register based: %s, byte %+d%s' % (base.show(), off, cont), boff
        if has_kind(base, ('unk',)):
            return 'computed', 'unknown base: %s, byte %+d%s' % (base.show(), off, cont), boff
        return 'computed', 'bp(%s) byte %+d%s' % (base.show(), off, cont), boff
    sp = sp_part(v)
    if sp is not None:
        return 'temp', 'WORD address %s%+d used as pointer (no <<1)' % (sp[0].show(), sp[1]), None
    if v.kind == 'load':
        return 'computed', 'loaded pointer %s' % v.show(), None
    if v.kind == 'const':
        return 'computed', 'raw constant %s' % v.show(), None
    if v.kind == 'fp':
        return 'computed', 'fp itself (word value)', None
    return 'computed', v.show(), None

def classify_count(v, ctx):
    """-> (class, detail).  class: const | frame | static | computed"""
    if v.kind == 'const':
        return 'const', str(v.k)
    if v.kind == 'load':
        addr = v.a
        w = 'W' if v.b == 32 else 'N'
        k = fp_offset(addr)
        if k is not None:
            return 'frame', '%s[fp%+d]' % (w, k)
        if addr.kind == 'const' and is_data_addr(addr.k):
            return 'static', '%s[0x%X]%s' % (w, addr.k, name_static(ctx, addr.k))
        if addr.kind == 'load':
            k2 = fp_offset(addr.a)
            if k2 is not None:
                return 'computed', 'deref: %s[*(fp%+d)] (length word of a string reached through a frame pointer slot)' % (w, k2)
            return 'computed', 'deref: %s' % v.show()
        return 'computed', 'load %s' % v.show()
    if v.kind == 'lin':
        t, c = lin_parts(v)
        if len(t) == 1 and t[0][0] == 1 and t[0][1].kind == 'load':
            cls, det = classify_count(t[0][1], ctx)
            return 'computed', '%s %+d (%s length word plus constant)' % (det, c, cls)
        return 'computed', v.show()
    if v.kind == 'end':
        return 'computed', 'residue of %s@%X (%s)' % (v.a, v.k, v.b)
    return 'computed', v.show()


# ----------------------------------------------------------------------
# WBLM word pointers, idiom labelling, destinations
# ----------------------------------------------------------------------

def classify_wptr(v, ctx):
    """WBLM operands are WORD addresses."""
    k = fp_offset(v)
    if k is not None:
        return 'frame', 'word fp%+d' % k, None
    if v.kind == 'const':
        if is_data_addr(v.k):
            return 'static', '0x%X%s' % (v.k, name_static(ctx, v.k)), None
        return 'computed', 'const 0x%X' % v.k, None
    if v.kind == 'end' and v.tag and v.tag[0] == 'cont':
        cls, det, _ = classify_wptr(v.tag[2], ctx)
        return cls, 'cont: end of %s@%X over %s' % (v.a, v.tag[1], det), None
    if v.kind == 'load':
        k = fp_offset(v.a)
        if k is not None:
            return 'computed', 'deref: word pointer in frame word fp%+d' % k, None
        if v.a.kind == 'const' and is_data_addr(v.a.k):
            return 'computed', 'deref: word pointer in static 0x%X%s' % (v.a.k, name_static(ctx, v.a.k)), None
        return 'computed', 'deref: %s' % v.show(), None
    t, c = lin_parts(v)
    loads = [x for x in t if x[1].kind == 'load']
    if loads and all(x[1].kind == 'load' for x in t):
        parts = []
        for cc, tt in t:
            a = tt.a
            if a.kind == 'const' and is_data_addr(a.k):
                parts.append('W[0x%X%s]' % (a.k, name_static(ctx, a.k)))
            else:
                parts.append(tt.show())
        return 'computed', 'pointer %s%+d words' % (' + '.join(parts), c), None
    sp = sp_part(v)
    if sp:
        return 'temp', 'word %s%+d' % (sp[0].show(), sp[1]), None
    if c and is_data_addr(c):
        return 'computed', 'static 0x%X%s indexed by %s' % (c, name_static(ctx, c), _mk(t, 0, frozenset()).show()), None
    return 'computed', v.show(), None

def cont_root(v):
    """Follow continuation tags to the originating (pc, operand)."""
    hops = 0
    while isinstance(v.tag, tuple) and v.tag[0] == 'cont':
        v = v.tag[2]
        hops += 1
        if hops > 50:
            break
    return v, hops

def find_len_store(site, ctx):
    """A store in the setup window whose target word A satisfies dst == bp(A)+2
    and whose value equals the dst count -> the varying-string length word."""
    lo, hi = site.window
    if lo is None:
        return None
    dst = site.acs[2]
    cnt = site.acs[0]
    root, _ = cont_root(dst)
    if root.kind != 'bp':
        return None
    for idx, addr, val in site.stores:
        if idx < lo or idx > site.ins.idx:
            continue
        if addr.kind == 'bp':
            continue
        # data pointer = 2*base+k bytes; the length word A satisfies 2A+2 = 2*base+k
        if root.b is None and root.k % 2 == 0:
            expect = add(root.a, (root.k - 2) // 2)
            if expect.show() == addr.show():
                same = val.show() == cnt.show() or (cnt.kind == 'const' and val.kind == 'const' and val.k == cnt.k)
                return (idx, addr, val, same)
        # static: dst bp[const word W:0], store to W-1
        if root.a.kind == 'const' and addr.kind == 'const' and root.b is None:
            w, b = bp_to_word(word_to_bp(root.a.k) + root.k)
            if b == 0 and addr.k == w - 1:
                same = val.show() == cnt.show()
                return (idx, addr, val, same)
    return None

def label_wcmv(site, ctx):
    """-> (idiom, features)"""
    c0, c1, dst, src = site.acs
    (c0c, c0d), (c1c, c1d), (dc, dd, doff), (sc, sd, soff) = site.klass
    f = []
    dst_cont = isinstance(dst.tag, tuple) and dst.tag[0] == 'cont' or dst.kind == 'end'
    src_cont = isinstance(src.tag, tuple) and src.tag[0] == 'cont' or src.kind == 'end'
    lenstore = find_len_store(site, ctx)
    if lenstore:
        f.append('lenword-store' + ('' if lenstore[3] else '(value differs from dst_count!)'))
    if dst_cont:
        f.append('dst-cont')
    if src_cont:
        f.append('src-cont')
    droot, _ = cont_root(dst)
    sroot, _ = cont_root(src)
    if droot.kind == 'bp' and droot.b is not None:
        f.append('dst-byte-offset')
    if sroot.kind == 'bp' and sroot.b is not None:
        f.append('src-byte-offset')
    same_count = c0.show() == c1.show()
    if same_count:
        f.append('counts-equal')
    elif c0.kind == 'const' and c1.kind == 'const':
        f.append('pad-fill(%d<-%d)' % (c0.k, c1.k) if c0.k > c1.k else 'truncates(%d<-%d)' % (c0.k, c1.k))
    if c0.kind == 'op' and c0.a == 'phi':
        f.append('dst_count=min()')
    if c1.kind == 'op' and c1.a == 'phi':
        f.append('src_count=phi')
    if c1.kind == 'end':
        f.append('src_count=residue')
    if c0.kind == 'end':
        f.append('dst_count=residue')
    if has_kind(c0, ('unk',)) or has_kind(c1, ('unk',)):
        f.append('count-from-call')
    # idiom
    if dst_cont and not src_cont:
        idiom = 'CONCAT-PIECE'
    elif src_cont and not dst_cont:
        idiom = 'TAIL-SPLIT'
    elif src_cont and dst_cont:
        idiom = 'CONCAT-PIECE+TAIL'
    elif sc == 'literal':
        if lenstore:
            idiom = 'ASSIGN-LIT-VARYING' if same_count else 'ASSIGN-LIT-VARYING-PAD'
        elif same_count:
            idiom = 'COPY-LIT-EXACT'
        else:
            idiom = 'ASSIGN-LIT-FIXED'
    elif dc == 'temp':
        idiom = 'TEMP-FIRST-PIECE'
    elif sc == 'temp':
        idiom = 'ASSIGN-FROM-TEMP' + ('-VARYING' if lenstore else '')
    else:
        if lenstore:
            idiom = 'ASSIGN-STR-VARYING' if same_count else 'ASSIGN-STR-VARYING-PAD'
        elif same_count:
            idiom = 'COPY-STR-EXACT'
        elif c0.kind == 'op' and c0.a == 'phi':
            idiom = 'ASSIGN-STR-MIN'
        else:
            idiom = 'ASSIGN-STR-FIXED'
    if 'dst-byte-offset' in f or 'src-byte-offset' in f:
        idiom += '+SUBSTR'
    if 'count-from-call' in f:
        idiom += '+CALLRESULT'
    return idiom, f

# --- destinations ------------------------------------------------------

def ultimate_dest(site, ctx, depth=0):
    """Follow (c) 'consumed by WCMV@pc(src)' links to the final destination."""
    cls, det = site.dest
    if cls != 'c' or depth > 6:
        return site.dest
    m = re.findall(r'WCMV@([0-9A-F]+)\(src\)', det)
    if not m:
        return site.dest
    nxt = ctx.site_by_pc.get(int(m[0], 16))
    if nxt is None or getattr(nxt, 'dest', None) is None:
        return site.dest
    ucls, udet = ultimate_dest(nxt, ctx, depth + 1)
    return ucls, 'via WCMV@%s: %s' % (m[0], udet)

WRITE_CALLS = ('?WRITE_SCREEN', '?WRITE', '?WRITE_ERROR_CHANNEL')

def frame_word_of_dst(v):
    """dst root -> frame word index of the DATA start (or None)."""
    root, _ = cont_root(v)
    if root.kind == 'bp':
        k = fp_offset(root.a)
        if k is not None:
            byte = 2 * k + root.k
            return byte // 2, root
    return None, root

def forward_scan(site, ctx, max_depth=12, max_instr=1200):
    """BFS forward from the site through successors in the same function,
    collecting arg pushes of frame words and the call they feed, WCMV/WCMP
    uses of frame byte pointers, and STASP releases."""
    b0 = site.block
    start_idx = site.ins.idx + 1
    seen = set()
    out = {'calls': [], 'copies': [], 'compares': []}
    queue = [(b0, start_idx, 0)]
    n = 0
    while queue:
        b, from_idx, depth = queue.pop(0)
        if b.start in seen or b.func != b0.func:
            continue
        seen.add(b.start)
        pushes = []
        for x in b.instrs:
            if x.idx < from_idx:
                continue
            n += 1
            if n > max_instr:
                return out
            if x.mn in ('XPEF', 'XPEFB', 'LPEF', 'LPEFB', 'WPSH'):
                pushes.append(x)
            elif x.mn in ('LCALL', 'XCALL'):
                out['calls'].append((x, list(pushes)))
                pushes = []
            elif x.mn in ('WCMV', 'WBLM'):
                out['copies'].append(x)
            elif x.mn == 'WCMP':
                out['compares'].append(x)
            elif x.mn in ('WRTN', 'LJSR', 'XJSR'):
                if x.mn == 'WRTN':
                    pass
        if depth < max_depth:
            succ = b.succs[1:] if b.kind == 'c' else ([] if b.kind == 'j' else b.succs)
            for t in succ:
                nb = ctx.by_start.get(t)
                if nb is not None:
                    queue.append((nb, nb.instrs[0].idx if nb.instrs else 0, depth + 1))
    return out

def destination(site, ctx):
    """-> (class letter, detail)"""
    x = site.ins
    if x.mn == 'WBLM':
        dst = site.acs[3]
        cls, det, _ = classify_wptr(dst, ctx)
    else:
        dst = site.acs[2]
        cls, det, _ = site.klass[2]
    root, hops = cont_root(dst)
    if cls == 'static':
        return 'b', 'static ' + det
    if cls == 'computed' and ('static' in det or 'SD_PTR' in det or 'OBJ_PTR' in det or 'CAS_PTR' in det or 'GWB_PTR' in det):
        return 'b', det
    if x.mn == 'WBLM' and cls == 'frame':
        k = fp_offset(root) if root.kind != 'bp' else None
        if k is not None:
            scan = forward_scan(site, ctx)
            hits = []
            for call, pushes in scan['calls']:
                for pu in pushes:
                    p = parse_ea(pu.args) if pu.mn in ('XPEF', 'XPEFB') else None
                    if p and p[1] == 'ac3' and sext15(p[2]) == k:
                        hits.append(call.comment or ('call@%X' % call.pc))
                        break
            if hits:
                return ('a' if any(h in WRITE_CALLS for h in hits) else 'd'), 'frame word fp+%d -> %s' % (k, ', '.join(hits))
            return '?', 'frame word fp+%d (WBLM): no call consumer found' % k
    if cls == 'computed' and 'block-entry register based' in det:
        return 'b?', det + ' (record pointer in ac2 at block entry: almost certainly a based record field)'
    if cls == 'computed' and 'deref: pointer in frame word' in det:
        return 'p', det + ' (by-reference argument: the caller decides)'
    if cls == 'temp':
        return 'c', det
    if cls == 'frame' or (cls == 'computed' and root.kind == 'bp' and fp_offset(root.a) is not None):
        w, r = frame_word_of_dst(dst)
        if w is None:
            return '?', det
        scan = forward_scan(site, ctx)
        # length word = w-1 (varying) ; data word = w
        targets = {w, w - 1}
        hits = []
        for call, pushes in scan['calls']:
            for pu in pushes:
                p = parse_ea(pu.args) if pu.mn in ('XPEF', 'XPEFB') else None
                if p and p[1] == 'ac3':
                    d = sext15(p[2]) if not p[0] else sext15(p[2])
                    if pu.mn == 'XPEFB':
                        d = sext16(p[2]) // 2
                    if d in targets:
                        hits.append(call.comment or ('call@%X' % call.pc))
                        break
        if any(h in WRITE_CALLS for h in hits):
            return 'a', 'frame word fp%+d -> %s' % (w, ', '.join(hits))
        # consumed by another copy/compare as source?
        consumers = []
        for c in scan['copies'] + scan['compares']:
            cs = ctx.site_by_pc.get(c.pc)
            if cs is None or cs.acs is None:
                continue
            for ai in (3, 2):
                v = cs.acs[ai]
                r2, _ = cont_root(v)
                if r2.kind == 'bp':
                    k = fp_offset(r2.a)
                    if k is not None and (2 * k + r2.k) // 2 == w and c.pc != site.ins.pc:
                        consumers.append('%s@%X(%s)' % (c.mn, c.pc, 'src' if ai == 3 else 'dst'))
        if hits:
            return 'd', 'frame word fp%+d -> passed to %s' % (w, ', '.join(hits)) + (('; also ' + ', '.join(consumers)) if consumers else '')
        if consumers:
            return 'c', 'frame word fp%+d -> %s' % (w, ', '.join(consumers))
        return '?', 'frame word fp%+d: no consumer found within %d blocks' % (w, 12)
    return '?', det

# ----------------------------------------------------------------------
# Driver
# ----------------------------------------------------------------------

class Ctx:
    pass

def v_has_entry(v):
    return has_kind(v, ('entry',)) or (v.kind == 'sp' and v.tag == 'entry') or \
        (v.kind == 'lin' and any(t.kind == 'sp' and t.tag == 'entry' for _, t in v.a)) or \
        (v.kind == 'bp' and v_has_entry(v.a))

def window_of(site):
    deps = set()
    for v in site.acs:
        deps |= v.deps
    deps.discard(site.ins.idx)
    if deps:
        return min(deps), max(deps), deps
    return None, None, deps

def build_preds(blocks):
    by_start = {b.start: b for b in blocks}
    preds = collections.defaultdict(set)
    entries = set()
    prev_func = None
    for b in blocks:
        if b.func != prev_func:
            entries.add(b.start)      # first block of a routine
            prev_func = b.func
        if b.kind == 'c':
            # succs = [callee, return-point]
            if b.succs:
                entries.add(b.succs[0])
            for t in b.succs[1:]:
                preds[t].add(b.start)
        elif b.kind == 'j':
            for t in b.succs:
                entries.add(t)        # LJSR/XJSR target is a routine
        else:
            for t in b.succs:
                preds[t].add(b.start)
    return by_start, preds, entries

class Merged:
    """A phi-merged predecessor state."""
    def __init__(self, evs, block):
        self.block = block
        self.chain = []
        self.ac = []
        for i in range(4):
            vals = [e.ac[i] for e in evs]
            shows = set(v.show() for v in vals)
            if len(shows) == 1:
                self.ac.append(vals[0].with_deps(set().union(*[v.deps for v in vals])))
            else:
                uniq = []
                for v in vals:
                    if v.show() not in [u.show() for u in uniq]:
                        uniq.append(v)
                self.ac.append(V('op', a='phi', k=V('lin', a=[(1, u) for u in uniq], k=0) if False else uniq[0], b=uniq[1],
                                 deps=set().union(*[v.deps for v in vals]), tag=('phi', uniq)))
        ws = set(e.wsp.show() for e in evs)
        self.wsp = evs[0].wsp if len(ws) == 1 else V('sp', k=block.start, tag='entry')
        self.mem = {}
        for k, v in evs[0].mem.items():
            if all(k in e.mem and e.mem[k].show() == v.show() for e in evs[1:]):
                self.mem[k] = v
        pcs = set.intersection(*[set(c.pc for c in e.claims) for e in evs])
        self.claims = [c for c in evs[0].claims if c.pc in pcs]
        self.pushes = list(evs[0].pushes) if all(len(e.pushes) == len(evs[0].pushes) for e in evs) else []

def evaluate_all(blocks, ins, syms):
    by_start, preds, entries = build_preds(blocks)
    state = {}       # block start -> Evaluator (end state)
    sys.setrecursionlimit(50000)

    def ev(b, depth=0):
        if b.start in state:
            return state[b.start]
        state[b.start] = None         # provisional (cycle guard)
        inherit = None
        ps = sorted(preds.get(b.start, set()))
        if ps and b.start not in entries and depth < 20000:
            evs = []
            for pstart in ps:
                p = by_start.get(pstart)
                if p is None or p.start == b.start:
                    evs = None
                    break
                pe = ev(p, depth + 1)
                if pe is None or not pe.finished:
                    evs = None            # in a cycle: fall back to entry state
                    break
                evs.append(pe)
            if evs:
                inherit = evs[0] if len(evs) == 1 else Merged(evs, b)
        e = Evaluator(b, ins, syms, inherit)
        e.finished = False
        e.run()
        e.finished = True
        state[b.start] = e
        return e

    for b in blocks:
        if state.get(b.start) is None:
            ev(b)
    return state, preds, entries

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dis', required=True)
    ap.add_argument('--blocks', required=True)
    ap.add_argument('--tags')
    ap.add_argument('--mem', required=True)
    ap.add_argument('--symbols', required=True)
    ap.add_argument('--sites', required=True)
    ap.add_argument('--strings', required=True)
    ap.add_argument('--census', required=True)
    args = ap.parse_args()
    t0 = time.time()

    ins = load_dis(args.dis)
    blocks = load_blocks(args.blocks, ins)
    mem, regions = load_mem(args.mem)
    syms = load_symbols(args.symbols)
    mem_sha = hashlib.sha256(open(args.mem, 'rb').read()).hexdigest()
    ctx = Ctx()
    ctx.mem, ctx.regions, ctx.syms = mem, regions, syms
    ctx.blocks, ctx.ins = blocks, ins
    ctx.by_start = {b.start: b for b in blocks}

    state, preds, entries = evaluate_all(blocks, ins, syms)
    ctx.state, ctx.preds = state, preds

    sites = []
    for b in blocks:
        e = state[b.start]
        for s in e.sites:
            s.block, s.func = b, b.func
            lo, hi, deps = window_of(s)
            s.window, s.deps = (lo, hi), deps
            s.crosses = lo is not None and ins[lo].block is not b
            s.entry_dep = any(v_has_entry(v) for v in s.acs)
            mn = s.ins.mn
            if mn in ('WCMV', 'WCMP'):
                s.klass = [classify_count(s.acs[0], ctx), classify_count(s.acs[1], ctx),
                           classify_ptr(s.acs[2], ctx), classify_ptr(s.acs[3], ctx)]
            elif mn == 'WBLM':
                s.klass = [None, classify_count(s.acs[1], ctx),
                           classify_wptr(s.acs[2], ctx), classify_wptr(s.acs[3], ctx)]
            else:
                s.klass = None
            sites.append(s)
        for i, x in enumerate(b.instrs[:-1]):
            if x.mn == 'WPSH' and b.instrs[i + 1].mn == 'LDASP':
                s = Site(x, None, None, None)
                s.block, s.func = b, b.func
                s.pbr = (x.args, b.instrs[i + 1].args)
                s.klass = None
                sites.append(s)
    sites.sort(key=lambda s: s.ins.pc)
    ctx.sites = sites
    ctx.site_by_pc = {s.ins.pc: s for s in sites}
    # claims: all WMSP sites, keyed by the base wsp they extend
    ctx.claims = [s for s in sites if s.ins.mn == 'WMSP']
    for s in sites:
        if hasattr(s, 'pbr'):
            continue
        if s.ins.mn == 'WCMV':
            s.idiom, s.features = label_wcmv(s, ctx)
        if s.ins.mn in ('WCMV', 'WBLM'):
            s.dest = destination(s, ctx)
    for s in sites:
        if getattr(s, 'dest', None) is not None:
            s.udest = ultimate_dest(s, ctx)
    # WSTB single-byte stores (the 1-char concat idiom), from the evaluators
    ctx.wstb = []
    for b in blocks:
        e = state[b.start]
        for idx, addr, val in e.stores:
            if ins[idx].mn == 'WSTB':
                ctx.wstb.append((ins[idx], addr, val))

    write_strings(args.strings, sites, ctx, mem_sha)
    with open(args.sites, 'w') as f:
        f.write('# string sites — Project 29 (string_sites.py)\n')
        f.write('# classes: literal | frame | static | temp | computed; counts: const | frame | static | computed\n')
        for s in sites:
            write_site(f, s, ctx)
    with open(args.census, 'w') as f:
        write_census(f, sites, ctx)

    dt = time.time() - t0
    print('sites: %d  (WCMV %d, WCMP %d, WBLM %d, WMSP %d, STASP %d, pbr-WPSH %d)  runtime %.1fs' % (
        len(sites), *[sum(1 for s in sites if s.ins.mn == m and not hasattr(s, 'pbr')) for m in STRING_OPS],
        sum(1 for s in sites if hasattr(s, 'pbr')), dt))

def lit_word_byte(det):
    m = re.match(r'^0x([0-9A-F]+):(\d)', det)
    return int(m.group(1), 16), int(m.group(2))

def lit_text(det, cnt, mem):
    w, b = lit_word_byte(det)
    if cnt.kind == 'const':
        t = read_bytes(mem, w, b, cnt.k)
        return "'%s'" % escape(t) if t is not None else '<unreadable>'
    return '<len not const>'

def write_strings(path, sites, ctx, mem_sha):
    lits = collections.defaultdict(lambda: collections.defaultdict(list))   # (word,byte) -> len -> [(pc, role)]
    for s in sites:
        if s.ins.mn not in ('WCMV', 'WCMP'):
            continue
        for role, ai, ci in (('src', 3, 1), ('dst', 2, 0)):
            cls, det, boff = s.klass[ai]
            if cls != 'literal':
                continue
            w, b = lit_word_byte(det)
            cnt = s.acs[ci]
            n = cnt.k if cnt.kind == 'const' else None
            lits[(w, b)][n].append((s.ins.pc, role + ('' if boff is None else '+off')))
    multi = 0
    with open(path, 'w') as f:
        f.write('# quest.strings — Project 29 literal table (tools/string_sites.py)\n')
        f.write('# provenance: Disassembled/quest.mem sha256 %s\n' % mem_sha)
        f.write('# columns: word:byte  len  nsites  text (escaped)  sites (pc[role])\n')
        f.write('# len=? : the count at the site is not a constant (text shown up to 40 bytes, unterminated)\n')
        for (w, b), bylen in sorted(lits.items()):
            if len(bylen) > 1:
                multi += 1
            for n in sorted(bylen, key=lambda k: (k is None, k or 0)):
                if n is not None:
                    raw = read_bytes(ctx.mem, w, b, n)
                    text = ("'%s'" % escape(raw)) if raw is not None else '<unreadable: outside quest.mem>'
                else:
                    raw = read_bytes(ctx.mem, w, b, 40)
                    text = ("~'%s...'" % escape(raw)) if raw is not None else '<unreadable>'
                pcs = ' '.join('%X[%s]' % (pc, role) for pc, role in bylen[n])
                f.write('%X:%d\t%s\t%d\t%s\t%s\n' % (w, b, n if n is not None else '?', len(bylen[n]), text, pcs))
            if len(bylen) > 1:
                f.write('#   ^ FINDING: literal used with %d different lengths: %s\n' % (
                    len(bylen), ', '.join(str(k) for k in sorted(bylen, key=lambda k: (k is None, k or 0)))))
        f.write('# %d literals, %d used with more than one length\n' % (len(lits), multi))
    ctx.lits = lits

def write_site(f, s, ctx):
    x = s.ins
    f.write('\n== %08X %s  block %08X  func %s\n' % (x.pc, x.mn, s.block.start, s.func))
    if hasattr(s, 'pbr'):
        f.write('   pass-by-reference temp: WPSH %s ; LDASP %s\n' % s.pbr)
        return
    lo, hi = s.window
    flags = []
    if s.crosses:
        flags.append('WINDOW CROSSES BLOCK BOUNDARY (unique-predecessor chain)')
    if s.entry_dep:
        flags.append('DEPENDS ON UNRESOLVED BLOCK-ENTRY STATE')
    if lo is not None:
        n = hi - lo + 1
        f.write('   window %08X..%08X (%d instr)%s\n' % (ctx.ins[lo].pc, ctx.ins[hi].pc, n, ('  ' + '; '.join(flags)) if flags else ''))
        shown = 0
        last_block = None
        for i in range(lo, hi + 1):
            xi = ctx.ins[i]
            if xi.block is not last_block and last_block is not None:
                f.write('     ---- block %08X\n' % xi.block.start)
            last_block = xi.block
            if n > 40 and i not in s.deps and xi.mn not in STRING_OPS:
                continue
            f.write('     %08X %s%s\n' % (xi.pc, xi.raw, '' if i in s.deps else '   .'))
    else:
        f.write('   window: none%s\n' % (('  ' + '; '.join(flags)) if flags else ''))
    names = ['ac0', 'ac1', 'ac2', 'ac3']
    if x.mn in ('WCMV', 'WCMP', 'WBLM'):
        roles = {'WCMV': ['dst_count', 'src_count', 'dst', 'src'], 'WCMP': ['dst_count', 'src_count', 'dst', 'src'],
                 'WBLM': ['-', 'count', 'src', 'dst']}[x.mn]
        for i in range(4):
            if s.klass[i] is None:
                continue
            if i < 2:
                cls, det = s.klass[i]
                boff = None
            else:
                cls, det, boff = s.klass[i]
            extra = ''
            if cls == 'literal':
                extra = '  ' + lit_text(det, s.acs[1] if i == 3 else s.acs[0], ctx.mem)
            if boff is not None and not boff.startswith('end@'):
                extra += '  BYTE OFFSET %s' % boff
            f.write('   %s %-9s %-8s %s%s   [%s]\n' % (names[i], roles[i], cls, det, extra, s.acs[i].show()))
        if x.mn == 'WCMV':
            f.write('   idiom %s   features: %s\n' % (s.idiom, ', '.join(s.features) or '-'))
        if x.mn in ('WCMV', 'WBLM'):
            f.write('   dest (%s) %s\n' % s.dest)
            if s.udest != s.dest:
                f.write('   ultimately (%s) %s\n' % s.udest)
    elif x.mn == 'WMSP':
        n = int(x.args)
        v = s.acs[n]
        f.write('   claim size (wides) = %s   wsp before = %s\n' % (v.show(), s.wsp.show()))
    elif x.mn == 'STASP':
        n = int(x.args)
        f.write('   wsp := %s   (claims seen on this path: %s)\n' % (s.acs[n].show(),
                ', '.join('%X' % c.pc for c in s.claims) or 'none'))

def write_census(f, sites, ctx):
    real = [s for s in sites if not hasattr(s, 'pbr')]
    W = f.write
    W('# string_sites.py raw census (input to docs/Project29/Census.md)\n\n')
    cnt = collections.Counter('pbr-WPSH' if hasattr(s, 'pbr') else s.ins.mn for s in sites)
    W('## counts\n')
    for k, v in sorted(cnt.items()):
        W('%-10s %d\n' % (k, v))
    W('WSTB       %d (byte stores; %d with a constant byte)\n' % (len(ctx.wstb), sum(1 for _, _, v in ctx.wstb if v.kind == 'const')))

    wcmv = [s for s in real if s.ins.mn == 'WCMV']
    W('\n## WCMV operand-class signatures (dst_count/src_count/dst/src)\n')
    sig = collections.Counter(); ex = {}
    for s in wcmv:
        k = '/'.join(c[0] for c in s.klass)
        sig[k] += 1; ex.setdefault(k, s.ins.pc)
    for k, v in sig.most_common():
        W('%5d  %-42s e.g. %X\n' % (v, k, ex[k]))

    W('\n## WCMV idioms\n')
    sig = collections.Counter(); ex = {}
    for s in wcmv:
        sig[s.idiom] += 1; ex.setdefault(s.idiom, s.ins.pc)
    for k, v in sig.most_common():
        W('%5d  %-40s e.g. %X\n' % (v, k, ex[k]))
    W('\n## WCMV features\n')
    fe = collections.Counter(); ex = {}
    for s in wcmv:
        for x in s.features:
            key = re.sub(r'\(\d+<-\d+\)', '', x)
            fe[key] += 1; ex.setdefault(key, s.ins.pc)
    for k, v in fe.most_common():
        W('%5d  %-40s e.g. %X\n' % (v, k, ex[k]))
    W('\n## pad-fill / truncate sites (dst_count != src_count, both constant)\n')
    for s in wcmv:
        for x in s.features:
            if x.startswith('pad-fill') or x.startswith('truncates'):
                W('%X %-22s %s  dst=%s  src=%s\n' % (s.ins.pc, x, s.func, s.klass[2][1][:40], s.klass[3][1][:40]))

    W('\n## WCMV per-operand class totals\n')
    for i, name in enumerate(('dst_count', 'src_count', 'dst', 'src')):
        c = collections.Counter(s.klass[i][0] for s in wcmv)
        W('%-9s %s\n' % (name, '  '.join('%s=%d' % kv for kv in c.most_common())))

    W('\n## destinations of WCMV/WBLM writes (direct, then ultimate through scratch buffers)\n')
    ds = [s for s in real if s.ins.mn in ('WCMV', 'WBLM')]
    d1 = collections.Counter(s.dest[0] for s in ds)
    d2 = collections.Counter(s.udest[0] for s in ds)
    W('class  direct  ultimate   meaning\n')
    meaning = {'a': 'buffer fed to ?WRITE_SCREEN/?WRITE (ephemeral, visible)', 'b': 'game state (static / shared-data record via SD_PTR,OBJ_PTR,CAS_PTR)',
               'b?': 'record via ac2 at block entry (based record field; pointer origin not resolved)', 'c': 'temp / scratch consumed only by another WCMV or WCMP',
               'd': 'frame local passed to some other call', 'p': 'written through a by-reference argument (caller decides)', '?': 'unresolved'}
    for k in ('a', 'b', 'b?', 'c', 'd', 'p', '?'):
        W('%-5s %6d %9d   %s\n' % (k, d1.get(k, 0), d2.get(k, 0), meaning[k]))
    W('\n### (b) game-state writes, in full (ultimate class b)\n')
    for s in ds:
        if s.udest[0] in ('b', 'b?'):
            W('%X %-4s %-22s %s\n' % (s.ins.pc, s.ins.mn, s.func, s.udest[1]))
    W('\n### (p) by-reference argument writes\n')
    for s in ds:
        if s.udest[0] == 'p':
            W('%X %-4s %-22s %s\n' % (s.ins.pc, s.ins.mn, s.func, s.udest[1]))
    W('\n### (d) passed to other calls\n')
    for s in ds:
        if s.udest[0] == 'd':
            W('%X %-4s %-22s %s\n' % (s.ins.pc, s.ins.mn, s.func, s.udest[1]))
    W('\n### unresolved\n')
    for s in ds:
        if s.udest[0] == '?':
            W('%X %-4s %-22s %s\n' % (s.ins.pc, s.ins.mn, s.func, s.udest[1]))

    W('\n## WCMP sites\n')
    for s in real:
        if s.ins.mn == 'WCMP':
            nxt = [ctx.ins[s.ins.idx + 1].mn, ctx.ins[s.ins.idx + 2].raw]
            W('%X %-20s dst_count=%-28s src_count=%-28s dst=%-40s src=%-40s then %s\n' % (
                s.ins.pc, s.func, s.klass[0][1][:28], s.klass[1][1][:28], s.klass[2][1][:40], s.klass[3][1][:40], nxt[1]))

    W('\n## WBLM sites\n')
    for s in real:
        if s.ins.mn == 'WBLM':
            W('%X %-20s count=%-8s src=%-45s dst=%-45s  dest(%s)\n' % (s.ins.pc, s.func, s.klass[1][1], s.klass[2][1][:45], s.klass[3][1][:45], s.udest[0]))

    W('\n## WMSP claims\n')
    for s in real:
        if s.ins.mn == 'WMSP':
            n = int(s.ins.args)
            W('%X %-20s size(wides)=%-60s wsp_before=%s\n' % (s.ins.pc, s.func, s.acs[n].show()[:60], s.wsp.show()))
    W('\n## STASP releases\n')
    for s in real:
        if s.ins.mn == 'STASP':
            n = int(s.ins.args)
            W('%X %-20s wsp:=%-50s claims on path: %s\n' % (s.ins.pc, s.func, s.acs[n].show()[:50], ', '.join('%X' % c.pc for c in s.claims) or 'none'))
    W('\n## WMSP/STASP per function\n')
    per = collections.defaultdict(lambda: [0, 0])
    for s in real:
        if s.ins.mn == 'WMSP':
            per[s.func][0] += 1
        if s.ins.mn == 'STASP':
            per[s.func][1] += 1
    for fn, (a, b) in sorted(per.items()):
        W('%-22s WMSP %2d  STASP %2d\n' % (fn, a, b))

    W('\n## pass-by-reference WPSH temps\n')
    for s in sites:
        if hasattr(s, 'pbr'):
            W('%X %-20s WPSH %s ; LDASP %s\n' % (s.ins.pc, s.func, s.pbr[0], s.pbr[1]))

    W('\n## WSTB constant bytes\n')
    c = collections.Counter()
    for x, addr, v in ctx.wstb:
        if v.kind == 'const':
            c['0x%02X' % (v.k & 0xFF)] += 1
    for k, v in c.most_common():
        W('%5d  %s\n' % (v, k))

    W('\n## windows\n')
    W('crossing a block boundary: %d ; depending on unresolved block-entry state: %d\n' % (
        sum(1 for s in real if s.crosses), sum(1 for s in real if s.entry_dep)))
    W('entry-dependent sites: %s\n' % ' '.join('%X' % s.ins.pc for s in real if s.entry_dep))
    W('\n## literals used with more than one length\n')
    for (w, b), bylen in sorted(ctx.lits.items()):
        if len(bylen) > 1:
            W('%X:%d lengths %s\n' % (w, b, ', '.join('%s(x%d)' % (k, len(v)) for k, v in bylen.items())))

if __name__ == '__main__':
    main()
